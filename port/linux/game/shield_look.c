/*
SHIELD_LOOK.C

Optional plasma profiles overriding biped shield materials. The map owns its tags and pixels; this module owns only
the draw-time shader copies. See docs/shield_look.md for the inspected
data, appearance precedence and shared-engine hooks.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "real_math.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "rasterizer/rasterizer_model_types.h"
#include "scenario/scenario.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "units/units.h"
#include "shield_look.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *config_string(const char *name);

/* Plasma tag layout, shared by the validated Xbox, CE and MCC runtime
tags. Keep the references intact: each map's own texture cache serves them. */
struct shield_look_plasma
{
	struct shader shader;
	byte reserved28[4];
	short intensity_source;
	short pad2E;
	real intensity_exponent;
	short offset_source;
	short pad36;
	real offset_amount;
	real offset_exponent;
	byte reserved40[0x20];
	real perpendicular_alpha;
	real_rgb_color perpendicular_color;
	real parallel_alpha;
	real_rgb_color parallel_color;
	short color_source;
	byte reserved82[0x3E];
	real primary_period;
	real_vector3d primary_direction;
	real primary_scale;
	struct tag_reference primary_map;
	byte reservedE4[0x24];
	real secondary_period;
	real_vector3d secondary_direction;
	real secondary_scale;
	struct tag_reference secondary_map;
	byte reserved12C[0x20];
};

typedef char shield_look_plasma_size_check[sizeof(struct shield_look_plasma) == 0x14C ? 1 : -1];
typedef char shield_look_primary_map_check[offsetof(struct shield_look_plasma, primary_map) == 0xD4 ? 1 : -1];
typedef char shield_look_secondary_map_check[offsetof(struct shield_look_plasma, secondary_map) == 0x11C ? 1 : -1];

struct shield_look_model_shader
{
	struct tag_reference shader;
	short permutation;
	short pad;
	long unused[3];
};

enum { SHIELD_LOOK_MAXIMUM = 64 };
struct shield_look_entry
{
	long definition_index;
	long hands_index;
	long model_index;
	struct shader const *authored_modifier;
	boolean eligible;
	boolean sprinkles;
	boolean honeycomb;
	struct shield_look_plasma body;
	struct shield_look_plasma arms;
};

/* Never recycled during a map: transparent draw packets retain pointers
until they are submitted, including each split-screen view. */
static struct shield_look_entry shield_looks[SHIELD_LOOK_MAXIMUM];
static short shield_look_count;
static boolean shield_look_noise_searched;
static long shield_look_noise_index = NONE;

static boolean shield_look_enabled(void)
{
	char const *value = config_string("display.shield_look");
	return value && (!strcmp(value, "enhanced") || !strcmp(value, "enhanced2") ||
		!strcmp(value, "classic_with_sprinkles") || !strcmp(value, "honeycomb"));
}

static uint32_t shield_look_hash(uint32_t hash, void const *data, size_t size)
{
	byte const *bytes = data;
	size_t index;

	for (index = 0; index < size; index++)
		hash = (hash ^ bytes[index]) * UINT32_C(16777619);
	return hash;
}

static boolean shield_look_stock_noise(long tag_index)
{
	struct bitmap_group *group;
	struct bitmap_data *bitmap;
	byte *pixels;
	long size;
	uint32_t hash;
	boolean complete = FALSE;

	if (tag_index == NONE || tag_get_group_tag(tag_index) != 'bitm')
		return FALSE;
	group = bitmap_group_get(tag_index);
	if (!cache_file_tag_cache_contains(group, sizeof(*group)) || group->bitmaps.count != 1 ||
		!cache_file_tag_cache_contains(group->bitmaps.address, sizeof(struct bitmap_data)))
		return FALSE;
	bitmap = group->bitmaps.address;
	if (bitmap->width != 32 || bitmap->height != 32 || bitmap->depth != 32 ||
		bitmap->type != 1 || bitmap->mipmap_count != 5 || (bitmap->format != 3 && bitmap->format != 11))
		return FALSE;
	/* Compression, palettes and linear addressing would interpret the
	stored bytes differently. Only the inspected volume layouts qualify. */
	if ((bitmap->flags & 0x1E) != 0 && (bitmap->flags & 0x1E) != 8)
		return FALSE;
	/* Full mip chain, excluding hardware padding. The cache rounds Xbox
	reads up to a sector; the allocation includes that tail. */
	size = bitmap->format == 3 ? 74898 : 149796;
	if (bitmap->pixels_size < size || bitmap->pixels_offset < 0)
		return FALSE;
	pixels = malloc((size + 4095) & ~4095);
	if (!pixels)
		return FALSE;
	memset(pixels, 0, (size + 4095) & ~4095);
	cache_file_read(tag_index, bitmap->pixels_offset, size, pixels, &complete, TRUE);
	while (!complete)
		SwitchToThread();
	hash = shield_look_hash(UINT32_C(2166136261), pixels, size);
	free(pixels);
	/* Xbox A8Y8, CE ARGB, and the same CE pixels packed by MCC's existing
	bitmap adapter. No foreign resource path or adapter is used here. */
	return complete == TRUE && (bitmap->format == 3 ?
		(bitmap->flags & 8) && hash == UINT32_C(0xF2F70788) :
		(bitmap->flags & 8) ? hash == UINT32_C(0x18AC1E94) : hash == UINT32_C(0xE7F72DA4));
}

