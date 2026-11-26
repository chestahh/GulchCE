"""Enemy name visibility rays are needed only for drawable labels."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run


def test_hud_name_projection():
    code = function(read("source/interface/hud.c"), "hud_draw_player_name")
    status, output = run(build("hud_name_projection", (("under_test.inc", code),)), "projection")
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6
