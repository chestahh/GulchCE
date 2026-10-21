/* Native presentation. No game state, shader translation or OpenGL calls. */
#include "vulkan_present.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(struct halo_vk_present *p, VkResult result, const char *operation)
{
	fprintf(stderr, "vulkan: %s failed (%d)\n", operation, (int)result);
	p->failed = 1;
	p->lost |= result == VK_ERROR_DEVICE_LOST;
	return -1;
}

static int poll(struct halo_vk_present *p, VkFence fence, int *pending, int wait)
{
	struct halo_vk_device *d = p->device;
	VkResult result;
	if (!*pending)
		return 1;
	result = wait ? d->WaitForFences(d->device, 1, &fence, VK_TRUE, UINT64_MAX) :
		d->GetFenceStatus(d->device, fence);
	if (result == VK_NOT_READY)
		return 0;
	if (result != VK_SUCCESS)
		return fail(p, result, "completion fence");
	*pending = 0;
	return 1;
}

static int verify_frame(struct halo_vk_present *p, struct halo_vk_present_frame *f)
{
	size_t count, i;
	FILE *file;
	unsigned char rgb[3];
	if (!f->capture_pending)
		return 1;
	count = (size_t)p->extent.width * p->extent.height;
	for (i = 0; i < count; i++)
		if (memcmp(f->pixels + i * 4, f->expected, 4))
			return fail(p, VK_ERROR_UNKNOWN, "presentation pixel comparison");
	f->capture_pending = 0;
	p->verified++;
	if (p->capture_path && *p->capture_path && !p->capture_written && p->submitted >= 60)
	{
		file = fopen(p->capture_path, "wb");
		if (!file)
			return fail(p, VK_ERROR_UNKNOWN, "capture file open");
		fprintf(file, "P6\n%u %u\n255\n", p->extent.width, p->extent.height);
		for (i = 0; i < count; i++)
		{
			rgb[0] = f->pixels[i * 4 + (p->bgra ? 2 : 0)];
			rgb[1] = f->pixels[i * 4 + 1];
			rgb[2] = f->pixels[i * 4 + (p->bgra ? 0 : 2)];
			fwrite(rgb, 1, 3, file);
		}
		if (ferror(file)) { fclose(file); return fail(p, VK_ERROR_UNKNOWN, "capture file write"); }
		if (fclose(file)) return fail(p, VK_ERROR_UNKNOWN, "capture file close");
		p->capture_written = 1;
	}
	return 1;
}

static int frame_ready(struct halo_vk_present *p, unsigned slot, int wait)
{
	struct halo_vk_present_frame *f = &p->frames[slot];
	int a, b, c;
	a = poll(p, f->acquire_done, &f->acquiring, wait);
	b = poll(p, p->device->frames[slot].fence, &f->rendering, wait);
	c = poll(p, f->present_done, &f->presenting, wait);
	if (a > 0 && b > 0 && verify_frame(p, f) < 0)
		return -1;
	return a < 0 || b < 0 || c < 0 ? -1 : a && b && c;
}

static void destroy_swapchain(struct halo_vk_present *p)
{
	unsigned i;
	struct halo_vk_device *d = p->device;
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		struct halo_vk_present_frame *f = &p->frames[i];
		if (f->pixels) d->UnmapMemory(d->device, f->memory);
		if (f->readback) d->DestroyBuffer(d->device, f->readback, NULL);
		if (f->memory) d->FreeMemory(d->device, f->memory, NULL);
		f->pixels = NULL;
		f->readback = VK_NULL_HANDLE;
		f->memory = VK_NULL_HANDLE;
		f->capture_pending = 0;
	}
	if (p->swapchain)
		p->device->DestroySwapchainKHR(p->device->device, p->swapchain, NULL);
	p->swapchain = VK_NULL_HANDLE;
	free(p->images);
	p->images = NULL;
	p->image_count = 0;
}

