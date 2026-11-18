#include "harness.h"
#include <wchar.h>
#define MAXIMUM_GAMETYPES 8
#define ROW_TEXT_LENGTH 64
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define PIN(x,a,b) MAX(a,MIN(x,b))
#define _game_mode_infection 1
#define _game_mode_gun_game 2
struct game_variant {long game_engine_index;struct {int teams;long score_to_win;} universal_variant;};
struct game_variant_options {int mode,time_limit;};
static int reads,option_reads,name_reads,fail_read,mode,minutes=7;
static wchar_t const *engine_names[]={L"Slayer",L"CTF",L"Oddball",L"King",L"Race",L"Other"};
static int playlist_profile_get(long p,struct game_variant *v) {
 CHECK(p!=NONE,"NONE must not advance the game's playlist");reads++;
 memset(v,0,sizeof(*v));v->universal_variant.score_to_win=25;return !fail_read;
}
static int playlist_profile_get_options(long p,struct game_variant_options *o) {
 (void)p;option_reads++;o->mode=mode;o->time_limit=minutes;return 1;
}
static void game_variant_options_default(struct game_variant const *v,struct game_variant_options *o) {(void)v;memset(o,0,sizeof(*o));}
static wchar_t const *game_modes_variant_kind_name(struct game_variant const *v,struct game_variant_options const *o) {(void)v;return o->mode==1?L"Infection":L"Slayer";}
static int game_modes_variant_mode(struct game_variant const *v,struct game_variant_options const *o) {(void)v;return o->mode;}
static int playlist_profile_get_display_name(long p,wchar_t *out) {(void)p;name_reads++;wcscpy(out,L"Saved name");return !fail_read;}
#define MAX_GAMENAME 128
#define ustrncpy wcsncpy
/* The production formatter uses Windows-style %s for wide strings. Record
 * its selected rule and numeric values without substituting the cache code. */
static wchar_t const *last_format;
static void usnprintf(wchar_t *text,int size,wchar_t const *format,...) {(void)size;last_format=format;wcscpy(text,L"formatted");}
#include "under_test.inc"
int main(int argc,char **argv) {
 wchar_t text[128];CHECK(argc==2,"case");char const *case_name=argv[1];
 CASE("names") {
  for(int i=0;i<30;i++)gametype_display_name(3,text);
  CHECK(name_reads==1&&!wcscmp(text,L"Saved name"),"name read once");
  gametype_shown_forget();gametype_display_name(3,text);CHECK(name_reads==2,"save/delete/rebuild invalidation");return 0;
 }
 CASE("none") {
  CHECK(!gametype_shown_get(NONE)&&!reads,"no profile is inert");
  CHECK(gametype_shown_get(1),"valid profile");gametype_shown_forget();
  CHECK(!gametype_shown_get(NONE),"forget clears validity");return 0;
 }
 CASE("failure") {
  fail_read=1;CHECK(!gametype_shown_get(2),"read failed");CHECK(!gametype_shown_get(2)&&reads==1,"miss cached");
  fail_read=0;gametype_shown_forget();CHECK(gametype_shown_get(2)&&reads==2,"retry after rebuild");return 0;
 }
 mode=1;
 for(int i=0;i<30;i++)gametype_rules_text(3,text,128);
 CHECK(reads==1&&option_reads==1,"variant and options read once");
 CHECK(wcsstr(last_format,L"Round time"),"Gulch infection rules retained");
 mode=2;gametype_shown_forget();gametype_rules_text(3,text,128);
 CHECK(reads==2&&option_reads==2&&wcsstr(last_format,L"each weapon"),"edited Gun Game options reloaded");
 CHECK(gametype_shown_get(4)&&reads==3,"selection change reloads");return 0;
}
