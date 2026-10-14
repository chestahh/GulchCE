#include "harness.h"
#include <stdlib.h>
#include <string.h>
#define MAX(a,b) ((a) > (b) ? (a) : (b))
typedef struct { int unused; } IDirect3DVolumeTexture8;
typedef IDirect3DVolumeTexture8 D3DBaseTexture;
typedef IDirect3DVolumeTexture8 D3DResource;
typedef struct { void *pBits; } D3DLOCKED_BOX;
enum { D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DTSS_MINFILTER, D3DTEXF_LINEAR, D3DTEXF_POINT };
static struct { struct { int index; } distance_attenuation; } globals, *global_rasterizer_data = &globals;
static IDirect3DVolumeTexture8 texture;
static unsigned long texels[64*64*64];
static int selected, fail_create, creates, releases, binds, restores, filter;
static int plasma_lighting_selected(long index) { return selected; }
static int D3DDevice_CreateVolumeTexture(int w,int h,int d,int levels,int usage,int format,int pool,IDirect3DVolumeTexture8 **out)
{ creates++; if (fail_create) return -1; *out = &texture; return 0; }
static int IDirect3DVolumeTexture8_LockBox(IDirect3DVolumeTexture8 *t,int l,D3DLOCKED_BOX *box,void *b,int f)
{ box->pBits = texels; return 0; }
static int IDirect3DVolumeTexture8_UnlockBox(IDirect3DVolumeTexture8 *t,int l) { return 0; }
static void rasterizer_xbox_bitmap_swizzle3d_long(void *d,void *s,int w,int h,int depth) { memcpy(d,s,w*h*depth*sizeof(long)); }
static void D3DDevice_SetTexture(int stage,D3DBaseTexture *t) { CHECK(stage == 2, "only attenuation stage"); binds++; }
static void D3DDevice_SetTextureState_Deferred(int stage,int state,int value) { filter = value; }
static void rasterizer_set_texture_direct(int stage,int bitmap,int mip) { restores++; }
static void D3DResource_Release(D3DResource *t) { releases++; }
void plasma_falloff_dispose(void);
#include "under_test.inc"
int main(int argc, char **argv)
{
	int i;
	real previous = 1.0f;
	if (!strcmp(argv[1], "curve"))
	{
		CHECK(plasma_falloff_value(0) == 1 && plasma_falloff_value(1) == 0 && plasma_falloff_value(2) == 0, "bounded endpoints");
		for (i=0; i<=100; i++)
		{
			real value = plasma_falloff_value(i/100.0f);
			CHECK(value >= 0 && value <= previous, "monotonic nonnegative falloff");
			previous = value;
		}
		CHECK(plasma_falloff_value(0.999f)/0.001f < 0.01f, "slope approaches zero at boundary");
	}
	else if (!strcmp(argv[1], "scope"))
	{
		plasma_falloff_begin(0); plasma_falloff_end();
		CHECK(!creates && !binds && !restores, "stock pass untouched");
		selected=1; plasma_falloff_begin(0);
		CHECK(creates == 1 && binds == 1 && filter == D3DTEXF_LINEAR, "selected pass uses smooth texture");
		CHECK(texels[0] == 0 && texels[64*64*64-1] == 0 && texels[(32*64+32)*64+32] > 0xF0000000UL, "dark corners and luminous center");
		selected=0; plasma_falloff_end();
		CHECK(restores == 1 && filter == D3DTEXF_POINT, "restore even after disabling");
		plasma_falloff_end(); plasma_falloff_dispose(); plasma_falloff_dispose();
		CHECK(restores == 1 && releases == 1, "idempotent end and cleanup");
	}
	else
	{
		selected=1; fail_create=1; plasma_falloff_begin(0); plasma_falloff_end();
		CHECK(!binds && !restores, "allocation failure retains stock rendering");
	}
	return 0;
}
