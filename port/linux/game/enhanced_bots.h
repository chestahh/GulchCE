/* Optional destinations and tactics; combat and path ownership stay in bots.c. */
#ifndef HALO_ENHANCED_BOTS_H
#define HALO_ENHANCED_BOTS_H
struct unit_datum;
void enhanced_bots_initialize_for_new_map(void);
void enhanced_bots_dispose_from_old_map(void);
void enhanced_bots_update(long count, real_point3d const *points);
void enhanced_bots_reset(long index, long player_index);
boolean enhanced_bots_goal(long index, long player_index, real_point3d const *position,
	boolean urgent_only, real_point3d *goal, long *surface);
boolean enhanced_bots_advancing(long index, long unit_index);
boolean enhanced_bots_holding_hill(long unit_index);
real enhanced_bots_powerup_score(struct unit_datum *unit, long definition_index, real distance);
#endif
