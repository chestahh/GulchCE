/*
FIRST_PERSON_LEGS.C

Your own legs in first person (Video Setup > FOV and Viewmodels > Legs,
display.first_person_legs; off, the stock game's: the body not drawn in its
own player's view). The body the game animates for everyone else is drawn
for its own player too, from the waist down: looking down while standing,
running or crouching, the legs move as the body's animation moves them.

Only the drawing changes, and only in that player's first-person view: its
shadow, its reflection, its hit boxes and markers are the game's as ever,
and nothing it carries or wears (its weapons, its children, its widgets) is
drawn with it.

The skeleton identifies the legs, torso, head and arms. The copied pose
keeps every bone's original scale and animation: folding torso bones into
the pelvis distorts the blended waist skin into a pouch. Feature-owned
index buffers omit the head and arms while retaining the intact torso-to-
hip surface (first_person_legs_geometry.c). The actual animated upper
surface and cut boundaries determine clearance, so no opening enters the
view as the player walks, crouches or changes FOV.

The pose is each local player's own (split-screen views draw their own
bodies). It is turned about the hips from the body's facing to the view's:
the game turns a standing body only once the aim is well away from it, and
slowly, and one in the air not at all, so the legs drawn as they face would
lag behind the view and, after a jump's turn, stand in front of it. It is
set back along the view's level facing, by at least FIRST_PERSON_LEGS_SET_BACK.
The clearance uses the animated surface, eye height and FOV, not the
current look pitch. Looking up and down alone does not slide the pose.
*/

#include "cseries.h"
#include "real_math.h"
#include "camera/director.h"
#include "game/players.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "render/render.h"
#include "render/render_cameras.h"
#include "tag_files/tag_groups.h"
#include "units/biped_definitions.h"

#include "first_person_legs.h"
#include "first_person_legs_geometry.h"

enum
{
	FIRST_PERSON_LEGS_MODELS = 8,
};

/* how far behind the eye the body is drawn, world units (about 49 cm), along
the view's level facing (tuned in-game) */
#define FIRST_PERSON_LEGS_SET_BACK 0.16f
/* port/linux/src/port_config.c */
int config_boolean(const char *name);
unsigned long config_changes(void);
/* port/linux/src (halo.log) */
void platform_log(const char *format, ...);

/* ---------- globals */

/* a biped definition's legs, as its skeleton has them */
struct first_person_legs_model
{
	long definition_index; /* NONE: free */
	/* the pelvis's child towards the head; NONE: no legs to draw */
	short spine_node_index;
	/* TRUE: head/arm geometry omitted from the local body */
	boolean omitted[MAXIMUM_NODES_PER_MODEL];
	boolean lower[MAXIMUM_NODES_PER_MODEL];
	short geometry_status; /* 0: unprepared, 1: ready, -1: unsupported */
};

static struct first_person_legs_model first_person_legs_models[FIRST_PERSON_LEGS_MODELS];
static short first_person_legs_next_model = 0;
static boolean first_person_legs_models_valid = FALSE;

/* each local player's pose of its legs, rewritten as its view is drawn */
static real_matrix4x3 first_person_legs_matrices[MAXIMUM_LOCAL_PLAYERS][MAXIMUM_NODES_PER_MODEL];

/* ---------- private code */

static boolean first_person_legs_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = FALSE;

	if (changes != config_changes())
	{
		changes = config_changes();
		if (enabled && !config_boolean("display.first_person_legs"))
			first_person_legs_reset();
		enabled = config_boolean("display.first_person_legs") != 0;
	}
	return enabled;
}

/* whether ancestor is node or above it (a map's parents are followed no
further than there are nodes, so a loop ends) */
static boolean first_person_legs_below(
	struct model const *model,
	short node_count,
	short node_index,
	short ancestor_index)
{
	short steps;

	for (steps = 0; steps <= node_count && node_index >= 0 && node_index < node_count; steps++)
	{
		if (node_index == ancestor_index)
			return TRUE;
		node_index = TAG_BLOCK_GET_ELEMENT(&model->nodes, node_index, struct model_node)->parent_node_index;
	}
	return FALSE;
}