/* Resolve the exact CMT noise through this map's existing resource loader.
A custom modifier may reference different pixels; never use those for ENHANCED. */
static long shield_look_noise(void)
{
	struct tag_iterator iterator;
	long index;

	if (!shield_look_noise_searched)
	{
		shield_look_noise_searched = TRUE;
		tag_iterator_new(&iterator, 'bitm');
		while ((index = tag_iterator_next(&iterator)) != NONE)
		{
			if (shield_look_stock_noise(index))
			{
				shield_look_noise_index = index;
				break;
			}
		}
	}
	return shield_look_noise_index;
}

static void shield_look_profile(struct shield_look_plasma *plasma, boolean arms)
{
	/* CMT Snow Grove's body tag 1935 and hands tag 2163. These are shader
	parameters, not extracted textures, geometry or executable code. */
	plasma->intensity_source = 2;
	plasma->intensity_exponent = 1.f;
	plasma->offset_source = 3;
	plasma->offset_amount = arms ? 0.009f : 0.035f;
	plasma->offset_exponent = 2.f;
	plasma->perpendicular_alpha = plasma->parallel_alpha = 1.f;
	plasma->perpendicular_color.red = 0.909804f;
	plasma->perpendicular_color.green = 0.772549f;
	plasma->perpendicular_color.blue = 0.f;
	plasma->parallel_color.red = 0.937255f;
	plasma->parallel_color.green = 0.694118f;
	plasma->parallel_color.blue = 0.0627451f;
	plasma->color_source = 0;
	plasma->primary_period = 2.1f;
	plasma->primary_direction.i = 2.f;
	plasma->primary_direction.j = 0.f;
	plasma->primary_direction.k = -0.5f;
	plasma->primary_scale = arms ? 45.f : 18.f;
	plasma->secondary_period = 3.2f;
	plasma->secondary_direction.i = 0.f;
	plasma->secondary_direction.j = 2.f;
	plasma->secondary_direction.k = 0.5f;
	plasma->secondary_scale = arms ? 62.f : 14.f;
}

/* Stock Xbox cyborg shield surface, inspected in Beaver Creek. Both presets
use the same verified noise volume. The separate layer envelope is shared. */
static void shield_look_classic_profile(struct shield_look_plasma *plasma)
{
	plasma->perpendicular_color.red = 1.f;
	plasma->perpendicular_color.green = 0.545098f;
	plasma->perpendicular_color.blue = 0.243137f;
	plasma->parallel_color.red = 0.0666667f;
	plasma->parallel_color.green = 0.0666667f;
	plasma->parallel_color.blue = 1.f;
	plasma->primary_period = 2.2f;
	plasma->primary_direction.k = -0.3f;
	plasma->primary_scale = 10.f;
	plasma->secondary_period = 3.f;
	plasma->secondary_scale = 6.f;
}

static struct shield_look_entry *shield_look_for_unit(long object_index, long hands_index)
{
	struct object_datum *object;
	struct object_definition *definition;
	struct shield_look_plasma const *plasma;
	struct shader const *shader;
	short intensity_source;
	struct shield_look_entry *entry;
	short index;
	long shader_index;
	long noise_index;
	boolean sprinkles;
	boolean honeycomb;

