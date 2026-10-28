/* Optional vehicle choices and ordinary player controls, owned by the host. */
#ifndef HALO_BOTS_VEHICLES_H
#define HALO_BOTS_VEHICLES_H
struct player_action;
void bots_vehicles_reset(void);
void bots_vehicles_reset_player(long index);
long bots_vehicle_parent(long unit_index);
long bots_vehicle_navigation(long unit_index, real *radius);
long bots_vehicle_weapon(long unit_index, long weapon_index);
boolean bots_vehicle_goal(long index, long unit_index, real_point3d const *destination,
	real_point3d *goal, long *surface);
void bots_vehicle_action(long index, long unit_index, real_point3d const *goal,
	boolean objective, boolean moving, real direction, struct player_action *action);
#endif
