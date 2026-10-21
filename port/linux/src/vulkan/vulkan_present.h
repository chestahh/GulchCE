/* Window presentation owns synchronization separately from visibility queries. */
#ifndef HALO_VULKAN_PRESENT_H
#define HALO_VULKAN_PRESENT_H
#include "vulkan_device.h"

struct halo_vk_present_frame
{
	VkSemaphore acquired, rendered;
	VkFence acquire_done, present_done;
	int acquiring, rendering, presenting;
	VkBuffer readback;
	VkDeviceMemory memory;
	unsigned char *pixels;
	unsigned char expected[4];
	int capture_pending;
};

struct halo_vk_present
{
	struct halo_vk_device *device;
	VkSwapchainKHR swapchain;
	VkImage *images;
	uint32_t image_count;
	VkExtent2D extent;
	unsigned width, height, slot, generations, submitted;
	int recreate, failed, lost;
	int verify_pixels, bgra, capture_written;
	unsigned verified;
	const char *capture_path;
	struct halo_vk_present_frame frames[HALO_VK_FRAMES];
};

int halo_vk_present_initialize(struct halo_vk_present *p, struct halo_vk_device *d);
/* 1 submitted, 0 deferred (GPU busy, minimized or unavailable), -1 failed.
No CPU wait in this path. Resize retires both render and presentation work. */
int halo_vk_present_color(struct halo_vk_present *p, unsigned width, unsigned height,
	const float color[4]);
/* Waits only during teardown; safe after partial initialization or device loss. */
int halo_vk_present_dispose(struct halo_vk_present *p);
#endif
