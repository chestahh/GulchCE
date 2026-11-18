#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef int boolean;
typedef unsigned short word;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define MAXIMUM_FILENAME_LENGTH 255
#define MAX_GAMENAME 128
#define NUMBER_OF_SUPPORTED_MEMORY_UNITS 1
#define NUMBER_OF_MEMORY_UNITS 1
#define MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT 32
#define _memory_unit_hard_drive 0
#define _permission_read_bit 0
#define _permission_write_bit 1
#define FLAG(n) (1u<<(n))
#define match_assert(file,line,test) assert(test)
#define error(...) ((void)0)
struct file_reference {unsigned long position;};
static struct {struct file_reference memory_unit_mapfile;int memory_units_dirty,enumeration_in_progress,initialized;short next_enumerated_profile_index;void *general_mutex,*mapfile_mutex;} saved_game_files_globals;
static char *memory_unit_mapfile_path[]={"z:\\saved\\hdmu.map"};
static unsigned char disk[65536];
static unsigned long disk_size;
static int opens,reads,closed,allocation_failed,read_failed,close_failed,write_failed;
static void *allocate(size_t n){return allocation_failed ? NULL : malloc(n);}
#define malloc allocate
static int saved_game_files_take_mapfile_mutex(void){return 1;}
static void saved_game_files_release_mapfile_mutex(void){}
static int file_reference_create_from_path(struct file_reference *f,char const *p,int d){(void)p;(void)d;f->position=0;return 1;}
static int file_open(struct file_reference *f,unsigned flags){(void)f;(void)flags;opens++;return 1;}
static int file_close(struct file_reference *f){(void)f;closed++;return !close_failed;}
static unsigned long file_get_eof(struct file_reference *f){(void)f;return disk_size;}
static int file_set_position(struct file_reference *f,unsigned long n){f->position=n;return 1;}
static int file_read(struct file_reference *f,unsigned long n,void *out){
    reads++;if(read_failed || f->position+n>disk_size)return 0;
    memcpy(out,disk+f->position,n);f->position+=n;return 1;
}
static int file_write(struct file_reference *f,unsigned long n,void const *in){
    if(write_failed || f->position+n>sizeof(disk))return 0;
    memcpy(disk+f->position,in,n);f->position+=n;if(f->position>disk_size)disk_size=f->position;return 1;
}
static int file_get_size(struct file_reference *f,unsigned long *n){(void)f;*n=disk_size;return 1;}
static int file_read_from_position(struct file_reference *f,unsigned long p,unsigned long n,void *out){file_set_position(f,p);return file_read(f,n,out);}
static int file_write_to_position(struct file_reference *f,unsigned long p,unsigned long n,void const *in){file_set_position(f,p);return file_write(f,n,in);}
static int file_set_eof(struct file_reference *f,unsigned long n){(void)f;disk_size=n;return 1;}
static int file_create(struct file_reference *f){(void)f;disk_size=0;return 1;}
static void dispose_mutex(void *m){(void)m;}
static void player_profiles_dispose(void){}
static void playlist_profiles_dispose(void){}
#include "under_test.inc"
int main(void){
    struct enumerated_saved_game_file input={0},out;long index;int i,old;
    strcpy(input.path,"first");assert(append_entry_to_mapfile(0,&input,&index) && index==0);
    opens=reads=closed=0;
    for(i=0;i<300;i++){assert(get_nth_entry_in_mapfile(0,0,&out));assert(!strcmp(out.path,"first"));}
    assert(opens==1 && reads==1 && closed==1);
    printf("Saved index 300 lookups: %d opens and %d reads (baseline: 300 each)\n",opens,reads);
    strcpy(input.path,"renamed");assert(set_nth_entry_in_mapfile(0,0,&input));
    assert(get_nth_entry_in_mapfile(0,0,&out) && !strcmp(out.path,"renamed"));
    strcpy(input.path,"second");assert(append_entry_to_mapfile(0,&input,&index));
    assert(get_nth_entry_in_mapfile(0,1,&out) && !strcmp(out.path,"second"));
    assert(remove_nth_entry_in_mapfile(0,0));
    assert(get_nth_entry_in_mapfile(0,0,&out) && !strcmp(out.path,"second"));
    assert(!get_nth_entry_in_mapfile(0,1,&out));
    old=reads;saved_game_files_notify_memory_units_changed();
    assert(get_nth_entry_in_mapfile(0,0,&out) && reads==old+1);saved_game_files_globals.memory_units_dirty=0;
    assert(enumerate_saved_game_files_start(0));assert(!mapfile_copy_read(0));
    assert(enumerate_saved_game_file(&input));assert(enumerate_saved_game_files_end(0));
    assert(get_nth_entry_in_mapfile(0,0,&out) && !strcmp(out.path,"second"));
    write_failed=1;assert(!set_nth_entry_in_mapfile(0,0,&input));assert(!mapfile_copy.valid);write_failed=0;
    assert(get_nth_entry_in_mapfile(0,0,&out));
    mapfile_copy_forget();read_failed=1;assert(!get_nth_entry_in_mapfile(0,0,&out));assert(!mapfile_copy.valid);read_failed=0;
    assert(get_nth_entry_in_mapfile(0,0,&out));
    mapfile_copy_forget();close_failed=1;assert(!mapfile_copy_read(0));assert(!mapfile_copy.valid);close_failed=0;
    assert(mapfile_copy_read(0));
    free(mapfile_copy.entries);mapfile_copy.entries=NULL;mapfile_copy_forget();allocation_failed=1;
    assert(get_nth_entry_in_mapfile(0,0,&out) && !strcmp(out.path,"second"));assert(!mapfile_copy.valid);
    allocation_failed=0;assert(mapfile_copy_read(0));
    disk_size++;mapfile_copy_forget();assert(!mapfile_copy_read(0));disk_size--;
    assert(!mapfile_copy_read(1));
    assert(mapfile_copy_read(0));old=reads;memory_unit_mapfile_path[0]="z:\\other\\hdmu.map";
    assert(mapfile_copy_read(0) && reads==old+1);
    saved_game_files_dispose();assert(!mapfile_copy.valid && !mapfile_copy.entries);
    assert(mapfile_copy_read(0));saved_game_files_dispose();
    return 0;
}
