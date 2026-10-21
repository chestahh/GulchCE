#include "vulkan_window.h"
#include "vulkan_present.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <string.h>

static struct halo_vk_device device;
static struct halo_vk_present presentation;
static SDL_Window *window;
static float clear_color[4];
static int video_owned, suspended;

static int create_surface(VkInstance instance, void *context, VkSurfaceKHR *surface)
{
	return SDL_Vulkan_CreateSurface(context, instance, NULL, surface);
}

int halo_vulkan_window_close(void)
{
	int ok = halo_vk_present_dispose(&presentation);
	/* A failed wait retains ownership rather than destroying live objects. */
	if (presentation.device)
		return 0;
	halo_vk_device_dispose(&device);
	ok &= device.validation_errors == 0;
	if (window) SDL_DestroyWindow(window);
	window = NULL;
	if (video_owned) SDL_QuitSubSystem(SDL_INIT_VIDEO);
	video_owned = 0;
	suspended = 0;
	return ok;
}

int halo_vulkan_window_open(unsigned width, unsigned height, int hidden, int validation, const char *capture_path)
{
	const char *const *extensions;
	Uint32 count;
	if (window || !width || !height || width > 16384 || height > 16384)
		return 0;
	memset(&presentation, 0, sizeof(presentation));
	memset(clear_color, 0, sizeof(clear_color));
	clear_color[3] = 1.0f;
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
		goto failed;
	video_owned = 1;
	window = SDL_CreateWindow("GulchCE - Vulkan presentation test (no scene rendering)",
		(int)width, (int)height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_HIGH_PIXEL_DENSITY | (hidden ? SDL_WINDOW_HIDDEN : 0));
	if (!window)
		goto failed;
	extensions = SDL_Vulkan_GetInstanceExtensions(&count);
	if (!extensions || !halo_vk_device_initialize_surface(&device, validation,
		extensions, count, create_surface, window))
		goto failed;
	if (halo_vk_present_initialize(&presentation, &device) < 0)
		goto failed;
	presentation.verify_pixels = 1;
	presentation.capture_path = capture_path;
	return 1;
failed:
	fprintf(stderr, "vulkan: window initialization failed: %s\n", SDL_GetError());
	halo_vulkan_window_close();
	return 0;
}

void halo_vulkan_window_clear(const float color[4])
{
	memcpy(clear_color, color, sizeof(clear_color));
}

int halo_vulkan_window_present(void)
{
	int width = 0, height = 0;
	if (!window)
		return -1;
	if (!suspended && !(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED))
		SDL_GetWindowSizeInPixels(window, &width, &height);
	return halo_vk_present_color(&presentation, width > 0 ? (unsigned)width : 0,
		height > 0 ? (unsigned)height : 0, clear_color);
}

unsigned halo_vulkan_window_submitted(void) { return presentation.submitted; }
unsigned halo_vulkan_window_generations(void) { return presentation.generations; }

int halo_vulkan_window_events(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			return 0;
	return 1;
}

void halo_vulkan_window_resize(unsigned width, unsigned height)
{
	SDL_SetWindowSize(window, (int)width, (int)height);
}

void halo_vulkan_window_suspend(int suspend)
{
	suspended = suspend;
	if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN))
	{
		if (suspend) SDL_MinimizeWindow(window);
		else SDL_RestoreWindow(window);
	}
}

unsigned long long halo_vulkan_window_ticks(void) { return SDL_GetTicks(); }
void halo_vulkan_window_delay(void) { SDL_Delay(1); }
