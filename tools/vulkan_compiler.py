"""Build the pinned native GLSL compiler; no compiler process is spawned by the game."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = '16.6.0'
SHA256 = '9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2'
URL = f'https://github.com/KhronosGroup/glslang/archive/refs/tags/{VERSION}.tar.gz'


# Unmodified Debian 11 i386 runtime/development packages. Static linking
# keeps the optional compiler independent of a player's libstdc++ version.
CPP_PACKAGES = (
    ('libstdc++-10-dev_10.2.1-6_i386.deb', '4b629a2acb5e19cc0c7bf2a39f7ab7c268f977ce07e78ad9853648da93868d40'),
    ('libstdc++6_10.2.1-6_i386.deb', 'ef9b1f03163ad32c8f940b9a9e417773ff3d2f08ad2044d9c28fed5b81de3b7e'),
    ('gcc-10-base_10.2.1-6_i386.deb', '81692425b611e12e961bb98d720ebcac8b7682acbfda87bf4dabbfd878bc47e2'),
)


def portable_cpp(cache):
    sys.path.insert(0, str(ROOT))
    from tools.linux_sysroot import make, download, deb_data, extract
    folder = ROOT / 'build/linux/third_party'
    make(folder)
    root = folder / 'sysroot'
    stamp = cache / 'cpp-stamp'
    signature = repr(CPP_PACKAGES)
    if not stamp.exists() or stamp.read_text() != signature or not (root/'usr/include/c++/10/vector').exists():
        for name, digest in CPP_PACKAGES:
            payload = download([mirror+'pool/main/g/gcc-10/'+name for mirror in
                                ('https://deb.debian.org/debian/', 'https://archive.debian.org/debian/')], digest)
            with deb_data(payload) as archive:
                extract(archive, root)
                if name.startswith('gcc-10-base'):
                    member = next(m for m in archive.getmembers() if m.name.endswith('/gcc-10-base/copyright'))
                    (cache/'gcc-runtime-copyright.txt').write_bytes(archive.extractfile(member).read())
        stamp.write_text(signature)
    return root


def build(host_validation=False):
    cache = ROOT / 'build/vulkan-compiler'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / f'glslang-{VERSION}.tar.gz'
    if not archive.exists():
        archive.write_bytes(urllib.request.urlopen(URL, timeout=120).read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise RuntimeError('glslang source checksum mismatch')
    source = cache / f'glslang-{VERSION}'
    if not source.exists():
        with tarfile.open(archive) as tar:
            tar.extractall(cache, filter='data')
    output = cache / (('windows' if os.name == 'nt' else 'linux') + ('-host' if host_validation else ''))
    target = 'x86_64' if host_validation else 'i686'
    args = ['cmake', '-S', str(ROOT / 'port/linux/src/vulkan/compiler'), '-B', str(output),
            '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', f'-DGLSLANG_SOURCE={source}']
    if os.name == 'nt':
        args += ['-DCMAKE_C_COMPILER=clang-cl', '-DCMAKE_CXX_COMPILER=clang-cl',
                 f'-DCMAKE_C_FLAGS=--target={target}-pc-windows-msvc',
                 f'-DCMAKE_CXX_FLAGS=--target={target}-pc-windows-msvc',
                 '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded']
        name = 'halo_vulkan_compiler.dll'
        destination = ROOT / 'build/windows'
    else:
        if host_validation:
            args += ['-DCMAKE_C_FLAGS=-m64', '-DCMAKE_CXX_FLAGS=-m64']
        else:
            root = portable_cpp(cache)
            args += [f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/port/linux/src/vulkan/compiler/linux.cmake', f'-DHALO_SYSROOT={root}']
        name = 'libhalo_vulkan_compiler.so'
        destination = ROOT / 'build/linux'
    if host_validation:
        destination = ROOT / "build/vulkan-host"
    subprocess.run(args, check=True)
    subprocess.run(['cmake', '--build', str(output), '--parallel', '4'], check=True)
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(output / name, destination / name)
    shutil.copy2(source / 'LICENSE.txt', destination / 'glslang-LICENSE.txt')
    if os.name != 'nt' and not host_validation:
        shutil.copy2(cache/'gcc-runtime-copyright.txt', destination)
        for name in ('COPYING3', 'COPYING.RUNTIME'):
            shutil.copy2(ROOT/'port/third_party/glslang/runtime-licenses'/name, destination/('gcc-'+name))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-validation', action='store_true')
    build(parser.parse_args().host_validation)
