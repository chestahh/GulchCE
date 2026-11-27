"""Visible surface IDs and dynamic triangle offsets are distinct across a gap."""
from tools.harness import build, function, read, run


def test_only_owned_surfaces_draw_at_their_original_offsets():
    source = read("source/structures/structure_render.c")
    program = build("structure_render_ranges", (("under_test.inc", function(source, "structure_render_pass")),))
    status, output = run(program, "gaps")
    assert status == 0, output
