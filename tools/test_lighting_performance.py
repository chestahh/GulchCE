"""Check that benchmark reports retain real stalls and separate capture overhead."""
import pytest
from lighting_performance_run import distribution, summarize, memory_window_mib, runtime_errors
from lighting_performance_build import ROOT, instrument_shader_stalls, instrument_renderer_profile
from harness import function


@pytest.mark.parametrize('name', ['prepare_draw', 'buffer_upload'])
def test_renderer_timers_preserve_the_production_body(name):
    source = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    timed = instrument_renderer_profile(source)
    assert function(timed, 'lighting_timed_'+name).replace('lighting_timed_'+name+'(', name+'(', 1) == function(source, name)
    wrapper = function(timed, name)
    assert wrapper.count('lighting_profile_begin(') == wrapper.count('lighting_profile_end(') == 1
    assert wrapper.index('lighting_profile_begin(') < wrapper.index('lighting_timed_'+name+'(') < wrapper.index('lighting_profile_end(')


@pytest.mark.parametrize('name,kind', [('xgpu_compile_shader', 0),
                                     ('xgpu_link_program', 1), ('program_get', 1)])
def test_shader_stall_clocks_cover_real_compile_and_link_paths(name, kind):
    source = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    body = function(instrument_shader_stalls(source), name)
    begin = body.index(f'lighting_stall_shader_begin({kind});')
    operation = body.index('glCompileShader(' if kind == 0 else 'glLinkProgram(')
    status = body.index('GL_COMPILE_STATUS' if kind == 0 else 'GL_LINK_STATUS')
    end = body.index(f'lighting_stall_shader_end({kind});')
    assert begin < operation < status < end
    assert body.count('lighting_stall_shader_begin(') == 1
    assert body.count('lighting_stall_shader_end(') == 1


def test_binary_cache_status_does_not_end_a_link_timer():
    source = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    body = function(instrument_shader_stalls(source), 'program_cache_load')
    assert 'lighting_stall_shader_' not in body


def test_percentiles_and_slow_frame_budget():
    result=distribution([10]*98+[50,100],100)
    assert result['p99_ms']==50
    assert result['low_1pct_fps']==10
    assert result['over_1_5_budget_percent']==2
    assert result['over_50ms']==1


def test_warmup_capture_and_minute_boundaries():
    rows=[dict(seconds=t,frame_ms=10,selected=2,scene_lights=4,
               occlusion_updates=1,capture_excluded=0,tick=t*30) for t in range(160)]
    rows[35].update(frame_ms=1000,capture_excluded=1)
    rows[95]['frame_ms']=200  # Real hitch must remain in both overall and per-minute data.
    result=summarize(rows,30,120,100)
    assert result['raw']['frames']==120 and result['pacing']['frames']==119
    assert result['raw']['worst_ms']==1000 and result['pacing']['worst_ms']==200
    assert result['minutes'][0]['worst_ms']==10
    assert result['minutes'][1]['worst_ms']==200
    assert result['measured_span']==119
    assert result['selected_max']==2


def test_missing_frame_samples_are_not_a_pass():
    with pytest.raises(ValueError,match='No frame samples'):
        distribution([],60)


def test_shadow_coverage_distinguishes_fallback_from_active_maps():
    rows=[dict(seconds=t, frame_ms=4, selected=2, scene_lights=4,
               occlusion_updates=1, capture_excluded=0, tick=t*30, shadowed=t%2) for t in range(60)]
    rows[31]['shadowed'] = 2
    result = summarize(rows, 30, 30, 240)
    assert result['shadowed_frame_percent'] == 50
    assert result['shadowed_max'] == 2
    assert result['shadowed_mean'] == pytest.approx(16/30)
    assert result['multiple_shadow_frame_percent'] == pytest.approx(100/30)
    for row in rows:
        del row['shadowed']
    assert 'shadowed_frame_percent' not in summarize(rows, 30, 30, 240)


def test_disabled_capture_keeps_stalls_even_with_old_fixture_markers():
    rows=[dict(seconds=t,frame_ms=10,selected=2,scene_lights=4,
               occlusion_updates=1,capture_excluded=0,tick=t*30) for t in range(60)]
    rows[35].update(frame_ms=200,capture_excluded=1)
    result=summarize(rows,30,30,120,captures=False)
    assert result['pacing']==result['raw']
    assert result['pacing']['worst_ms']==200
    assert result['excluded_capture_frames']==0
    assert result['minutes'][0]['worst_ms']==200


def test_early_exit_keeps_missing_memory_windows_distinct_from_zero():
    samples=[dict(seconds=45,private=100*2**20),dict(seconds=50,private=120*2**20)]
    assert memory_window_mib(samples,45,75)==110
    assert memory_window_mib(samples,585) is None


