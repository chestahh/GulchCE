"""Run the repository's native/browser transport oracle using a Windows harness.

Only the harness OS services and runner paths/build step are adapted. Production
WebRTC, DTLS and crypto sources and browser acceptance thresholds are unchanged.
Generated copies and compiler output stay under build/m0/webrtc.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/m0/webrtc'

WINDOWS_HEADERS = r'''
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned long in_addr_t;
typedef int socklen_t;
typedef int ssize_t;
#define close closesocket
#define read _read
/* poll's socket and stdin-pipe readiness, without changing the test loop. */
static int m0_poll(struct pollfd *waits, int count, int milliseconds)
{
    fd_set sockets;
    struct timeval timeout = {0, 0};
    DWORD available = 0;
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    (void)count;
    waits[0].revents = waits[1].revents = 0;
    FD_ZERO(&sockets);
    FD_SET(waits[0].fd, &sockets);
    if (select(0, &sockets, NULL, NULL, &timeout) > 0)
        waits[0].revents = POLLIN;
    if (!PeekNamedPipe(input, NULL, 0, NULL, &available, NULL))
        waits[1].revents = POLLHUP;
    else if (available)
        waits[1].revents = POLLIN;
    if (!waits[0].revents && !waits[1].revents) Sleep(milliseconds);
    return !!waits[0].revents + !!waits[1].revents;
}
'''


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Unexpected anchor: {old[:100]!r}')
    return text.replace(old, new)


def main():
    if os.name != 'nt':
        raise SystemExit('Use the original webrtc_native.mjs on POSIX')
    WORK.mkdir(parents=True, exist_ok=True)
    harness = (ROOT / 'port/web/tests/webrtc_native.c').read_text()
    first = harness.index('#include <arpa/inet.h>')
    last = harness.index('#include <unistd.h>') + len('#include <unistd.h>')
    harness = harness[:first] + WINDOWS_HEADERS + harness[last:]
    harness = replace(harness, 'if (getrandom(buffer, size, 0) != (ssize_t)size)',
                      'if (BCryptGenRandom(NULL, buffer, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)')
    harness = replace(harness, '\tstruct timespec now;\n\n\tclock_gettime(CLOCK_MONOTONIC, &now);\n'
                      '\treturn (unsigned long)(now.tv_sec * 1000 + now.tv_nsec / 1000000);',
                      '\treturn (unsigned long)GetTickCount64();')
    harness = replace(harness, '\tunsigned long reported = 0;',
                      '\tunsigned long reported = 0;\n\tWSADATA startup;\n\tWSAStartup(MAKEWORD(2, 2), &startup);')
    harness = replace(harness, '\t\tpoll(waits, 2, 5);', '\t\tm0_poll(waits, 2, 5);')
    source = WORK / 'webrtc_native_windows.c'
    source.write_text(harness)
    mbed = ROOT / 'port/third_party/mbedtls'
    mono = ROOT / 'port/third_party/monocypher'
    sources = sorted((mbed / 'library').glob('*.c')) + [
        ROOT / 'port/linux/src/posix_dtls.c', ROOT / 'port/linux/src/p2p_webrtc.c',
        ROOT / 'port/linux/src/p2p_crypto.c', mono / 'monocypher.c', mono / 'monocypher-ed25519.c', source]
    flags = ['--target=i686-pc-windows-msvc', '-std=gnu11', '-O1', '-g', '-w',
             '-D_CRT_SECURE_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS',
             '-D_WIN32_WINNT=0x0601', '-I' + str(mbed / 'include'), '-I' + str(mbed / 'library'),
             '-I' + str(mono), '-I' + str(ROOT / 'port/linux/src'),
             '-I' + str(ROOT / 'port/windows/include/posix')]
    objects = [WORK / f'{i}.obj' for i in range(len(sources))]
    def compile_one(item):
        path, target = item
        subprocess.run(['clang', *flags, '-c', str(path), '-o', str(target)], check=True)
    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(compile_one, zip(sources, objects)))
    program = WORK / 'webrtc_native.exe'
    subprocess.run(['clang', '--target=i686-pc-windows-msvc', '-fuse-ld=lld', '-o', str(program),
                    *map(str, objects), '-lws2_32', '-lbcrypt'], check=True)
    script = (ROOT / 'port/web/tests/webrtc_native.mjs').read_text()
    script = re.sub(r'^const root = .*;', 'const root = ' + json.dumps(ROOT.as_posix()) + ';', script, flags=re.M)
    script = replace(script, "const work = mkdtempSync(path.join(tmpdir(), 'opence-webrtc-'));",
                     'const work = mkdtempSync(path.join(' + json.dumps(WORK.as_posix()) + ", 'run-'));")
    script = replace(script, 'const program = await build();',
                     'const program = ' + json.dumps(program.as_posix()) + ';')
    script = replace(script, 'playwright[kind].launch()',
                     'playwright[kind].launch({executablePath: process.env[kind === "chromium" ? "M0_CHROMIUM" : "M0_FIREFOX"]})')
    script = replace(script, "const [key, value] = line.split(' ');",
                     "const [key, value] = line.trim().split(' ');")
    runner = WORK / 'webrtc_native_windows.mjs'
    runner.write_text(script)
    manifest = {'production_sources': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in sources if p != source},
                'harness_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'runner_sha256': hashlib.sha256(runner.read_bytes()).hexdigest(),
                'binary_sha256': hashlib.sha256(program.read_bytes()).hexdigest()}
    (WORK / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    subprocess.run(['node', str(runner), 'chromium', 'firefox'], check=True, cwd=ROOT)


if __name__ == '__main__':
    main()
