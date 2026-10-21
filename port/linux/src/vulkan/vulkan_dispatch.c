/* Native D3D8 dispatch for the presentation milestone. No virtual GPU. */
#include "platform.h"
#include "port_config.h"
#include "vulkan_dispatch.h"
#include "vulkan_window.h"
#include <stdio.h>
#include <string.h>

int halo_vulkan_dispatch_active;
static int created, failed;
static unsigned device_identity;

HRESULT halo_vulkan_create(D3DPRESENT_PARAMETERS *parameters, D3DDevice **result)
{
	if (result) *result = NULL;
	if (!result || !parameters || created || !halo_vulkan_dispatch_active)
		return E_FAIL;
	if (!halo_vulkan_window_open(parameters->BackBufferWidth, parameters->BackBufferHeight,
		config_boolean("debug.hidden_window"), config_boolean("debug.vulkan_validation"),
		config_string("debug.vulkan_capture")))
	{
		failed = 1;
		return E_FAIL;
	}
	created = 1;
	*result = (D3DDevice *)&device_identity;
	return S_OK;
}

void halo_vulkan_clear(DWORD count, CONST D3DRECT *rectangles, DWORD flags,
	D3DCOLOR color, float depth, DWORD stencil)
{
	float rgba[4];
	(void)depth; (void)stencil;
	/* This milestone has a color presentation target only. Reject other
	clear contracts explicitly until stage three implements game targets. */
	if (!created || count || rectangles || flags != D3DCLEAR_TARGET)
	{
		platform_log("Vulkan presentation: unsupported clear; game rendering is not implemented");
		failed = 1;
		return;
	}
	rgba[0] = (float)((color >> 16) & 255) / 255.0f;
	rgba[1] = (float)((color >> 8) & 255) / 255.0f;
	rgba[2] = (float)(color & 255) / 255.0f;
	rgba[3] = (float)((color >> 24) & 255) / 255.0f;
	halo_vulkan_window_clear(rgba);
}

void halo_vulkan_present(CONST RECT *source, CONST RECT *destination, void *unused, void *unused2)
{
	if (!created || source || destination || unused || unused2 || failed)
	{
		failed = 1;
		return;
	}
	if (halo_vulkan_window_present() < 0)
		failed = 1;
}

ULONG halo_vulkan_release(void)
{
	if (created)
	{
		failed |= !halo_vulkan_window_close();
		created = 0;
	}
	return 0;
}

/* -1 means ordinary game startup. This opt-in test exits before loading maps:
	unimplemented draws can never silently disappear behind a Vulkan label. */
int halo_vulkan_startup_test(void)
{
	D3DPRESENT_PARAMETERS parameters;
	D3DDevice *device;
	unsigned session, frames, pause_ticks, generations, pause_frame = 0;
	unsigned long long start;
	int resized, paused, restored;
	if (!config_boolean("debug.vulkan_presentation"))
		return -1;
	platform_log("Vulkan presentation test: native D3D8 create/clear/present/release; no Halo scene or OpenGL fallback");
	halo_vulkan_dispatch_active = 1;
	failed = 0;
	for (session = 0; session < 2 && !failed; session++)
	{
		memset(&parameters, 0, sizeof(parameters));
		parameters.BackBufferWidth = 640;
		parameters.BackBufferHeight = 480;
		if (Direct3D_CreateDevice(0, D3DDEVTYPE_HAL, NULL, 0, &parameters, &device) != S_OK || !device)
		{
			failed = 1;
			break;
		}
		start = halo_vulkan_window_ticks();
		resized = paused = restored = 0;
		pause_ticks = 0;
		while (!failed && (frames = halo_vulkan_window_submitted()) < 60)
		{
			if (!halo_vulkan_window_events() || halo_vulkan_window_ticks() - start > 15000)
			{
				failed = 1;
				break;
			}
			if (frames >= 15 && !resized)
			{
				halo_vulkan_window_resize(800, 450);
				resized = 1;
			}
			if (frames >= 30 && !paused)
			{
				halo_vulkan_window_suspend(1);
				paused = 1;
				pause_frame = frames;
			}
			if (paused && !restored && ++pause_ticks == 30)
			{
				if (halo_vulkan_window_submitted() != pause_frame) failed = 1;
				halo_vulkan_window_suspend(0);
				halo_vulkan_window_resize(640, 480);
				restored = 1;
			}
			D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET,
				frames < 20 ? 0xffff0000 : frames < 40 ? 0xff00ff00 : 0xff0000ff, 1.0f, 0);
			D3DDevice_Present(NULL, NULL, NULL, NULL);
			halo_vulkan_window_delay();
		}
		generations = halo_vulkan_window_generations();
		frames = halo_vulkan_window_submitted();
		failed |= frames != 60 || generations < 3 || !restored;
		D3DDevice_Release();
		platform_log("Vulkan presentation session=%u frames=%u generations=%u result=%s",
			session, frames, generations, failed ? "FAIL" : "PASS");
	}
	halo_vulkan_release();
	halo_vulkan_dispatch_active = 0;
	return failed ? 1 : 0;
}
