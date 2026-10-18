/* Real GPU tests for the native backend. No game data or window required. */
#include "../vulkan_device.h"
#include "depth_spirv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define WIDTH 64
#define HEIGHT 64

struct test_image
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
};

struct test_frame
{
	struct test_image color, depth;
	VkFramebuffer framebuffer;
	VkBuffer readback;
	VkDeviceMemory memory;
	unsigned char *pixels;
};

struct test_scene
{
	struct halo_vk_device device;
	VkRenderPass pass;
	VkPipelineLayout layout;
	VkPipeline pipeline;
	VkShaderModule vertex, fragment;
	struct test_frame frames[HALO_VK_FRAMES];
};

static int check(VkResult result, const char *operation)
{
	if (result == VK_SUCCESS)
		return 1;
	fprintf(stderr, "vulkan smoke: %s failed (%d)\n", operation, (int)result);
	return 0;
}

static int create_image(struct halo_vk_device *d, struct test_image *image,
	VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	VkMemoryAllocateInfo allocate = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	VkMemoryRequirements requirements;
	VkFormatProperties properties;
	VkFormatFeatureFlags required = aspect == VK_IMAGE_ASPECT_COLOR_BIT ?
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT : VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

	d->GetPhysicalDeviceFormatProperties(d->physical, format, &properties);
	if ((properties.optimalTilingFeatures & required) != required)
	{
		fprintf(stderr, "vulkan smoke: attachment format %d unsupported\n", (int)format);
		return 0;
	}
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent.width = WIDTH;
	info.extent.height = HEIGHT;
	info.extent.depth = 1;
	info.mipLevels = info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage;
	if (!check(d->CreateImage(d->device, &info, NULL, &image->image), "create image"))
		return 0;
	d->GetImageMemoryRequirements(d->device, image->image, &requirements);
	allocate.allocationSize = requirements.size;
	if (!halo_vk_memory_type(d, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &allocate.memoryTypeIndex) ||
		!check(d->AllocateMemory(d->device, &allocate, NULL, &image->memory), "allocate image") ||
		!check(d->BindImageMemory(d->device, image->image, image->memory, 0), "bind image"))
		return 0;
	view.image = image->image;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.subresourceRange.aspectMask = aspect;
	view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
	return check(d->CreateImageView(d->device, &view, NULL, &image->view), "create view");
}

