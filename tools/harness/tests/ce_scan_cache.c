#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>
#include <stdarg.h>
#ifdef _WIN32
#define csstrcasecmp _stricmp
#define _strnicmp _strnicmp
#else
#include <strings.h>
#define csstrcasecmp strcasecmp
#define _strnicmp strncasecmp
#endif
typedef int boolean;
typedef unsigned char byte;
typedef void *HANDLE;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NUMBEROF(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAXIMUM_FILENAME_LENGTH 255
#define MAXIMUM_MAP_NAME_LENGTH 63
#define MAXIMUM_DESCRIPTION_LENGTH 255
#define MAXIMUM_DESCRIPTION_FILE_BYTES 512
#define CUSTOM_EDITION_MAPS_MAXIMUM 512
#define MAXIMUM_CUSTOM_EDITION_CAMPAIGNS 512
#define MAXIMUM_XBOX_LEVELS 20
#define CUSTOM_EDITION_MAP_DIRECTORY "d:\\custom_maps\\"
#define CUSTOM_EDITION_INSTALL_MAP_DIRECTORY "h:\\maps\\"
#define CUSTOM_EDITION_LEVEL_NAME_PREFIX "custom_maps\\"
#define DESCRIPTION_EXTENSION ".txt"
#define FILE_ATTRIBUTE_DIRECTORY 16
#define INVALID_HANDLE_VALUE ((void *)-1)
#define GetFileExInfoStandard 0
#define _scenario_type_solo 0
#define _scenario_type_multiplayer 1
#define csmemcpy memcpy
#define csmemset memset
#define csstrlen strlen
#define csstrcpy strcpy
#define csstrncpy strncpy
#define csprintf sprintf
#define _error_silent 0
static void error(int priority,char const *format,...){(void)priority;(void)format;}
typedef struct {uint32_t dwHighDateTime,dwLowDateTime;} FILETIME;
typedef struct {uint32_t dwFileAttributes,nFileSizeHigh,nFileSizeLow;FILETIME ftLastWriteTime;char cFileName[260];} WIN32_FIND_DATAA;
typedef WIN32_FIND_DATAA WIN32_FILE_ATTRIBUTE_DATA;
static int entry_count=300,index_now,listings,opens,descriptions,installed,enabled=1;
static int realloc_fail,desc_fail,invalid_first,changed_index=-1,has_description;
static unsigned generation;
static size_t allocation_size;
static char *allocation;
static void *moving_realloc(void *old,size_t bytes){
    char *next;
    if(realloc_fail)return NULL;
    next=malloc(bytes);assert(next);
    if(old){memcpy(next,old,allocation_size);memset(old,0xdd,allocation_size);free(old);}
    allocation=next;allocation_size=bytes;return next;
}
static void *description_alloc(size_t bytes){return desc_fail ? NULL : malloc(bytes);}
static void strict_free(void *p){assert(p);free(p);}
#define realloc moving_realloc
#define malloc description_alloc
#define free strict_free
static void fill_entry(WIN32_FIND_DATAA *d){
    memset(d,0,sizeof(*d));
    if(has_description && index_now==entry_count)sprintf(d->cFileName,"map000.txt");
    else sprintf(d->cFileName,"map%03d.map",index_now);
    d->nFileSizeLow=100+(index_now==changed_index);d->ftLastWriteTime.dwLowDateTime=1;
}
static HANDLE FindFirstFileA(char const *p,WIN32_FIND_DATAA *d){
    listings++;index_now=0;(void)p;
    if(!entry_count)return INVALID_HANDLE_VALUE;
    fill_entry(d);return (void *)1;
}
static int FindNextFileA(HANDLE h,WIN32_FIND_DATAA *d){(void)h;if(++index_now>=entry_count+has_description)return 0;fill_entry(d);return 1;}
static void CloseHandle(HANDLE h){(void)h;}
static int GetFileAttributesExA(char const *p,int kind,WIN32_FILE_ATTRIBUTE_DATA *d){(void)p;(void)kind;(void)d;return 1;}
static short custom_edition_cache_file_scenario_type(char const *p){opens++;return invalid_first && strstr(p,"map000") ? NONE : _scenario_type_multiplayer;}
static FILE *description_open(char const *p,char const *mode){(void)p;(void)mode;descriptions++;return NULL;}
#define fopen description_open
static int map_share_name_hashed(char const *s){(void)s;return 0;}
static short map_share_name_base_length(char const *s){return (short)strlen(s);}
static void map_share_map_found(char const *p,char const *n){(void)p;(void)n;}
static void bitmap_delete(void *p){(void)p;}
static int halo_custom_edition_tag_cache(void){return enabled;}
static int custom_edition_install_present(void){return installed;}
static unsigned long config_changes(void){return generation;}
static unsigned long GetTickCount(void){return 0;}
static void display_name_make(char const *n,wchar_t *out){while(*n)*out++=(unsigned char)*n++;*out=0;}
#include "under_test.inc"
int main(void){
    int first,old;
    custom_edition_maps_look_for();
    assert(custom_edition_maps_globals.map_count==300 && opens==300 && descriptions==0 && listings==1);
    first=opens;custom_edition_maps_look_for();
    assert(opens==first && listings==2 && descriptions==0);
    printf("CE 300 maps: cold %d header opens, warm %d; absent description opens %d\n",first,opens-first,descriptions);
    changed_index=17;custom_edition_maps_look_for();assert(opens==first+1);
    entry_count=301;custom_edition_maps_look_for();assert(opens==first+2);
    entry_count=299;custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==299);
    has_description=1;custom_edition_maps_look_for();assert(descriptions==1);
    has_description=0;old=descriptions;custom_edition_maps_look_for();assert(descriptions==old);
    generation++;invalid_first=1;custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==298);
    invalid_first=0;custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==299);
    installed=1;invalid_first=1;generation++;custom_edition_maps_look_for();
    assert(custom_edition_maps_globals.map_count==298);installed=0;invalid_first=0;
    realloc_fail=1;custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==299);
    realloc_fail=0;desc_fail=1;has_description=1;old=descriptions;
    custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==299 && descriptions>old);
    enabled=0;custom_edition_maps_look_for();assert(custom_edition_maps_globals.map_count==0);
    (void)allocation;
    return 0;
}
