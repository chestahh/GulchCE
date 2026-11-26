"""Check the Windows BSP light index stream against the bytes requested by each draw."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run, structure


@pytest.mark.parametrize("case", ["dense", "sparse", "wrap", "new-pass"])
def test_light_index_upload(case):
    renderer = read("port/linux/src/d3d8_gl.c")
    code = "\n".join([
        structure(renderer, "bsp_light_indices"),
        "static struct bsp_light_indices bsp_index_batch;",
        function(renderer, "bsp_light_index_batch_set"),
        function(renderer, "index_upload"),
        function(renderer, "bsp_light_index_upload"),
    ])
    status, output = run(build("bsp_light_indices", (("under_test.inc", code),)), case)
    assert status == 0, output