static int create_frame(struct test_scene *s, unsigned slot)
{
	struct halo_vk_device *d = &s->device;
	struct test_frame *f = &s->frames[slot];
	VkImageView views[2];
	VkFramebufferCreateInfo framebuffer = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	VkBufferCreateInfo buffer = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	VkMemoryAllocateInfo allocation = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	VkMemoryRequirements requirements;

	if (!create_image(d, &f->color, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT) ||
		!create_image(d, &f->depth, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
		return 0;
	views[0] = f->color.view;
	views[1] = f->depth.view;
	framebuffer.renderPass = s->pass;
	framebuffer.attachmentCount = 2;
	framebuffer.pAttachments = views;
	framebuffer.width = WIDTH;
	framebuffer.height = HEIGHT;
	framebuffer.layers = 1;
	if (!check(d->CreateFramebuffer(d->device, &framebuffer, NULL, &f->framebuffer), "create framebuffer"))
		return 0;
	buffer.size = WIDTH * HEIGHT * 4;
	buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (!check(d->CreateBuffer(d->device, &buffer, NULL, &f->readback), "create readback"))
		return 0;
	d->GetBufferMemoryRequirements(d->device, f->readback, &requirements);
	allocation.allocationSize = requirements.size;
	if (!halo_vk_memory_type(d, requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &allocation.memoryTypeIndex) ||
		!check(d->AllocateMemory(d->device, &allocation, NULL, &f->memory), "allocate readback") ||
		!check(d->BindBufferMemory(d->device, f->readback, f->memory, 0), "bind readback"))
		return 0;
	return check(d->MapMemory(d->device, f->memory, 0, VK_WHOLE_SIZE, 0, (void **)&f->pixels), "map readback");
}

static int create_pass(struct test_scene *s)
{
	VkAttachmentDescription attachments[2] = { { 0 } };
	VkAttachmentReference color = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depth = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription subpass = { 0 };
	VkRenderPassCreateInfo pass = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	VkSubpassDependency dependency = { 0 };
	unsigned i;

	for (i = 0; i < 2; i++)
	{
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[0].finalLayout = color.layout;
	attachments[1].format = VK_FORMAT_D32_SFLOAT;
	attachments[1].finalLayout = depth.layout;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color;
	subpass.pDepthStencilAttachment = &depth;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = dependency.srcStageMask;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	pass.attachmentCount = 2;
	pass.pAttachments = attachments;
	pass.subpassCount = 1;
	pass.pSubpasses = &subpass;
	pass.dependencyCount = 1;
	pass.pDependencies = &dependency;
	return check(s->device.CreateRenderPass(s->device.device, &pass, NULL, &s->pass), "create render pass");
}

static int create_pipeline(struct test_scene *s)
{
	struct halo_vk_device *d = &s->device;
	VkShaderModuleCreateInfo module = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	VkPipelineShaderStageCreateInfo stages[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	VkPipelineVertexInputStateCreateInfo vertex = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	VkPipelineViewportStateCreateInfo viewport_state = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	VkViewport viewport = { 0, 0, WIDTH, HEIGHT, 0, 1 };
	VkRect2D scissor = { { 0, 0 }, { WIDTH, HEIGHT } };
	VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	VkPipelineMultisampleStateCreateInfo samples = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	VkPipelineDepthStencilStateCreateInfo depth = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState blend_attachment = { 0 };
	VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	VkPushConstantRange constants = { VK_SHADER_STAGE_VERTEX_BIT, 0, 32 };
	VkPipelineLayoutCreateInfo layout = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	VkGraphicsPipelineCreateInfo pipeline = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	module.codeSize = sizeof(depth_vert);
	module.pCode = depth_vert;
	if (!check(d->CreateShaderModule(d->device, &module, NULL, &s->vertex), "create vertex shader"))
		return 0;
	module.codeSize = sizeof(depth_frag);
	module.pCode = depth_frag;
	if (!check(d->CreateShaderModule(d->device, &module, NULL, &s->fragment), "create fragment shader"))
		return 0;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = s->vertex;
	stages[0].pName = "main";
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = s->fragment;
	stages[1].pName = "main";
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	viewport_state.viewportCount = viewport_state.scissorCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.pScissors = &scissor;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.lineWidth = 1;
	samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE;
	depth.depthCompareOp = VK_COMPARE_OP_LESS;
	blend_attachment.colorWriteMask = 15;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_attachment;
	layout.pushConstantRangeCount = 1;
	layout.pPushConstantRanges = &constants;
	if (!check(d->CreatePipelineLayout(d->device, &layout, NULL, &s->layout), "create pipeline layout"))
		return 0;
	pipeline.stageCount = 2;
	pipeline.pStages = stages;
	pipeline.pVertexInputState = &vertex;
	pipeline.pInputAssemblyState = &assembly;
	pipeline.pViewportState = &viewport_state;
	pipeline.pRasterizationState = &raster;
	pipeline.pMultisampleState = &samples;
	pipeline.pDepthStencilState = &depth;
	pipeline.pColorBlendState = &blend;
	pipeline.layout = s->layout;
	pipeline.renderPass = s->pass;
	return check(d->CreateGraphicsPipelines(d->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &s->pipeline), "create pipeline");
}

static int draw(struct test_scene *s, unsigned slot)
{
	struct halo_vk_device *d = &s->device;
	struct halo_vk_frame *frame = &d->frames[slot];
	struct test_frame *target = &s->frames[slot];
	VkClearValue clear[2];
	VkRenderPassBeginInfo pass = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	VkImageMemoryBarrier image = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	VkBufferMemoryBarrier host = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	VkBufferImageCopy copy = { 0 };
	float constants[8] = { 0.25f, 0, 0, 0, 1, 0, 0, 1 };
	unsigned query;

	if (!halo_vk_frame_begin(d, slot))
		return 0;
	memset(clear, 0, sizeof(clear));
	clear[1].depthStencil.depth = 1;
	pass.renderPass = s->pass;
	pass.framebuffer = target->framebuffer;
	pass.renderArea.extent.width = WIDTH;
	pass.renderArea.extent.height = HEIGHT;
	pass.clearValueCount = 2;
	pass.pClearValues = clear;
	d->CmdBeginRenderPass(frame->command, &pass, VK_SUBPASS_CONTENTS_INLINE);
	d->CmdBindPipeline(frame->command, VK_PIPELINE_BIND_POINT_GRAPHICS, s->pipeline);
	for (query = 0; query < HALO_VK_QUERIES_PER_FRAME; query++)
	{
		/* Red in front, then blue behind the same triangle. */
		if (query)
		{
			constants[0] = 0.75f;
			constants[4] = 0;
			constants[6] = 1;
		}
		d->CmdPushConstants(frame->command, s->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), constants);
		d->CmdBeginQuery(frame->command, d->queries, frame->query_first + query, VK_QUERY_CONTROL_PRECISE_BIT);
		d->CmdDraw(frame->command, 3, 1, 0, 0);
		d->CmdEndQuery(frame->command, d->queries, frame->query_first + query);
	}
	d->CmdEndRenderPass(frame->command);
	image.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	image.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	image.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	image.srcQueueFamilyIndex = image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image.image = target->color.image;
	image.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image.subresourceRange.levelCount = image.subresourceRange.layerCount = 1;
	d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &image);
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = WIDTH;
	copy.imageExtent.height = HEIGHT;
	copy.imageExtent.depth = 1;
	d->CmdCopyImageToBuffer(frame->command, target->color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target->readback, 1, &copy);
	host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	host.buffer = target->readback;
	host.size = VK_WHOLE_SIZE;
	d->CmdPipelineBarrier(frame->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0, 0, NULL, 1, &host, 0, NULL);
	return halo_vk_frame_submit(d, slot);
}

static void destroy_image(struct halo_vk_device *d, struct test_image *image)
{
	if (image->view) d->DestroyImageView(d->device, image->view, NULL);
	if (image->image) d->DestroyImage(d->device, image->image, NULL);
	if (image->memory) d->FreeMemory(d->device, image->memory, NULL);
}

static void dispose(struct test_scene *s)
{
	struct halo_vk_device *d = &s->device;
	unsigned i;
	if (d->device)
	{
		d->DeviceWaitIdle(d->device);
		for (i = 0; i < HALO_VK_FRAMES; i++)
		{
			struct test_frame *f = &s->frames[i];
			if (f->framebuffer) d->DestroyFramebuffer(d->device, f->framebuffer, NULL);
			destroy_image(d, &f->color);
			destroy_image(d, &f->depth);
			if (f->pixels) d->UnmapMemory(d->device, f->memory);
			if (f->readback) d->DestroyBuffer(d->device, f->readback, NULL);
			if (f->memory) d->FreeMemory(d->device, f->memory, NULL);
		}
		if (s->pipeline) d->DestroyPipeline(d->device, s->pipeline, NULL);
		if (s->layout) d->DestroyPipelineLayout(d->device, s->layout, NULL);
		if (s->vertex) d->DestroyShaderModule(d->device, s->vertex, NULL);
		if (s->fragment) d->DestroyShaderModule(d->device, s->fragment, NULL);
		if (s->pass) d->DestroyRenderPass(d->device, s->pass, NULL);
	}
}

static int verify(struct test_scene *s, unsigned slot)
{
	struct halo_vk_frame *frame = &s->device.frames[slot];
	unsigned char *pixels = s->frames[slot].pixels;
	unsigned center = (HEIGHT / 2 * WIDTH + WIDTH / 2) * 4;
	return frame->samples[0] > 0 && frame->samples[1] == 0 &&
		pixels[center] == 255 && pixels[center + 1] == 0 && pixels[center + 2] == 0 &&
		pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 0;
}

int main(int argc, char **argv)
{
	struct test_scene scene;
	unsigned cycle, slot, completed, polls = 0;
	int validation = 0, success = 0, status;
	time_t deadline;
	const char *capture = NULL;
	FILE *file;
	unsigned pixel;

	for (status = 1; status < argc; status++)
	{
		if (!strcmp(argv[status], "--validation")) validation = 1;
		else if (!strcmp(argv[status], "--capture") && status + 1 < argc) capture = argv[++status];
		else { fprintf(stderr, "usage: vulkan-smoke [--validation] [--capture image.ppm]\n"); return 2; }
	}
	memset(&scene, 0, sizeof(scene));
	if (!halo_vk_device_initialize(&scene.device, validation))
		return 1;
	if (!create_pass(&scene) || !create_pipeline(&scene))
		goto done;
	for (slot = 0; slot < HALO_VK_FRAMES; slot++)
		if (!create_frame(&scene, slot)) goto done;
	for (cycle = 0; cycle < 12; cycle++)
	{
		for (slot = 0; slot < HALO_VK_FRAMES; slot++)
			if (!draw(&scene, slot)) goto done;
		deadline = time(NULL) + 10;
		do
		{
			completed = 0;
			for (slot = 0; slot < HALO_VK_FRAMES; slot++)
			{
				status = halo_vk_frame_poll(&scene.device, slot);
				if (status < 0) goto done;
				if (status > 0)
				{
					if (!verify(&scene, slot)) { fprintf(stderr, "vulkan smoke: incorrect depth, pixels or query result\n"); goto done; }
					completed++;
				}
			}
			polls++;
			if (time(NULL) > deadline) { fprintf(stderr, "vulkan smoke: GPU completion timeout\n"); goto done; }
			if (completed != HALO_VK_FRAMES)
			{
#ifdef _WIN32
				Sleep(1);
#else
				usleep(1000);
#endif
			}
		} while (completed != HALO_VK_FRAMES);
	}
	if (capture)
	{
		file = fopen(capture, "wb");
		if (!file) goto done;
		fprintf(file, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
		for (pixel = 0; pixel < WIDTH * HEIGHT; pixel++)
			if (fwrite(scene.frames[0].pixels + pixel * 4, 1, 3, file) != 3) { fclose(file); goto done; }
		if (fclose(file)) goto done;
	}
	success = 1;
done:
	dispose(&scene);
	halo_vk_device_dispose(&scene.device);
	if (scene.device.validation_errors) success = 0;
	fprintf(stderr, "vulkan smoke: %s; cycles=%u polls=%u validation_errors=%u\n",
		success ? "PASS" : "FAIL", success ? 12u : 0u, polls, scene.device.validation_errors);
	return success ? 0 : 1;
}
