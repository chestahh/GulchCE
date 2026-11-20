"""Run the capture-only build with isolated data, retaining images and timing.

Example: python tools/plasma_visual_run.py --data-root C:/Games/GulchCE --fps 60 --enabled 1
Only this runner's copied maps and generated map caches are removed.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import statistics
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('--data-root', required=True, type=Path)
parser.add_argument('--capture-dir', type=Path, help='Directory containing halo-capture.exe and its build manifest')
parser.add_argument('--mcc-package', type=Path, help='Read-only Workshop package for --map mcc-digsite or mcc-c40')
parser.add_argument('--fps', required=True, type=int, choices=[30, 60, 120, 240])
parser.add_argument('--enabled', required=True, type=int, choices=[0, 1])
parser.add_argument('--shadow-switch', action='store_true', help='Cycle Off/Hard/Soft in the isolated capture config')
parser.add_argument('--shadow-resource-failure', action='store_true', help='Fail the first shadow resource creation in the capture build; requires --shadow-switch')
parser.add_argument('--shadow-view-switch', action='store_true', help='Switch first/third/first person during the shadow capture')
parser.add_argument('--modern-shadows', choices=['off', 'hard', 'soft'], default='off')
parser.add_argument('--shadow-resolution', type=int, choices=[128, 256, 512, 1024, 2048, 4096],
                    help='Explicit shared stock/modern shadow resolution for acceptance captures')
parser.add_argument('--label', default='')
parser.add_argument('--map', choices=['bloodgulch', 'sidewinder', 'carousel', 'a10', 'b40', 'c40', 'ce-deathisland', 'mcc-digsite', 'mcc-ruby-c40', 'mcc-c40'])
parser.add_argument('--kind', choices=['rifle','pistol','charged','rocket','needle','ghost','banshee','banshee-fuel','shade','fuel','wraith','mp-needle','mp-banshee-fuel','mp-shade','ce-fuel','grenade'], default='rifle')
parser.add_argument('--muzzle', choices=['ar','handgun','shotgun','sniper','rifle','pistol','charged','needler','rocket','flamethrower','warthog','scorpion'])
parser.add_argument('--muzzle-source-offset', type=float, help='Capture-only vehicle muzzle source distance ahead of the rendered marker, in world units')
parser.add_argument('--turret-sources', type=int, choices=[1,2,4], help='Requires the turret-prototype capture build')
parser.add_argument('--turret-radius', type=float, default=.06)
parser.add_argument('--turret-ahead', type=float, default=.06)
parser.add_argument('--turret-timing', type=int, help='Seconds of capture-only uncapped vehicle timing; excludes screenshots')
parser.add_argument('--both-lighting', action='store_true')
parser.add_argument('--prong-lighting', action='store_true', help='Isolate the standalone plasma prong spill in a rifle/pistol combat capture')
parser.add_argument('--weapon-glow', action='store_true', help='Exercise authored sword and charge lights')
parser.add_argument('--emitter', choices=['sword-elite','jackal','major','fuel','hunter','stationary','ammo-fp','ammo-marine','ammo-player','needler-fp','needler-marine','needler-player','needler-grunt'])
parser.add_argument('--ambient', choices=['ghost','banshee','dropship','warthog','scorpion','overshield','camo','teleporter'])
parser.add_argument('--shadow-light', action='store_true', help='Hold a real charged plasma projectile above the vehicle for silhouette inspection')
parser.add_argument('--shadow-scene', choices=['warthog', 'ghost', 'ghost-engine', 'jackal-wall', 'player', 'marine', 'corpse', 'first-person', 'dropped-weapon', 'frag-grenade', 'plasma-grenade'],
                    help='Fixed shadow acceptance subject and tick-indexed moving light; five baseline keyframes')
parser.add_argument('--dump-shaders', action='store_true', help='Retain generated GPU programs for this isolated run')
parser.add_argument('--shadow-omit', choices=['weapon', 'body', 'vehicle', 'occupants'], help='Capture-only counterfactual: omit selected casters, retaining their visible models')
parser.add_argument('--shadow-motion', action='store_true', help='Capture every rendered frame during ticks 100-160 in a shadow scene')
parser.add_argument('--shadow-composition', action='store_true', help='Move a real source into range and back out; optional motion capture covers ticks 100-220')
parser.add_argument('--shadow-camera-angle', type=float, help='Rotate only the acceptance camera around the subject, in degrees')
parser.add_argument('--shadow-weapon', help='Loaded weapon tag path for a held-weapon shadow acceptance scene')
parser.add_argument('--shadow-holder', help='Loaded biped tag path replacing the marine in that acceptance scene')
parser.add_argument('--shadow-vehicle', help='Loaded vehicle tag path replacing the vehicle in a shadow acceptance scene')
parser.add_argument('--shield', choices=['normal', 'charge', 'fp', 'elite'])
parser.add_argument('--glow-intensity', choices=['default','light_show'], default='default')
parser.add_argument('--shield-look', default='classic', choices=['classic','enhanced','classic_with_sprinkles','classic_honeycomb2'])
parser.add_argument('--turn', action='store_true', help='Capture-only yaw/pitch sweep for light orientation checks')
parser.add_argument('--hide-viewmodel', action='store_true', help='Keep the local rifle from obscuring third-person receivers')
parser.add_argument('--face-camera', action='store_true', help='Face a held-weapon fixture toward the camera to inspect torso and hand spill')
parser.add_argument('--grounded', action='store_true', help='Place the held-weapon actor on the actual floor')
parser.add_argument('--crouch', action='store_true', help='Exercise a held-weapon actor crouching and standing again')
parser.add_argument('--exterior', action='store_true', help='View the vehicle fixture from outside to inspect chassis blocking')
parser.add_argument('--sparse-images', action='store_true', help='Retain one image every three simulation ticks')
parser.add_argument('--image-step', type=int, choices=range(1,31), default=3, help='Simulation ticks between sparse captures')
parser.add_argument('--counter-detail', action='store_true', help='View a held world-model AR display over its holder\'s shoulder')
parser.add_argument('--resolution', default='640x480', help='Capture window size, for example 1280x720')
parser.add_argument('--startup-grace', type=int, choices=range(61), default=0,
                    help='Extra wall-clock seconds for cold loading; capture ticks and acceptance checks are unchanged')
parser.add_argument('--mixed', action='store_true')
parser.add_argument('--model', action='store_true')
parser.add_argument('--occlusion', type=int, choices=[0,1], default=0)
parser.add_argument('--probe', action='store_true')
parser.add_argument('--no-images', action='store_true', help='Measure without screenshot readback or disk writes')
parser.add_argument('--combat', action='store_true')
parser.add_argument('--door', action='store_true')
parser.add_argument('--toggle', action='store_true')
parser.add_argument('--span', type=float)
parser.add_argument('--burst', type=int, choices=range(1,33), default=1)
parser.add_argument('--specular-only', action='store_true')
parser.add_argument('--per-pixel', action='store_true')
parser.add_argument('--distance', type=float)
parser.add_argument('--height', type=float)
parser.add_argument('--drop', type=float)
parser.add_argument('--receiver-distance', type=float)
parser.add_argument('--shadow-wall-gap', type=float, default=.7, help='Jackal fixture distance from the wall in world units')
args = parser.parse_args()
if args.map == 'mcc-c40' and not args.mcc_package:
    parser.error('--map mcc-c40 requires --mcc-package')
if args.turret_sources and (args.muzzle not in ('warthog','scorpion') or not 0 <= args.turret_radius <= .2 or not 0 <= args.turret_ahead <= .2):
    parser.error('--turret-sources requires a vehicle muzzle and disk dimensions in [0, 0.2]')
if args.turret_timing and (args.muzzle not in ('warthog','scorpion') or not 30 <= args.turret_timing <= 120):
    parser.error('--turret-timing requires a vehicle muzzle and 30..120 seconds')
if args.muzzle_source_offset is not None and (args.muzzle not in ('warthog', 'scorpion') or not 0 <= args.muzzle_source_offset <= .5):
    parser.error('--muzzle-source-offset requires a vehicle muzzle fixture and a value between 0 and 0.5')
if not .1 <= args.shadow_wall_gap <= 2:
    parser.error('--shadow-wall-gap must be between 0.1 and 2 world units')
if args.shadow_omit in ('weapon', 'body') and args.shadow_scene not in ('player', 'marine', 'corpse'):
    parser.error('--shadow-omit requires a world-space character --shadow-scene')
if args.shadow_omit in ('vehicle', 'occupants') and args.shadow_scene != 'ghost-engine' and args.muzzle not in ('warthog', 'scorpion'):
    parser.error('--shadow-omit vehicle/occupants requires ghost-engine or a vehicle muzzle fixture')
if args.shadow_weapon and args.shadow_scene not in ('player', 'marine', 'corpse', 'first-person'):
    parser.error('--shadow-weapon requires a held-weapon --shadow-scene')
if args.shadow_holder and args.shadow_scene != 'marine':
    parser.error('--shadow-holder requires --shadow-scene marine')
if args.shadow_vehicle and args.shadow_scene not in ('warthog', 'ghost', 'ghost-engine'):
    parser.error('--shadow-vehicle requires a vehicle --shadow-scene')
if args.shadow_composition and args.shadow_scene not in ('player', 'marine', 'corpse', 'warthog', 'ghost'):
    parser.error('--shadow-composition requires an external-light biped or vehicle scene')
if args.shadow_motion and not args.shadow_scene:
    parser.error('--shadow-motion requires --shadow-scene')
if args.shadow_light and not args.ambient:
    parser.error('--shadow-light requires --ambient')
if args.shadow_scene and (args.ambient or args.emitter or args.shield or args.muzzle or args.combat or
                         args.toggle or args.shadow_switch or args.shadow_view_switch or args.shadow_light or
                         args.probe or args.door or args.turn or args.model or args.mixed):
    parser.error('--shadow-scene cannot be combined with another scene fixture')
if args.shadow_resource_failure and not args.shadow_switch:
    parser.error('--shadow-resource-failure requires --shadow-switch')
if args.mcc_package and args.map not in ('mcc-digsite', 'mcc-c40'):
    parser.error('--mcc-package requires --map mcc-digsite or mcc-c40')
if args.prong_lighting:
    if not args.combat or args.kind not in ('rifle', 'pistol') or args.weapon_glow or args.muzzle:
        parser.error('--prong-lighting requires rifle/pistol --combat, without other weapon-light options')
if args.toggle and (args.emitter or args.ambient or args.shield or args.muzzle):
    parser.error('--toggle requires a projectile capture; emitter, shield and muzzle fixtures use separate tick handlers')
if not re.fullmatch(r'[1-9][0-9]{2,3}x[1-9][0-9]{2,3}', args.resolution):
    parser.error('--resolution must be WIDTHxHEIGHT')
if args.counter_detail and args.emitter not in ('ammo-player', 'ammo-marine'):
    parser.error('--counter-detail requires a third-person AR holder')
if args.exterior and args.muzzle not in ('warthog', 'scorpion'):
    parser.error('--exterior requires --muzzle warthog or scorpion')
if args.crouch and not args.shield and args.emitter not in ('ammo-player', 'ammo-marine', 'needler-player', 'needler-marine'):
    parser.error('--crouch requires a third-person player or Marine emitter')
if args.map is None:
    # Blood Gulch has no machine door for the moving-occluder fixture.
    args.map = 'a10' if args.door else 'bloodgulch'
if args.muzzle:
    args.combat = True
root = Path(__file__).resolve().parents[1]
assert re.fullmatch(r'[a-zA-Z0-9_-]*', args.label)
out = root / f'build/plasma-visual/{args.fps}-{args.enabled}{"-" + args.label if args.label else ""}'
out.mkdir(parents=True, exist_ok=False)
(out / 'maps').mkdir()
(out / 'screenshots').mkdir()
capture_dir = args.capture_dir.resolve() if args.capture_dir else root/'build/plasma-visual'
manifest = capture_dir/'build-manifest.json'
if manifest.exists():
    shutil.copy2(manifest, out/'build-manifest.json')
shutil.copy2(args.data_root/'maps/ui.map',out/'maps/ui.map')
if args.map=='ce-deathisland':
    (out/'custom_maps').mkdir()
    # Accept the original CE filename as well as the earlier fixture's name.
    ce_map = 'deathisland_451254bb' if (args.data_root/'custom_maps/deathisland_451254bb.map').is_file() else 'deathisland'
    for name in [ce_map+'.map','bitmaps.map','sounds.map','loc.map']:
        shutil.copy2(args.data_root/'custom_maps'/name,out/'custom_maps'/name)
    level='custom_maps\\'+ce_map
elif args.map=='mcc-digsite':
    # The normal host setup uses the stock lobby map while MCC admission
    # validates the selected package. Keep that bootstrap resource available.
    shutil.copy2(args.data_root/'maps/carousel.map', out/'maps/carousel.map')
    for name in ['1.map','bitmaps.map','sounds.map']:
        target=out/'mcc_maps'/'DigsiteMP'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        source = args.data_root/'mcc_maps'/'DigsiteMP'/name
        if args.mcc_package:
            source = args.mcc_package/('maps' if name == '1.map' else 'halo1/maps')/name
        shutil.copy2(source,target)
    level=r'mcc_maps\DigsiteMP\1'
elif args.map=='mcc-c40':
    for name in ['c40.map', 'bitmaps.map', 'sounds.map']:
        target=out/'mcc_maps'/'CampaignProbe'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        source=args.mcc_package/('maps' if name=='c40.map' else 'halo1/maps')/name
        shutil.copy2(source,target)
    level=r'mcc_maps\CampaignProbe\c40'
elif args.map=='mcc-ruby-c40':
    for name in ['RubyRebalanced/c40.map', '_shared/singleplayer/bitmaps.map']:
        target=out/'mcc_maps'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(args.data_root/'mcc_maps'/name,target)
    level=r'mcc_maps\RubyRebalanced\c40'
else:
    shutil.copy2(args.data_root/'maps'/(args.map+'.map'),out/'maps'/(args.map+'.map'))
    level=f'levels\\{args.map}\\{args.map}' if args.map in ('a10','b40','c40') else f'levels\\test\\{args.map}\\{args.map}'
(out/'init.txt').write_text(f'map_name {level}\n')
if args.specular_only:
    with (out/'init.txt').open('a') as init:
        init.write('(set rasterizer_environment_diffuse_lights false)\n')

env = {k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
if args.dump_shaders:
    (out/'shaders').mkdir()
    env['HALO_GPU_DUMP_SHADERS'] = str(out/'shaders')
if args.shadow_resource_failure:
    env['HALO_SHADOW_CAPTURE_RESOURCE_FAILURE'] = '1'
if args.shadow_switch:
    env['HALO_SHADOW_CAPTURE_SWITCH'] = '1'
    (out/'config.toml').write_text('[display]\nmodern_shadows="off"\ndynamic_lighting_reset=true\n')
else:
    env['HALO_MODERN_SHADOWS'] = args.modern_shadows
if args.shadow_resolution is not None:
    env['HALO_SHADOW_RESOLUTION'] = str(args.shadow_resolution)
if args.shadow_view_switch:
    env.update(HALO_SHADOW_CAPTURE_VIEW_SWITCH='1', HALO_SESSION_COMPATIBILITY='gulchce_third_person', HALO_CAMERA_POLICY='player_choice')
env['PATH'] = str(root / 'build/windows') + os.pathsep + env.get('PATH', '')
if args.shadow_omit:
    env['HALO_SHADOW_CAPTURE_OMIT'] = args.shadow_omit
if args.muzzle_source_offset is not None:
    env['HALO_MUZZLE_CAPTURE_OFFSET'] = str(args.muzzle_source_offset)
env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='1',
           HALO_NO_AUDIO='1', HALO_MAX_FPS=str(args.fps), HALO_NO_VSYNC='1', HALO_EXIT_AFTER='25' if args.door else '13',
           HALO_NET_ONLINE='0', HALO_NET_PUBLIC_LOBBY='0', HALO_UPDATE_ANSWER='never',
           HALO_NET_JOIN_FROM_CLIPBOARD='0', HALO_NET_ALLOW_UPNP='0',
           HALO_CRASH_REPORTS='no', HALO_WINDOW_SIZE=args.resolution, HALO_PLASMA_LIGHTING=str(args.enabled),
           HALO_SCREENSHOT_DIR=str(out/'screenshots'), HALO_PLASMA_OCCLUSION=str(args.occlusion))
if args.map == 'mcc-digsite':
    # This multiplayer package's scenario script is not a player-spawn path.
    # Start an isolated loopback host, as the compatibility runner does,
    # rather than capturing its empty campaign-style map_name session.
    (out/'init.txt').write_text('')
    env.update(HALO_NETWORK_TEST='host:'+level+':slayer', HALO_NETWORK_TEST_START='12',
               HALO_NET_ADDRESS='127.0.0.1', HALO_NET_BROADCAST='127.0.0.1',
               HALO_EXIT_AFTER='35')
if args.map in ('b40', 'c40', 'mcc-ruby-c40', 'mcc-c40'):
    env['HALO_COVENANT_CAPTURE_CAMPAIGN'] = '1'
    env['HALO_EXIT_AFTER'] = '70' if args.map in ('b40', 'mcc-ruby-c40', 'mcc-c40') else '35'
if args.prong_lighting:
    env.update(HALO_PLASMA_PRONG_LIGHTING=str(args.enabled), HALO_PLASMA_LIGHTING='0')
if args.weapon_glow:
    env['HALO_WEAPON_GLOW'] = str(args.enabled)
if args.turn:
    env['HALO_LIGHTING_CAPTURE_TURN'] = '1'
if args.hide_viewmodel:
    env['HALO_VIEWMODEL_VISIBLE'] = '0'
if args.face_camera:
    env['HALO_EMITTER_CAPTURE_FACE_CAMERA'] = '1'
if args.grounded:
    env['HALO_EMITTER_CAPTURE_GROUNDED'] = '1'
if args.crouch:
    env['HALO_EMITTER_CAPTURE_CROUCH'] = '1'
if args.exterior and args.muzzle == 'warthog':
    env['HALO_WARTHOG_CAPTURE_EXTERIOR'] = '1'
if args.sparse_images:
    env['HALO_LIGHTING_CAPTURE_SPARSE'] = '1'
    env['HALO_LIGHTING_CAPTURE_IMAGE_STEP'] = str(args.image_step)
if args.counter_detail:
    env['HALO_EMITTER_CAPTURE_COUNTER_DETAIL'] = '1'
if args.emitter:
    env['HALO_EXIT_AFTER'] = '70' if args.map in ('b40', 'mcc-ruby-c40', 'mcc-c40') else '35'
    env['HALO_EMITTER_CAPTURE'] = args.emitter
    env['HALO_PLASMA_LIGHTING'] = '0'
    env[{'jackal':'HALO_JACKAL_SHIELD_LIGHTING', 'major':'HALO_JACKAL_SHIELD_LIGHTING',
         'fuel':'HALO_FUEL_ROD_LIGHTING', 'hunter':'HALO_FUEL_ROD_LIGHTING', 'stationary':'HALO_COVENANT_SHIELD_LIGHTING',
         'ammo-fp':'HALO_AMMO_COUNTER_LIGHTING','ammo-marine':'HALO_AMMO_COUNTER_LIGHTING',
         'ammo-player':'HALO_AMMO_COUNTER_LIGHTING', 'needler-fp':'HALO_NEEDLER_LIGHTING',
         'needler-player':'HALO_NEEDLER_LIGHTING','needler-marine':'HALO_NEEDLER_LIGHTING',
         'needler-grunt':'HALO_NEEDLER_LIGHTING', 'sword-elite':'HALO_WEAPON_GLOW'}[args.emitter]] = str(args.enabled)
if args.ambient:
    env['HALO_AMBIENT_CAPTURE']=args.ambient
    env['HALO_PLASMA_LIGHTING']='0'
    env['HALO_VEHICLE_LIGHTING' if args.ambient in ('ghost','banshee','dropship','warthog','scorpion') else 'HALO_WORLD_LIGHTING']=str(args.enabled)
if args.shadow_light:
    env.update(HALO_SHADOW_CAPTURE_LIGHT='1', HALO_PLASMA_LIGHTING=str(args.enabled))
if args.shield:
    env.update(HALO_SHIELD_CAPTURE=args.shield, HALO_SHIELD_GLOW=str(args.enabled),
               HALO_OVERSHIELD_EFFECT=str(int(args.shield in ('charge','fp'))), HALO_VIEWMODEL_SHIELD='1',
               HALO_SHIELD_LOOK=args.shield_look, HALO_SHIELD_GLOW_INTENSITY=args.glow_intensity, HALO_PLASMA_LIGHTING='0')
if args.muzzle:
    muzzle_tags = {
        'ar': r'weapons\assault rifle\assault rifle', 'handgun': r'weapons\pistol\pistol',
        'shotgun': r'weapons\shotgun\shotgun', 'sniper': r'weapons\sniper rifle\sniper rifle',
        'rifle': r'weapons\plasma rifle\plasma rifle', 'pistol': r'weapons\plasma pistol\plasma pistol',
        'charged': r'weapons\plasma pistol\plasma pistol',
        'needler': r'weapons\needler\mp_needler' if args.map == 'ce-deathisland' else r'weapons\needler\needler',
        'rocket': r'weapons\rocket launcher\rocket launcher', 'flamethrower': r'weapons\flamethrower\flamethrower',
        'warthog': r'vehicles\warthog\warthog gun',
        'scorpion': r'vehicles\scorpion\scorpion cannon',
    }
    env['HALO_MUZZLE_CAPTURE_WEAPON'] = muzzle_tags[args.muzzle]
    env['HALO_MUZZLE_LIGHTING'] = str(args.enabled)
    env['HALO_PLASMA_LIGHTING'] = str(int(args.both_lighting))
    if args.muzzle in ('warthog', 'scorpion'):
        env['HALO_' + args.muzzle.upper() + '_CAPTURE'] = '1'
        # The vehicle fixture equips its own weapon. Use the combat fixture's
        # sustained bursts rather than the handheld muzzle fixture's tap input.
        env.pop('HALO_MUZZLE_CAPTURE_WEAPON')
    if args.muzzle == 'charged':
        env['HALO_MUZZLE_CAPTURE_CHARGED'] = '1'
        args.kind = 'charged'
for key in ['distance', 'height', 'receiver_distance', 'span', 'burst', 'drop']:
    value = getattr(args, key)
    if value is not None:
        env['HALO_PLASMA_CAPTURE_'+key.upper()] = str(value)
env['HALO_PLASMA_CAPTURE_KIND'] = args.kind
for key in ['combat','door','toggle','mixed']:
    if getattr(args,key):env['HALO_PLASMA_CAPTURE_'+key.upper()]='1'
if args.probe:
    env['HALO_PLASMA_CAPTURE_PROBE']='1'
if args.no_images:
    env['HALO_PLASMA_CAPTURE_NO_IMAGES']='1'
if args.model:
    env['HALO_PLASMA_CAPTURE_MODEL'] = '1'
if args.per_pixel:
    env['HALO_PER_PIXEL_LIGHTING'] = '1'
env['HALO_EXIT_AFTER'] = str(int(env['HALO_EXIT_AFTER']) + args.startup_grace)
if args.shadow_scene:
    if args.shadow_weapon:
        env['HALO_SHADOW_CAPTURE_WEAPON'] = args.shadow_weapon
    if args.shadow_holder:
        env['HALO_SHADOW_CAPTURE_HOLDER'] = args.shadow_holder
    if args.shadow_vehicle:
        env['HALO_SHADOW_CAPTURE_VEHICLE'] = args.shadow_vehicle
    if args.shadow_camera_angle is not None:
        env['HALO_SHADOW_CAPTURE_CAMERA_ANGLE'] = str(args.shadow_camera_angle)
    if args.shadow_motion:
        env['HALO_SHADOW_CAPTURE_MOTION'] = '1'
    if args.shadow_composition:
        env['HALO_SHADOW_CAPTURE_COMPOSITION'] = '1'
    env['HALO_SHADOW_CAPTURE_WALL_GAP'] = str(args.shadow_wall_gap)
    env.update(HALO_SHADOW_CAPTURE_SCENE=args.shadow_scene, HALO_JACKAL_SHIELD_LIGHTING=str(args.enabled if args.shadow_scene == 'jackal-wall' else 0),
               HALO_VEHICLE_LIGHTING=str(args.enabled if args.shadow_scene == 'ghost-engine' else 0), HALO_SESSION_COMPATIBILITY='gulchce_third_person',
               HALO_CAMERA_POLICY='player_choice', HALO_THIRD_PERSON=str(int(args.shadow_scene == 'player')))
    if args.shadow_scene != 'first-person':
        env['HALO_VIEWMODEL_VISIBLE'] = '0'
if args.turret_sources:
    assert json.loads((capture_dir/'build-manifest.json').read_text()).get('turret_prototype'), 'Wrong capture build'
    env.update(HALO_TURRET_SOURCES=str(args.turret_sources), HALO_TURRET_RADIUS=str(args.turret_radius), HALO_TURRET_AHEAD=str(args.turret_ahead))
if args.turret_timing:
    assert json.loads((capture_dir/'build-manifest.json').read_text()).get('vehicle_timing'), 'Wrong vehicle timing build'
    env.update(HALO_TURRET_TIMING='1',HALO_MAX_FPS='-1',HALO_EXIT_AFTER=str(args.turret_timing+20),HALO_PLASMA_CAPTURE_NO_IMAGES='1',HALO_SCREENSHOT_DIR='')
    args.no_images=True
with (out/'stdout.log').open('wb') as so, (out/'stderr.log').open('wb') as se:
    capture_executable = capture_dir/'halo-capture.exe'
    if args.shadow_switch or args.shadow_view_switch or args.shadow_scene or args.turret_sources or args.turret_timing:
        shutil.copy2(capture_executable, out/'halo-capture.exe')
        capture_executable = out/'halo-capture.exe'
    if args.shadow_scene or args.turret_sources or args.turret_timing:
        (out/'scene-manifest.json').write_text(json.dumps(dict(
            environment={key: value for key, value in env.items() if key.startswith('HALO_')},
            executable_sha256=hashlib.sha256(capture_executable.read_bytes()).hexdigest(),
            maps={path.relative_to(out).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in (out/'maps').glob('*.map')}), indent=2))
    process = subprocess.Popen([str(capture_executable)], cwd=out,
                               env=env, stdout=so, stderr=se, creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        process.wait(timeout=(args.turret_timing+45 if args.turret_timing else 145 if args.map in ('b40', 'c40', 'mcc-ruby-c40', 'mcc-c40') else 45) + args.startup_grace)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    if args.shadow_scene:
        assert capture_executable.resolve().parent == out.resolve()
        capture_executable.unlink()
logs = '\n'.join(p.read_text(errors='replace') for p in out.glob('*.log'))
times = [int(x) for x in re.findall(r'PLASMA frame (\d+)', logs)]
deltas = [b-a for a,b in zip(times, times[1:]) if b>a]
report = dict(kind=args.kind, mixed=args.mixed, cap=args.fps, enabled=args.enabled, exit=process.returncode,
              shadow_shader_failures=len(re.findall(r'GPU: modern shadow variant[^\r\n]+failed', logs)),
              modern_shadows=args.modern_shadows,
              shadow_active_samples=len(re.findall(r'SHADOW selected \d+ complete 1 active 1', logs)),
              shadow_incomplete_samples=len(re.findall(r'SHADOW selected \d+ complete 0', logs)),
              screenshots=len(list((out/'screenshots').glob('*.bmp'))), frames=len(times),
              selected_frames=len(re.findall(r'selected [1-9]', logs)),
              average_fps=1000*(len(times)-1)/(times[-1]-times[0]) if len(times)>1 else None,
              median_ms=statistics.median(deltas) if deltas else None,
              shots=len(re.findall('PLASMA shot', logs)),
              model_draws=sum(int(x) for x in re.findall(r'PLASMA draws model (\d+)', logs)),
              specular_draws=sum(int(x) for x in re.findall(r'PLASMA draws model \d+ specular (\d+)', logs)),
              max_selected=max([int(x) for x in re.findall(r'selected (\d+)',logs)] or [0]), burst=args.burst, combat=args.combat, door=args.door, toggle=args.toggle, occlusion=args.occlusion, probe=args.probe, occlusion_updates=len(re.findall('PLASMA occlusion extent',logs)), map=args.map, model=args.model, per_pixel=args.per_pixel, specular_only=args.specular_only)
report['authored_light_flags'] = sorted({int(x, 16) for x in re.findall(r'PLASMA enabled [01] light flags ([0-9a-f]+)', logs)})
report['glow_template_flags'] = sorted({int(x, 16) for x in re.findall(r'PLASMA glow template [^\r\n]+ flags ([0-9a-f]+)', logs)})
report['no_images'] = args.no_images
report['shadow_resource_failure'] = args.shadow_resource_failure
active_frames = [(int(t), int(n)) for t, n in re.findall(r'PLASMA frame (\d+)[^\r\n]+ selected (\d+)', logs)]
active_deltas = [b[0]-a[0] for a, b in zip(active_frames, active_frames[1:]) if a[1] and b[1] and b[0] > a[0]]
report['active_average_fps'] = 1000 / statistics.mean(active_deltas) if active_deltas else None
report['active_p95_ms'] = sorted(active_deltas)[int((len(active_deltas)-1)*.95)] if active_deltas else None
report['homing_shots'] = len(re.findall('PLASMA homing', logs))
report['selected_tags'] = sorted(set(re.findall(r'PLASMA prepare [^\r\n]+ tag ([^\r\n]+)', logs)))
report['muzzle'] = args.muzzle
report['shield'] = args.shield
report['shield_look'] = args.shield_look
report['glow_intensity'] = args.glow_intensity
report['shield_created'] = bool(re.search(r'SHIELD created (?!-1 )', logs))
report['ambient'] = args.ambient
report['ambient_created'] = bool(re.search(r'AMBIENT created (?!-1 )', logs))
report['emitter'] = args.emitter
report['hide_viewmodel'] = args.hide_viewmodel
report['face_camera'] = args.face_camera
report['grounded'] = args.grounded
report['crouch'] = args.crouch
report['exterior'] = args.exterior
report['holder_crouch_samples'] = [float(x) for x in re.findall(r'EMITTER holder tick \d+ crouch ([0-9.]+)', logs)]
report['turn'] = args.turn
report['counter_detail'] = args.counter_detail
report['resolution'] = args.resolution
report['startup_grace'] = args.startup_grace
report['emitter_samples'] = len(re.findall('EMITTER sample', logs))
report['emitter_created'] = bool(re.search(r'EMITTER created (?!-1 )', logs))
report['both_lighting'] = args.both_lighting
report['muzzle_submissions'] = len(re.findall('MUZZLE submit', logs))
report['muzzle_fade_samples'] = sum(0 < float(x) < .99 for x in re.findall(r'MUZZLE sample[^\r\n]+ strength ([0-9.]+)', logs))
report['shadow_scene'] = args.shadow_scene
report['shadow_omit'] = args.shadow_omit
report['muzzle_source_offset'] = args.muzzle_source_offset
report['turret_sources'] = args.turret_sources
report['turret_radius'] = args.turret_radius
report['turret_ahead'] = args.turret_ahead
if args.turret_timing:
    import csv
    from lighting_performance_run import distribution
    rows=[{k:float(v) for k,v in row.items()} for row in csv.DictReader((out/'turret-frames.csv').open())]
    rows=[r for r in rows if 10 <= r['seconds'] < 10+args.turret_timing]
    assert len(rows)>100 and rows[-1]['seconds']-rows[0]['seconds']>args.turret_timing-1
    report['timing']=distribution([r['frame_ms'] for r in rows],240)
    active=[r for r in rows if r['turrets']>0]
    assert active, 'No shadowed turret samples'
    report['active_timing']=distribution([r['frame_ms'] for r in active],240)
    report['turret_active_fraction']=len(active)/len(rows)
    report['average_fps']=report['timing']['average_fps']
    report['shadow_active_samples']=sum(r['shadowed']>0 for r in rows)
    report['selected_frames']=len(active)
report['shadow_wall_gap'] = args.shadow_wall_gap if args.shadow_scene == 'jackal-wall' else None
report['shadow_scene_created'] = bool(re.search(r'SHADOW SCENE created ', logs))
report['shadow_scene_ticks'] = [int(x) for x in re.findall(r'SHADOW SCENE sample (\d+)', logs)]
(out/'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
for path in out.rglob('*.map'):
    assert path.resolve().is_relative_to(out.resolve())
    path.unlink()
if report['shadow_shader_failures']:
    raise SystemExit('Modern shadow shader fell back to existing lighting; capture is not acceptance evidence')
if process.returncode or (report['shots'] != 3 and not args.combat and not args.emitter and not args.shield and not args.ambient and not args.shadow_scene) or (not args.no_images and not report['screenshots']):
    raise SystemExit('Capture incomplete; inspect report.json and logs')
if args.shadow_scene and (not report['shadow_scene_created'] or report['shadow_scene_ticks'] != [100, 130, 160, 190, 220] or 'SHADOW SCENE failed' in logs):
    raise SystemExit('Shadow scene did not complete its five baseline samples')
if args.shadow_scene:
    # _object_dead_bit is bit 2 in objects.h. Stock non-damageable vehicles
    # legitimately have zero body vitality, so vitality alone is not death.
    damage_flags = [int(x) for x in re.findall(r'SHADOW SCENE sample[^\r\n]+ flags (\d+)', logs)]
    if len(damage_flags) != 5 or any(bool(value & 4) != (args.shadow_scene == 'corpse') for value in damage_flags):
        raise SystemExit('Shadow subject death state did not match the requested scene')
if args.ambient and not report['ambient_created']:
    raise SystemExit('Ambient fixture missing; this is not a passing capture')
if args.shield and not report['shield_created']:
    raise SystemExit('Shield fixture did not create its actor')
if args.emitter and (not report['emitter_created'] or (args.emitter != 'sword-elite' and bool(report['emitter_samples']) != bool(args.enabled))):
    raise SystemExit('Emitter creation or activation failed')
if args.emitter == 'sword-elite' and args.enabled and r'weapons\energy sword\energy sword' not in report['selected_tags']:
    raise SystemExit('Sword attachment never entered enhanced lighting')
if args.crouch and not args.shield and report['holder_crouch_samples'] != [1.0, 1.0, 0.0]:
    raise SystemExit('Holder did not crouch and return to standing')
if not args.turret_timing and bool(report['selected_frames']) != bool(args.enabled or (args.muzzle and args.both_lighting)):
    raise SystemExit('Lighting selection did not match the requested mode')
if not args.turret_timing and (report['average_fps'] is None or report['average_fps'] < args.fps * .95):
    raise SystemExit('Capture did not sustain the requested FPS within 5 percent')

if args.enabled and args.model and not report['model_draws']:
    raise SystemExit('Capture never reached a selected model draw')
if args.enabled and args.specular_only:
    flags = report['authored_light_flags'] or report['glow_template_flags']
    # Bit 1 is _light_definition_no_specular_bit in object_lights.c.
    if flags and all(value & 2 for value in flags):
        if report['specular_draws']:
            raise SystemExit('Capture ignored the authored no-specular flag')
    elif not report['specular_draws']:
        raise SystemExit('Capture never reached a selected specular draw')

if not args.shadow_switch and args.modern_shadows == 'off' and report['shadow_active_samples']:
    raise SystemExit('Disabled modern shadows still rendered maps')
if args.modern_shadows != 'off' and report['selected_frames'] and not report['shadow_active_samples']:
    raise SystemExit('Selected lighting never produced a complete modern shadow map')
if report['shadow_incomplete_samples']:
    raise SystemExit('Modern shadow caster collection was incomplete; inspect fallback diagnostics')
if args.enabled and args.occlusion and not (report['occlusion_updates'] or report['shadow_active_samples']):
    raise SystemExit('Capture did not update any occlusion fields')

if args.combat and not args.turret_timing and 'PLASMA combat equipped' not in logs:
    # The AR emitter fixture equips the same weapon and returns before the
    # combat fixture. Require both successful creation and real firing.
    if not (args.muzzle == 'ar' and args.emitter == 'ammo-fp' and
            report['emitter_created'] and report['muzzle_submissions'] > 0):
        raise SystemExit('Combat fixture did not equip the requested weapon')
if args.probe and args.enabled and report['max_selected'] < (min(args.burst, 8) if args.kind in ('needle', 'mp-needle', 'wraith') else args.burst):
    raise SystemExit('Stress capture did not select the requested simultaneous light count')
if args.door:
    positions = [float(x) for x in re.findall(r'PLASMA door position ([0-9.]+)', logs)]
    if not positions or min(positions) > .1 or max(positions) < .9:
        raise SystemExit('Door did not complete both open and closed states')
if args.toggle:
    intervals = re.split(r'PLASMA toggle ([01])', logs)
    if len(intervals) < 5:
        raise SystemExit('Toggle capture did not exercise both states')
    for i in range(1, len(intervals)-1, 2):
        if intervals[i] == '0' and 'PLASMA occlusion extent' in intervals[i+1]:
            raise SystemExit('Occlusion updated while disabled')

expected_tags = {
    'grenade': r'weapons\plasma grenade\plasma grenade',
    'rifle': r'weapons\plasma rifle\bolt',
    'pistol': r'weapons\plasma pistol\bolt',
    'charged': r'weapons\plasma rifle\charged bolt',
    'rocket': r'weapons\rocket launcher\rocket',
    'needle': 'weapons\\needler\\needle',
    'ghost': 'vehicles\\ghost\\ghost bolt',
    'banshee': 'vehicles\\banshee\\banshee bolt',
    'banshee-fuel': 'vehicles\\banshee\\banshee fuel rod',
    'shade': 'vehicles\\c gun turret\\c gun turret',
    'fuel': 'weapons\\fuel rod gun\\fuel rod',
    'wraith': 'vehicles\\wraith\\wraith mortar',
    'mp-needle': 'weapons\\needler\\mp_needle',
    'mp-banshee-fuel': 'vehicles\\banshee\\mp_banshee fuel rod',
    'mp-shade': 'vehicles\\c gun turret\\mp gun turret',
    'ce-fuel': 'weapons\\plasma_cannon\\plasma_cannon',
}
if args.enabled and not args.prong_lighting and not args.muzzle and not args.emitter and not args.shield and not args.ambient and not args.shadow_scene:
    expected = {expected_tags[args.kind]}
    if args.mixed:
        expected = {expected_tags[kind] for kind in ('rifle', 'pistol', 'charged', 'rocket') if args.kind == 'rocket' or kind != 'rocket'}
    if not expected.issubset(report['selected_tags']):
        raise SystemExit('Capture did not select every requested projectile kind')
if not args.enabled and not (args.muzzle and args.both_lighting and args.occlusion) and report['occlusion_updates']:
    raise SystemExit('Disabled lighting still updated occlusion')
if args.kind == 'charged' and args.model and not args.probe and not args.combat and report['homing_shots'] != 3:
    raise SystemExit('Charged-shot capture did not exercise the homing target')

# Timing mode proves active turret maps from the buffered CSV; verbose fade/profile logs are suppressed.
if not args.turret_timing and args.muzzle and args.enabled and (not report['muzzle_submissions'] or not report['muzzle_fade_samples']):
    raise SystemExit('Muzzle capture did not exercise submission and smooth fade')
if args.muzzle and not args.enabled and report['muzzle_submissions']:
    raise SystemExit('Disabled muzzle feature still submitted lights')

if not args.turret_timing and args.muzzle == "warthog" and args.enabled and r"MUZZLE profile vehicles\warthog\warthog gun" not in logs:
    raise SystemExit("Capture did not fire the Warthog turret")
if not args.turret_timing and args.muzzle == 'scorpion' and args.enabled:
    if r'MUZZLE profile vehicles\scorpion\scorpion cannon' not in logs or set(re.findall(r'MUZZLE sample[^\r\n]+ trigger ([01])', logs)) != {'0', '1'}:
        raise SystemExit('Capture did not fire both Scorpion guns')

if args.prong_lighting and args.enabled:
    expected = 'weapons\\plasma '+args.kind+'\\plasma '+args.kind
    if expected not in report['selected_tags']:
        raise SystemExit('Prong capture never selected the intended weapon attachment')

if args.shadow_switch:
    intervals = re.split(r'SHADOW MODE tick (\d+) value (off|hard|soft)', logs)
    assert [intervals[i+1] for i in range(1,len(intervals),3)] == ['off','hard','soft']*3
    for i in range(1,len(intervals),3):
        if intervals[i+1] == 'off':
            assert not re.search(r'SHADOW selected[^\r\n]+ active 1', intervals[i+2])
    from harness import constant
    budget = constant((root/'port/linux/include/modern_shadows.h').read_text(), 'MODERN_SHADOW_LIGHTS')
    released = [tuple(map(int, row)) for row in re.findall(r'SHADOW FREE cubes (\d+) remaining (\d+) meshes (\d+)', logs)]
    assert released, 'No shadow-resource cleanup was recorded'
    assert all(remaining == meshes == 0 for _, remaining, meshes in released), 'Shadow resources survived cleanup'
    if args.enabled:
        assert any(0 < cubes <= 2 * budget for cubes, _, _ in released), 'No local shadow maps were released'
        assert report['shadow_active_samples'] > 0
    else:
        # Environmental maps are independent of the local-light cube budget.
        # This path requires the environmental capture builder's GPU evidence.
        assert all(cubes == 0 for cubes, _, _ in released), 'Disabled local lighting allocated shadow cubes'
        for i in range(1, len(intervals), 3):
            completed = 'ENV GPU complete 1' in intervals[i+2]
            assert completed == (intervals[i+1] != 'off'), 'Environmental shadow mode did not match completed draws'
    if args.shadow_resource_failure:
        assert logs.count('SHADOW TEST resource failure injected') == 1
        assert logs.count('modern shadow resources unavailable; retaining existing lighting until reset') == 1
        # First Hard/Soft pair falls back; the pair after Off recovers.
        assert not re.search(r'SHADOW selected[^\r\n]+ active 1', intervals[6] + intervals[9])
        assert re.search(r'SHADOW selected[^\r\n]+ active 1', intervals[15] + intervals[18])
        assert report['occlusion_updates'] > 0, 'Existing lighting must survive shadow-resource failure'

if args.shadow_view_switch:
    assert re.findall(r'SHADOW CAMERA tick (\d+) third (\d+)', logs) == [('110','1'),('170','0')]
    intervals = re.split(r'SHADOW CAMERA tick \d+ third [01]', logs)
    assert all(re.search(r'SHADOW selected[^\r\n]+ view_weapon (?!-1\b)-?\d+', intervals[i]) for i in (0,2))
    assert re.search(r'SHADOW selected[^\r\n]+ view_weapon -1 view_hands -1', intervals[1])
