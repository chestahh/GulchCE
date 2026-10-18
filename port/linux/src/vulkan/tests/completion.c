/* Exercise the actual backend against delayed and failing Vulkan calls. */
#include "../vulkan_device.h"
#include <stdio.h>
#include <string.h>

static VkResult fence_result, query_result, reset_result, submit_result;
static uint64_t values[2], available[2];
static unsigned query_calls, begin_calls, submit_calls, reset_calls;
static uint32_t queried_first;
static VkQueryResultFlags queried_flags;
static struct halo_vk_device *disposing;

#define CHECK(expression) do { if (!(expression)) { fprintf(stderr, "line %d: %s\n", __LINE__, #expression); return 1; } } while (0)

static VKAPI_ATTR VkResult VKAPI_CALL fence_status(VkDevice device, VkFence fence)
{
	(void)device; (void)fence;
	return fence_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL query_results(VkDevice device, VkQueryPool pool,
	uint32_t first, uint32_t count, size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags)
{
	unsigned i;
	(void)device; (void)pool;
	query_calls++;
	queried_first = first;
	queried_flags = flags;
	if (count != 2 || size != 32 || stride != 16)
		return VK_ERROR_UNKNOWN;
	for (i = 0; i < count; i++)
	{
		((uint64_t *)data)[i * 2] = values[i];
		((uint64_t *)data)[i * 2 + 1] = available[i];
	}
	return query_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL reset_command(VkCommandBuffer command, VkCommandBufferResetFlags flags)
{
	(void)command; (void)flags;
	reset_calls++;
	return reset_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL begin_command(VkCommandBuffer command, const VkCommandBufferBeginInfo *info)
{
	(void)command; (void)info;
	begin_calls++;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL reset_queries(VkCommandBuffer command, VkQueryPool pool, uint32_t first, uint32_t count)
{
	(void)command; (void)pool; (void)first; (void)count;
}

static VKAPI_ATTR VkResult VKAPI_CALL end_command(VkCommandBuffer command)
{
	(void)command;
	return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL reset_fences(VkDevice device, uint32_t count, const VkFence *fences)
{
	(void)device; (void)count; (void)fences;
	return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue queue, uint32_t count, const VkSubmitInfo *info, VkFence fence)
{
	(void)queue; (void)count; (void)info; (void)fence;
	submit_calls++;
	return submit_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL idle(VkDevice device)
{
	(void)device;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL destroy_device(VkDevice device, const VkAllocationCallbacks *allocation)
{
	(void)device; (void)allocation;
	disposing->validation_errors++;
}

static VKAPI_ATTR void VKAPI_CALL destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocation)
{
	(void)instance; (void)allocation;
	disposing->validation_errors++;
}

int main(void)
{
	struct halo_vk_device device;
	struct halo_vk_frame *frame;
	unsigned i;
	memset(&device, 0, sizeof(device));
	device.GetFenceStatus = fence_status;
	device.GetQueryPoolResults = query_results;
	device.ResetCommandBuffer = reset_command;
	device.BeginCommandBuffer = begin_command;
	device.CmdResetQueryPool = reset_queries;
	device.EndCommandBuffer = end_command;
	device.ResetFences = reset_fences;
	device.QueueSubmit = submit;
	frame = &device.frames[2];
	frame->query_first = 4;
	frame->samples[0] = 99;
	frame->samples[1] = 88;
	CHECK(halo_vk_frame_begin(&device, 2));
	CHECK(!halo_vk_frame_begin(&device, 2));
	CHECK(halo_vk_frame_submit(&device, 2));
	CHECK(!halo_vk_frame_submit(&device, 2));
	fence_result = VK_NOT_READY;
	for (i = 0; i < 9; i++)
	{
		CHECK(halo_vk_frame_poll(&device, 2) == 0);
		CHECK(!halo_vk_frame_begin(&device, 2));
		CHECK(query_calls == 0 && reset_calls == 1 && begin_calls == 1 && submit_calls == 1);
		CHECK(frame->samples[0] == 99 && frame->samples[1] == 88);
	}
	fence_result = VK_SUCCESS;
	values[0] = 1234; values[1] = 0;
	available[0] = 1; available[1] = 0;
	query_result = VK_NOT_READY;
	CHECK(halo_vk_frame_poll(&device, 2) == 0);
	query_result = VK_SUCCESS;
	CHECK(halo_vk_frame_poll(&device, 2) == 0);
	CHECK(frame->samples[0] == 99 && frame->samples[1] == 88);
	CHECK(!halo_vk_frame_begin(&device, 2));
	available[1] = 1;
	CHECK(halo_vk_frame_poll(&device, 2) == 1);
	CHECK(queried_first == 4);
	CHECK(queried_flags == (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
	CHECK(frame->samples[0] == 1234 && frame->samples[1] == 0);
	CHECK(frame->state == _halo_vk_frame_ready);
	i = query_calls;
	CHECK(halo_vk_frame_poll(&device, 2) == 1 && query_calls == i);
	CHECK(halo_vk_frame_begin(&device, 2));
	CHECK(halo_vk_frame_submit(&device, 2));
	query_result = VK_ERROR_DEVICE_LOST;
	CHECK(halo_vk_frame_poll(&device, 2) == -1);
	CHECK(!halo_vk_frame_begin(&device, 2));
	CHECK(frame->samples[0] == 1234 && frame->samples[1] == 0);
	CHECK(frame->state == _halo_vk_frame_failed);
	device.frames[1].state = _halo_vk_frame_pending;
	fence_result = VK_ERROR_DEVICE_LOST;
	i = query_calls;
	CHECK(halo_vk_frame_poll(&device, 1) == -1 && query_calls == i);
	CHECK(!halo_vk_frame_begin(&device, 1));
	reset_result = VK_ERROR_DEVICE_LOST;
	CHECK(!halo_vk_frame_begin(&device, 0));
	CHECK(device.frames[0].state == _halo_vk_frame_failed);
	device.frames[0].state = _halo_vk_frame_recording;
	submit_result = VK_ERROR_DEVICE_LOST;
	CHECK(!halo_vk_frame_submit(&device, 0));
	CHECK(device.frames[0].state == _halo_vk_frame_failed);
	CHECK(!halo_vk_frame_begin(&device, HALO_VK_FRAMES));
	CHECK(!halo_vk_frame_submit(&device, HALO_VK_FRAMES));
	CHECK(halo_vk_frame_poll(&device, HALO_VK_FRAMES) == -1);
	disposing = &device;
	device.device = (VkDevice)(uintptr_t)1;
	device.instance = (VkInstance)(uintptr_t)1;
	device.DeviceWaitIdle = idle;
	device.DestroyDevice = destroy_device;
	device.DestroyInstance = destroy_instance;
	device.validation_errors = 1;
	halo_vk_device_dispose(&device);
	CHECK(device.validation_errors == 3);
	CHECK(!device.device && !device.instance && !device.library);
	return 0;
}
