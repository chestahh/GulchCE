"""Capture shield/menu output in a hidden window with isolated maps and saves.

Retains screenshots, saves and logs; removes only this run's copied maps and
generated cacheNNN.map files. Each run creates a new output directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--data-root", required=True, type=Path)
parser.add_argument("--label", required=True)
parser.add_argument("--look", choices=["classic", "enhanced"], default="classic")
parser.add_argument("--menu", choices=["video", "extra", "fov", "lighting"])
parser.add_argument("--size", default="960x540")
parser.add_argument("--map", default="bloodgulch")
parser.add_argument("--family", choices=["xbox", "ce", "mcc"], default="xbox")
parser.add_argument("--seconds", type=int, default=23)
parser.add_argument("--skip-cinematic", action="store_true")
parser.add_argument("--toggle", action="store_true")
parser.add_argument("--elite", action="store_true")
parser.add_argument("--no-target", action="store_true")
parser.add_argument("--arms", action="store_true")
parser.add_argument("--hide-viewmodel", action="store_true")
parser.add_argument("--glow", action="store_true")
parser.add_argument("--color", default="default")
args = parser.parse_args()
assert re.fullmatch(r"[a-zA-Z0-9_-]+", args.label)
out = ROOT / "build/enhanced-shields" / args.label
out.mkdir(exist_ok=False)
for name in ("maps", "frames"):
    (out / name).mkdir()
copies = []

def copy_map(source, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, dest)
    copies.append(dest)

def cleanup():
    for path in copies + list((out / "saves/z").glob("cache[0-9]*.map")):
        assert path.resolve().is_relative_to(out.resolve()) and not path.is_symlink()
        path.unlink(missing_ok=True)

try:
    copy_map(args.data_root / "maps/ui.map", out / "maps/ui.map")
    if not args.menu:
        if args.family == "ce":
            for name in (args.map, "bitmaps", "sounds", "loc"):
                copy_map(args.data_root / "custom_maps" / (name + ".map"), out / "custom_maps" / (name + ".map"))
            level = "custom_maps\\" + args.map
        elif args.family == "mcc":
            assert ".." not in Path(args.map).parts and not Path(args.map).is_absolute()
            copy_map(args.data_root / "mcc_maps" / (args.map + ".map"), out / "mcc_maps" / (args.map + ".map"))
            # Preserve MCC's package-local and family-specific resource lookup.
            for directory in (Path(args.map).parent, Path("_shared/singleplayer"), Path("_shared/multiplayer")):
                for name in ("bitmaps.map", "sounds.map", "loc.map"):
                    relative = Path("mcc_maps") / directory / name
                    if (args.data_root / relative).is_file():
                        copy_map(args.data_root / relative, out / relative)
            level = "mcc_maps\\" + args.map.replace("/", "\\")
        else:
            copy_map(args.data_root / "maps" / (args.map + ".map"), out / "maps" / (args.map + ".map"))
            level = args.map
        (out / "init.txt").write_text('map_name "' + level + '"\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith("HALO_")}
    env["PATH"] = str(ROOT / "build/windows") + os.pathsep + env.get("PATH", "")
    env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out / "saves"),
               HALO_HIDDEN_WINDOW="1", HALO_NO_AUDIO="1", HALO_NET_ONLINE="0",
               HALO_NET_PUBLIC_LOBBY="0", HALO_NET_JOIN_FROM_CLIPBOARD="0",
               HALO_UPDATE_ANSWER="never", HALO_CRASH_REPORTS="no", HALO_MAX_FPS="30",
               HALO_NO_VSYNC="1", HALO_WINDOW_SIZE=args.size, HALO_SHIELD_LOOK=args.look,
               HALO_VIEWMODEL_SHIELD=str(int(args.arms)), HALO_VIEWMODEL_VISIBLE=str(int(not args.hide_viewmodel)),
               HALO_SHIELD_GLOW=str(int(args.glow)), HALO_SHIELD_COLOR=args.color,
               HALO_EXIT_AFTER="12" if args.menu else str(args.seconds), HALO_SCREENSHOT_DIR=str(out / "frames"),
               HALO_SCREENSHOT_EVERY="180")
    if args.skip_cinematic:
        env.update(HALO_TELNET_CONSOLE="1", HALO_TELNET_CONSOLE_PORT="23374")
    if args.toggle:
        env["HALO_SHIELD_CAPTURE_TOGGLE"] = "1"
    if args.elite:
        env["HALO_SHIELD_CAPTURE_ELITE"] = "1"
    if args.no_target:
        env["HALO_SHIELD_CAPTURE_NO_TARGET"] = "1"
    if args.menu:
        menu = {"video": "video_settings_screen", "extra": "extra_effects/extra_effects_settings_screen",
                "fov": "fov_viewmodels/fov_viewmodel_settings_screen",
                "lighting": "dynamic_lighting/dynamic_lighting_settings_screen"}[args.menu]
        env["HALO_MENU_OPEN"] = "main_menu/settings_select/player_setup/player_profile_edit/video_settings/" + menu
        exe = ROOT / "build/windows/halo.exe"
    else:
        exe = ROOT / "build/enhanced-shields/halo-capture.exe"
        shutil.copyfile(ROOT / "build/enhanced-shields/capture-manifest.json", out / "capture-manifest.json")
    # Native configuration lives beside the executable, not HALO_DATA_ROOT.
    # Keep live setting writes confined to this run as well as its saves.
    copy_map(exe, out / "halo.exe")
    exe = out / "halo.exe"
    start = time.monotonic()
    with (out / "stdout.log").open("wb") as stdout, (out / "stderr.log").open("wb") as stderr:
        process = subprocess.Popen([str(exe)], cwd=out, env=env, stdout=stdout, stderr=stderr,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            if args.skip_cinematic:
                time.sleep(12)
                with socket.create_connection(("127.0.0.1", 23374), timeout=5) as connection:
                    connection.recv(4096)
                    connection.sendall(b'(cinematic_stop)\r\n(camera_control false)\r\n')
                    # Let the nonblocking game console process and acknowledge
                    # the commands before closing its socket.
                    time.sleep(1)
                    connection.settimeout(1)
                    try:
                        connection.recv(65536)
                    except socket.timeout:
                        pass
            process.wait(timeout=args.seconds + 45)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    logs = (out / "stderr.log").read_text(errors="replace")
    frames = list((out / "frames").glob("*"))
    report = dict(label=args.label, look=args.look, size=args.size, menu=args.menu, map=args.map,
                  family=args.family, exit=process.returncode, frames=len(frames),
                  options=vars(args) | {"data_root": str(args.data_root)},
                  environment={k: v for k, v in env.items() if k.startswith("HALO_")},
                  executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  seconds=round(time.monotonic() - start, 2),
                  ticks=re.findall(r"SHIELD screenshot tick (\d+)", logs),
                  shader_errors=bool(re.search(r"(?i)(shader.*(?:failed|error)|cannot (?:compile|link)|GL_INVALID)", logs)))
    (out / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)
    assert process.returncode == 0 and frames and not report["shader_errors"], report
finally:
    cleanup()
