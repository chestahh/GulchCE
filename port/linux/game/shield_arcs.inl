/* SHIELD_ARCS.INL: Bigass sprite animation on depleted enhanced shields.
Included by shield_look.c; no simulation particles or map tags are modified. */
#include "render/render_sprite.h"
#include "render/render.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "viewmodel_shield.h"
#include <stdio.h>

int config_boolean(const char *name);
void platform_log(const char *format, ...);
void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);

struct shield_arcs_tag
{
	long group, parents[2], index;
	char *name;
	void *address;
	unsigned long unused[2];
};

enum { SHIELD_ARCS_FRAMES = 31, SHIELD_ARCS_PIXELS = 172032, SHIELD_ARCS_BYTES = 345064 };
static struct shield_arcs_tag *shield_arcs_table, *shield_arcs_original;
static long shield_arcs_original_count, shield_arcs_index = NONE;
static boolean shield_arcs_attempted;
static byte *shield_arcs_asset;
static struct bitmap_group shield_arcs_bitmap_group;
static struct bitmap_group_sequence shield_arcs_sequence;
static struct bitmap_group_sprite shield_arcs_sprites[SHIELD_ARCS_FRAMES];
static struct bitmap_data shield_arcs_bitmaps[2];

/* Runs after texture-cache shutdown, before other appended tables unwind. */
void shield_arcs_unload(void)
{
	short i;
	if (shield_arcs_original)
		cache_files_set_tag_instances(shield_arcs_original, shield_arcs_original_count);
	for (i = 0; i < 2; i++)
		if (shield_arcs_bitmaps[i].hardware_format)
			rasterizer_bitmap_delete(&shield_arcs_bitmaps[i]);
	if (shield_arcs_table)
		free(shield_arcs_table);
	if (shield_arcs_asset)
		free(shield_arcs_asset);
	shield_arcs_table = shield_arcs_original = NULL;
	shield_arcs_asset = NULL;
	shield_arcs_index = NONE;
	shield_arcs_attempted = FALSE;
	memset(shield_arcs_bitmaps, 0, sizeof(shield_arcs_bitmaps));
}

static boolean shield_arcs_load(void)
{
	FILE *file;
	long count, i;
	struct shield_arcs_tag *original;
	struct bitmap_data *bitmap;
	struct bitmap_group_sprite *sprite;
	short j;
	if (shield_arcs_attempted)
		return shield_arcs_index != NONE;
	shield_arcs_attempted = TRUE;
	file = fopen("d:\\shield_assets\\bigass_arcs.bin", "rb");
	if (!file)
	{
		platform_log("shield arcs: install shield_assets/bigass_arcs.bin with tools/extract_shield_arcs.py");
		return FALSE;
	}
	shield_arcs_asset = malloc(SHIELD_ARCS_BYTES);
	if (!shield_arcs_asset || fread(shield_arcs_asset, 1, SHIELD_ARCS_BYTES, file) != SHIELD_ARCS_BYTES ||
		fgetc(file) != EOF || memcmp(shield_arcs_asset, "GCARCS1", 8))
	{
		fclose(file);
		goto failed;
	}
	fclose(file);
	memcpy(shield_arcs_sprites, shield_arcs_asset + 8, sizeof(shield_arcs_sprites));
	for (i = 0; i < SHIELD_ARCS_FRAMES; i++)
	{
		sprite = &shield_arcs_sprites[i];
		if (sprite->bitmap_index < 0 || sprite->bitmap_index > 1)
			goto failed;
		for (j = 0; j < 4; j++)
			if (!(sprite->bounds.n[j] >= 0.f && sprite->bounds.n[j] <= 1.f))
				goto failed;
		if (!(sprite->bounds.x0 < sprite->bounds.x1 && sprite->bounds.y0 < sprite->bounds.y1 &&
			sprite->registration_point.x >= 0.f && sprite->registration_point.x <= 1.f &&
			sprite->registration_point.y >= 0.f && sprite->registration_point.y <= 1.f))
			goto failed;
	}
	original = cache_files_tag_instances(&count);
	if (!original || count < 0 || count >= 32767)
		goto failed;
	shield_arcs_table = malloc((count + 1) * sizeof(*shield_arcs_table));
	if (!shield_arcs_table)
		goto failed;
	memcpy(shield_arcs_table, original, count * sizeof(*original));
	memset(&shield_arcs_table[count], 0, sizeof(*original));
	/* The new absolute slot cannot collide with an existing handle. */
	shield_arcs_index = (0x6000L << 16) | count;
	shield_arcs_table[count].group = 'bitm';
	shield_arcs_table[count].parents[0] = shield_arcs_table[count].parents[1] = NONE;
	shield_arcs_table[count].index = shield_arcs_index;
	shield_arcs_table[count].name = "gulch\\shield_arcs";
	shield_arcs_table[count].address = &shield_arcs_bitmap_group;
	memset(&shield_arcs_bitmap_group, 0, sizeof(shield_arcs_bitmap_group));
	memset(&shield_arcs_sequence, 0, sizeof(shield_arcs_sequence));
	shield_arcs_bitmap_group.type = 4;
	shield_arcs_bitmap_group.sequences.count = 1;
	shield_arcs_bitmap_group.sequences.address = &shield_arcs_sequence;
	shield_arcs_bitmap_group.bitmaps.count = 2;
	shield_arcs_bitmap_group.bitmaps.address = shield_arcs_bitmaps;
	shield_arcs_sequence.sprites.count = SHIELD_ARCS_FRAMES;
	shield_arcs_sequence.sprites.address = shield_arcs_sprites;
	for (i = 0; i < 2; i++)
	{
		bitmap = &shield_arcs_bitmaps[i];
		bitmap->signature = 'bitm';
		bitmap->width = bitmap->height = 512;
		bitmap->depth = 1;
		bitmap->format = 14;
		bitmap->flags = 3; /* power-of-two, compressed; privately owned, not cached */
		bitmap->mipmap_count = 2;
		bitmap->pixels_size = SHIELD_ARCS_PIXELS;
		bitmap->tag_index = shield_arcs_index;
		bitmap->cache_block_index = NONE;
		bitmap->base_address = shield_arcs_asset + 8 + sizeof(shield_arcs_sprites) + i * SHIELD_ARCS_PIXELS;
		if (!rasterizer_bitmap_new(bitmap))
			goto failed;
		rasterizer_bitmap_changed(bitmap);
	}
	shield_arcs_original = original;
	shield_arcs_original_count = count;
	cache_files_set_tag_instances(shield_arcs_table, count + 1);
	return TRUE;
failed:
	shield_arcs_unload();
	shield_arcs_attempted = TRUE;
	platform_log("shield arcs: invalid asset or unavailable texture; normal shields retained");
	return FALSE;
}

