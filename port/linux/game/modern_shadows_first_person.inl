/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

/* Included by first_person_weapons.c: its pose and remapping tables are
   private. Only the enabled desktop shadow collector calls this accessor;
   it never advances animation or changes the visible model's matrices. */
#include "modern_shadows.h"

#ifndef HALO_GLES
int modern_shadows_viewmodel(int hands, long *model_index, long *owner,
	real_matrix4x3 *nodes)
{
	struct first_person_weapon *first;
	struct weapon_definition const *definition;
	struct model const *model;
	struct animation_graph const *graph;
	short const *remap;
	long player_index, unit_index, animation_index;
	int node;
	*model_index = *owner = NONE;
	if (render.local_player_index == NONE || render.camera.mirrored || !viewmodel_is_visible()) return 0;
	player_index = local_player_get_player_index(render.local_player_index);
	if (player_index == NONE) return 0;
	unit_index = player_get(player_index)->unit_index;
	first = first_person_weapon_get(render.local_player_index);
	if (unit_index == NONE || !first->visible || first->unit_index != unit_index || first->weapon_index == NONE) return 0;
	definition = weapon_definition_get(weapon_get(first->weapon_index)->definition_index);
	animation_index = definition->weapon.interface_definition.first_person_animations.index;
	if (animation_index == NONE) return 0;
	/* A map can omit either model. Its hidden world counterpart must still
	   stay out of the other viewmodel's shadow map. */
	*owner = hands ? unit_index : first->weapon_index;
	if (hands)
	{
		struct tag_block const *interfaces = &scenario_get_game_globals()->first_person_interface;
		struct game_globals_first_person_interface const *interface;
		if (!first->hands_node_remapping_table_valid || interfaces->count < 1) return 0;
		interface = TAG_BLOCK_GET_ELEMENT(interfaces, 0, struct game_globals_first_person_interface);
		*model_index = interface->hands.index;
		remap = first->hands_node_remapping_table;
	}
	else
	{
		if (!first->weapon_node_remapping_table_valid) return 0;
		*model_index = definition->weapon.interface_definition.first_person_model.index;
		remap = first->weapon_node_remapping_table;
	}
	if (*model_index == NONE) return 0;
	model = model_definition_get(*model_index);
	graph = animation_graph_definition_get(animation_index);
	if (model->nodes.count < 1 || model->nodes.count > MAXIMUM_NODES_PER_ANIMATION) return -1;
	for (node = 0; node < model->nodes.count; node++)
		if (!VALID_INDEX(remap[node], graph->nodes.count) || remap[node] >= MAXIMUM_NODES_PER_ANIMATION) return -1;
	model_remap_node_matrices_to_match_animation_graph(*model_index, nodes,
		animation_index, first->node_matrices, remap);
	return 1;
}
#endif