def test_240_fps_budget_counts_sub_120_stalls():
    result = distribution([4.0, 4.16, 4.18, 8.0], 240)
    assert result['budget_ms'] == pytest.approx(1000 / 240)
    assert result['over_budget_percent'] == 50
    assert result['over_1_5_budget_percent'] == 25


@pytest.mark.parametrize('custom', [False, True])
def test_campaign_route_owns_and_releases_input(tmp_path, custom):
    import os
    import subprocess
    from test_mcc_identity import compile_tool
    source = tmp_path / 'route.c'
    source.write_text(r'''
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define TRUE 1
#define FALSE 0
#define NUMBEROF(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define XINPUT_GAMEPAD_X 2
#define XINPUT_GAMEPAD_Y 3
#define XINPUT_GAMEPAD_LEFT_TRIGGER 6
typedef struct { short sThumbLX,sThumbLY,sThumbRX,sThumbRY; unsigned short wButtons;
 unsigned char bAnalogButtons[8]; } XINPUT_GAMEPAD;
''' + function((ROOT/'tools/fixtures/lighting_performance.c').read_text(), 'lighting_snowy_route') + r'''
int main(void) {
 XINPUT_GAMEPAD pad, original, zero;
 memset(&zero,0,sizeof(zero)); memset(&pad,0x7f,sizeof(pad)); original=pad;
 if(lighting_snowy_route(&pad,-1) || memcmp(&pad,&original,sizeof(pad))) return 1;
 if(getenv("HALO_LIGHTING_BENCH_ROUTE_FILE")) {
  if(!lighting_snowy_route(&pad,0) || pad.sThumbLX!=12 || pad.sThumbLY!=13 || pad.sThumbRX!=14 ||
     pad.bAnalogButtons[2]!=255 || pad.bAnalogButtons[3]!=255 || pad.bAnalogButtons[6]!=255) return 5;
  if(!lighting_snowy_route(&pad,1) || memcmp(&pad,&zero,sizeof(pad))) return 6;
 } else {
  if(!lighting_snowy_route(&pad,0) || memcmp(&pad,&zero,sizeof(pad))) return 2;
  memset(&pad,0x7f,sizeof(pad));
  if(!lighting_snowy_route(&pad,3) || pad.sThumbLY!=32767 || pad.sThumbRY ||
     pad.wButtons || pad.bAnalogButtons[6] || pad.bAnalogButtons[7]) return 3;
 }
 memset(&pad,0x7f,sizeof(pad));
 if(!lighting_snowy_route(&pad,1000) || memcmp(&pad,&zero,sizeof(pad))) return 4;
 return 0;
}
''')
    env = dict(os.environ)
    env.pop('HALO_LIGHTING_BENCH_TURN', None)
    env.pop('HALO_LIGHTING_BENCH_ROUTE_FILE', None)
    if custom:
        route = tmp_path/'route.txt'
        route.write_text('1 12 13 14 7\n2 0 0 0 0\n')
        env['HALO_LIGHTING_BENCH_ROUTE_FILE'] = str(route)
    result = subprocess.run([str(compile_tool(tmp_path, [source]))], capture_output=True, env=env)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize('message', [
    'GPU: viewmodel shadow map unavailable; retaining existing lighting until reset',
    'GPU: modern shadow resources unavailable; retaining existing lighting until reset',
    'GPU: modern shadow mesh upload failed; retaining existing lighting until reset',
])
def test_shadow_resource_fallback_fails_acceptance(message):
    assert runtime_errors('halo-linux: '+message+'\n')
    assert not runtime_errors('halo-linux: shadow maps: 1024x1024\n')


@pytest.mark.parametrize('acknowledged,occupancy,camera,accepted', [
    (True, 19, '327.8 -649.3 74.5', True),
    (False, 19, '327.8 -649.3 74.5', False),
    (True, 0, '327.8 -649.3 74.5', False),
    (True, 19, '0.0 0.0 0.0', False),
])
def test_combat_core_requires_the_saved_fight(tmp_path, acknowledged, occupancy, camera, accepted):
    from lighting_combat_core import verify
    reply = "loaded 'canyon.bin'\n" if acknowledged else 'load failed\n'
    (tmp_path/'stage.log').write_text(reply + f'(ai_living_count c3_base_tier_2)\n{occupancy}\n' * 4)
    log = ''.join(f'network test: tick {tick} player 0: local 0 camera ({camera})\n' for tick in range(0, 900, 30))
    if accepted:
        assert verify(tmp_path, log, 25, 30, (0, 899)) == [19] * 4
    else:
        with pytest.raises(ValueError):
            verify(tmp_path, log, 25, 30, (0, 899))
