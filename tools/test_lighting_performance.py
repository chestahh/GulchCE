"""Check that benchmark reports retain real stalls and separate capture overhead."""
import re
from unittest.mock import MagicMock, Mock
import pytest
from lighting_performance_run import distribution, summarize, sustained_pacing, memory_window_mib, runtime_errors
from lighting_performance_build import ROOT, instrument_present_stalls, instrument_shader_stalls, instrument_renderer_profile, instrument_bsp_lights
from harness import function


@pytest.mark.parametrize('name', ['prepare_draw', 'buffer_upload'])
def test_renderer_timers_preserve_the_production_body(name):
    source = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    timed = instrument_renderer_profile(source)
    assert function(timed, 'lighting_timed_'+name).replace('lighting_timed_'+name+'(', name+'(', 1) == function(source, name)
    wrapper = function(timed, name)
    assert wrapper.count('lighting_profile_begin(') == wrapper.count('lighting_profile_end(') == 1
    assert wrapper.index('lighting_profile_begin(') < wrapper.index('lighting_timed_'+name+'(') < wrapper.index('lighting_profile_end(')


@pytest.mark.parametrize('cluster_lists,path', [
    (True, 'source/objects/object_lights.c'),
    (False, 'source/structures/structure_render.c'),
])
def test_bsp_timers_preserve_light_geometry_and_draw_order(cluster_lists, path):
    source = (ROOT/path).read_text()
    timed = instrument_bsp_lights(source, cluster_lists)
    phases = (28, 32) if cluster_lists else (29, 30, 31, 33, 34, 35)
    for phase in phases:
        begin, end = (f'lighting_profile_{op}({phase});' for op in ('begin', 'end'))
        assert timed.count(begin) == timed.count(end) == 1
        assert timed.index(begin) < timed.index(end)
    assert re.sub(r'^[\t ]*lighting_profile_(?:begin|end)\(\d+\);\n', '', timed, flags=re.M) == source


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


@pytest.mark.parametrize('name,operation,phase', [
    ('visibility_send_batch', 'glGetQueryBufferObjectuiv', 0),
    ('D3DDevice_Present', 'platform_video_swap', 1),
    ('frame_pacing_wait', 'glClientWaitSync', 2),
    ('D3DDevice_Present', 'visibility_read_batch', 3),
])
def test_present_stall_clocks_preserve_driver_operations(name, operation, phase):
    source = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    original = function(source, name)
    measured = function(instrument_present_stalls(source), name)
    assert measured.count(operation + '(') == original.count(operation + '(') == 1
    end = measured.index(f'lighting_stall_present_end({phase});')
    start = measured.rindex('lighting_stall_present_begin();', 0, end)
    assert start < measured.index(operation + '(') < end
    if phase == 0:
        assert start < measured.index('for (', start) < measured.index(operation + '(')


def test_present_stall_instrumentation_only_adds_clock_calls():
    original = (ROOT/'port/linux/src/d3d8_gl.c').read_text()
    timed = instrument_present_stalls(original)
    driver, replacements = re.subn(
        r'^(?:void lighting_stall_present_(?:begin\(void\)|end\(int\))|'
        r'\t\tlighting_stall_present_(?:begin\(\)|end\([0-3]\)));\n',
        '', timed, flags=re.M)
    assert replacements == 10
    assert driver == original


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
               occlusion_updates=1, capture_excluded=0, tick=t*30, shadowed=t%2, sun_casters=t) for t in range(60)]
    rows[31].update(shadowed=2, scene_lights=7)
    result = summarize(rows, 30, 30, 240)
    assert result['shadowed_frame_percent'] == 50
    assert result['shadowed_max'] == 2
    assert result['shadowed_mean'] == pytest.approx(16/30)
    assert result['multiple_shadow_frame_percent'] == pytest.approx(100/30)
    assert result['scene_lights_mean'] == pytest.approx(4.1)
    assert result['sun_families_mean'] == 44.5
    for row in rows:
        del row['shadowed']
        del row['sun_casters']
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


def test_sustained_report_distinguishes_repeated_and_continuous_dips():
    def report(counts, captures=False, complete=True):
        rows = []
        for second, count in enumerate([1, *counts, 1]):
            for i in range(count):
                rows.append(dict(seconds=second+i/count, frame_ms=4, selected=0,
                                 scene_lights=0, occlusion_updates=0, capture_excluded=0))
        return summarize(rows if complete else rows[:-1], 1, len(counts), 240, captures)

    # Identical totals and frame-time distributions can hide different pacing.
    clustered = [240, 230, 230, 0, 250, 250]
    dispersed = [230, 240, 230, 250, 0, 250]
    a, b = report(clustered), report(dispersed)
    assert a['raw'] == b['raw']
    assert a['sustained']['fps_per_second'] == clustered
    assert a['sustained']['targets']['240'] == dict(seconds_below=3, episodes=1, longest_below_seconds=3)
    assert b['sustained']['targets']['240'] == dict(seconds_below=3, episodes=3, longest_below_seconds=1)
    assert a['sustained']['targets']['360']['longest_below_seconds'] == len(clustered)
    assert 'sustained' not in report(clustered, captures=True)
    assert 'sustained' not in report(clustered, complete=False)


def test_tick_times_follow_completion_windows_and_skip_resets():
    rows = [dict(seconds=t, tick=tick, frame_ms=ms) for t, tick, ms in [
        (2.1, 8, 4), (2.5, 8, 3), (2.8, 9, 5), (3.1, 9, 7),
        (4.1, 1, 9), (4.3, 1, 2), (4.6, 3, 11),
    ]]
    result = sustained_pacing(rows, 2, 3, [3])
    assert result['tick_frame_times']['3'] == {
        'at_or_above': {'same_tick': dict(frames=2, mean_ms=2.5),
                        'tick_advanced': dict(frames=2, mean_ms=8)},
        'below': {'same_tick': dict(frames=1, mean_ms=7)},
    }
    for row in rows:
        del row['tick']
    assert 'tick_frame_times' not in sustained_pacing(rows, 2, 3, [3])


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

def test_combat_stages_camera_only_after_checkpoint_acknowledgement(tmp_path, monkeypatch):
    import lighting_combat_core as combat

    (tmp_path/'frames.csv').touch()
    console = MagicMock()
    monkeypatch.setattr(combat.socket, 'create_connection', lambda *args: console)
    load = b'(begin (sleep_until (= (game_time) 62) 1) (core_load_name canyon.bin))\r\n'
    camera = b'(begin (sleep_until (= (game_time) 540) 1) (debug_camera_load))\r\n'

    def replies():
        assert [call.args[0] for call in console.sendall.call_args_list] == [load]
        yield b"loaded 'can"
        assert console.sendall.call_count == 1
        yield b"yon.bin'"
        assert [call.args[0] for call in console.sendall.call_args_list] == [load, camera]
        yield b''

    console.recv.side_effect = replies()
    combat.play(tmp_path, Mock(poll=lambda: None), 0)
    assert (tmp_path/'stage.log').read_bytes() == b"loaded 'canyon.bin'"


# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
