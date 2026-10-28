#include "harness.h"
#include <math.h>
#define __stdcall
#define TICKS_PER_SECOND 30
#define BOTS_SEARCHES_A_TICK 3
#define BOTS_PATHFINDING_WALKABLE_BIT 0
#define INFINITE 0xffffffffUL
#define WAIT_OBJECT_0 0
#define csmemset memset
#define csmemcpy memcpy
#define SET_FLAG(f,b,v) ((v)?((f)|=FLAG(b)):((f)&=~FLAG(b)))
#define BIT_VECTOR_TEST_FLAG(p,b) TEST_FLAG((p)[(b)/32],(b)%32)
#define platform_log(...) ((void)0)
struct collision_surface { byte flags, breakable_surface_index; };
struct collision_bsp { struct { long count; struct collision_surface *address; } surfaces; };
struct structure_bsp {
    struct { long count; byte *address; } pathfinding_surfaces;
    struct { long count; struct collision_bsp *address; } collision_bsp;
    struct { long count; } breakable_surfaces;
};
#define TAG_BLOCK_GET_ELEMENT(block,i,type) (&(block)->address[i])
struct path_state { struct structure_bsp *structure; struct { boolean ignore_broken_surfaces; } input; };
struct bots_bot { long unit_index; short goal_kind; real_point3d goal_point, last_position; };
struct bots_nav_route { short phase, link; };
static struct { struct bots_bot bots[63]; } bots_globals;
static struct { struct bots_nav_route routes[63]; } bots_navigation;
static int bots_worker_enabled = 1, handles;
static long now = 100, broken;
static long game_time_get(void) { return now; }
static void *breakable_surface_flags_get(void) { return &broken; }
static real bots_distance3d(real_point3d const *a, real_point3d const *b)
{ real x=a->x-b->x,y=a->y-b->y,z=a->z-b->z; return (real)sqrt(x*x+y*y+z*z); }
struct event { int signaled, manual; };
static void *CreateEventA(void *a, int manual, int signaled, void *b)
{ struct event *e=calloc(1,sizeof(*e)); (void)a;(void)b; e->manual=manual;e->signaled=signaled;handles++;return e; }
static int SetEvent(void *p) { ((struct event*)p)->signaled=1;return 1; }
static int ResetEvent(void *p) { ((struct event*)p)->signaled=0;return 1; }
static unsigned long WaitForSingleObject(void *p, unsigned long delay)
{ struct event *e=p;(void)delay;if (!e->signaled)return 258;if(!e->manual)e->signaled=0;return 0; }
static int CloseHandle(void *p) { CHECK(p!=NULL,"null handle");free(p);handles--;return 1; }
static void *CreateThread(void *a,long stack,unsigned long (*entry)(void*),void *arg,long flags,void *id)
{ (void)a;(void)stack;(void)entry;(void)arg;(void)flags;(void)id;return CreateEventA(NULL,1,1,NULL); }
static void bots_path_find_snapshot(struct path_state *state) { (void)state; }
static void checked_free(void *pointer) { CHECK(pointer!=NULL,"engine free received NULL");free(pointer); }
#define free checked_free
#include "under_test.inc"
int main(int argc, char **argv)
{
    struct bots_bot *bot=&bots_globals.bots[0];
    struct structure_bsp structure;
    struct collision_bsp bsp;
    struct collision_surface surfaces[2]={{0,0},{8,0}};
    byte walkable[2]={1,129};
    struct path_state state;
    real_point3d start={0,0,0},goal={20,0,0};
    struct bots_path_job *job;
    CHECK(argc==2,"case");
    memset(&structure,0,sizeof(structure));memset(&state,0,sizeof(state));
    structure.pathfinding_surfaces.address=walkable;structure.pathfinding_surfaces.count=2;
    structure.collision_bsp.address=&bsp;structure.collision_bsp.count=1;
    structure.breakable_surfaces.count=1;bsp.surfaces.address=surfaces;bsp.surfaces.count=2;
    state.structure=&structure;bot->unit_index=10;bot->goal_point=goal;
    if (!strcmp(argv[1],"disabled"))
    {
        bots_worker_enabled=0;
        CHECK(!bots_worker_submit(bot,&state,&start,&goal,1,FALSE,FALSE,.25f),"worker changed default");
        CHECK(!handles && !bots_worker.jobs,"disabled worker allocated");
        return 0;
    }
    CHECK(bots_worker_submit(bot,&state,&start,&goal,1,FALSE,FALSE,.25f),"submission failed");
    job=bots_worker_job(bot);CHECK(job && job->pending,"pending ownership lost");
    if (!strcmp(argv[1],"snapshot"))
    {
        CHECK(job->state.input.ignore_broken_surfaces,"worker still reads live breakage");
        CHECK(!(job->walkable[1]&1) && walkable[1]==129,"breakage snapshot changed live graph");
        walkable[0]=0;broken=1;
        CHECK(job->walkable[0]==1 && !(job->walkable[1]&1),"snapshot reads mutable state");
    }
    else if (!strcmp(argv[1],"stale"))
    {
        bot->unit_index=11;
        CHECK(bots_worker_job(bot)==job && job->pending,"in-flight answer overwritten on respawn");
        SetEvent(job->done);
        CHECK(!bots_worker_job(bot) && !job->pending,"respawn consumed stale answer");
    }
    else CHECK(!strcmp(argv[1],"cleanup"),"case");
    bots_worker_stop();bots_worker_stop();
    CHECK(!handles && !bots_worker.jobs && !bots_worker.thread,"worker cleanup leaked ownership");
    return 0;
}