int halo_vk_present_initialize(struct halo_vk_present *p, struct halo_vk_device *d)
{
	VkSemaphoreCreateInfo semaphore = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fence = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	unsigned i;
	VkResult result;
	memset(p, 0, sizeof(*p));
	p->device = d;
	p->recreate = 1;
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		struct halo_vk_present_frame *f = &p->frames[i];
#define CREATE(call) do { result = (call); if (result != VK_SUCCESS) return fail(p, result, "presentation synchronization"); } while (0)
		CREATE(d->CreateSemaphore(d->device, &semaphore, NULL, &f->acquired));
		CREATE(d->CreateSemaphore(d->device, &semaphore, NULL, &f->rendered));
		CREATE(d->CreateFence(d->device, &fence, NULL, &f->acquire_done));
		CREATE(d->CreateFence(d->device, &fence, NULL, &f->present_done));
#undef CREATE
	}
	return 1;
}

static int create_readback(struct halo_vk_present *p)
{
	struct halo_vk_device *d = p->device;
	VkBufferCreateInfo buffer = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	VkMemoryAllocateInfo memory = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	VkMemoryRequirements requirements;
	VkResult result;
	unsigned i;
	buffer.size = (VkDeviceSize)p->extent.width * p->extent.height * 4;
	if (buffer.size > SIZE_MAX)
		return fail(p, VK_ERROR_OUT_OF_HOST_MEMORY, "capture size");
	buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		struct halo_vk_present_frame *f = &p->frames[i];
		if ((result = d->CreateBuffer(d->device, &buffer, NULL, &f->readback)) != VK_SUCCESS)
			return fail(p, result, "capture buffer");
		d->GetBufferMemoryRequirements(d->device, f->readback, &requirements);
		memory.allocationSize = requirements.size;
		if (!halo_vk_memory_type(d, requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory.memoryTypeIndex))
			return fail(p, VK_ERROR_FEATURE_NOT_PRESENT, "capture memory type");
		if ((result = d->AllocateMemory(d->device, &memory, NULL, &f->memory)) != VK_SUCCESS ||
			(result = d->BindBufferMemory(d->device, f->readback, f->memory, 0)) != VK_SUCCESS ||
			(result = d->MapMemory(d->device, f->memory, 0, buffer.size, 0, (void **)&f->pixels)) != VK_SUCCESS)
			return fail(p, result, "capture memory");
	}
	return 1;
}

static unsigned clamp_extent(unsigned value, unsigned minimum, unsigned maximum)
{
	return value < minimum ? minimum : value > maximum ? maximum : value;
}

