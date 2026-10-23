/* The local player's own legs in first person (port/linux/game/first_person_legs.c) */
#include "harness.h"
#include <math.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define PIN(x, lo, hi) MIN(MAX((x), (lo)), (hi))
#define _pi ((real)3.14159265358979323846f)
#define MAXIMUM_LOCAL_PLAYERS 4
#define MAXIMUM_NODES_PER_MODEL 64
#define TAG_BLOCK_GET_ELEMENT(block, index, type) (&((type *)(block)->address)[index])
typedef unsigned short word;
typedef struct { real scale; real_vector3d forward, left, up; real_point3d position; } real_matrix4x3;
struct tag_block { long count; void *address; };
struct model_node { short parent_node_index; };
struct model { struct tag_block nodes; };
struct biped_definition
{
	struct { struct { long index; } model; } object;
	struct { short runtime_pelvis_node_index, runtime_head_node_index; } biped;
};
struct object_datum
{
	long definition_index;
	struct
	{
		unsigned long flags;
		real_vector3d forward;
		short type;
		word damage_flags;
		long parent_object_index;
		struct { short size, offset; } node_matrices;
	} object;
};
enum { _object_type_biped = 0, _object_type_vehicle = 1 };
enum { _object_invisible_bit = 0 };
enum { _object_dead_bit = 2 };
enum { _director_perspective_first_person = 0, _director_perspective_third_person = 1 };

static real_vector3d const zero_vector = {0, 0, 0};
static real_vector3d const *global_zero_vector3d = &zero_vector;
static real normalize3d(real_vector3d *v)
{
	real length = sqrtf(v->i * v->i + v->j * v->j + v->k * v->k);
	if (length > 0) { v->i /= length; v->j /= length; v->k /= length; }
	return length;
}

static int setting = 1;
static int config_boolean(const char *name) { (void)name; return setting; }
static unsigned long config_changes(void) { static unsigned long changes; return ++changes; }
static int logs;
static void platform_log(const char *format, ...) { (void)format; logs++; }
static char const *tag_get_name(long index) { (void)index; return "characters\\cyborg\\cyborg"; }

/* two local players, each with a biped (objects 0 and 1, definitions 10 and 11) */
static short perspective[MAXIMUM_LOCAL_PLAYERS];
static struct { long unit_index; } players[2] = {{0}, {1}};
static struct object_datum objects[2];
static struct biped_definition definitions[2];
static struct model model;
static struct model_node skeleton[16];
static real_matrix4x3 node_matrices[2][16];
static short director_get_perspective(short local) { return perspective[local]; }
static long local_player_get_player_index(short local) { return local < 2 ? local : NONE; }
#define player_get(index) (&players[index])
static struct object_datum *object_try_and_get(long index) { return index >= 0 && index < 2 ? &objects[index] : NULL; }
#define biped_definition_get(index) (&definitions[(index) - 10])
static struct model *model_definition_get(long index) { (void)index; return &model; }
static real_matrix4x3 *object_get_node_matrices(long index) { return node_matrices[index]; }
/* the matrices as the last tick left them (object_get_node_matrices': between ticks) */
static real_matrix4x3 tick_nodes[2][16];
static void *object_header_block_get(long index, void *reference) { (void)reference; return tick_nodes[index]; }
/* the view */
static struct { struct { real_point3d position; real_vector3d forward, up; real vertical_field_of_view; } camera; } render;
/* the vertical angle of a view this many degrees across at 16:9 */
static real vertical_of(double degrees) { return (real)(2.0 * atan(tan(degrees * 3.14159265358979323846 / 360.0) * (9.0 / 16.0))); }
static real dot_product3d(real_vector3d const *a, real_vector3d const *b) { return a->i * b->i + a->j * b->j + a->k * b->k; }
#define REAL_MAX 3.402823466e+38F
static int geometry_ready = 1, geometry_resets;
static boolean first_person_legs_geometry_prepare(long definition, struct model const *model, boolean const *upper, boolean const *lower)
{ (void)definition; (void)model; (void)upper; (void)lower; return geometry_ready; }
static real first_person_legs_geometry_setback(long definition, real_matrix4x3 const *nodes, real_point3d const *pivot,
    real cosine, real sine, real_vector3d const *view, real_point3d const *eye, real fov, real minimum)
{ (void)definition; (void)nodes; (void)pivot; (void)cosine; (void)sine; (void)view; (void)eye; (void)fov; return minimum; }
static void first_person_legs_geometry_reset(void) { geometry_resets++; }
#include "under_test.inc"

