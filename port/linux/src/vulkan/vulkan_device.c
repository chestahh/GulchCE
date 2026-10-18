/*
VULKAN_DEVICE.C

The native Vulkan backend's device and bounded command/query ownership.
The game-facing D3D8 adapter will supply draws, not emulated registers,
DMA commands or a virtual GPU. All functions run on one rendering thread.
*/
#include "vulkan_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static int vk_result_ok(VkResult result, const char *operation)
{
	if (result == VK_SUCCESS)
		return 1;
	fprintf(stderr, "vulkan: %s failed (%d)\n", operation, (int)result);
	return 0;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_message(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data, void *context)
{
	struct halo_vk_device *device = context;
	(void)type;
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		__atomic_fetch_add(&device->validation_errors, 1, __ATOMIC_RELAXED);
	fprintf(stderr, "vulkan validation: %s\n", data->pMessage);
	return VK_FALSE;
}

static int vk_create_instance(struct halo_vk_device *d, int validation)
{
	PFN_vkCreateInstance create_instance;
	PFN_vkEnumerateInstanceLayerProperties enumerate_layers;
	PFN_vkCreateDebugUtilsMessengerEXT create_debug;
	VkApplicationInfo application = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	VkDebugUtilsMessengerCreateInfoEXT debug = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	const char *layer = "VK_LAYER_KHRONOS_validation";
	const char *extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	VkLayerProperties *layers = NULL;
	uint32_t count = 0, i;
	int found = 0;

#ifdef _WIN32
	d->library = LoadLibraryA("vulkan-1.dll");
	if (d->library)
		d->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress((HMODULE)d->library, "vkGetInstanceProcAddr");
#else
	d->library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
	if (d->library)
		d->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(d->library, "vkGetInstanceProcAddr");
#endif
	if (!d->GetInstanceProcAddr)
	{
		fprintf(stderr, "vulkan: no native Vulkan loader for this process architecture\n");
		return 0;
	}
	create_instance = (PFN_vkCreateInstance)d->GetInstanceProcAddr(NULL, "vkCreateInstance");
	enumerate_layers = (PFN_vkEnumerateInstanceLayerProperties)d->GetInstanceProcAddr(NULL, "vkEnumerateInstanceLayerProperties");
	if (!create_instance || !enumerate_layers)
		return 0;
	if (validation)
	{
		if (!vk_result_ok(enumerate_layers(&count, NULL), "enumerate layers"))
			return 0;
		layers = calloc(count ? count : 1, sizeof(*layers));
		if (!layers)
			return 0;
		if (enumerate_layers(&count, layers) == VK_SUCCESS)
			for (i = 0; i < count; i++)
				if (!strcmp(layers[i].layerName, layer))
					found = 1;
		free(layers);
		if (!found)
		{
			fprintf(stderr, "vulkan: validation requested but VK_LAYER_KHRONOS_validation is unavailable\n");
			return 0;
		}
		debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug.pfnUserCallback = vk_debug_message;
		debug.pUserData = d;
		info.enabledLayerCount = 1;
		info.ppEnabledLayerNames = &layer;
		info.enabledExtensionCount = 1;
		info.ppEnabledExtensionNames = &extension;
		info.pNext = &debug;
	}
	application.pApplicationName = "GulchCE native Vulkan milestone";
	application.apiVersion = VK_API_VERSION_1_1;
	info.pApplicationInfo = &application;
	if (!vk_result_ok(create_instance(&info, NULL, &d->instance), "create instance"))
		return 0;
#define HALO_VK_LOAD_INSTANCE(name) \
	d->name = (PFN_vk##name)d->GetInstanceProcAddr(d->instance, "vk" #name); \
	if (!d->name) { fprintf(stderr, "vulkan: missing vk%s\n", #name); return 0; }
	HALO_VK_INSTANCE_FUNCTIONS(HALO_VK_LOAD_INSTANCE)
#undef HALO_VK_LOAD_INSTANCE
	if (validation)
	{
		create_debug = (PFN_vkCreateDebugUtilsMessengerEXT)d->GetInstanceProcAddr(d->instance, "vkCreateDebugUtilsMessengerEXT");
		d->DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)d->GetInstanceProcAddr(d->instance, "vkDestroyDebugUtilsMessengerEXT");
		if (!create_debug || !d->DestroyDebugUtilsMessengerEXT ||
			!vk_result_ok(create_debug(d->instance, &debug, NULL, &d->debug_messenger), "create validation messenger"))
			return 0;
	}
	return 1;
}

static int vk_choose_device(struct halo_vk_device *d)
{
	VkPhysicalDevice *devices;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkQueueFamilyProperties *families;
	uint32_t count = 0, family_count, i, j;
	int selected = 0;

	if (!vk_result_ok(d->EnumeratePhysicalDevices(d->instance, &count, NULL), "enumerate devices") || !count)
		return 0;
	devices = calloc(count, sizeof(*devices));
	if (!devices)
		return 0;
	if (d->EnumeratePhysicalDevices(d->instance, &count, devices) != VK_SUCCESS)
	{
		free(devices);
		return 0;
	}
	for (i = 0; i < count && !selected; i++)
	{
		d->GetPhysicalDeviceProperties(devices[i], &properties);
		d->GetPhysicalDeviceFeatures(devices[i], &features);
		/* Precise counts are part of the Xbox visibility contract. */
		if (properties.apiVersion < VK_API_VERSION_1_1 || !features.occlusionQueryPrecise)
			continue;
		d->GetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, NULL);
		families = calloc(family_count ? family_count : 1, sizeof(*families));
		if (!families)
			break;
		d->GetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, families);
		for (j = 0; j < family_count; j++)
			if (families[j].queueCount && (families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				d->physical = devices[i];
				d->properties = properties;
				d->queue_family = j;
				selected = 1;
				break;
			}
		free(families);
	}
	free(devices);
	if (!selected)
		fprintf(stderr, "vulkan: need Vulkan 1.1, a graphics queue and precise occlusion queries\n");
	return selected;
}

int halo_vk_device_initialize(struct halo_vk_device *d, int validation)
{
	VkDeviceQueueCreateInfo queue = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	VkDeviceCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	VkPhysicalDeviceFeatures features = { 0 };
	VkCommandPoolCreateInfo commands = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	VkCommandBufferAllocateInfo allocation = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	VkQueryPoolCreateInfo queries = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	VkFenceCreateInfo fence = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	float priority = 1.0f;
	unsigned i;

	memset(d, 0, sizeof(*d));
	if (!vk_create_instance(d, validation) || !vk_choose_device(d))
		goto failed;
	queue.queueFamilyIndex = d->queue_family;
	queue.queueCount = 1;
	queue.pQueuePriorities = &priority;
	features.occlusionQueryPrecise = VK_TRUE;
	info.queueCreateInfoCount = 1;
	info.pQueueCreateInfos = &queue;
	info.pEnabledFeatures = &features;
	if (!vk_result_ok(d->CreateDevice(d->physical, &info, NULL, &d->device), "create device"))
		goto failed;
#define HALO_VK_LOAD_DEVICE(name) \
	d->name = (PFN_vk##name)d->GetDeviceProcAddr(d->device, "vk" #name); \
	if (!d->name) { fprintf(stderr, "vulkan: missing vk%s\n", #name); goto failed; }
	HALO_VK_DEVICE_FUNCTIONS(HALO_VK_LOAD_DEVICE)
#undef HALO_VK_LOAD_DEVICE
	d->GetDeviceQueue(d->device, d->queue_family, 0, &d->queue);
	d->GetPhysicalDeviceMemoryProperties(d->physical, &d->memory);
	commands.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commands.queueFamilyIndex = d->queue_family;
	if (!vk_result_ok(d->CreateCommandPool(d->device, &commands, NULL, &d->commands), "create command pool"))
		goto failed;
	queries.queryType = VK_QUERY_TYPE_OCCLUSION;
	queries.queryCount = HALO_VK_FRAMES * HALO_VK_QUERIES_PER_FRAME;
	if (!vk_result_ok(d->CreateQueryPool(d->device, &queries, NULL, &d->queries), "create query pool"))
		goto failed;
	allocation.commandPool = d->commands;
	allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocation.commandBufferCount = 1;
	for (i = 0; i < HALO_VK_FRAMES; i++)
	{
		d->frames[i].query_first = i * HALO_VK_QUERIES_PER_FRAME;
		if (!vk_result_ok(d->AllocateCommandBuffers(d->device, &allocation, &d->frames[i].command), "allocate command buffer") ||
			!vk_result_ok(d->CreateFence(d->device, &fence, NULL, &d->frames[i].fence), "create fence"))
			goto failed;
	}
	fprintf(stderr, "vulkan: native Vulkan %u.%u on %s; %u frames; precise queries; validation=%s\n",
		VK_VERSION_MAJOR(d->properties.apiVersion), VK_VERSION_MINOR(d->properties.apiVersion),
		d->properties.deviceName, HALO_VK_FRAMES, validation ? "on" : "off");
	return 1;
failed:
	halo_vk_device_dispose(d);
	return 0;
}

void halo_vk_device_dispose(struct halo_vk_device *d)
{
	unsigned i;
	if (d->device)
	{
		/* Only teardown waits; normal query delivery never forces completion. */
		if (d->DeviceWaitIdle)
			d->DeviceWaitIdle(d->device);
		for (i = 0; i < HALO_VK_FRAMES; i++)
			if (d->frames[i].fence)
				d->DestroyFence(d->device, d->frames[i].fence, NULL);
		if (d->queries)
			d->DestroyQueryPool(d->device, d->queries, NULL);
		if (d->commands)
			d->DestroyCommandPool(d->device, d->commands, NULL);
		if (d->DestroyDevice)
			d->DestroyDevice(d->device, NULL);
	}
	if (d->debug_messenger)
		d->DestroyDebugUtilsMessengerEXT(d->instance, d->debug_messenger, NULL);
	if (d->instance && d->DestroyInstance)
		d->DestroyInstance(d->instance, NULL);
	if (d->library)
	{
#ifdef _WIN32
		FreeLibrary((HMODULE)d->library);
#else
		dlclose(d->library);
#endif
	}
	memset(d, 0, sizeof(*d));
}

int halo_vk_frame_begin(struct halo_vk_device *d, unsigned slot)
{
	struct halo_vk_frame *frame;
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	if (slot >= HALO_VK_FRAMES)
		return 0;
	frame = &d->frames[slot];
	if (frame->state != _halo_vk_frame_free && frame->state != _halo_vk_frame_ready)
		return 0;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!vk_result_ok(d->ResetCommandBuffer(frame->command, 0), "reset command buffer") ||
		!vk_result_ok(d->BeginCommandBuffer(frame->command, &begin), "begin command buffer"))
	{
		frame->state = _halo_vk_frame_failed;
		return 0;
	}
	d->CmdResetQueryPool(frame->command, d->queries, frame->query_first, HALO_VK_QUERIES_PER_FRAME);
	frame->state = _halo_vk_frame_recording;
	return 1;
}

int halo_vk_frame_submit(struct halo_vk_device *d, unsigned slot)
{
	struct halo_vk_frame *frame;
	VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	if (slot >= HALO_VK_FRAMES)
		return 0;
	frame = &d->frames[slot];
	if (frame->state != _halo_vk_frame_recording)
		return 0;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &frame->command;
	if (!vk_result_ok(d->EndCommandBuffer(frame->command), "end command buffer") ||
		!vk_result_ok(d->ResetFences(d->device, 1, &frame->fence), "reset fence") ||
		!vk_result_ok(d->QueueSubmit(d->queue, 1, &submit, frame->fence), "submit"))
	{
		frame->state = _halo_vk_frame_failed;
		return 0;
	}
	frame->state = _halo_vk_frame_pending;
	return 1;
}

int halo_vk_frame_poll(struct halo_vk_device *d, unsigned slot)
{
	struct halo_vk_frame *frame;
	uint64_t results[HALO_VK_QUERIES_PER_FRAME][2] __attribute__((aligned(8))) = { { 0 } };
	VkResult status;
	unsigned i;
	if (slot >= HALO_VK_FRAMES)
		return -1;
	frame = &d->frames[slot];
	if (frame->state == _halo_vk_frame_ready)
		return 1;
	if (frame->state != _halo_vk_frame_pending)
		return -1;
	status = d->GetFenceStatus(d->device, frame->fence);
	if (status == VK_NOT_READY)
		return 0;
	if (status != VK_SUCCESS)
		goto failed;
	status = d->GetQueryPoolResults(d->device, d->queries, frame->query_first,
		HALO_VK_QUERIES_PER_FRAME, sizeof(results), results, sizeof(results[0]),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
	if (status == VK_NOT_READY)
		return 0;
	if (status != VK_SUCCESS)
		goto failed;
	/* Never publish a partial batch, including a valid zero beside an
	unfinished query. Its command buffer and query range remain owned. */
	for (i = 0; i < HALO_VK_QUERIES_PER_FRAME; i++)
		if (!results[i][1])
			return 0;
	for (i = 0; i < HALO_VK_QUERIES_PER_FRAME; i++)
		frame->samples[i] = results[i][0];
	frame->state = _halo_vk_frame_ready;
	return 1;
failed:
	vk_result_ok(status, "read completed frame");
	frame->state = _halo_vk_frame_failed;
	return -1;
}

int halo_vk_memory_type(struct halo_vk_device *d, uint32_t bits,
	VkMemoryPropertyFlags flags, uint32_t *index)
{
	uint32_t i;
	for (i = 0; i < d->memory.memoryTypeCount; i++)
		if ((bits & (1u << i)) && (d->memory.memoryTypes[i].propertyFlags & flags) == flags)
		{
			*index = i;
			return 1;
		}
	fprintf(stderr, "vulkan: no compatible memory type (flags=%u)\n", (unsigned)flags);
	return 0;
}
