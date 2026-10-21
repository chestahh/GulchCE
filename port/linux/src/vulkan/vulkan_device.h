/* Native Vulkan device and completion ownership. No Xbox GPU emulation. */
#ifndef HALO_VULKAN_DEVICE_H
#define HALO_VULKAN_DEVICE_H

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#define HALO_VK_FRAMES 3
#define HALO_VK_QUERIES_PER_FRAME 2

#define HALO_VK_INSTANCE_FUNCTIONS(X) \
	X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties) \
	X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceQueueFamilyProperties) \
	X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceFormatProperties) \
	X(CreateDevice) X(GetDeviceProcAddr) X(GetPhysicalDeviceFeatures2) \
	X(EnumerateDeviceExtensionProperties)

#define HALO_VK_DEVICE_FUNCTIONS(X) \
	X(DestroyDevice) X(GetDeviceQueue) X(DeviceWaitIdle) X(CreateCommandPool) \
	X(DestroyCommandPool) X(FreeCommandBuffers) X(AllocateCommandBuffers) X(ResetCommandBuffer) \
	X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) \
	X(GetFenceStatus) X(ResetFences) X(QueueSubmit) X(CreateQueryPool) \
	X(DestroyQueryPool) X(CmdResetQueryPool) X(CmdBeginQuery) X(CmdEndQuery) \
	X(GetQueryPoolResults) X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) \
	X(AllocateMemory) X(FreeMemory) X(BindImageMemory) X(CreateImageView) \
	X(DestroyImageView) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) \
	X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateRenderPass) \
	X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
	X(CreateShaderModule) X(DestroyShaderModule) X(CreatePipelineLayout) \
	X(DestroyPipelineLayout) X(CreateGraphicsPipelines) X(DestroyPipeline) \
	X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline) X(CmdPushConstants) \
	X(CmdDraw) X(CmdDrawIndexed) X(CmdBindIndexBuffer) X(CmdPipelineBarrier) X(CmdCopyImageToBuffer) \
	X(CreateSemaphore) X(DestroySemaphore) X(WaitForFences) X(CmdClearColorImage) \
	X(CreateSampler) X(DestroySampler) X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) \
	X(CreateDescriptorPool) X(DestroyDescriptorPool) X(ResetDescriptorPool) X(AllocateDescriptorSets) \
	X(UpdateDescriptorSets) X(CmdBindDescriptorSets) X(CmdBindVertexBuffers) X(CmdSetViewport) \
	X(CmdSetScissor) X(CmdSetBlendConstants) X(CmdCopyBufferToImage) X(CmdBlitImage) X(CmdCopyImage) \
	X(CmdClearAttachments) X(CmdClearDepthStencilImage)

enum halo_vk_frame_state
{
	_halo_vk_frame_free,
	_halo_vk_frame_recording,
	_halo_vk_frame_pending,
	_halo_vk_frame_ready,
	_halo_vk_frame_failed
};

struct halo_vk_frame
{
	VkCommandBuffer command;
	VkFence fence;
	enum halo_vk_frame_state state;
	uint32_t query_first;
	uint64_t samples[HALO_VK_QUERIES_PER_FRAME];
};

struct halo_vk_device
{
	void *library;
	PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
#define HALO_VK_DECLARE(name) PFN_vk##name name;
	HALO_VK_INSTANCE_FUNCTIONS(HALO_VK_DECLARE)
	HALO_VK_DEVICE_FUNCTIONS(HALO_VK_DECLARE)
#undef HALO_VK_DECLARE
	PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
	VkDebugUtilsMessengerEXT debug_messenger;
	unsigned validation_errors;
	VkInstance instance;
	VkSurfaceKHR surface;
	PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
	PFN_vkQueuePresentKHR QueuePresentKHR;
	VkPhysicalDevice physical;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties memory;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	VkCommandPool commands;
	VkQueryPool queries;
	struct halo_vk_frame frames[HALO_VK_FRAMES];
};

int halo_vk_device_initialize(struct halo_vk_device *device, int validation);
/* Surface belongs to the device. The callback runs after instance creation. */
int halo_vk_device_initialize_surface(struct halo_vk_device *device, int validation,
	const char *const *extensions, unsigned count,
	int (*create_surface)(VkInstance, void *, VkSurfaceKHR *), void *context);
void halo_vk_device_dispose(struct halo_vk_device *device);
int halo_vk_frame_begin(struct halo_vk_device *device, unsigned slot);
/* The caller must record both queries in the slot's disjoint range before
submitting. This initial capacity serves the foundation test, not Halo yet. */
int halo_vk_frame_submit(struct halo_vk_device *device, unsigned slot);
/* 1: completed and safe to read/reuse, 0: still pending, -1: failed. */
int halo_vk_frame_poll(struct halo_vk_device *device, unsigned slot);
int halo_vk_memory_type(struct halo_vk_device *device, uint32_t bits,
	VkMemoryPropertyFlags flags, uint32_t *index);

#endif
