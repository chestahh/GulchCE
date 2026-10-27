#include "harness.h"

struct object_datum { struct { long parent_object_index; } object; };
static struct object_datum objects[4];
static long identifiers[4], players[4];
static boolean units[4];

static struct object_datum *object_try_and_get(long index)
{
	unsigned long slot = (unsigned long)index & 0xffff;
	return index != NONE && slot < 4 && identifiers[slot] == index ? &objects[slot] : NULL;
}
static struct object_datum *object_get(long index)
{
	struct object_datum *object = object_try_and_get(index);
	CHECK(object != NULL, "strict lookup dereferenced a retired object");
	return object;
}
static struct object_datum *unit_try_and_get(long index)
{
	struct object_datum *object = object_try_and_get(index);
	return object && units[index & 0xffff] ? object : NULL;
}
static long player_index_from_unit_index(long index)
{
	CHECK(unit_try_and_get(index) != NULL, "player lookup on missing unit");
	return players[index & 0xffff];
}
#include "under_test.inc"

int main(int argc, char **argv)
{
	long index, expected, result;
	int i;
	CHECK(argc == 2, "case required");
	for (i = 0; i < 4; i++)
	{
		identifiers[i] = 0x81070000L | i;
		players[i] = NONE;
		objects[i].object.parent_object_index = NONE;
	}
	units[0] = TRUE; players[0] = 17;
	index = identifiers[0]; expected = 17;
	if (!strcmp(argv[1], "none")) { index = NONE; expected = NONE; }
	if (!strcmp(argv[1], "ai")) { players[0] = NONE; expected = NONE; }
	if (!strcmp(argv[1], "parent") || !strcmp(argv[1], "chain") || !strcmp(argv[1], "deleted-parent"))
	{
		index = identifiers[1]; objects[1].object.parent_object_index = identifiers[0];
	}
	if (!strcmp(argv[1], "chain"))
	{
		index = identifiers[2]; objects[2].object.parent_object_index = identifiers[1];
	}
	if (!strcmp(argv[1], "unparented")) { index = identifiers[1]; expected = NONE; }
	if (!strcmp(argv[1], "deleted") || !strcmp(argv[1], "deleted-parent"))
	{
		identifiers[0] = NONE; expected = NONE;
	}
	if (!strcmp(argv[1], "reused"))
	{
		identifiers[0] += 0x10000; players[0] = 99; expected = NONE;
	}
	result = get_player_index_from_object_or_parents(index);
	CHECK(result == expected, "wrong player or replacement object credited");
	return 0;
}
