/* Exercise production presentation ownership with delayed GPU/WSI completions. */
#include "../vulkan_present.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
#define HANDLE(type, n) ((type)(uintptr_t)(n))
#define INDEX(h) ((unsigned)(uintptr_t)(h))
static int signaled[256];
static unsigned next_handle, acquire_calls, resets, submits, presents, destroys, waits, creates;
static unsigned fail_create, create_calls;
static VkResult acquire_result, present_result, submit_result, fence_result;
static uint32_t acquire_index;
static int invalid;

static VKAPI_ATTR VkResult VKAPI_CALL fence_status(VkDevice d, VkFence f)
{
	(void)d;
	return fence_result != VK_SUCCESS ? fence_result : signaled[INDEX(f)] ? VK_SUCCESS : VK_NOT_READY;
}
static VKAPI_ATTR VkResult VKAPI_CALL wait_fences(VkDevice d, uint32_t n, const VkFence *f, VkBool32 all, uint64_t timeout)
{
	uint32_t i;
	(void)d; (void)all; (void)timeout;
	waits++;
	for (i = 0; i < n; i++) signaled[INDEX(f[i])] = 1;
	return fence_result;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_fences(VkDevice d, uint32_t n, const VkFence *f)
{
	uint32_t i; (void)d;
	resets++;
	for (i = 0; i < n; i++) signaled[INDEX(f[i])] = 0;
	return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL make_semaphore(VkDevice d, const VkSemaphoreCreateInfo *info, const VkAllocationCallbacks *a, VkSemaphore *out)
{
	(void)d; (void)info; (void)a;
	if (++create_calls == fail_create) return VK_ERROR_OUT_OF_HOST_MEMORY;
	*out = HANDLE(VkSemaphore, next_handle++); return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL make_fence(VkDevice d, const VkFenceCreateInfo *info, const VkAllocationCallbacks *a, VkFence *out)
{
	(void)d; (void)info; (void)a;
	if (++create_calls == fail_create) return VK_ERROR_OUT_OF_HOST_MEMORY;
	*out = HANDLE(VkFence, next_handle++); return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_semaphore(VkDevice d, VkSemaphore s, const VkAllocationCallbacks *a) { (void)d; (void)s; (void)a; destroys++; }
static VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice d, VkFence f, const VkAllocationCallbacks *a) { (void)d; (void)f; (void)a; destroys++; }
static VKAPI_ATTR VkResult VKAPI_CALL capabilities(VkPhysicalDevice d, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *out)
{
	(void)d; (void)surface;
	memset(out, 0, sizeof(*out)); out->minImageCount = 2; out->maxImageCount = 3;
	out->currentExtent.width = out->currentExtent.height = UINT32_MAX;
	out->minImageExtent.width = out->minImageExtent.height = 1;
	out->maxImageExtent.width = out->maxImageExtent.height = 4096;
	out->supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	out->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL formats(VkPhysicalDevice d, VkSurfaceKHR s, uint32_t *n, VkSurfaceFormatKHR *out)
{
	(void)d; (void)s; *n = 1;
	if (out) { out->format = VK_FORMAT_B8G8R8A8_UNORM; out->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; }
	return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL make_swapchain(VkDevice d, const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *a, VkSwapchainKHR *out)
{
	(void)d; (void)a;
	if (info->minImageCount != 3 || info->presentMode != VK_PRESENT_MODE_FIFO_KHR) invalid = 1;
	*out = HANDLE(VkSwapchainKHR, 200); creates++; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_swapchain(VkDevice d, VkSwapchainKHR s, const VkAllocationCallbacks *a) { (void)d; (void)s; (void)a; destroys++; }
static VKAPI_ATTR VkResult VKAPI_CALL images(VkDevice d, VkSwapchainKHR s, uint32_t *n, VkImage *out)
{
	(void)d; (void)s; *n = 3;
	if (out) { out[0] = HANDLE(VkImage, 201); out[1] = HANDLE(VkImage, 202); out[2] = HANDLE(VkImage, 203); }
	return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL acquire(VkDevice d, VkSwapchainKHR s, uint64_t timeout, VkSemaphore sem, VkFence f, uint32_t *image)
{
	(void)d; (void)s; (void)sem; (void)f;
	if (timeout != 0) invalid = 1;
	acquire_calls++; *image = acquire_index; return acquire_result;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_command(VkCommandBuffer c, VkCommandBufferResetFlags flags) { (void)c; (void)flags; return VK_SUCCESS; }
static VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer c, const VkCommandBufferBeginInfo *info) { (void)c; (void)info; return VK_SUCCESS; }
static VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer c) { (void)c; return VK_SUCCESS; }
static VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer c, VkPipelineStageFlags src, VkPipelineStageFlags dst, VkDependencyFlags flags,
	uint32_t m, const VkMemoryBarrier *mb, uint32_t b, const VkBufferMemoryBarrier *bb, uint32_t i, const VkImageMemoryBarrier *ib)
{
	(void)c; (void)src; (void)dst; (void)flags; (void)m; (void)mb; (void)b; (void)bb;
	if (i != 1 || ib->image != HANDLE(VkImage, 201 + acquire_index)) invalid = 1;
}
static VKAPI_ATTR void VKAPI_CALL clear(VkCommandBuffer c, VkImage image, VkImageLayout layout, const VkClearColorValue *color, uint32_t n, const VkImageSubresourceRange *range)
{
	(void)c; (void)image; (void)range;
	if (layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || n != 1 || color->float32[0] != 1.0f) invalid = 1;
}
static VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue q, uint32_t n, const VkSubmitInfo *info, VkFence f)
{
	(void)q; (void)f; submits++;
	if (n != 1 || info->waitSemaphoreCount != 1 || info->signalSemaphoreCount != 1 ||
		*info->pWaitDstStageMask != VK_PIPELINE_STAGE_TRANSFER_BIT) invalid = 1;
	return submit_result;
}
static VKAPI_ATTR VkResult VKAPI_CALL present(VkQueue q, const VkPresentInfoKHR *info)
{
	const VkSwapchainPresentFenceInfoEXT *f = info->pNext;
	(void)q; presents++;
	if (!f || f->sType != VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT || f->swapchainCount != 1 || info->waitSemaphoreCount != 1) invalid = 1;
	return present_result;
}
static void setup(struct halo_vk_device *d)
{
	unsigned i;
	memset(d, 0, sizeof(*d)); memset(signaled, 0, sizeof(signaled));
	next_handle = 10; acquire_calls = resets = submits = presents = destroys = waits = creates = invalid = 0;
	fail_create = create_calls = acquire_index = 0;
	acquire_result = present_result = submit_result = fence_result = VK_SUCCESS;
	d->GetFenceStatus = fence_status; d->WaitForFences = wait_fences; d->ResetFences = reset_fences;
	d->CreateSemaphore = make_semaphore; d->CreateFence = make_fence; d->DestroySemaphore = destroy_semaphore; d->DestroyFence = destroy_fence;
	d->GetPhysicalDeviceSurfaceCapabilitiesKHR = capabilities; d->GetPhysicalDeviceSurfaceFormatsKHR = formats;
	d->CreateSwapchainKHR = make_swapchain; d->DestroySwapchainKHR = destroy_swapchain; d->GetSwapchainImagesKHR = images;
	d->AcquireNextImageKHR = acquire; d->ResetCommandBuffer = reset_command; d->BeginCommandBuffer = begin; d->EndCommandBuffer = end;
	d->CmdPipelineBarrier = barrier; d->CmdClearColorImage = clear; d->QueueSubmit = submit; d->QueuePresentKHR = present;
	for (i = 0; i < HALO_VK_FRAMES; i++) { d->frames[i].fence = HANDLE(VkFence, i + 1); d->frames[i].command = HANDLE(VkCommandBuffer, i + 1); }
}
static void complete(struct halo_vk_present *p, int presentation)
{
	unsigned i;
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		signaled[INDEX(p->device->frames[i].fence)] = 1;
		signaled[INDEX(p->frames[i].acquire_done)] = 1;
		if (presentation) signaled[INDEX(p->frames[i].present_done)] = 1;
	}
}
int main(void)
{
	struct halo_vk_device d;
	struct halo_vk_present p;
	float color[4] = { 1, 0, 0, 1 };
	unsigned i, calls, destruction;
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	CHECK(halo_vk_present_color(&p, 0, 0, color) == 0 && acquire_calls == 0);
	for (i = 0; i < 3; i++) { acquire_index = 2 - i; CHECK(halo_vk_present_color(&p, 640, 480, color) == 1); }
	calls = acquire_calls;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 0 && acquire_calls == calls);
	complete(&p, 0);
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 0 && acquire_calls == calls);
	destruction = destroys;
	CHECK(halo_vk_present_color(&p, 800, 450, color) == 0 && destroys == destruction);
	complete(&p, 1);
	CHECK(halo_vk_present_color(&p, 800, 450, color) == 1 && creates == 2 && destroys == destruction + 1);
	CHECK(waits == 0 && !invalid);
	CHECK(halo_vk_present_dispose(&p) && waits == 3);
	CHECK(halo_vk_present_dispose(&p));
	for (i = 1; i <= 12; i++)
	{
		setup(&d); fail_create = i;
		CHECK(halo_vk_present_initialize(&p, &d) == -1);
		CHECK(!halo_vk_present_dispose(&p) && destroys == i - 1 && waits == 0);
	}
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	acquire_result = VK_TIMEOUT;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 0 && submits == 0 && resets == 1);
	acquire_result = VK_ERROR_OUT_OF_DATE_KHR;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 0 && p.recreate && submits == 0);
	acquire_result = VK_SUBOPTIMAL_KHR;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 1 && p.recreate);
	CHECK(halo_vk_present_dispose(&p));
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	present_result = VK_ERROR_OUT_OF_DATE_KHR;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == 1 && p.frames[0].presenting && p.recreate);
	CHECK(halo_vk_present_dispose(&p) && waits == 3);
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	submit_result = VK_ERROR_OUT_OF_HOST_MEMORY;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == -1 && p.frames[0].acquiring && !p.frames[0].rendering);
	CHECK(!halo_vk_present_dispose(&p) && waits == 1);
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	present_result = VK_ERROR_SURFACE_LOST_KHR;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == -1 && p.frames[0].presenting);
	CHECK(!halo_vk_present_dispose(&p) && waits == 3);
	setup(&d); CHECK(halo_vk_present_initialize(&p, &d) == 1);
	present_result = VK_ERROR_DEVICE_LOST;
	CHECK(halo_vk_present_color(&p, 640, 480, color) == -1 && p.lost);
	CHECK(!halo_vk_present_dispose(&p) && waits == 0);
	puts("presentation ownership PASS");
	return 0;
}