static boolean shield_arcs_active(struct object_datum const *object, boolean arms)
{
	return shield_look_enabled() && (!arms || config_boolean("display.viewmodel_shield")) &&
		object && object->object.type == _object_type_biped &&
		!TEST_FLAG(object->object.damage_flags, _object_dead_bit) &&
		TEST_FLAG(object->object.damage_flags, _object_shield_depleted_bit) &&
		object->object.shield_vitality <= 0.f && object->object.shield_stun_ticks > 0;
}

/* Integer hashing supplies visual variation without consuming game RNG. */
static uint32_t shield_arcs_random(uint32_t value)
{
	value ^= value >> 16;
	value *= UINT32_C(0x7FEB352D);
	value ^= value >> 15;
	value *= UINT32_C(0x846CA68B);
	return value ^ (value >> 16);
}

void shield_arcs_draw(long object_index, long model_index, real_matrix4x3 const *nodes,
	struct render_model_effect const *effect, boolean arms)
{
	struct object_datum *object = object_try_and_get(object_index);
	struct model *model;
	struct build_sprite_data sprites;
	real time, phase, life, age, fade, radius, offset;
	real_point3d position;
	real_vector3d direction;
	real_rgb_color parallel;
	real_argb_color color;
	uint32_t seed, cycle;
	short slot, node, count;
	if (!shield_arcs_active(object, arms) || !nodes || !effect || (arms && !effect->modifier_shader))
		return;
	model = model_definition_get(model_index);
	if (!cache_file_tag_cache_contains(model, sizeof(*model)) || model->nodes.count < 2 ||
		model->nodes.count > MAXIMUM_NODES_PER_MODEL || !shield_arcs_load())
		return;
	time = global_frame_parameters.game_time_sec;
	if (!(time >= 0.f && time < 10000000.f))
		return;
	color.alpha = 1.f;
	color.red = color.green = color.blue = 1.f;
	parallel = color.rgb;
	if (effect->modifier_shader)
		shield_look_colors(effect->modifier_shader, object->object.outgoing_change_colors, &color.rgb, &parallel);
	count = arms ? 2 : 6;
	build_sprites_begin(&sprites, count, shield_arcs_index, &global_shader_effect_additive,
		arms ? FLAG(_build_sprites_first_person_bit) : 0);
	for (slot = 0; slot < count; slot++)
	{
		phase = time + slot * 0.173f;
		cycle = (uint32_t)phase;
		age = phase - (real)cycle;
		seed = shield_arcs_random((uint32_t)object_index ^ (cycle * 31 + slot * 997));
		life = 0.5f + (seed & 255) * (0.5f / 255.f);
		if (age >= life)
			continue;
		node = 1 + ((seed >> 8) % (model->nodes.count - 1));
		position = nodes[node].position;
		vector_from_points3d(&position, &render.camera.position, &direction);
		normalize3d(&direction);
		offset = arms ? viewmodel_shield_offset_amount(effect->modifier_animation.values, 0.03f) : 0.025f;
		point_from_line3d(&position, &direction, offset, &position);
		radius = (arms ? 0.025f : 0.05f) * (1.f + ((seed >> 16) & 255) / 255.f);
		fade = age < 0.05f ? age / 0.05f : 1.f;
		if (life - age < 0.15f)
			fade *= (life - age) / 0.15f;
		/* Bigass's 31-frame sequence runs at 30 fps with radius 0.05-0.1. */
		build_sprite(&sprites, _build_sprite_normal, 0, ((short)(age * 30.f)) % SHIELD_ARCS_FRAMES,
			&position, &direction, 0.f, radius * 0.016f, &color, fade, 0);
	}
	build_sprites_end(&sprites);
}