static int create_swapchain(struct halo_vk_present *p, unsigned width, unsigned height)
{
	struct halo_vk_device *d = p->device;
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR *formats, format;
	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	VkResult result;
	uint32_t count = 0, i;
	if ((result = d->GetPhysicalDeviceSurfaceCapabilitiesKHR(d->physical, d->surface, &caps)) != VK_SUCCESS)
		return fail(p, result, "surface capabilities");
	if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		return fail(p, VK_ERROR_FORMAT_NOT_SUPPORTED, "surface cannot receive presentation clears");
	if ((result = d->GetPhysicalDeviceSurfaceFormatsKHR(d->physical, d->surface, &count, NULL)) != VK_SUCCESS || !count || count > 256)
		return fail(p, result, "surface formats");
	formats = calloc(count, sizeof(*formats));
	if (!formats)
		return fail(p, VK_ERROR_OUT_OF_HOST_MEMORY, "surface formats allocation");
	result = d->GetPhysicalDeviceSurfaceFormatsKHR(d->physical, d->surface, &count, formats);
	format = formats[0];
	for (i = 0; i < count; i++)
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			format = formats[i];
	free(formats);
	if (result != VK_SUCCESS)
		return fail(p, result, "surface formats");
	if (format.format == VK_FORMAT_UNDEFINED)
		format.format = VK_FORMAT_B8G8R8A8_UNORM;
	info.surface = d->surface;
	info.minImageCount = caps.minImageCount + 1;
	if (caps.maxImageCount && info.minImageCount > caps.maxImageCount)
		info.minImageCount = caps.maxImageCount;
	info.imageFormat = format.format;
	info.imageColorSpace = format.colorSpace;
	info.imageExtent = caps.currentExtent;
	if (info.imageExtent.width == UINT32_MAX)
	{
		info.imageExtent.width = clamp_extent(width, caps.minImageExtent.width, caps.maxImageExtent.width);
		info.imageExtent.height = clamp_extent(height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}
	if (!info.imageExtent.width || !info.imageExtent.height)
		return 0;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (p->verify_pixels)
	{
		if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
			(format.format != VK_FORMAT_B8G8R8A8_UNORM && format.format != VK_FORMAT_R8G8B8A8_UNORM))
			return fail(p, VK_ERROR_FORMAT_NOT_SUPPORTED, "presentation test needs readable RGBA8/BGRA8 UNORM");
		info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		p->bgra = format.format == VK_FORMAT_B8G8R8A8_UNORM;
	}
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = caps.currentTransform;
	/* Choose a supported alpha mode, preferring opaque. */
	for (i = 1; i <= VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; i <<= 1)
		if (caps.supportedCompositeAlpha & i)
		{
			info.compositeAlpha = (VkCompositeAlphaFlagBitsKHR)i;
			break;
		}
	info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	info.clipped = VK_TRUE;
	if ((result = d->CreateSwapchainKHR(d->device, &info, NULL, &p->swapchain)) != VK_SUCCESS)
		return fail(p, result, "create swapchain");
	if ((result = d->GetSwapchainImagesKHR(d->device, p->swapchain, &count, NULL)) != VK_SUCCESS || !count || count > 256)
		return fail(p, result, "swapchain image count");
	p->images = calloc(count, sizeof(*p->images));
	if (!p->images)
		return fail(p, VK_ERROR_OUT_OF_HOST_MEMORY, "swapchain image allocation");
	if ((result = d->GetSwapchainImagesKHR(d->device, p->swapchain, &count, p->images)) != VK_SUCCESS)
		return fail(p, result, "swapchain images");
	p->image_count = count;
	p->extent = info.imageExtent;
	if (p->verify_pixels && create_readback(p) < 0)
		return -1;
	p->width = width;
	p->height = height;
	p->recreate = 0;
	p->generations++;
	fprintf(stderr, "vulkan: swapchain generation=%u extent=%ux%u images=%u\n",
		p->generations, p->extent.width, p->extent.height, count);
	return 1;
}

int halo_vk_present_color(struct halo_vk_present *p, unsigned width, unsigned height, const float color[4])
{
	struct halo_vk_device *d = p->device;
	struct halo_vk_present_frame *f = &p->frames[p->slot];
	struct halo_vk_frame *frame = &d->frames[p->slot];
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	VkSwapchainPresentFenceInfoEXT completion = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT };
	VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkClearColorValue clear;
	VkBufferImageCopy copy = { 0 };
	VkMemoryBarrier host = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	VkResult result;
	unsigned i;
	uint32_t image;
	int ready;
	if (p->failed)
		return -1;
	if (!width || !height)
	{
		p->recreate = 1;
		return 0;
	}
	if (p->recreate || width != p->width || height != p->height)
	{
		for (i = 0; i < HALO_VK_FRAMES; i++)
		{
			ready = frame_ready(p, i, 0);
			if (ready <= 0)
				return ready;
		}
		destroy_swapchain(p);
		ready = create_swapchain(p, width, height);
		if (ready <= 0)
			return ready;
	}
	ready = frame_ready(p, p->slot, 0);
	if (ready <= 0)
		return ready;
