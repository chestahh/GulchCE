"""Exercise the actual private mesh builder and animated surface clearance."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run

CASES = ["mesh", "strip", "weights", "palette", "malformed", "failure",
         "lifecycle", "boundary", "clearance", "invalid-view"]
CONTROLS = {
    "retain-head": (("node >= node_count || upper[node]", "node >= node_count"), "mesh"),
    "strip-winding": (("(triangle & 1) ? b : a", "FALSE ? b : a"), "strip"),
    "ignore-second-weight": (("index < 2", "index < 1"), "weights"),
    "ignore-cut": (("edge[vertex] || !leg_only", "!leg_only"), "boundary"),
    "ignore-torso": (("edge[vertex] || !leg_only", "edge[vertex] || (leg_only && FALSE)"), "boundary"),
    "bone-instead-of-surface": (("&boundary->local[0], &a", "&origin, &a"), "clearance"),
    "ignore-animation": (("&nodes[boundary->nodes[0]]", "&identity"), "clearance"),
    "ignore-weight": (("a.z * boundary->weight + b.z * (1.f - boundary->weight)", "a.z"), "clearance"),
    "no-clearance": (("result = MAX(result, required - current);", ";"), "clearance"),
    "no-release": (("rasterizer_triangle_buffer_delete(&first_person_legs_parts[index].triangles);", ";"), "lifecycle"),
}


def generated(control=None):
    source = read("port/linux/game/first_person_legs_geometry.c")
    # The complete production implementation, with only its engine includes
    # replaced by the fixture's API/type declarations.
    source = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
    if control:
        source = mutated(source, *CONTROLS[control][0])
    return (("under_test.inc", source),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("first_person_legs_geometry", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", CONTROLS)
def test_negative_control(control):
    status, output = run(build("first_person_legs_geometry", generated(control)), CONTROLS[control][1])
    assert status == CHECK_FAILED, f"{control} escaped its check: {output}"
