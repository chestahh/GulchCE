"""Run real shadow-setting transactions and verify a portable process restart."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tomllib

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root', type=Path, required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/modern-shadow-menu-build')
    parser.add_argument('--inspect-modern', action='store_true', help='Capture the modern frontend scrolled to the shadow row')
    parser.add_argument('--resolution-setting', action='store_true', help='Verify the shared resolution control instead of Off/Hard/Soft')
    parser.add_argument('--frontend', choices=['off', 'modern'], default='off')
    args = parser.parse_args()
    assert re.fullmatch(r'[A-Za-z0-9_-]+', args.label)
    out = ROOT/'build/modern-shadow-menu'/args.label
    build = args.build_dir.resolve()
    assert build.is_relative_to((ROOT/'build').resolve())
    out.mkdir(parents=True, exist_ok=False)
    (out/'maps').mkdir(); (out/'screenshots').mkdir()
    shutil.copy2(args.data_root/'maps/ui.map', out/'maps/ui.map')
    # Portable config belongs beside the executable, not the data-root override.
    shutil.copy2(build/'halo-menu.exe', out/'halo-menu.exe')
    shutil.copy2(build/'manifest.json', out/'manifest.json')
    manifest = json.loads((out/'manifest.json').read_text())
    assert hashlib.sha256((out/'halo-menu.exe').read_bytes()).hexdigest() == manifest['exe_sha256']
    (out/'config.toml').write_text('[display]\nmenu_frontend="off"\nmodern_shadows="off"\ndynamic_lighting_reset=true\nmode="windowed"\n')
    env = {k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
    env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='1',
               HALO_NO_AUDIO='1', HALO_WINDOW_SIZE='2560x1440', HALO_MAX_FPS='60', HALO_NO_VSYNC='1',
               HALO_EXIT_AFTER='30', HALO_NET_ONLINE='0', HALO_UPDATE_ANSWER='never', HALO_SHADOW_MENU_ACCEPT='1',
               HALO_MENU_FRONTEND='off', HALO_MENU_OPEN='main_menu/settings_select/player_setup/player_profile_edit/video_settings/dynamic_lighting/dynamic_lighting_settings_screen',
               HALO_SCREENSHOT_DIR=str(out/'screenshots'), HALO_SCREENSHOT_EVERY='60')
    env['PATH'] = str(ROOT/'build/windows')+os.pathsep+env.get('PATH','')
    if args.frontend == 'modern':
        env.update(HALO_MENU_FRONTEND='modern', HALO_MENU_OPEN='modern/lighting/settings')
    if args.inspect_modern:
        env.update(HALO_MENU_FRONTEND='modern', HALO_MENU_OPEN='modern/lighting/settings', HALO_SHADOW_MENU_INSPECT='1')
    if args.resolution_setting:
        env['HALO_SHADOW_MENU_RESOLUTION'] = '1'
        env['HALO_MENU_OPEN'] = ('modern/graphics/settings' if env['HALO_MENU_FRONTEND'] == 'modern' else
                                'main_menu/settings_select/player_setup/player_profile_edit/video_settings/graphics/graphics_settings_screen')
    for label in (('actions',) if args.inspect_modern else ('actions', 'restart')):
        if label == 'restart': env.update(HALO_SHADOW_MENU_VERIFY='1', HALO_SCREENSHOT_EVERY='0')
        with (out/(label+'.log')).open('wb') as log:
            result = subprocess.run([str(out/'halo-menu.exe')], cwd=out, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=55, creationflags=subprocess.CREATE_NO_WINDOW)
        assert result.returncode == 0, label
    log = (out/'actions.log').read_text(errors='replace')
    if args.inspect_modern:
        assert re.findall(r'SHADOW MENU inspect (\d+)', log) == [str(i) for i in range(18 if args.resolution_setting else 9)]
        (out/'report.json').write_text(json.dumps(dict(inspected=True, resolution='2560x1440'), indent=2))
        print('Modern frontend shadow-setting captures ready for visual inspection')
        return
    if args.resolution_setting:
        observed = re.findall(r'SHADOW MENU resolution step (\d+) saved (\d+) selected (\d+) modern (\w+)', log)
        expected = [(128,0),(128,5),(128,5),(128,0),(256,1),(512,2),(1024,3),(2048,4),(4096,5),
                    (4096,0),(4096,0),(4096,5),(128,0),(1024,3),(1024,3)]
        assert observed == [(str(i),str(value),str(index),'off') for i,(value,index) in enumerate(expected)], observed
        assert 'SHADOW MENU resolution restart 1024 selected 3' in (out/'restart.log').read_text(errors='replace')
        assert tomllib.loads((out/'config.toml').read_text())['display']['shadow_resolution'] == 1024
        (out/'report.json').write_text(json.dumps(dict(passed=True, stages=observed, restart=1024, frontend=args.frontend),indent=2))
        print('Resolution options, Save/Cancel/Defaults and restart persistence passed; modern shadows stayed off')
        return
    expected = ['off','off','off','hard','soft','soft','soft','off','soft','soft']
    observed = re.findall(r'SHADOW MENU step (\d+) saved (\w+) selected (\d+) lights (\d+) (\d+)', log)
    assert [(s,m) for s,m,*_ in observed] == [(str(i),v) for i,v in enumerate(expected)], observed
    assert all(a == b == '0' for _,_,_,a,b in observed), observed
    restart = (out/'restart.log').read_text(errors='replace')
    assert 'SHADOW MENU restart soft selected 2' in restart
    for text in (log,restart):
        paths = re.findall(r'^halo-linux: settings: ([^\r\n]*config\.toml)', text, re.M)
        assert paths and all(Path(p).resolve() == (out/'config.toml').resolve() for p in paths)
    assert tomllib.loads((out/'config.toml').read_text())['display']['modern_shadows'] == 'soft'
    (out/'report.json').write_text(json.dumps(dict(passed=True, stages=observed, restart='soft', resolution='2560x1440'),indent=2))
    print('Menu Save/Cancel/Defaults and restart persistence passed')


if __name__ == '__main__':
    main()