/* a skeleton's legs, from its pelvis and head: the spine node, or NONE (and
why not, for the log) */
static short first_person_legs_find(
	struct model const *model,
	short node_count,
	short pelvis_index,
	short head_index,
	boolean *omitted,
	char const **reason)
{
	short spine_index = NONE;
	short leg_count = 0;
	short node_index;
	short steps;

	if (pelvis_index < 0 || pelvis_index >= node_count || head_index < 0 || head_index >= node_count ||
		head_index == pelvis_index)
	{
		*reason = "no pelvis and head nodes";
		return NONE;
	}
	/* (the spine: the head's ancestor whose parent is the pelvis) */
	node_index = head_index;
	for (steps = 0; steps <= node_count && node_index >= 0 && node_index < node_count; steps++)
	{
		short parent_index = TAG_BLOCK_GET_ELEMENT(&model->nodes, node_index, struct model_node)->parent_node_index;

		if (parent_index == pelvis_index)
		{
			spine_index = node_index;
			break;
		}
		node_index = parent_index;
	}
	if (spine_index == NONE)
	{
		*reason = "its head is not below its pelvis";
		return NONE;
	}
	for (node_index = 0; node_index < node_count; node_index++)
	{
		/* (the pelvis and the frames above it are the body's place, kept) */
		if (first_person_legs_below(model, node_count, pelvis_index, node_index))
			omitted[node_index] = FALSE;
		else if (first_person_legs_below(model, node_count, node_index, pelvis_index) &&
			!first_person_legs_below(model, node_count, node_index, spine_index))
		{
			omitted[node_index] = FALSE;
			leg_count++;
		}
		else if (node_index != head_index &&
			first_person_legs_below(model, node_count, head_index, node_index))
			omitted[node_index] = FALSE;
		else
			omitted[node_index] = TRUE;
	}
	if (leg_count == 0)
	{
		*reason = "no nodes below its pelvis but its spine";
		return NONE;
	}
	return spine_index;
}

/* a biped definition's legs, found once (and, when it has none, said once) */
static struct first_person_legs_model *first_person_legs_model_get(
	long definition_index,
	struct model const *model,
	short node_count,
	short pelvis_index,
	short head_index)
{
	struct first_person_legs_model *entry;
	char const *reason = "";
	short index;

	if (!first_person_legs_models_valid)
		first_person_legs_reset();
	for (index = 0; index < FIRST_PERSON_LEGS_MODELS; index++)
	{
		if (first_person_legs_models[index].definition_index == definition_index)
			return &first_person_legs_models[index];
	}
	entry = &first_person_legs_models[first_person_legs_next_model];
	first_person_legs_next_model = (short)((first_person_legs_next_model + 1) % FIRST_PERSON_LEGS_MODELS);
	entry->definition_index = definition_index;
	entry->geometry_status = 0;
	entry->spine_node_index = first_person_legs_find(model, node_count, pelvis_index, head_index,
		entry->omitted, &reason);
	for (index = 0; index < node_count; index++)
		entry->lower[index] = index != pelvis_index &&
			first_person_legs_below(model, node_count, index, pelvis_index) &&
			!first_person_legs_below(model, node_count, index, entry->spine_node_index);
	if (entry->spine_node_index == NONE)
		platform_log("first-person legs: %s has %s; its legs are not drawn\n", tag_get_name(definition_index), reason);
	return entry;
}

/* the way the view looks, level: its forward's level part, with its up's
(which points the way the forward did) so that looking straight down or up
it still has one; FALSE for none */
static boolean first_person_legs_view_heading(
	real_vector3d const *forward,
	real_vector3d const *up,
	real_vector3d *heading)
{
	heading->i = forward->i - forward->k * up->i;
	heading->j = forward->j - forward->k * up->j;
	heading->k = 0.f;
	return normalize3d(heading) > 0.f;
}

/* the way the body faces, level, in the pose drawn: the object's facing (as
the last tick turned it), taken into its first node's frame as that tick
left it, and out of that node's frame as drawn (between ticks), so the body
turns as smoothly as its pose; FALSE for none */
static boolean first_person_legs_body_heading(
	real_vector3d const *facing,
	real_matrix4x3 const *tick_root,
	real_matrix4x3 const *drawn_root,
	real_vector3d *heading)
{
	real forward = dot_product3d(facing, &tick_root->forward);
	real left = dot_product3d(facing, &tick_root->left);
	real up = dot_product3d(facing, &tick_root->up);

	heading->i = forward * drawn_root->forward.i + left * drawn_root->left.i + up * drawn_root->up.i;
	heading->j = forward * drawn_root->forward.j + left * drawn_root->left.j + up * drawn_root->up.j;
	heading->k = 0.f;
	return normalize3d(heading) > 0.f;
}

/* a vector turned about the vertical by the angle whose cosine and sine
these are */
static void first_person_legs_turn(
	real_vector3d *vector,
	real cosine,
	real sine)
{
	real i = vector->i;

	vector->i = cosine * i - sine * vector->j;
	vector->j = sine * i + cosine * vector->j;
}

/* ---------- public code */

void first_person_legs_reset(
	void)
{
	short index;

	first_person_legs_geometry_reset();
	for (index = 0; index < FIRST_PERSON_LEGS_MODELS; index++)
		first_person_legs_models[index].definition_index = NONE;
	first_person_legs_next_model = 0;
	first_person_legs_models_valid = TRUE;
}

