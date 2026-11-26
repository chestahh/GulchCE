"""Build a passive Windows multiplayer policy probe, then restore the normal build."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

from harness import function

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/lighting-compatibility-build')
    options=parser.parse_args()
    output=options.output_dir.resolve()
    assert output.is_relative_to((ROOT/'build').resolve())
    output.mkdir(parents=True,exist_ok=True)
    fixture = ROOT/'port/linux/game/lighting_compatibility_fixture.c'
    assert not fixture.exists()
    target = ROOT/'port/linux/game/plasma_lighting.c'
    original = target.read_bytes()
    shadow = ROOT/'port/linux/game/modern_shadows.c'
    shadow_original = shadow.read_bytes()
    input_source = ROOT/'port/linux/src/xinput_sdl.c'
    input_original = input_source.read_bytes()
    policy = ROOT/'port/linux/game/network_shield_effects.c'
    policy_original = policy.read_bytes()
    network = ROOT/'port/linux/game/network_test.c'
    network_original = network.read_bytes()
    args = shlex.split(re.search(r'^configure_args = (.*)$', (ROOT/'build.ninja').read_text(), re.M)[1])
    manifest = dict(revision=subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=ROOT).strip(),
                    configure_args=args, source_sha256=hashlib.sha256(original).hexdigest(),
                    fixture_sha256=hashlib.sha256((ROOT/'tools/fixtures/lighting_compatibility.c').read_bytes()).hexdigest())
    manifest['emitter_hashes'] = {name: hashlib.sha256((ROOT/'port/linux/game'/name).read_bytes()).hexdigest()
                                for name in ('authored_emitter_lighting.c', 'ambient_emitter_lighting.c',
                                             'covenant_emitter_lighting.c', 'lighting_menu.c', 'lighting_config.c',
                                             'plasma_collision.c', 'plasma_occlusion.c')}
    manifest['collision_hashes'] = {name: hashlib.sha256((ROOT/'source/physics'/name).read_bytes()).hexdigest()
                                    for name in ('collision_bsp.c', 'collision_bsp.h')}
    manifest['shadow_sha256']=hashlib.sha256(shadow_original).hexdigest()
    manifest['input_sha256']=hashlib.sha256(input_original).hexdigest()
    manifest['policy_sha256']=hashlib.sha256(policy_original).hexdigest()
    try:
        # Drive a new selection through the usual listen-server lobby. The
        # joining peer learns the map through the game's normal messages.
        text = network_original.decode().replace('\r\n', '\n')
        anchor = '\t\t\t\t/* (a level of its own path, else a multiplayer map\'s name) */'
        assert text.count(anchor) == 1
        choose = '''                char key[64];
                const char *choice;
                snprintf(key, sizeof(key), "HALO_SHADOW_TEST_MAP_%d", network_test.variant_index + 1);
                choice = getenv(key);
                if (choice && *choice && strlen(choice) < sizeof(network_test.map_name))
                    snprintf(network_test.map_name, sizeof(network_test.map_name), "%s", choice);
'''
        network.write_text(text.replace(anchor, choose + anchor))
        text = policy_original.decode().replace('\r\n', '\n')
        for anchor, extra in [('\tclient_effects = _shield_effects_stock;', '\n\tlighting_policy_received = 0;'),
                              ('\tclient_effects = (short)message.effects;', '\n\tlighting_policy_received = 1;')]:
            assert text.count(anchor) == 1
            text = text.replace(anchor, anchor + extra)
        policy.write_text('static int lighting_policy_received;\n'+text+
            '\nint lighting_compatibility_policy_received(void) { return game_connection() != _game_connection_network_client || lighting_policy_received; }\n')
        # The network damage probe does not fire a weapon. Add ordinary trigger
        # input to the stationary look sequence for real muzzle-light coverage.
        text = input_original.decode().replace('\r\n', '\n')
        anchor = '\t\tpad->sThumbRY = (SHORT)(sin(t * 0.3) * 32000.0);'
        assert text.count(anchor) == 1
        input_source.write_text(text.replace(anchor, anchor+'\n\t\tif (getenv("HALO_SHADOW_COMPAT_FIRE") && fmod(t, 3.0) < 0.3) pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = 255;'))
        text = shadow_original.decode().replace('\r\n','\n')
        prepare = function(text, 'modern_shadows_prepare')
        measured = prepare.replace('void modern_shadows_prepare(', 'static void modern_shadow_probe_prepare(', 1)
        wrapper = '\nvoid modern_shadows_prepare(int primary)\n{\n\tmodern_shadow_probe_prepare(primary);\n\tif (primary) modern_shadow_compatibility_sample(setting(), active, prepared_sky != NONE, modern_spot_mask());\n}\n'
        shadow.write_text('void modern_shadow_compatibility_sample(int mode, int maps, int sun, int spots);\n'+text.replace(prepare, measured+wrapper, 1))
        text = original.decode().replace('\r\n','\n')
        anchor = '\tshield_glow_frame_end();'
        assert text.count(anchor)==1
        target.write_text('void lighting_compatibility_sample(void);\n'+text.replace(anchor,'\tlighting_compatibility_sample();\n'+anchor))
        shutil.copyfile(ROOT/'tools/fixtures/lighting_compatibility.c',fixture)
        subprocess.run([sys.executable,'configure.py',*args],cwd=ROOT,check=True)
        subprocess.run(['ninja','windows'],cwd=ROOT,check=True)
        shutil.copy2(ROOT/'build/windows/halo.exe',output/'halo-compatibility.exe')
        manifest['executable_sha256']=hashlib.sha256((output/'halo-compatibility.exe').read_bytes()).hexdigest()
        (output/'manifest.json').write_text(json.dumps(manifest,indent=2))
    finally:
        target.write_bytes(original)
        shadow.write_bytes(shadow_original)
        input_source.write_bytes(input_original)
        policy.write_bytes(policy_original)
        network.write_bytes(network_original)
        fixture.unlink(missing_ok=True)
        subprocess.run([sys.executable,'configure.py',*args],cwd=ROOT,check=True)
        subprocess.run(['ninja','windows'],cwd=ROOT,check=True)


if __name__=='__main__':
    main()
