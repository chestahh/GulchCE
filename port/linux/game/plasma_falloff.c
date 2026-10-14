/* Feature-owned attenuation for the existing additive world-light pass. */
#include "cseries.h"
#include <xtl.h>
#include "game/game_globals.h"
#include "rasterizer/common/rasterizer_common.h"
#include "rasterizer/xbox/rasterizer_xbox.h"
#include "rasterizer/rasterizer_swizzle.h"
#include "plasma_lighting.h"
#include <stdlib.h>

enum { PLASMA_FALLOFF_SIZE = 64 };
static IDirect3DVolumeTexture8 *plasma_falloff_texture;
static boolean plasma_falloff_bound;

/* Zero value and zero slope at the radius, without a hard luminous rim. */
static real plasma_falloff_value(real distance_squared)
{
	real remaining = MAX(0.0f, 1.0f - distance_squared);
	return remaining * remaining;
}

static boolean plasma_falloff_create(void)
{
	D3DLOCKED_BOX locked;
	unsigned long *pixels;
	int x, y, z;
	boolean success = FALSE;

	pixels = malloc(PLASMA_FALLOFF_SIZE * PLASMA_FALLOFF_SIZE * PLASMA_FALLOFF_SIZE * sizeof(*pixels));
	if (!pixels)
		return FALSE;
	for (z = 0; z < PLASMA_FALLOFF_SIZE; z++)
		for (y = 0; y < PLASMA_FALLOFF_SIZE; y++)
			for (x = 0; x < PLASMA_FALLOFF_SIZE; x++)
			{
				real dx = 2.0f * (x + 0.5f) / PLASMA_FALLOFF_SIZE - 1.0f;
				real dy = 2.0f * (y + 0.5f) / PLASMA_FALLOFF_SIZE - 1.0f;
				real dz = 2.0f * (z + 0.5f) / PLASMA_FALLOFF_SIZE - 1.0f;
				unsigned long value = (unsigned long)(255.0f * plasma_falloff_value(dx*dx + dy*dy + dz*dz) + 0.5f);
				pixels[(z * PLASMA_FALLOFF_SIZE + y) * PLASMA_FALLOFF_SIZE + x] = value * 0x01010101UL;
			}
	if (D3DDevice_CreateVolumeTexture(PLASMA_FALLOFF_SIZE, PLASMA_FALLOFF_SIZE, PLASMA_FALLOFF_SIZE,
		1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &plasma_falloff_texture) >= 0)
	{
		if (IDirect3DVolumeTexture8_LockBox(plasma_falloff_texture, 0, &locked, NULL, 0) >= 0 && locked.pBits)
		{
			rasterizer_xbox_bitmap_swizzle3d_long(locked.pBits, pixels,
				PLASMA_FALLOFF_SIZE, PLASMA_FALLOFF_SIZE, PLASMA_FALLOFF_SIZE);
			success = IDirect3DVolumeTexture8_UnlockBox(plasma_falloff_texture, 0) >= 0;
		}
	}
	free(pixels);
	if (!success)
		plasma_falloff_dispose();
	return success;
}

void plasma_falloff_begin(long rasterizer_index)
{
	if (!plasma_lighting_selected(rasterizer_index))
		return;
	if (!plasma_falloff_texture && !plasma_falloff_create())
		return; /* allocation failure leaves the stock pass intact */
	D3DDevice_SetTexture(2, (D3DBaseTexture *)plasma_falloff_texture);
	D3DDevice_SetTextureState_Deferred(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	plasma_falloff_bound = TRUE;
}

void plasma_falloff_end(void)
{
	if (!plasma_falloff_bound)
		return;
	rasterizer_set_texture_direct(2, global_rasterizer_data->distance_attenuation.index, 0);
	D3DDevice_SetTextureState_Deferred(2, D3DTSS_MINFILTER, D3DTEXF_POINT);
	plasma_falloff_bound = FALSE;
}

void plasma_falloff_dispose(void)
{
	plasma_falloff_end();
	if (plasma_falloff_texture)
		D3DResource_Release((D3DResource *)plasma_falloff_texture);
	plasma_falloff_texture = NULL;
}