/* 0 frame; 1 pelvis; 2 spine, 3 spine1, 4 head, 9 clavicle; 5 l thigh, 6 l calf; 7 r thigh, 8 r calf; 10 a node
on the frame (a gun's) */
enum { FRAME, PELVIS, SPINE, SPINE1, HEAD, L_THIGH, L_CALF, R_THIGH, R_CALF, CLAVICLE, GUN, NODES };
static short const parents[NODES] = {NONE, FRAME, PELVIS, SPINE, SPINE1, PELVIS, L_THIGH, PELVIS, R_THIGH, SPINE1, FRAME};

static void world(void)
{
	short i, o;
	for (i = 0; i < NODES; i++) skeleton[i].parent_node_index = parents[i];
	model.nodes.count = NODES;
	model.nodes.address = skeleton;
	for (o = 0; o < 2; o++) {
		memset(&objects[o], 0, sizeof(objects[o]));
		objects[o].definition_index = 10 + o;
		objects[o].object.type = _object_type_biped;
		objects[o].object.parent_object_index = NONE;
		objects[o].object.forward.i = 1;
		objects[o].object.node_matrices.size = (short)(NODES * sizeof(real_matrix4x3));
		definitions[o].object.model.index = 5;
		definitions[o].biped.runtime_pelvis_node_index = PELVIS;
		definitions[o].biped.runtime_head_node_index = HEAD;
		for (i = 0; i < NODES; i++) {
			memset(&node_matrices[o][i], 0, sizeof(real_matrix4x3));
			node_matrices[o][i].scale = 1;
			node_matrices[o][i].forward.i = 1;
			node_matrices[o][i].left.j = 1;
			node_matrices[o][i].up.k = 1;
			node_matrices[o][i].position.x = 10.f * o + i;
			node_matrices[o][i].position.z = (real)i;
		}
	}
	memcpy(tick_nodes, node_matrices, sizeof(tick_nodes));
	/* (looking ahead along +x, as the body faces) */
	memset(&render, 0, sizeof(render));
	/* Keep the closure outside the view for the basic pose tests. The
	   clipping cases below place the eye close to the actual waist. */
	render.camera.position.x = 10.f;
	render.camera.forward.i = 1;
	render.camera.up.k = 1;
	render.camera.vertical_field_of_view = vertical_of(90.0);
	perspective[0] = perspective[1] = _director_perspective_first_person;
	setting = 1;
	logs = 0;
	first_person_legs_reset();
}

