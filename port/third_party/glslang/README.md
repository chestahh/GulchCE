# Khronos glslang

The native Vulkan backend compiles translated GLSL to SPIR-V through a small
host-ABI library. `tools/vulkan_compiler.py` downloads the unmodified upstream
source into the ignored build directory and verifies its checksum. No xemu
code or Xbox SDK material is used.

- Upstream: https://github.com/KhronosGroup/glslang
- Version: 16.6.0
- Source archive: https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz
- SHA256: `9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2`
- Licence: upstream `LICENSE.txt` (BSD-style notices, MIT and Apache 2.0
  components), copied unchanged beside the compiled library and in packages.

The independently written wrapper is in `port/linux/src/vulkan/compiler/`.
HLSL, SPIRV-Tools optimization, tests and external dependencies are disabled.
OpenGL never loads this compiler. The native game remains 32-bit; the
compiler library uses its host ABI and owns all of its internal allocations.

## Portable Linux C++ runtime

The 32-bit compiler uses the existing Debian 11 sysroot plus unmodified GCC
10.2.1-6 i386 development/runtime packages. The C++ and GCC runtime libraries
are linked statically and hidden from the library's public symbol table;
this preserves the game's glibc 2.31 baseline without requiring a newer
system libstdc++. The package paths and SHA256 digests are in
`tools/vulkan_compiler.py`, from Debian's official bullseye package index.
Their upstream is GCC (https://gcc.gnu.org/); libstdc++ is GPL-3.0-or-later
with GCC Runtime Library Exception 3.1. The original Debian copyright notice
and the following unchanged licence texts ship with the Linux library:

- `runtime-licenses/COPYING3`: GCC `releases/gcc-10.2.0`, SHA256
  `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903`.
- `runtime-licenses/COPYING.RUNTIME`: same tag, SHA256
  `9d6b43ce4d8de0c878bf16b54d8e7a10d9bd42b75178153e3af6a815bdc90f74`.

Source for those texts: https://github.com/gcc-mirror/gcc/tree/releases/gcc-10.2.0.
No GCC runtime source is copied into the project; the verified development
packages stay in the ignored build sysroot. The supplemental 64-bit validation
compiler uses its host's runtime and is not a distributed game component.
