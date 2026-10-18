"""Build an isolated shield capture executable; always restore production sources.

Run with the normal Windows compiler environment. No fixture is shipped.
"""
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/enhanced-shields"
OUT.mkdir(parents=True, exist_ok=True)
tick = ROOT / "port/linux/game/render_interpolation.c"
backend = ROOT / "port/linux/src/d3d8_gl.c"
fixture = ROOT / "port/linux/game/shield_capture_fixture.c"
assert not fixture.exists()
originals = {p: p.read_bytes() for p in (tick, backend)}
args = shlex.split(re.search(r"^configure_args = (.*)$", (ROOT / "build.ninja").read_text(), re.M)[1])
manifest = {"base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "configure_args": args, "sources": {}}
for p in list((ROOT / "port").rglob("*shield*.c")) + [tick, backend] + [ROOT / name for name in (
        "port/linux/src/nv2a_psh.c", "port/linux/src/nv2a_vsh.c", "port/linux/src/xgpu.h",
        "port/linux/include/shield_look.h", "source/interface/first_person_weapons.c",
        "source/rasterizer/xbox/rasterizer_xbox_models.c",
        "source/rasterizer/xbox/rasterizer_xbox_plasma_energy.c", "tools/fixtures/shield_look_capture.c")]:
    manifest["sources"][str(p.relative_to(ROOT))] = hashlib.sha256(p.read_bytes()).hexdigest()
try:
    shutil.copyfile(ROOT / "tools/fixtures/shield_look_capture.c", fixture)
    text = originals[tick].decode().replace("\r\n", "\n")
    text = text.replace("void render_interpolation_tick(void)", "void shield_capture_tick(void);\nvoid render_interpolation_tick(void)", 1)
    marker = "long previous_tick = interpolation_tick++;"
    assert text.count(marker) == 1
    tick.write_text(text.replace(marker, marker + "\n\tshield_capture_tick();", 1))
    text = originals[backend].decode().replace("\r\n", "\n")
    marker = "if (screenshot_every > 0 && device.frame % (unsigned long)screenshot_every == 0)"
    assert text.count(marker) == 1
    backend.write_text("int shield_capture_should_capture(void);\n" + text.replace(marker, "if (shield_capture_should_capture())", 1))
    subprocess.run([sys.executable, "configure.py", *args], cwd=ROOT, check=True)
    subprocess.run(["ninja", "windows"], cwd=ROOT, check=True)
    shutil.copyfile(ROOT / "build/windows/halo.exe", OUT / "halo-capture.exe")
    manifest["sha256"] = hashlib.sha256((OUT / "halo-capture.exe").read_bytes()).hexdigest()
    (OUT / "capture-manifest.json").write_text(json.dumps(manifest, indent=2))
finally:
    for p, data in originals.items():
        p.write_bytes(data)
    fixture.unlink(missing_ok=True)
    subprocess.run([sys.executable, "configure.py", *args], cwd=ROOT, check=True)
    subprocess.run(["ninja", "windows"], cwd=ROOT, check=True)