static int same_place(real_matrix4x3 const *a, real_matrix4x3 const *b, real dx)
{
	return fabsf(a->position.x - (b->position.x + dx)) < 1e-5f && fabsf(a->position.y - b->position.y) < 1e-5f &&
		fabsf(a->position.z - b->position.z) < 1e-5f;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	real const back = -FIRST_PERSON_LEGS_SET_BACK;
	world();
	CASE("off") {
		int before;
		CHECK(first_person_legs_node_matrices(0, 0), "enable before disabling");
		before = geometry_resets;
		setting = 0;
		CHECK(!first_person_legs_node_matrices(0, 0), "off, the body is not drawn in its own view");
		CHECK(geometry_resets == before + 1, "disable releases private geometry");
		setting = 1;
		CHECK(first_person_legs_node_matrices(0, 0), "enable rebuilds the pose");
	}
	CASE("legs") {
        real_matrix4x3 const *pose = first_person_legs_node_matrices(0, 0);
        short i;
        CHECK(pose != NULL, "the player's own biped on foot");
        for (i = 0; i < NODES; i++) {
            CHECK(same_place(&pose[i], &node_matrices[0][i], back), "node %d has its original position", i);
            CHECK(pose[i].scale == node_matrices[0][i].scale, "node %d keeps its scale", i);
        }
        CHECK(!first_person_legs_models[0].omitted[SPINE] && first_person_legs_models[0].omitted[HEAD],
            "retain the torso, omit the head");
        CHECK(first_person_legs_models[0].lower[L_THIGH] && !first_person_legs_models[0].lower[PELVIS] &&
            !first_person_legs_models[0].lower[SPINE], "surface clearance distinguishes legs from torso");
    }
    CASE("geometry-failure") {
        geometry_ready = 0;
        CHECK(!first_person_legs_node_matrices(0, 0), "unsafe/incomplete geometry hides the optional body");
    }
    CASE("geometry-reset") {
        int before = geometry_resets;
        first_person_legs_reset();
        CHECK(geometry_resets == before + 1, "map reset releases the private geometry");
    }
	CASE("setback") {
		real_matrix4x3 const *pose;
		/* (looking 60 degrees down, straight down, straight up: the view's heading, level, each time) */
		real const views[3][6] = {{0.5f, 0, -0.8660254f, 0.8660254f, 0, 0.5f}, {0, 0, -1, 1, 0, 0}, {0, 0, 1, -1, 0, 0}};
		short v;
		for (v = 0; v < 3; v++) {
			render.camera.forward.i = views[v][0]; render.camera.forward.j = views[v][1]; render.camera.forward.k = views[v][2];
			render.camera.up.i = views[v][3]; render.camera.up.j = views[v][4]; render.camera.up.k = views[v][5];
			pose = first_person_legs_node_matrices(0, 0);
			CHECK(pose && same_place(&pose[L_CALF], &node_matrices[0][L_CALF], back), "the whole set-back, level (view %d)", v);
		}
		/* (the first node's axis up the spine, as a pelvis's is: the body's facing is the object's) */
		node_matrices[0][FRAME].forward.i = 0; node_matrices[0][FRAME].forward.k = 1;
		node_matrices[0][FRAME].up.i = -1; node_matrices[0][FRAME].up.k = 0;
		memcpy(tick_nodes[0], node_matrices[0], sizeof(tick_nodes[0]));
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && same_place(&pose[L_CALF], &node_matrices[0][L_CALF], back), "no turn: the body faces the view");
	}
	CASE("turn") {
		/* (the body faces +x, the view +y: turned about the pelvis, set back along -y) */
		real_matrix4x3 const *pose;
		/* (the first node's axis up the spine, as a pelvis's is) */
		node_matrices[0][FRAME].forward.i = 0; node_matrices[0][FRAME].forward.k = 1;
		node_matrices[0][FRAME].up.i = -1; node_matrices[0][FRAME].up.k = 0;
		memcpy(tick_nodes[0], node_matrices[0], sizeof(tick_nodes[0]));
		render.camera.forward.i = 0; render.camera.forward.j = 1;
		render.camera.position.y = 10.f;
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose != NULL, "drawn");
		CHECK(fabsf(pose[L_CALF].position.x - 1.f) < 1e-5f &&
			fabsf(pose[L_CALF].position.y - (5.f - FIRST_PERSON_LEGS_SET_BACK)) < 1e-5f &&
			fabsf(pose[L_CALF].position.z - 6.f) < 1e-5f,
			"a calf 5 ahead of the pelvis turns about the hips, set back from the view (%g %g)",
			pose[L_CALF].position.x, pose[L_CALF].position.y);
		CHECK(fabsf(pose[L_CALF].forward.i) < 1e-5f && fabsf(pose[L_CALF].forward.j - 1.f) < 1e-5f,
			"its axes turned with it");
		CHECK(fabsf(pose[L_CALF].up.k - 1.f) < 1e-5f, "about the vertical");
		/* (a jump's half turn: the body still faces +x, the view -x) */
		render.camera.forward.i = -1; render.camera.forward.j = 0;
		render.camera.position.x = -10.f;
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && fabsf(pose[L_CALF].position.x - (-4.f + FIRST_PERSON_LEGS_SET_BACK)) < 1e-5f,
			"half turned, still ahead of the view, not behind it (%g)", pose ? pose[L_CALF].position.x : 0);
	}
	CASE("pivot") {
		real const headings[3][2] = {{0.f, 1.f}, {-1.f, 0.f}, {0.f, -1.f}};
		short root, turn;
		/* Both a separate offset root and the stock layout, with the pelvis at node zero. */
		for (root = 0; root < 2; root++) {
			short pelvis = root ? FRAME : PELVIS;
			world();
			definitions[0].biped.runtime_pelvis_node_index = pelvis;
			skeleton[SPINE].parent_node_index = pelvis;
			skeleton[L_THIGH].parent_node_index = pelvis;
			skeleton[R_THIGH].parent_node_index = pelvis;
			node_matrices[0][pelvis].position.x = 0.1f;
			node_matrices[0][pelvis].position.y = 0.2f;
			memcpy(tick_nodes, node_matrices, sizeof(tick_nodes));
			for (turn = 0; turn < 3; turn++) {
				real_matrix4x3 const *pose;
				real_point3d expected = node_matrices[0][pelvis].position;
				render.camera.forward.i = headings[turn][0];
				render.camera.forward.j = headings[turn][1];
				render.camera.position.x = expected.x + headings[turn][0] * 10.f;
				render.camera.position.y = expected.y + headings[turn][1] * 10.f;
				expected.x -= headings[turn][0] * FIRST_PERSON_LEGS_SET_BACK;
				expected.y -= headings[turn][1] * FIRST_PERSON_LEGS_SET_BACK;
				pose = first_person_legs_node_matrices(0, 0);
				CHECK(pose && fabsf(pose[pelvis].position.x - expected.x) < 1e-5f &&
					fabsf(pose[pelvis].position.y - expected.y) < 1e-5f &&
					fabsf(pose[pelvis].position.z - expected.z) < 1e-5f,
					"pelvis %d stays anchored when the view turns %d", pelvis, turn);
				CHECK(memcmp(node_matrices, tick_nodes, sizeof(tick_nodes)) == 0,
					"the world pose remains unchanged");
			}
		}
	}
	CASE("interpolated") {
		/* (between ticks the drawn pose has turned 30 degrees on from the tick's: the body's facing goes with it) */
		real_matrix4x3 const *pose;
		real const c = 0.8660254f, s = 0.5f;
		short i;
		for (i = 0; i < NODES; i++) {
			real x = node_matrices[0][i].position.x - 1.f, y = node_matrices[0][i].position.y;
			node_matrices[0][i].position.x = 1.3f + c * x - s * y;
			node_matrices[0][i].position.y = 0.2f + s * x + c * y;
			node_matrices[0][i].forward.i = c; node_matrices[0][i].forward.j = s;
			node_matrices[0][i].left.i = -s; node_matrices[0][i].left.j = c;
		}
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && fabsf(pose[L_CALF].forward.i - 1.f) < 1e-5f && fabsf(pose[L_CALF].forward.j) < 1e-5f,
			"the drawn body faces the view, whatever the tick's");
		CHECK(fabsf(pose[PELVIS].position.x - (1.3f - FIRST_PERSON_LEGS_SET_BACK)) < 1e-5f &&
			fabsf(pose[PELVIS].position.y - 0.2f) < 1e-5f &&
			fabsf(pose[L_CALF].position.x - (6.3f - FIRST_PERSON_LEGS_SET_BACK)) < 1e-5f &&
			fabsf(pose[L_CALF].position.y - 0.2f) < 1e-5f,
			"the interpolated pelvis anchors the turned pose, not the tick's pelvis or the root");
	}
	CASE("fov-shape") {
		real_matrix4x3 baseline[NODES];
		real const fovs[] = {20.f, 90.f, 110.f, 120.f, 150.f};
		short f, i;
		render.camera.position.x = 0.3f;
		render.camera.position.y = -0.2f;
		render.camera.position.z = 2.7f;
		render.camera.vertical_field_of_view = vertical_of(20.f);
		memcpy(baseline, first_person_legs_node_matrices(0, 0), sizeof(baseline));
		for (f = 0; f < 5; f++) {
			real_matrix4x3 const *pose;
			real dx, dy;
			render.camera.vertical_field_of_view = vertical_of(fovs[f]);
			pose = first_person_legs_node_matrices(0, 0);
			CHECK(pose != NULL, "the legs remain drawn");
			dx = pose[PELVIS].position.x - baseline[PELVIS].position.x;
			dy = pose[PELVIS].position.y - baseline[PELVIS].position.y;
			for (i = 0; i < NODES; i++) {
				CHECK(fabsf(pose[i].position.x - baseline[i].position.x - dx) < 1e-4f &&
					fabsf(pose[i].position.y - baseline[i].position.y - dy) < 1e-4f &&
					pose[i].position.z == baseline[i].position.z &&
					pose[i].scale == baseline[i].scale,
					"FOV %g translates the whole pose without deforming node %d", fovs[f], i);
			}
		}
	}
	CASE("pitch-stable") {
		real const views[][6] = {
			{1, 0, 0, 0, 0, 1}, {0.5f, 0, -0.8660254f, 0.8660254f, 0, 0.5f},
			{0, 0, -1, 1, 0, 0}, {0, 0, 1, -1, 0, 0}
		};
		real_matrix4x3 baseline[NODES];
		short v;
		render.camera.position.x = 0.3f;
		render.camera.position.y = -0.2f;
		render.camera.position.z = 2.7f;
		render.camera.vertical_field_of_view = vertical_of(150.f);
		memcpy(baseline, first_person_legs_node_matrices(0, 0), sizeof(baseline));
		for (v = 0; v < 4; v++) {
			real_matrix4x3 const *pose;
			render.camera.forward.i = views[v][0]; render.camera.forward.j = views[v][1];
			render.camera.forward.k = views[v][2]; render.camera.up.i = views[v][3];
			render.camera.up.j = views[v][4]; render.camera.up.k = views[v][5];
			pose = first_person_legs_node_matrices(0, 0);
			CHECK(pose && memcmp(pose, baseline, sizeof(baseline)) == 0,
				"looking up/down does not move or deform the body (view %d)", v);
		}
	}
	CASE("who") {
		CHECK(!first_person_legs_node_matrices(0, 1), "another player's body");
		CHECK(!first_person_legs_node_matrices(-1, 0) && !first_person_legs_node_matrices(MAXIMUM_LOCAL_PLAYERS, 0),
			"no such local player");
		CHECK(!first_person_legs_node_matrices(2, 0), "a local player without a player");
		perspective[0] = _director_perspective_third_person;
		CHECK(!first_person_legs_node_matrices(0, 0), "third person");
		perspective[0] = _director_perspective_first_person;
		objects[0].object.damage_flags = FLAG(_object_dead_bit);
		CHECK(!first_person_legs_node_matrices(0, 0), "dead");
		objects[0].object.damage_flags = 0;
		objects[0].object.flags = FLAG(_object_invisible_bit);
		CHECK(!first_person_legs_node_matrices(0, 0), "hidden");
		objects[0].object.flags = 0;
		objects[0].object.parent_object_index = 1;
		CHECK(!first_person_legs_node_matrices(0, 0), "riding something");
		objects[0].object.parent_object_index = NONE;
		objects[0].object.type = _object_type_vehicle;
		CHECK(!first_person_legs_node_matrices(0, 0), "not a biped");
		objects[0].object.type = _object_type_biped;
		objects[0].object.node_matrices.size = (short)((NODES - 1) * sizeof(real_matrix4x3));
		CHECK(!first_person_legs_node_matrices(0, 0), "fewer matrices than the model's nodes");
		objects[0].object.node_matrices.size = (short)(NODES * sizeof(real_matrix4x3));
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "and drawn again");
	}
	CASE("unusable") {
		definitions[0].biped.runtime_pelvis_node_index = NONE;
		CHECK(!first_person_legs_node_matrices(0, 0) && !first_person_legs_node_matrices(0, 0), "no pelvis");
		CHECK(logs == 1, "said once (%d)", logs);
		definitions[1].biped.runtime_head_node_index = L_CALF; /* (a head not below the spine: under a leg) */
		skeleton[L_THIGH].parent_node_index = FRAME;
		CHECK(!first_person_legs_node_matrices(1, 1), "a head not below the pelvis");
		CHECK(logs == 2, "said once more (%d)", logs);
		world();
		skeleton[L_THIGH].parent_node_index = SPINE;
		skeleton[R_THIGH].parent_node_index = SPINE;
		CHECK(!first_person_legs_node_matrices(0, 0), "no legs below the pelvis but the spine");
		CHECK(logs == 1, "said (%d)", logs);
	}
	CASE("loops") {
		skeleton[SPINE].parent_node_index = SPINE1; /* (spine and spine1 each other's parent) */
		CHECK(!first_person_legs_node_matrices(0, 0), "a loop in a map's parents ends");
		world();
		skeleton[PELVIS].parent_node_index = 40; /* (a parent past the nodes) */
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "a parent past the nodes is no node");
	}
	CASE("split") {
		real_matrix4x3 const *first = first_person_legs_node_matrices(0, 0);
		real_matrix4x3 const *second;
		render.camera.position.x = 20.f;
		second = first_person_legs_node_matrices(1, 1);
		CHECK(first && second && first != second, "each view's own pose");
		CHECK(same_place(&first[L_CALF], &node_matrices[0][L_CALF], back), "the first view keeps its body's pose");
		CHECK(same_place(&second[L_CALF], &node_matrices[1][L_CALF], back), "the second its own");
	}
	CASE("reset") {
		definitions[0].biped.runtime_pelvis_node_index = NONE;
		CHECK(!first_person_legs_node_matrices(0, 0), "a map whose biped has no legs");
		definitions[0].biped.runtime_pelvis_node_index = PELVIS; /* (the next map's, at the same index) */
		CHECK(!first_person_legs_node_matrices(0, 0), "the same map: as found");
		first_person_legs_reset();
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "a new map: found again");
	}
	return 0;
}