real_matrix4x3 const *first_person_legs_node_matrices(
	short local_player_index,
	long object_index)
{
	struct object_datum *object;
	struct biped_definition *definition;
	struct model *model;
	struct first_person_legs_model *legs;
	real_matrix4x3 const *nodes;
	real_matrix4x3 *pose;
	real_vector3d view;
	real_vector3d body;
	real_vector3d back;
	real_point3d pivot;
	real set_back;
	real cosine = 1.f;
	real sine = 0.f;
	long player_index;
	short node_count;
	short index;

	if (!first_person_legs_enabled() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		director_get_perspective(local_player_index) != _director_perspective_first_person)
	{
		return NULL;
	}
	/* (the player's own living, visible biped, on foot) */
	player_index = local_player_get_player_index(local_player_index);
	object = object_try_and_get(object_index);
	if (player_index == NONE || player_get(player_index)->unit_index != object_index || !object ||
		object->object.type != _object_type_biped || object->object.parent_object_index != NONE ||
		TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||
		TEST_FLAG(object->object.flags, _object_invisible_bit))
	{
		return NULL;
	}
	definition = biped_definition_get(object->definition_index);
	if (!definition || definition->object.model.index == NONE)
		return NULL;
	model = model_definition_get(definition->object.model.index);
	node_count = (short)MIN(model->nodes.count, MAXIMUM_NODES_PER_MODEL);
	/* (the object's own matrices, as many as the model's nodes) */
	if (node_count <= 0 || object->object.node_matrices.size < node_count * (long)sizeof(real_matrix4x3))
		return NULL;
	legs = first_person_legs_model_get(object->definition_index, model, node_count,
		definition->biped.runtime_pelvis_node_index, definition->biped.runtime_head_node_index);
	if (legs->spine_node_index == NONE)
		return NULL;
	if (!legs->geometry_status)
	{
		legs->geometry_status = first_person_legs_geometry_prepare(object->definition_index,
			model, legs->omitted, legs->lower) ? 1 : -1;
		if (legs->geometry_status < 0)
			platform_log("first-person legs: %s has unsupported geometry; local body not drawn\n", tag_get_name(object->definition_index));
	}
	if (legs->geometry_status < 0)
		return NULL;

	nodes = object_get_node_matrices(object_index);
	pose = first_person_legs_matrices[local_player_index];
	/* (turned from the body's facing, the object's, to the view's; not
	from its first node's, which in the game's characters is the pelvis,
	its axis up the spine) */
	if (!first_person_legs_view_heading(&render.camera.forward, &render.camera.up, &view))
		return NULL;
	else if (first_person_legs_body_heading(&object->object.forward,
		(real_matrix4x3 const *)object_header_block_get(object_index, &object->object.node_matrices),
		&nodes[0], &body))
	{
		cosine = dot_product3d(&body, &view);
		sine = body.i * view.j - body.j * view.i;
	}
	back.i = -view.i;
	back.j = -view.j;
	back.k = 0.f;
	/* A model may have a separate root above the pelvis. Turn about the
	drawn hips so that an offset root does not move them as the view turns. */
	pivot = nodes[definition->biped.runtime_pelvis_node_index].position;
	set_back = first_person_legs_geometry_setback(object->definition_index, nodes, &pivot,
		cosine, sine, &view, &render.camera.position, render.camera.vertical_field_of_view, FIRST_PERSON_LEGS_SET_BACK);
	if (!(set_back < REAL_MAX))
		return NULL;
	for (index = 0; index < node_count; index++)
	{
		pose[index] = nodes[index];
		{
			real x = pose[index].position.x - pivot.x;
			real y = pose[index].position.y - pivot.y;

			pose[index].position.x = pivot.x + cosine * x - sine * y + back.i * set_back;
			pose[index].position.y = pivot.y + sine * x + cosine * y + back.j * set_back;
		}
		first_person_legs_turn(&pose[index].forward, cosine, sine);
		first_person_legs_turn(&pose[index].left, cosine, sine);
		first_person_legs_turn(&pose[index].up, cosine, sine);
	}
	return pose;
}

struct triangle_buffer const *first_person_legs_triangles(
	long object_index, struct model const *model,
	struct vertex_buffer const *vertices, struct triangle_buffer const *triangles)
{
	struct object_datum *object = object_try_and_get(object_index);
	short index;

	if (!object || !first_person_legs_enabled())
		return NULL;
	for (index = 0; index < FIRST_PERSON_LEGS_MODELS; index++)
	{
		struct first_person_legs_model const *legs = &first_person_legs_models[index];
		if (legs->definition_index == object->definition_index && legs->geometry_status > 0)
			return first_person_legs_geometry_get(object->definition_index, model,
				legs->omitted, legs->lower, vertices, triangles);
	}
	return NULL;
}
