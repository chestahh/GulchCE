"""Build an isolated menu acceptance executable and restore production sources."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shlex
import shutil
import subprocess
from harness import function

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/modern-shadow-menu-build')
    options = parser.parse_args()
    out = options.output_dir.resolve()
    assert out.is_relative_to((ROOT/'build').resolve())
    out.mkdir(parents=True, exist_ok=True)
    menu = ROOT/'port/linux/game/menu_functions.c'
    ui = ROOT/'source/interface/ui_widget.c'
    originals = {p: p.read_bytes() for p in (menu, ui)}
    args = shlex.split(re.search(r'^configure_args = (.*)$', (ROOT/'build.ninja').read_text(), re.M)[1])
    probe = (ROOT/'tools/fixtures/modern_shadow_menu.inc').read_text()
    manifest = dict(configure_args=args, probe_sha256=hashlib.sha256(probe.encode()).hexdigest(),
                    source_hashes={p.relative_to(ROOT).as_posix(): hashlib.sha256(v).hexdigest() for p,v in originals.items()})
    try:
        menu.write_text(menu.read_text()+'\n'+probe)
        text = ui.read_text()
        body = function(text, 'render_ui_widgets')
        anchor = '\tmatch_assert('
        assert body.count(anchor) == 1
        body = body.replace(anchor, '\tmodern_shadow_menu_accept(widget_globals.active_widgets[0]);\n'+anchor)
        text = text.replace(function(text, 'render_ui_widgets'), body)
        ui.write_text('struct widget_instance; void modern_shadow_menu_accept(struct widget_instance *);\n'+text)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
        shutil.copy2(ROOT/'build/windows/halo.exe', out/'halo-menu.exe')
        manifest['exe_sha256'] = hashlib.sha256((out/'halo-menu.exe').read_bytes()).hexdigest()
        (out/'manifest.json').write_text(json.dumps(manifest, indent=2))
    finally:
        for path, content in originals.items(): path.write_bytes(content)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
