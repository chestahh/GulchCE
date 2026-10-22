/* SHIELD_RECHARGE.INL: a draw-only envelope on the approved ENHANCED profile. */
#include "rasterizer/rasterizer.h"
#include "viewmodel_shield.h"

unsigned long config_changes(void);
int config_boolean(char const *name);

enum { SHIELD_RECHARGE_MAXIMUM = 128 };
struct shield_recharge
{
	boolean used, charging;
	boolean classic_surface[2];
	long object_index;
	real seen, retreat, progress, fade;
	struct shader const *shaders[2];
	real const *values[2];
};
static struct shield_recharge shield_recharges[SHIELD_RECHARGE_MAXIMUM];
static unsigned long shield_recharge_changes;

void shield_recharge_reset(void)
{
	memset(shield_recharges, 0, sizeof(shield_recharges));
	shield_recharge_changes = config_changes();
}

static real shield_recharge_ease(real value)
{
	value = PIN(value, 0.f, 1.f);
	return value * value * (3.f - 2.f * value);
}

/* The charging flag distinguishes regeneration from a shield held below full.
Stun/depletion and overcharge always win, even if a charging flag is stale. */
static boolean shield_recharge_allowed(struct object_datum const *object)
{
	return object && object->object.type == _object_type_biped &&
		!TEST_FLAG(object->object.damage_flags, _object_dead_bit) &&
		!TEST_FLAG(object->object.damage_flags, _object_shield_depleted_bit) &&
		!TEST_FLAG(object->object.damage_flags, _object_shield_over_charging_bit) &&
		object->object.shield_stun_ticks == 0 &&
		object->object.shield_vitality > 0.f && object->object.shield_vitality <= 1.f;
}

static void shield_recharge_update(struct shield_recharge *state,
	struct object_datum const *object, real now)
{
	real shield = object->object.shield_vitality;
	if (!shield_recharge_allowed(object))
	{
		state->used = FALSE;
		return;
	}
	if (shield < 1.f && TEST_FLAG(object->object.damage_flags, _object_shield_charging_bit))
	{
		state->charging = TRUE;
		state->progress = shield_recharge_ease(shield);
		state->fade = shield_recharge_ease(shield / 0.06f);
	}
	else if (shield >= 1.f && (state->charging || state->fade > 0.f))
	{
		if (state->charging)
		{
			state->retreat = now;
			state->charging = FALSE;
		}
		state->progress = 1.f;
		state->fade = 1.f - shield_recharge_ease((now - state->retreat) / 0.5f);
		if (state->fade <= 0.f)
			state->used = FALSE;
	}
	else
		state->used = FALSE;
	state->seen = now;
}

/* Register existing values pointers, preserving Arm Shields and Overshield
ownership. Transparent packets outlive this call, so no stack copies are used. */
void shield_recharge_prepare(long object_index, struct render_model_effect const *effect, boolean arms)
{
	struct object_datum *object;
	struct shield_recharge *state = NULL, *available = NULL;
	real now = global_frame_parameters.game_time_sec;
	short i, side = arms ? 1 : 0;
	boolean owned = FALSE, classic_surface = FALSE;
	if (shield_recharge_changes != config_changes())
		shield_recharge_reset();
	if (!shield_look_enabled() || !effect || !effect->modifier_animation.values ||
		(arms && !config_boolean("display.viewmodel_shield")))
		return;
	for (i = 0; i < shield_look_count; i++)
		if (effect->modifier_shader == (arms ? &shield_looks[i].arms.shader : &shield_looks[i].body.shader))
		{
			owned = TRUE;
			classic_surface = shield_looks[i].sprinkles;
		}
	if (!owned || !(now >= 0.f && now < 10000000.f))
		return;
	object = object_try_and_get(object_index);
	for (i = 0; i < SHIELD_RECHARGE_MAXIMUM; i++)
	{
		struct shield_recharge *each = &shield_recharges[i];
		if (each->used && (now < each->seen || now - each->seen > 1.f))
			each->used = FALSE;
		if (each->used && each->object_index == object_index)
			state = each;
		else if (each->used && each->values[side] == effect->modifier_animation.values)
			each->values[side] = NULL; /* respawn or reassigned split-screen arms */
		if (!each->used && !available)
			available = each;
	}
	if (!shield_recharge_allowed(object))
	{
		if (state) state->used = FALSE;
		return;
	}
	if (!state)
	{
		if (!available || object->object.shield_vitality >= 1.f ||
			!TEST_FLAG(object->object.damage_flags, _object_shield_charging_bit))
			return;
		state = available;
		memset(state, 0, sizeof(*state));
		state->used = TRUE;
		state->object_index = object_index;
	}
	shield_recharge_update(state, object, now);
	state->classic_surface[side] = classic_surface;
	state->shaders[side] = effect->modifier_shader;
	state->values[side] = effect->modifier_animation.values;
}

static struct shield_recharge const *shield_recharge_find(struct shader const *shader,
	real const *values, short *side)
{
	short i, j;
	if (!shader || !values || !shield_look_enabled() || shield_recharge_changes != config_changes())
		return NULL;
	for (i = 0; i < SHIELD_RECHARGE_MAXIMUM; i++)
	{
		struct shield_recharge const *state = &shield_recharges[i];
		if (!state->used || state->fade <= 0.f ||
			state->seen != global_frame_parameters.game_time_sec)
			continue;
		for (j = 0; j < 2; j++)
			if (state->shaders[j] == shader && state->values[j] == values &&
				(!j || config_boolean("display.viewmodel_shield")))
			{
				*side = j;
				return state;
			}
	}
	return NULL;
}

boolean shield_recharge_visible(struct shader const *shader, real const *values)
{
	short side;
	return shield_recharge_find(shader, values, &side) != NULL;
}

void shield_recharge_draw(struct shader const *shader, real const *values, real *intensity, real *offset)
{
	short side;
	struct shield_recharge const *state = shield_recharge_find(shader, values, &side);
	struct shield_look_plasma const *profile = (struct shield_look_plasma const *)shader;
	real maximum, visibility;
	if (!state)
		return;
	/* Arms grow within their existing cap. Body peak is only 15% above the
	approved offset; no profile, colour, texture or animation speed changes. */
	maximum = viewmodel_shield_offset_amount(values, profile->offset_amount * (side ? 1.f : 1.15f));
	*offset = maximum * (0.3f + 0.7f * state->progress) * state->fade;
	visibility = state->fade;
	/* The classic surface loses broad bands sooner as intensity falls.
	Keep them readable through the shared retreat without changing its
	duration, displacement, shader profile or the approved ENHANCED curve. */
	if (state->classic_surface[side])
		visibility *= 1.f + 0.75f * (1.f - visibility);
	*intensity = MAX(*intensity, (0.22f + 0.25f * state->progress) * visibility);
}
