"""The local player's own legs in first person (port/linux/game/first_person_legs.c, display.first_person_legs): the
legs found from the biped's pelvis and head, the intact pose guarded by geometry preparation, only the player's own
living biped on foot in first person, a pose per local player, and the legs found forgotten with a map."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run

CASES = ["off", "legs", "setback", "turn", "pivot", "interpolated", "fov-shape", "geometry-failure", "geometry-reset",
         "pitch-stable", "who", "unusable", "loops", "split", "reset"]
CONTROLS = {
    # a separate root makes the hips orbit as the view turns
    "root-pivot": (("pivot = nodes[definition->biped.runtime_pelvis_node_index].position;",
                    "pivot = nodes[0].position;"), "pivot"),
    # interpolation must move the pivot with the drawn pelvis, not its tick pose
    "tick-pivot": (("pivot = nodes[definition->biped.runtime_pelvis_node_index].position;",
                    "pivot = ((real_matrix4x3 const *)object_header_block_get(object_index, "
                    "&object->object.node_matrices))[definition->biped.runtime_pelvis_node_index].position;"),
                   "interpolated"),
    # the setting ignored
    "always-on": (("if (!first_person_legs_enabled() || local_player_index", "if (local_player_index"), "off"),
    "collapsed-again": (("pose[index] = nodes[index];", "pose[index] = nodes[index]; pose[index].scale *= 0.001f;"), "legs"),
    "head-retained": (("node_index != head_index &&", "TRUE &&"), "legs"),
    "geometry-failure-ignored": (("model, legs->omitted, legs->lower) ? 1 : -1", "model, legs->omitted, legs->lower) ? 1 : 1"), "geometry-failure"),
    "geometry-not-reset": (("first_person_legs_geometry_reset();", ""), "geometry-reset"),
    # the view's heading from its forward alone: none looking straight down
    "forward-only-heading": (("heading->i = forward->i - forward->k * up->i;", "heading->i = forward->i;"), "setback"),
    # the body's heading from its first node (the pelvis's axis, up the spine)
    "first-node-facing": (("first_person_legs_body_heading(&object->object.forward,",
                           "first_person_legs_body_heading(&nodes[0].forward,"), "turn"),
    # not turned to the view: the legs lag its turns, and after a jump's half turn stand in front of it
    "not-turned": (("cosine = dot_product3d(&body, &view);\n\t\tsine = body.i * view.j - body.j * view.i;", ""), "turn"),
    # set back along the body's facing, not the view's
    "body-set-back": (("back.i = -view.i;\n\tback.j = -view.j;",
                       "back.i = -object->object.forward.i;\n\tback.j = -object->object.forward.j;"), "turn"),
    # the bones' axes left as they were
    "axes-unturned": (("first_person_legs_turn(&pose[index].forward, cosine, sine);", ""), "turn"),
    # the body's facing from the tick's frame alone, a tick behind the pose drawn
    "tick-facing": (("(real_matrix4x3 const *)object_header_block_get(object_index, &object->object.node_matrices),",
                     "&nodes[0],"), "interpolated"),
    # in third person too
    "third-person": (("director_get_perspective(local_player_index) != _director_perspective_first_person", "FALSE"),
                     "who"),
    # a dead body's legs
    "dead-drawn": (("TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||", ""), "who"),
    # found again (and said again) each frame
    "not-cached": (("if (first_person_legs_models[index].definition_index == definition_index)\n"
                    "\t\t\treturn &first_person_legs_models[index];", ""), "unusable"),
    # one pose for every view
    "one-pose": (("pose = first_person_legs_matrices[local_player_index];", "pose = first_person_legs_matrices[0];"),
                 "split"),
    # the last map's legs kept
    "no-reset": (("first_person_legs_models[index].definition_index = NONE;", ";"), "reset"),
}


def define(source, name):
    line = next((line for line in source.splitlines() if line.startswith(f"#define {name} ")), None)
    if line is None:
        raise LookupError(f"#define not found in the sources: {name}")
    return line


def generated(control=None):
    source = read("port/linux/game/first_person_legs.c")
    globals_ = source[source.index("/* ---------- globals */"):source.index("/* ---------- private code */")]
    code = "\n".join([enum_with(source, "FIRST_PERSON_LEGS_MODELS"),
                      define(source, "FIRST_PERSON_LEGS_SET_BACK"),
                      "void first_person_legs_reset(void);", globals_] +
                     [function(source, name) for name in ["first_person_legs_enabled", "first_person_legs_below",
                                                          "first_person_legs_find", "first_person_legs_model_get",
                                                          "first_person_legs_reset", "first_person_legs_view_heading",
                                                          "first_person_legs_body_heading",
                                                          "first_person_legs_turn",
                                                          "first_person_legs_node_matrices"]])
    if control:
        code = mutated(code, *CONTROLS[control][0])
    return (("under_test.inc", code),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("first_person_legs", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", CONTROLS)
def test_negative_control(control):
    status, output = run(build("first_person_legs", generated(control)), CONTROLS[control][1])
    assert status == CHECK_FAILED, f"{control} escaped its check: {output}"
