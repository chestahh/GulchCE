/* Exercise sparse shader backups with two tags in the same hash bucket. */
#include "harness.h"

#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define MAXIMUM_SHADER_SIZE 8
#include "records.inc"

struct shader_group { long group_tag; const char *extension; };
static const struct shader_group shader_groups[] = { { 1, "shader_model" } };
static const long tags[] = { 0x10001, 0x10041 };
static byte definitions[] = { 3, 4 };
static int files[] = { 0, 1 };
static unsigned long version = 1;
static unsigned long retained, budget;
static int invalid, map_alive = 1, relinks;

struct tag_iterator { int next; };
#define tag_iterator_new(it, group) ((it)->next = 0)
#define tag_iterator_next(it) ((it)->next < 2 ? tags[(it)->next++] : NONE)
#define mods_tag_file_find(tag, extension) files[(tag) == tags[1]]
#define mods_map_loaded() map_alive
#define models_fix_transparent_part_links() (++relinks)
#define mods_say() FALSE
static void console_printf(boolean clear, const char *format, ...) { (void)clear; (void)format; }
#define console_warning(...) ((void)0)
#define error(...) ((void)0)

static byte *tag_get(long group, long tag)
{
	CHECK(map_alive && group == 1, "access after map unload");
	return &definitions[tag == tags[1]];
}
#define tag_get_group_tag(tag) 1
static const char *port_mods_file_path(long file) { return file ? "second" : "first"; }
static boolean port_mods_file_stamp(long file, unsigned long *size, unsigned long stamp[2])
{
	*size = 1;
	stamp[0] = version;
	stamp[1] = 0;
	return TRUE;
}

static boolean mods_memory_take(unsigned long size)
{
	if (size > budget - retained) return FALSE;
	retained += size;
	return TRUE;
}
static void mods_memory_give(unsigned long size)
{
	CHECK(size <= retained, "invalid refund");
	retained -= size;
}

static struct mod_shader *mod_shader_new(const struct shader_group *group, long tag, long file,
	const byte *original, const char **reason)
{
	struct mod_shader *made;
	if (invalid) { *reason = "invalid edit"; return NULL; }
	CHECK(*original == (tag == tags[1] ? 4 : 3), "reload used modified definition");
	made = calloc(1, sizeof(*made));
	CHECK(made, "allocation");
	made->tag_index = tag;
	made->size = 1;
	made->definition[0] = (byte)(*original + version);
	strcpy(made->path, port_mods_file_path(file));
	port_mods_file_stamp(file, &made->file_size, made->stamp);
	return made;
}
#include "under_test.inc"

int main(void)
{
	budget = sizeof(struct mod_shader);
	CHECK(!mod_shaders_update(FALSE, TRUE) && !retained && !mod_shaders_globals.shaders,
		"failed backup retained shader or spent budget");
	budget = 100000;
	CHECK(mod_shaders_update(FALSE, TRUE) == 2 && definitions[0] == 4 && definitions[1] == 5,
		"colliding tags failed to install");
	CHECK(mod_shaders_update(FALSE, FALSE) == 2 && !relinks, "unchanged files reloaded");
	version++;
	invalid = 1;
	CHECK(mod_shaders_update(FALSE, FALSE) == 2 && definitions[0] == 4 && definitions[1] == 5 && !relinks,
		"invalid reload displaced active shader");
	invalid = 0;
	files[0] = NONE;
	CHECK(mod_shaders_update(FALSE, FALSE) == 1 && definitions[0] == 3 && definitions[1] == 6 && relinks == 1,
		"removing one collision lost the other");
	mod_shaders_tags_unloaded(TRUE);
	CHECK(!retained && definitions[1] == 4, "normal unload lost original or budget");
	CHECK(mod_shaders_update(FALSE, TRUE) == 1, "second map failed to install");
	map_alive = 0;
	mod_shaders_tags_unloaded(FALSE);
	CHECK(!retained && !mod_shaders_globals.override_count, "failed map retained state");
	return 0;
}