	if (!shield_look_enabled())
		return NULL;
	sprinkles = !strcmp(config_string("display.shield_look"), "classic_with_sprinkles");
	honeycomb = !strcmp(config_string("display.shield_look"), "honeycomb");
	object = object_try_and_get(object_index);
	if (!object || object->object.type != _object_type_biped)
		return NULL;
	for (index = 0; index < shield_look_count; index++)
	{
		entry = &shield_looks[index];
		if (entry->definition_index == object->definition_index && entry->hands_index == hands_index &&
			entry->sprinkles == sprinkles && entry->honeycomb == honeycomb)
			return entry->eligible ? entry : NULL;
	}
	/* Refuse overflow rather than recycling memory still used by a draw. */
	if (shield_look_count == SHIELD_LOOK_MAXIMUM)
		return NULL;
	entry = &shield_looks[shield_look_count++];
	entry->definition_index = object->definition_index;
	entry->hands_index = hands_index;
	entry->sprinkles = sprinkles;
	entry->honeycomb = honeycomb;
	definition = object_definition_get(object->definition_index);
	if (!cache_file_tag_cache_contains(definition, sizeof(*definition)))
		return NULL;
	entry->model_index = definition->object.model.index;
	shader_index = definition->object.modifier_shader.index;
	if (shader_index == NONE || entry->model_index == NONE || hands_index == NONE)
		return NULL;
	shader = shader_definition_get(shader_index);
	if (!cache_file_tag_cache_contains(shader, sizeof(*shader)) ||
		!shader_type_is_valid_for_modifier(shader->base.type))
		return NULL;
	plasma = shader->base.type == 10 ? (struct shield_look_plasma const *)shader : NULL;
	if (plasma && !cache_file_tag_cache_contains(plasma, sizeof(*plasma)))
		return NULL;
	noise_index = shield_look_noise();
	if (noise_index == NONE)
		return NULL;
	if (plasma)
		entry->body = entry->arms = *plasma;
	else
	{
		entry->body.shader = entry->arms.shader = *shader;
		entry->body.shader.base.type = entry->arms.shader.base.type = 10;
	}
	entry->authored_modifier = shader;
	intensity_source = plasma ? plasma->intensity_source : 2;
	entry->body.primary_map.index = entry->body.secondary_map.index = noise_index;
	entry->arms.primary_map.index = entry->arms.secondary_map.index = noise_index;
	shield_look_profile(&entry->body, FALSE);
	shield_look_profile(&entry->arms, TRUE);
	if (sprinkles)
	{
		shield_look_classic_profile(&entry->body);
		shield_look_classic_profile(&entry->arms);
	}
	/* The map still owns hit timing and the outgoing function channel.
	Overshield animation writes that same channel; only appearance changes. */
	entry->body.intensity_source = entry->arms.intensity_source = intensity_source;
	/* Use the established Arm Shields stand-off, capped by its existing
	distance/FOV calculation. An authored zero-offset shell must not remove it. */
	entry->arms.offset_source = intensity_source;
	entry->arms.offset_amount = 0.03f;
	entry->arms.offset_exponent = 2.f;
	/* A small normal displacement follows the visible hit intensity (B),
	not the unused stock C function. At full intensity this is 3.8 cm;
	the square root keeps a gentle outer layer while the hit fades. */
	entry->body.offset_source = intensity_source;
	entry->body.offset_amount = 0.0125f;
	entry->body.offset_exponent = 0.5f;
	entry->eligible = TRUE;
	return entry;
}

/* Compare colour data, not tag paths: protected maps can rename tags. */
static boolean shield_look_color_near(real_rgb_color const *color, real red, real green, real blue)
{
	return color->red > red - 0.00001f && color->red < red + 0.00001f &&
		color->green > green - 0.00001f && color->green < green + 0.00001f &&
		color->blue > blue - 0.00001f && color->blue < blue + 0.00001f;
}

static boolean shield_look_default_colors(struct shield_look_plasma const *p)
{
	if (p->color_source != 0)
		return FALSE;
	return (shield_look_color_near(&p->perpendicular_color, 1.f, 0.545098f, 0.243137f) &&
		shield_look_color_near(&p->parallel_color, 0.0666667f, 0.0666667f, 1.f)) ||
		(shield_look_color_near(&p->perpendicular_color, 0.65098f, 1.f, 1.f) &&
		shield_look_color_near(&p->parallel_color, 0.627451f, 0.215686f, 1.f)) ||
		(shield_look_color_near(&p->perpendicular_color, 0.909804f, 0.772549f, 0.f) &&
		shield_look_color_near(&p->parallel_color, 0.937255f, 0.694118f, 0.0627451f));
}

static real shield_look_color_peak(real_rgb_color const *color)
{
	real peak;
	/* Reject negative, nonfinite and out-of-range authored RGB values. */
	if (!(color->red >= 0.f && color->red <= 1.f && color->green >= 0.f &&
		color->green <= 1.f && color->blue >= 0.f && color->blue <= 1.f))
		return 0.f;
	peak = color->red > color->green ? color->red : color->green;
	return peak > color->blue ? peak : color->blue;
}

static void shield_look_color_apply(real_rgb_color const *color, real_rgb_color *out)
{
	real brightness = shield_look_color_peak(out);
	real peak = shield_look_color_peak(color);
	out->red = (color->red / peak) * brightness;
	out->green = (color->green / peak) * brightness;
	out->blue = (color->blue / peak) * brightness;
}

