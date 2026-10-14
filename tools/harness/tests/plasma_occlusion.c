#include "harness.h"
#include <math.h>
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
typedef struct
{
	unsigned long pixels[4096];
} IDirect3DVolumeTexture8;
typedef IDirect3DVolumeTexture8 D3DResource;
typedef struct
{
	void *pBits;
} D3DLOCKED_BOX;
enum
{
	D3DFMT_A8R8G8B8,
	D3DPOOL_MANAGED,
	_collision_test_structure_bit,
	_collision_test_front_facing_surfaces_bit,
	_collision_test_back_facing_surfaces_bit,
	_collision_test_objects_bit,
	_collision_test_objects_scenery_bit,
	_collision_test_objects_machines_bit
};
struct collision_result
{
	real t;
};
struct rasterizer_light_submit_parameters
{
	real_point3d position;
	real radius;
};
static struct
{
	long light_count;
	struct rasterizer_light_submit_parameters lights[32];
} rasterizer_lights;
static int on = 1, queries, allocations, blocked = 1, door, fail_upload;
static unsigned long time_ms = 1000;
int config_boolean(const char *name)
{
	return on;
}
static unsigned long GetTickCount(void)
{
	return time_ms;
}
float plasma_occlusion_specular_scale(long index)
{
	return 1;
}
static int collision_test_vector(unsigned long flags, const real_point3d *p, const real_vector3d *v,
								 long ignore, struct collision_result *hit)
{
	float t, y;
	queries++;
	if (!blocked || v->i == 0)
		return 0;
	t = (0.3f - p->x) / v->i;
	y = p->y + t * v->j;
	if (t < 0 || t >= 1 || (door && fabsf(y) < 0.3f))
		return 0;
	hit->t = t;
	return 1;
}
static int D3DDevice_CreateVolumeTexture(int w, int h, int d, int levels, int usage, int format, int pool,
										 IDirect3DVolumeTexture8 **t)
{
	if (fail_upload)
		return -1;
	*t = calloc(1, sizeof(**t));
	allocations++;
	return 0;
}
static int IDirect3DVolumeTexture8_LockBox(IDirect3DVolumeTexture8 *t, int l, D3DLOCKED_BOX *box, void *b,
										   int f)
{
	box->pBits = t->pixels;
	return 0;
}
static int IDirect3DVolumeTexture8_UnlockBox(IDirect3DVolumeTexture8 *t, int l)
{
	return 0;
}
static void D3DResource_Release(D3DResource *p)
{
	free(p);
	allocations--;
}
static void rasterizer_xbox_bitmap_swizzle3d_long(void *d, void *s, int w, int h, int depth)
{
	memcpy(d, s, 4096 * sizeof(long));
}
void plasma_occlusion_window(void);
#include "under_test.inc"
int main(int argc, char **argv)
{
	struct occlusion_field *f;
	real previous;
	int n, cap, step;
	real temporal[3];
	float transform[4];
	rasterizer_lights.light_count = 1;
	rasterizer_lights.lights[0].radius = 1;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	CHECK(f, "admitted light");
	CHECK(sample_field(f->visibility, 0.9f, 0.5f, 0.5f) < 0.01f, "wall blocks clearly hidden receiver");
	CHECK(sample_field(f->visibility, 0.4f, 0.5f, 0.5f) > 0.99f, "near side stays lit");
	n = queries;
	time_ms++;
	CHECK(field_for(0) == f && queries == n, "one bounded update per window, not per model draw");
	n = queries;
	time_ms += 4;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	CHECK(queries == n, "collision depths are briefly reused while render projection updates");
	rasterizer_lights.lights[0].position.x = 0.2f;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	CHECK(queries == n + 1536, "fast movement invalidates cached depths immediately");
	rasterizer_lights.lights[0].position.x = 0;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	blocked = 0;
	previous = f->visibility[4095];
	time_ms += 16;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	CHECK(f->visibility[4095] > previous && f->visibility[4095] < 0.5f,
		  "clearing a corner fades rather than popping");
	time_ms += 200;
	plasma_occlusion_window();
	plasma_occlusion_submit(1, 0);
	f = field_for(0);
	CHECK(f->visibility[4095] > 0.9f, "clear view converges");
	on = 0;
	n = queries;
	CHECK(!field_for(0) && n == queries, "disabled makes no queries");
	plasma_occlusion_reset();
	CHECK(!allocations && !submitted_count, "map cleanup releases fields");
	on = 1;
	blocked = 1;
	door = 1;
	plasma_occlusion_window();
	plasma_occlusion_submit(2, 0);
	f = field_for(0);
	CHECK(sample_field(f->visibility, 0.9f, 0.5f, 0.5f) > 0.99f, "door opening admits light");
	CHECK(sample_field(f->visibility, 0.9f, 0.95f, 0.5f) < 0.35f,
		  "filtered doorway edge remains predominantly blocked");
	CHECK(sample_field(f->visibility, 0.9f, 0.99f, 0.5f) < 0.01f,
		  "receiver beyond filtered edge is fully blocked");
	door = 0;
	time_ms += 16;
	plasma_occlusion_window();
	plasma_occlusion_submit(2, 0);
	f = field_for(0);
	CHECK(sample_field(f->visibility, 0.9f, 0.5f, 0.5f) < 0.3f,
		  "closing door blocks within bounded refresh and fade");
	time_ms += 64;
	plasma_occlusion_window();
	plasma_occlusion_submit(2, 0);
	f = field_for(0);
	CHECK(sample_field(f->visibility, 0.9f, 0.9f, 0.5f) < 0.01f,
		  "thin blocker remains closed across cube face seam");
	door = 1;
	time_ms += 16;
	plasma_occlusion_window();
	plasma_occlusion_submit(2, 0);
	f = field_for(0);
	CHECK(sample_field(f->visibility, 0.9f, 0.5f, 0.5f) > 0.3f &&
			  sample_field(f->visibility, 0.9f, 0.5f, 0.5f) < 0.8f,
		  "opening door clears gradually");
	plasma_occlusion_reset();
	door = 0;
	rasterizer_lights.light_count = 9;
	for (n = 0; n < 9; n++)
	{
		rasterizer_lights.lights[n].radius = 1;
		plasma_occlusion_submit(10 + n, n);
	}
	queries = 0;
	for (n = 0; n < 8; n++)
		CHECK(field_for(n), "eight fields admitted");
	CHECK(!field_for(8) && queries == 8 * 1536, "ninth light falls back without evicting active fields");
	CHECK(allocations == 24, "bounded texture allocation");
	time_ms += 16;
	plasma_occlusion_window();
	for (n = 0; n < 9; n++)
		plasma_occlusion_submit(10 + n, n);
	queries = 0;
	CHECK(!field_for(8), "overflow cannot replace an active field");
	for (n = 0; n < 8; n++)
		CHECK(field_for(n), "rejected overflow does not consume an admitted light's update budget");
	CHECK(queries == 8 * 1536, "only admitted fields spend the query budget");
	time_ms += 120;
	plasma_occlusion_window();
	plasma_occlusion_submit(18, 8);
	f = field_for(8);
	CHECK(f && f->fading, "previous overflow is admitted with a fade");
	CHECK(f->texture[0]->pixels[4095] == 0xffffffffUL, "admission starts at previous unoccluded appearance");
	time_ms += 40;
	plasma_occlusion_window();
	plasma_occlusion_submit(18, 8);
	f = field_for(8);
	CHECK(f->texture[0]->pixels[4095] != 0xffffffffUL && f->texture[0]->pixels[4095] != 0,
		  "admission blends instead of snapping");
	plasma_occlusion_submit(10, 0);
	plasma_occlusion_model_slot(0, 0);
	CHECK(plasma_occlusion_model_texture(0, transform), "model uses selected field");
	CHECK(!plasma_occlusion_model_texture(-1, transform), "invalid model slot rejected");
	plasma_occlusion_window();
	CHECK(!plasma_occlusion_model_texture(0, transform), "view transition clears model slots");
	plasma_occlusion_reset();
	CHECK(!allocations, "all fields released");
	fail_upload = 1;
	plasma_occlusion_submit(30, 0);
	CHECK(!field_for(0), "allocation failure falls back");
	plasma_occlusion_reset();
	fail_upload = 0;
	time_ms = 1000;
	for (cap = 0; cap < 3; cap++)
	{
		blocked = 1;
		door = 0;
		plasma_occlusion_submit(40 + cap, 0);
		f = field_for(0);
		blocked = 0;
		for (step = 1; step <= (cap == 0 ? 3 : cap == 1 ? 6 : 12); step++)
		{
			unsigned long next = 1000 + (unsigned long)(100.0f * step / (cap == 0 ? 3 : cap == 1 ? 6 : 12));
			if (step == 1)
				time_ms = 1000;
			time_ms = next;
			plasma_occlusion_window();
			plasma_occlusion_submit(40 + cap, 0);
			f = field_for(0);
		}
		temporal[cap] = f->visibility[4095];
		plasma_occlusion_reset();
		time_ms = 1000;
	}
	CHECK(fabsf(temporal[0] - temporal[1]) < 0.025f && fabsf(temporal[1] - temporal[2]) < 0.025f,
		  "30/60/120 response agrees within the bounded collision refresh interval");
	blocked = 0;
	plasma_occlusion_submit(99, 0);
	f = field_for(0);
	time_ms += 4;
	rasterizer_lights.lights[0].position.x = 0.1f;
	plasma_occlusion_window();
	plasma_occlusion_submit(99, 0);
	f = field_for(0);
	CHECK(f->visibility[4095] > 0.99f, "a clear cached ray is not a wall at the old field boundary");
	plasma_occlusion_reset();
	return 0;
}