#define TRY(call, operation) do { result = (call); if (result != VK_SUCCESS) return fail(p, result, operation); } while (0)
	TRY(d->ResetFences(d->device, 1, &f->acquire_done), "reset acquire fence");
	result = d->AcquireNextImageKHR(d->device, p->swapchain, 0, f->acquired, f->acquire_done, &image);
	if (result == VK_NOT_READY || result == VK_TIMEOUT)
		return 0;
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		p->recreate = 1;
		return 0;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		return fail(p, result, "acquire swapchain image");
	f->acquiring = 1;
	p->recreate |= result == VK_SUBOPTIMAL_KHR;
	if (image >= p->image_count)
		return fail(p, VK_ERROR_UNKNOWN, "swapchain image index");
	TRY(d->ResetCommandBuffer(frame->command, 0), "reset presentation command");
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	TRY(d->BeginCommandBuffer(frame->command, &begin), "begin presentation command");
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.image = p->images[image];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier);
	memcpy(clear.float32, color, sizeof(clear.float32));
	d->CmdClearColorImage(frame->command, barrier.image, barrier.newLayout, &clear, 1, &barrier.subresourceRange);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	if (p->verify_pixels)
	{
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier);
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent.width = p->extent.width;
		copy.imageExtent.height = p->extent.height;
		copy.imageExtent.depth = 1;
		d->CmdCopyImageToBuffer(frame->command, barrier.image, barrier.newLayout, f->readback, 1, &copy);
		host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			0, 1, &host, 0, NULL, 0, NULL);
		for (i = 0; i < 4; i++)
			f->expected[i] = (unsigned char)(color[p->bgra && i < 3 ? 2 - i : i] * 255.0f + 0.5f);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.dstAccessMask = 0;
	d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier);
	TRY(d->EndCommandBuffer(frame->command), "end presentation command");
	TRY(d->ResetFences(d->device, 1, &frame->fence), "reset rendering fence");
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &f->acquired;
	submit.pWaitDstStageMask = &stage;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &frame->command;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &f->rendered;
	TRY(d->QueueSubmit(d->queue, 1, &submit, frame->fence), "submit presentation command");
	f->rendering = 1;
	f->capture_pending = p->verify_pixels;
	TRY(d->ResetFences(d->device, 1, &f->present_done), "reset presentation fence");
	completion.swapchainCount = 1;
	completion.pFences = &f->present_done;
	present.pNext = &completion;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &f->rendered;
	present.swapchainCount = 1;
	present.pSwapchains = &p->swapchain;
	present.pImageIndices = &image;
	result = d->QueuePresentKHR(d->queue, &present);
	/* These results still enqueue the semaphore wait and presentation fence. */
	f->presenting = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR ||
		result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR;
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
		p->recreate = 1;
	else if (result != VK_SUCCESS)
		return fail(p, result, "present");
	p->submitted++;
	p->slot = (p->slot + 1) % HALO_VK_FRAMES;
	return 1;
#undef TRY
}

int halo_vk_present_dispose(struct halo_vk_present *p)
{
	struct halo_vk_device *d = p->device;
	unsigned i;
	int ok = !p->failed;
	if (!d)
		return ok;
	for (i = 0; i < HALO_VK_FRAMES && !p->lost; i++)
		if (frame_ready(p, i, 1) < 0)
			ok = 0;
	/* A lost device permits releasing its children. On other wait failures,
	leave ownership intact; the caller must not destroy the device/window. */
	if (!p->lost)
		for (i = 0; i < HALO_VK_FRAMES; i++)
			if (p->frames[i].acquiring || p->frames[i].rendering || p->frames[i].presenting)
				return 0;
	fprintf(stderr, "vulkan: presentation completed submitted=%u verified=%u\n", p->submitted, p->verified);
	destroy_swapchain(p);
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		struct halo_vk_present_frame *f = &p->frames[i];
		if (f->acquired) d->DestroySemaphore(d->device, f->acquired, NULL);
		if (f->rendered) d->DestroySemaphore(d->device, f->rendered, NULL);
		if (f->acquire_done) d->DestroyFence(d->device, f->acquire_done, NULL);
		if (f->present_done) d->DestroyFence(d->device, f->present_done, NULL);
	}
	memset(p, 0, sizeof(*p));
	return ok;
}
