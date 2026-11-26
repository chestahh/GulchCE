"""Compare actual packed reconstruction against scalar at cell boundaries."""
import re
from tools.harness import build, read, run, function


def test_packed_visibility_preserves_every_cell():
    source = read('port/linux/game/plasma_occlusion.c')
    start = source.index('/* Four receiver cells')
    end = source.index('static real filter_visibility', start)
    code = 'enum { OCC_SIZE=16 };\n' + function(source, 'depth_visibility') + '\n' + source[start:end]
    for variant in (code, code.replace('#if defined(__SSE2__)', '#if 0')):
        status, output = run(build('plasma_visibility', (('under_test.inc', variant),)), 'compare')
        assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
