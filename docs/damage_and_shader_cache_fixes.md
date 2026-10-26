# Projectile owner lifetime and shader cache follow-up

This follows the [collision optimisation investigation](dynamic_lighting_tracing.md).
The scope is two existing-engine defects: a stale damage-owner lookup, then
redundant compilation of byte-identical fragment shader sources. Lighting
settings/defaults, visual equations, host policy and Xbox/CE/MCC loading paths
must remain unchanged.

## Projectile-damage crash

Both original failures assert that an object datum is "unused or changed":
`0x8107004a` in the disabled control and `0x8e8b0048` in the enabled control.
The subsequent null dereference is in `object_get_and_verify_type`, reached
from `object_cause_damage`'s gametype multiplier lookup. In
`get_player_index_from_object_or_parents`, a failed `unit_try_and_get` was
followed by a strict `object_get` on the same handle.

The engine permits a projectile to outlive its owner. Its deleted-object
handler clears a homing target but does not retire the owner handle. The
damage structure also carries player/team ownership separately. Therefore the
fix checks the object before following its parent and returns `NONE` if it no
longer exists, rather than erasing ownership or suppressing damage. Datum
generation checking prevents a reused slot from being treated as the old
owner. Valid units and parent chains retain their original results.

This is a general object-lifetime defect reproduced on an Xbox multiplayer
map with lighting off and on, not an adaptation to MCC data. Only this lookup
in `source/objects/damage.c` changes; it carries a `port:` comment. Host damage
admission still precedes calculation. No wire format, prediction rule or
valid-input damage calculation changes, so the pinned OpenCE and MCC network
versions remain unchanged. A departed player's separate damage attribution
still follows the existing validation and network rules.

The combined owner-lookup, stale-player-damage and MCC-damage harness run passed
52 cases, including negative controls that restore the strict lookup and fail
on deleted/reused objects and missing parents. The normal Windows release
build passed. Runtime and platform checks are recorded below when complete.
