"""Build the native Vulkan foundation test without changing the game backend."""
import argparse
import os
from pathlib import Path
import subprocess

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


def build(compiler, host_validation=False):
    output = ROOT / OUTPUT
    if host_validation:
        output = ROOT / "build/vulkan-host" / output.name
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
    subprocess.run([compiler, *flags, *SOURCES, *libraries, "-o", str(output)], cwd=ROOT, check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--host-validation", action="store_true",
                        help="additional 64-bit test when only 64-bit validation layers are installed")
    args = parser.parse_args()
    build(args.cc, args.host_validation)
