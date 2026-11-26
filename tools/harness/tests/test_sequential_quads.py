"""Check sequential quad submission against the renderer's generated indices."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run


@pytest.mark.parametrize("gles", [False, True])
def test_quad_submission(gles):
    source = read("port/linux/src/d3d8_gl.c")
    start = source.index("#define SEQUENTIAL_QUAD_VERTICES")
    limit = source[start:source.index("\n", start)]
    parts = ["#define HALO_GLES" if gles else "", limit, function(source, "quad_indices")]
    if not gles:
        parts.append(function(source, "create_sequential_quad_buffer"))
    parts.append(function(source, "draw_sequential_quads"))
    executable = build("sequential_quads", (("under_test.inc", "\n".join(parts)),))
    status, output = run(executable, "quadlists")
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
