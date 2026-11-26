"""Build a stock Windows benchmark from a local OpenCE or GulchCE Git revision."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROBE = Path(__file__).with_name('lighting_stock_control.inl')


def build(revision, output):
    commit = subprocess.check_output(
        ['git', 'rev-parse', '--verify', '--end-of-options', revision + '^{commit}'],
        cwd=ROOT, text=True).strip()
    build_root = (ROOT / 'build').resolve()
    if output == build_root or not output.is_relative_to(build_root) or output.exists():
        raise ValueError('Choose a new output directory below build/')
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build_root, prefix='stock-source-') as temp:
        tree = Path(temp) / 'source-tree'
        tree.mkdir()
        archive = Path(temp) / 'source.tar'
        subprocess.run(['git', 'archive', '--format=tar', '-o', str(archive), commit],
                       cwd=ROOT, check=True)
        with tarfile.open(archive) as files:
            for entry in files:
                if entry.isdir():
                    continue
                dest = tree / entry.name
                if not entry.isfile() or not dest.resolve().is_relative_to(tree):
                    raise ValueError(f'Unsupported archive entry {entry.name}')
                dest.parent.mkdir(parents=True, exist_ok=True)
                with files.extractfile(entry) as contents, dest.open('xb') as target:
                    shutil.copyfileobj(contents, target)
        main = tree / 'source/main/main.c'
        text = main.read_text(encoding='utf-8')
        hook = '\t\tmain_frame_rate_debug();'
        if text.count(hook) != 1:
            raise ValueError('Review the frame boundary in this revision')
        start = text.index('static void main_game_render(')
        probe = PROBE.read_text(encoding='utf-8')
        text = text[:start] + probe + '\n' + text[start:]
        main.write_text(text.replace(hook, hook + '\n\t\tif (render_frame && game_in_progress() && '
                                     '!game_time_menu_scene()) stock_benchmark_frame();'), encoding='utf-8')
        flags = ['--portable', '--release']
        subprocess.run([sys.executable, 'configure.py', *flags], cwd=tree, check=True)
        subprocess.run(['ninja', 'windows', '-j8'], cwd=tree, check=True)
        release = tree / 'build/windows'
        exe = release / 'halo.exe'
        manifest = dict(commit=commit, configure_args=flags,
                        probe_sha256=hashlib.sha256(probe.encode()).hexdigest(),
                        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
        staged = Path(temp) / 'package'
        staged.mkdir()
        shutil.copy2(exe, staged / 'halo-performance.exe')
        shutil.copy2(release / 'SDL3.dll', staged / 'SDL3.dll')
        (staged / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        output.parent.mkdir(parents=True, exist_ok=True)
        staged.rename(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('revision', help='Local commit or ref such as upstream/main')
    parser.add_argument('output', type=Path, help='New benchmark build directory')
    args = parser.parse_args()
    build(args.revision, args.output.resolve())

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