/* Draw-local colours preserve per-object channels without mutating cached
profiles or map tags. Only the two RGB constants change; a black authored
endpoint borrows the other hue rather than removing the preset's layer. */
boolean shield_look_colors(struct shader const *shader, real_rgb_color const *colors,
	real_rgb_color *perpendicular, real_rgb_color *parallel)
{
	struct shield_look_plasma const *authored;
	struct shield_look_entry const *entry;
	real_rgb_color a, b;
	real_rgb_color const *tint;
	short index;
	if (!shield_look_enabled())
		return FALSE;
	for (index = 0; index < shield_look_count; index++)
	{
		entry = &shield_looks[index];
		if (!entry->eligible || (shader != &entry->body.shader && shader != &entry->arms.shader) ||
			entry->authored_modifier->base.type != 10)
			continue;
		authored = (struct shield_look_plasma const *)entry->authored_modifier;
		if (shield_look_default_colors(authored))
			return FALSE;
		a = authored->perpendicular_color;
		b = authored->parallel_color;
		if (authored->color_source != 0)
		{
			if (!colors || authored->color_source < 1 || authored->color_source > 4)
				return FALSE;
			tint = &colors[authored->color_source - 1];
			if (shield_look_color_peak(tint) == 0.f)
				return FALSE;
			a.red *= tint->red; a.green *= tint->green; a.blue *= tint->blue;
			b.red *= tint->red; b.green *= tint->green; b.blue *= tint->blue;
		}
		if (shield_look_color_peak(&a) == 0.f)
			a = b;
		if (shield_look_color_peak(&b) == 0.f)
			b = a;
		if (shield_look_color_peak(&a) == 0.f)
			return FALSE;
		shield_look_color_apply(&a, perpendicular);
		shield_look_color_apply(&b, parallel);
		return TRUE;
	}
	return FALSE;
}

/* Identity comes from the retained draw profile, never a global menu value:
queued draws may outlive a setting change. */
real shield_look_honeycomb_scale(struct shader const *shader)
{
	short index;
	for (index = 0; index < shield_look_count; index++)
	{
		struct shield_look_entry const *entry = &shield_looks[index];
		if (entry->eligible && entry->honeycomb &&
			(shader == &entry->body.shader || shader == &entry->arms.shader))
			return shader == &entry->arms.shader ? 225.f : 87.5f;
	}
	return 0.f;
}

void shield_look_reset(void)
{
	shield_recharge_reset();
	memset(shield_looks, 0, sizeof(shield_looks));
	shield_look_count = 0;
	shield_look_noise_searched = FALSE;
	shield_look_noise_index = NONE;
}

struct shader *shield_look_body(long object_index, struct shader *original)
{
	struct game_globals *globals;
	struct game_globals_first_person_interface *first_person;
	struct shield_look_entry *entry;

	if (!shield_look_enabled())
		return original;
	globals = scenario_get_game_globals();
	if (!cache_file_tag_cache_contains(globals, sizeof(*globals)) || globals->first_person_interface.count != 1 ||
		!cache_file_tag_cache_contains(globals->first_person_interface.address, sizeof(*first_person)))
		return original;
	first_person = TAG_BLOCK_GET_ELEMENT(&globals->first_person_interface, 0, struct game_globals_first_person_interface);
	entry = shield_look_for_unit(object_index, first_person->hands.index);
	return entry ? &entry->body.shader : original;
}

boolean shield_look_arms(short local_player_index, long unit_index, long hands_model_index,
	struct render_model_effect *effect)
{
	struct shield_look_entry *entry;

	/* The caller first applies Arm Shields. OFF leaves no pass to restyle;
	ON retains its values pointer, including its offset cap and overshield.
	Register the model even when OFF so authored hand shells are suppressed. */
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return FALSE;
	entry = shield_look_for_unit(unit_index, hands_model_index);
	if (!entry || !effect->modifier_shader)
		return FALSE;
	effect->modifier_shader = &entry->arms.shader;
	return TRUE;
}

/* A map can duplicate its body/arm geometry for a plasma shell. When the
selected modifier already supplies that layer, omit the authored plasma
part before submitting either the base material or another modifier copy.
Other transparent materials (visors, lamps and decals) are not shields. */
boolean shield_look_skip_shell(struct model const *model, struct shader const *surface)
{
	short index;

	if (!surface || !model || !shield_look_enabled())
		return FALSE;
	for (index = 0; index < shield_look_count; index++)
	{
		if (shield_looks[index].eligible &&
			(surface->base.type == 10 ||
			 (surface->base.type > 4 && surface == shield_looks[index].authored_modifier)) &&
			(model == model_definition_get(shield_looks[index].model_index) ||
			model == model_definition_get(shield_looks[index].hands_index)))
			return TRUE;
	}
	return FALSE;
}

#include "shield_arcs.inl"

#include "shield_recharge.inl"
