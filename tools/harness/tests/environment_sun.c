#include "harness.h"
#include <string.h>
#define REAL_MAX 3.4e38f
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define VALID_INDEX(i,n) ((i)>=0 && (i)<(n))
typedef struct { real_vector3d n; float d; } real_plane3d;
struct block { int count; void *address; };
struct structure_leaf { int cluster_index, first_surface_reference_index, surface_reference_count; };
struct structure_cluster { short sky_index; };
static struct { struct block leaves, surfaces, surface_references, clusters; } bsp, *cached_bsp=&bsp;
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (((t *)(b)->address)+(i))
static struct { struct { real_plane3d world_planes[6]; } frustum; } render;
static unsigned char *sun_surface_sky;
static real dot_product3d(const real_vector3d *a,const real_vector3d *b)
{ return a->i*b->i+a->j*b->j+a->k*b->k; }
static real plane3d_distance_to_point(const real_plane3d *p,const real_point3d *v)
{ return p->n.i*v->x+p->n.j*v->y+p->n.k*v->z-p->d; }
#include "under_test.inc"
int main(void)
{
    real_point3d center={3,0,0};
    real_vector3d toward={1,0,0};
    long refs[][2]={{0,0},{1,0},{1,0},{2,0}};
    struct structure_leaf leaves[]={{0,0,2},{1,2,2}};
    struct structure_cluster skies[]={{0},{-1}};
    render.frustum.world_planes[0]=(real_plane3d){{1,0,0},1};
    render.frustum.world_planes[1]=(real_plane3d){{-1,0,0},1};
    render.frustum.world_planes[2]=(real_plane3d){{0,1,0},1};
    CHECK(sun_reaches_view(&center,.25f,&toward),"off-camera caster reaches visible ground");
    toward.i=-1;
    CHECK(!sun_reaches_view(&center,.25f,&toward),"caster projects farther outside");
    center.x=0;center.y=1.2f;
    CHECK(sun_reaches_view(&center,.25f,&toward),"sphere straddling the edge survives");
    center.y=1.3f;
    CHECK(!sun_reaches_view(&center,.25f,&toward),"parallel ray misses the view");
    bsp.surfaces.count=4;
    bsp.surface_references=(struct block){4,refs};
    bsp.leaves=(struct block){2,leaves};
    bsp.clusters=(struct block){2,skies};
    CHECK(sun_classify_surfaces(),"BSP ownership available");
    CHECK(sun_surface_sky[0]==0 && sun_surface_sky[1]==255 &&
        sun_surface_sky[2]==255 && sun_surface_sky[3]==254,
        "only a positively identified single-sky triangle may receive sunlight");
    free(sun_surface_sky);
    return 0;
}
