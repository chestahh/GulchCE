"""Exercise the decal submission code using vertices at and outside the viewport edge."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, inline, read, run


@pytest.mark.parametrize("case", ["mixed", "offscreen", "edge", "enclosing", "mirror", "no-projection"])
def test_decal_clip(case):
    renderer = read("source/rasterizer/xbox/rasterizer_xbox_decals.c")
    math = read("source/math/real_math.h")
    functions = [inline(math, name) for name in ("dot_product3d", "plane3d_distance_to_point")]
    functions += [function(renderer, name) for name in
                  ("decal_outside_window", "rasterizer_decal_set_vertex", "rasterizer_decals_draw_batch")]
    code = (("under_test.inc", "\n".join(functions)),)
    status, output = run(build("decal_clip", code), case)
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
