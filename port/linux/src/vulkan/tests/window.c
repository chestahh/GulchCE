/* Supplemental host-ABI WSI validation; the game tests the actual 32-bit D3D8 dispatch. */
#include "../vulkan_window.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
	unsigned session, frames, attempt, generations;
	unsigned long long start;
	int validation = 0, hidden = 0, ok = 1, resized, suspended, restored;
	float color[4] = { 1, 0, 0, 1 };
	for (attempt = 1; attempt < (unsigned)argc; attempt++)
	{
		if (!strcmp(argv[attempt], "--validation")) validation = 1;
		else if (!strcmp(argv[attempt], "--hidden")) hidden = 1;
		else return 2;
	}
	for (session = 0; session < 2 && ok; session++)
	{
		if (!halo_vulkan_window_open(640, 480, hidden, validation, NULL)) return 1;
		start = halo_vulkan_window_ticks();
		resized = suspended = restored = 0;
		attempt = 0;
		while ((frames = halo_vulkan_window_submitted()) < 60 && ok)
		{
			if (!halo_vulkan_window_events() || halo_vulkan_window_ticks() - start > 15000) { ok = 0; break; }
			if (frames >= 15 && !resized) { halo_vulkan_window_resize(800, 450); resized = 1; }
			if (frames >= 30 && !suspended) { halo_vulkan_window_suspend(1); suspended = 1; }
			if (suspended && !restored && ++attempt == 30) { halo_vulkan_window_suspend(0); halo_vulkan_window_resize(640, 480); restored = 1; }
			halo_vulkan_window_clear(color);
			if (halo_vulkan_window_present() < 0) ok = 0;
			halo_vulkan_window_delay();
		}
		generations = halo_vulkan_window_generations();
		ok &= frames == 60 && generations >= 3 && restored;
		ok &= halo_vulkan_window_close();
		fprintf(stderr, "vulkan window session=%u frames=%u generations=%u %s\n", session, frames, generations, ok ? "PASS" : "FAIL");
	}
	return ok ? 0 : 1;
}
