/* Reuse the fake world; all queue/reconstruction functions are extracted
 * unchanged from the production implementation by the Python generator. */
#define main baseline_main
#include "stress_world.inc"
#undef main

static int queue_case(char const *name)
{
    struct distributed_inventory inventory;
    long unit, weapon;
    int i, original_time;
    setup();
    unit = add_replica(3, _object_type_biped);
    weapon = DATUM_INDEX_NEW(4, 0x1234);
    inventory = inventory_for(unit, weapon);
    now = 100;
    if (!strcmp(name, "capacity")) {
        for (i = 0; i < MAXIMUM_CLIENT_RETRIED_INVENTORIES; i++) {
            inventory.unit_index = DATUM_INDEX_NEW(i, 0x1234);
            now++;
            distributed_client_retry_inventory(&inventory);
        }
        CHECK(objects_client_retried_inventory_count == MAXIMUM_CLIENT_RETRIED_INVENTORIES, "production capacity reached");
        inventory.unit_index = DATUM_INDEX_NEW(1, 0x1234);
        now++;
        distributed_client_retry_inventory(&inventory);
        CHECK(objects_client_retried_inventory_count == MAXIMUM_CLIENT_RETRIED_INVENTORIES, "same unit replaces in place");
        inventory.unit_index = DATUM_INDEX_NEW(MAXIMUM_CLIENT_RETRIED_INVENTORIES, 0x1234);
        now++;
        distributed_client_retry_inventory(&inventory);
        CHECK(objects_client_retried_inventory_count == MAXIMUM_CLIENT_RETRIED_INVENTORIES, "overflow bounded");
        CHECK(objects_client_retried_inventories[0].inventory.unit_index == inventory.unit_index, "oldest replaced on overflow");
        return 0;
    }
    distributed_client_apply_inventory(&inventory);
    CHECK(objects_client_retried_inventory_count == 1, "missing weapon queued");
    original_time = now;
    if (!strcmp(name, "refresh")) {
        now += EMPTY_INVENTORY_TICKS - 1;
        inventory.rounds_total[0][0] = 31;
        distributed_client_apply_inventory(&inventory);
        now = original_time + EMPTY_INVENTORY_TICKS;
        distributed_client_apply_retried_inventories();
        CHECK(objects_client_retried_inventory_count == 1, "fresh update extends lifetime");
        CHECK(add_replica(4, _object_type_weapon) == weapon, "weapon arrives");
        distributed_client_apply_retried_inventories();
        CHECK(objects_client_retried_inventory_count == 0, "latest inventory consumed");
        CHECK(object_get(weapon)->weapon.magazines[0].rounds_total == 31, "latest payload applied");
        return 0;
    }
    now += EMPTY_INVENTORY_TICKS - 1;
    distributed_client_apply_retried_inventories();
    CHECK(objects_client_retried_inventory_count == 1, "retained before expiry");
    now++;
    distributed_client_apply_retried_inventories();
    CHECK(objects_client_retried_inventory_count == 0, "expires at exact boundary");
    CHECK(add_replica(4, _object_type_weapon) == weapon, "late weapon arrives");
    distributed_client_apply_retried_inventories();
    CHECK(object_get(unit)->unit.weapon_object_indices[0] == NONE, "expired inventory not resurrected");
    distributed_client_apply_inventory(&inventory);
    CHECK(object_get(unit)->unit.weapon_object_indices[0] == weapon, "fresh host refresh repairs inventory");
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    CHECK(argc == 2, "case");
    result = queue_case(argv[1]);
    free(object_header_data);
    free(player_data);
    return result;
}
