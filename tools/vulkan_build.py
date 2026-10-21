"""Build the native Vulkan foundation test without changing the game backend."""
import argparse
import os
from pathlib import Path
import subprocess
import shlex
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ("port/linux/src/vulkan/vulkan_device.c", "port/linux/src/vulkan/tests/smoke.c")
OUTPUT = Path("build/vulkan/vulkan-smoke" + (".exe" if os.name == "nt" else ""))


def generate_build(n, sln):
    compiler = getattr(sln, "windows_cc" if os.name == "nt" else "linux_cc", None) or "clang"
    n.rule("vulkan_smoke", command='$python tools/vulkan_build.py --cc "$vulkan_cc"',
           description="VULKAN FOUNDATION $out")
    inputs = [Path(source) for source in SOURCES]
    inputs += list(Path("port/linux/src/vulkan").rglob("*.h"))
    inputs += list(Path("port/third_party/vulkan_headers/include").rglob("*.h"))
    inputs += [Path("tools/vulkan_build.py")]
    n.build(OUTPUT, "vulkan_smoke", implicit=inputs, variables={"vulkan_cc": compiler})
    n.build("vulkan", "phony", inputs=[OUTPUT])
    library = Path("build/windows/halo_vulkan_compiler.dll" if os.name == "nt" else "build/linux/libhalo_vulkan_compiler.so")
    n.rule("vulkan_compiler", command="$python tools/vulkan_compiler.py", description="VULKAN SHADER COMPILER $out")
    n.build(library, "vulkan_compiler", implicit=[Path("tools/vulkan_compiler.py"),
            Path("port/linux/src/vulkan/compiler/CMakeLists.txt"), Path("port/linux/src/vulkan/compiler/compiler.c"), Path("port/linux/src/vulkan/compiler/linux.cmake")])
    n.build("vulkan_compiler", "phony", inputs=[library])


def build(compiler, host_validation=False, window=False, scene=False):
    output = ROOT / OUTPUT
    if host_validation:
        output = ROOT / "build/vulkan-host" / output.name
    if window:
        output = output.with_name("vulkan-window" + output.suffix)
    sources = list(SOURCES) if not window else ["port/linux/src/vulkan/" + name for name in
        ("vulkan_device.c", "vulkan_present.c", "vulkan_window.c", "vulkan_scene.c", "vulkan_shader.c", "tests/window.c")]
    if scene:
        output = output.with_name("vulkan-scene" + output.suffix)
        sources = ["port/linux/src/vulkan/" + name for name in ("vulkan_device.c", "vulkan_shader.c", "tests/scene.c")]
    output.parent.mkdir(parents=True, exist_ok=True)
    flags = ["-std=gnu89", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
             "-Wno-missing-field-initializers", "-ffp-contract=off",
             "-Iport/third_party/vulkan_headers/include"]
    if os.name == "nt":
        target = "x86_64" if host_validation else "i686"
        flags += [f"--target={target}-pc-windows-msvc", "-D_CRT_SECURE_NO_WARNINGS", "-fuse-ld=lld"]
        libraries = []
    else:
        flags += ["-m64" if host_validation else "-m32"]
        libraries = ["-ldl"]
    if window or scene:
        if os.name == "nt":
            import sys
            sys.path.insert(0, str(ROOT))
            from tools.windows_build import SDL_DIR
            architecture = "x64" if host_validation else "x86"
            flags += ["-I" + str(SDL_DIR / "include")]
            libraries += [str(SDL_DIR / "lib" / architecture / "SDL3.lib")]
            shutil.copy2(SDL_DIR / "lib" / architecture / "SDL3.dll", output.parent)
        else:
            flags += shlex.split(subprocess.check_output(["pkg-config", "--cflags", "sdl3"], text=True))
            libraries += shlex.split(subprocess.check_output(["pkg-config", "--libs", "sdl3"], text=True))
    subprocess.run([compiler, *flags, *sources, *libraries, "-o", str(output)], cwd=ROOT, check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--host-validation", action="store_true",
                        help="additional 64-bit test when only 64-bit validation layers are installed")
    parser.add_argument("--window", action="store_true", help="SDL window lifecycle and pixel verification test")
    parser.add_argument("--scene", action="store_true", help="native textures, draws and visibility queries")
    args = parser.parse_args()
    build(args.cc, args.host_validation, args.window, args.scene)
