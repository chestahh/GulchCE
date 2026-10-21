#include "vulkan_scene.h"
#include "vulkan_device.h"
#include "vulkan_present.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCENE_IMAGES 4096
#define SCENE_SHADERS 1024
#define SCENE_PIPELINES 4096
#define SCENE_DRAWS 8192
#define SCENE_ARENA (64u * 1024u * 1024u)
struct scene_image
{
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	unsigned width, height, levels, faces, depth, identity, snapshot, volume, format;
};
struct scene_batch
{
	unsigned first, count, index, serial;
};
struct scene_frame
{
	VkQueryPool queries;
	unsigned query_count, batch_count;
	struct scene_batch batches[256];
	VkCommandBuffer command;
	VkFence fence;
	VkBuffer buffer;
	VkDeviceMemory memory;
	unsigned char *mapped;
	VkDescriptorPool descriptors;
	unsigned used, pending;
};
struct scene_pipeline
{
	struct halo_vk_raster state;
	unsigned vs, ps, depth;
	VkPipeline pipeline;
};
struct scene_framebuffer
{
	unsigned color, depth;
	VkFramebuffer framebuffer;
};
struct scene_sampler
{
	unsigned address[3], filter[3];
	float bias;
	VkSampler sampler;
};
static struct
{
	struct halo_vk_device *d;
	struct scene_frame frames[3];
	unsigned slot, attempted, recording, failed, image_count, shader_count, pipeline_count, framebuffer_count,
		sampler_count;
	struct scene_image images[SCENE_IMAGES];
	VkShaderModule shaders[SCENE_SHADERS];
	struct scene_pipeline pipelines[SCENE_PIPELINES];
	struct scene_framebuffer framebuffers[256];
	struct scene_sampler samplers[256];
	VkRenderPass passes[2];
	VkDescriptorSetLayout bindings;
	VkPipelineLayout layout;
	SDL_SharedObject *compiler;
	int (*compile)(int, const char *, unsigned *, unsigned, char *, unsigned);
	void (*compiler_close)(void);
	uint64_t image_bytes;
	unsigned draws, submitted, query_active, query_first, serial;
	struct
	{
		unsigned serial, pixels;
	} results[2048];
} scene;

void halo_vk_scene_error(const char *reason)
{
	if (!scene.failed)
		fprintf(stderr, "vulkan scene: %s\n", reason);
	scene.failed = 1;
}
int halo_vk_scene_owned(void)
{
	return scene.d != NULL;
}
int halo_vk_scene_failed(void)
{
	return scene.failed != 0;
}
static int check(VkResult result, const char *operation)
{
	if (result == VK_SUCCESS)
		return 1;
	fprintf(stderr, "vulkan scene: %s (%d)\n", operation, (int)result);
	scene.failed = 1;
	return 0;
}
#define CHECK(call)                                                                                          \
	do                                                                                                       \
	{                                                                                                        \
		if (!check((call), #call))                                                                           \
			return 0;                                                                                        \
	} while (0)
static void barrier(void)
{
	VkMemoryBarrier b = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	b.dstAccessMask = b.srcAccessMask;
	scene.d->CmdPipelineBarrier(scene.frames[scene.slot].command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &b, 0, NULL, 0, NULL);
}
static int collect_queries(struct scene_frame *f)
{
	uint64_t results[2048];
	unsigned i, j;
	if (f->query_count)
		CHECK(scene.d->GetQueryPoolResults(scene.d->device, f->queries, 0, f->query_count,
										   f->query_count * 16, results, 16,
										   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
	for (i = 0; i < f->batch_count; i++)
	{
		struct scene_batch *b = &f->batches[i];
		uint64_t sum = 0;
		for (j = b->first; j < b->first + b->count; j++)
		{
			if (!results[j * 2 + 1])
			{
				halo_vk_scene_error("completed query unavailable");
				return 0;
			}
			sum += results[j * 2];
		}
		if (b->serial > scene.results[b->index].serial)
		{
			scene.results[b->index].serial = b->serial;
			scene.results[b->index].pixels = sum > UINT32_MAX ? UINT32_MAX : (unsigned)sum;
		}
	}
	return 1;
}
void halo_vk_scene_query_begin(void)
{
	if (!halo_vk_scene_begin())
		return;
	if (scene.query_active)
	{
		halo_vk_scene_error("nested visibility query");
		return;
	}
	scene.query_active = 1;
	scene.query_first = scene.frames[scene.slot].query_count;
}
int halo_vk_scene_query_end(unsigned index)
{
	struct scene_frame *f = &scene.frames[scene.slot];
	struct scene_batch *b;
	if (!scene.recording)
		return 1;
	if (!scene.query_active || index >= 2048 || f->batch_count == 256)
	{
		halo_vk_scene_error("invalid visibility query end");
		return 0;
	}
	scene.query_active = 0;
	b = &f->batches[f->batch_count++];
	b->first = scene.query_first;
	b->count = f->query_count - b->first;
	b->index = index;
	b->serial = ++scene.serial;
	return 1;
}
int halo_vk_scene_query_result(unsigned index, unsigned *pixels)
{
	unsigned i;
	if (index >= 2048)
		return 0;
	for (i = 0; i < 3; i++)
	{
		struct scene_frame *f = &scene.frames[i];
		if (f->pending)
		{
			VkResult result = scene.d->GetFenceStatus(scene.d->device, f->fence);
			if (result == VK_NOT_READY)
				continue;
			CHECK(result);
			if (!collect_queries(f))
				return 0;
			f->pending = 0;
		}
	}
	if (!scene.results[index].serial)
		return 0;
	if (pixels)
		*pixels = scene.results[index].pixels;
	return 1;
}
int halo_vk_scene_begin(void)
{
	struct scene_frame *f = &scene.frames[scene.slot];
	struct halo_vk_device *d = scene.d;
	VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	VkResult result;
	if (scene.failed)
		return 0;
	if (scene.attempted)
		return scene.recording;
	scene.attempted = 1;
	if (f->pending)
	{
		result = d->GetFenceStatus(d->device, f->fence);
		if (result == VK_NOT_READY)
			return 0;
		CHECK(result);
		if (!collect_queries(f))
			return 0;
		f->pending = 0;
	}
	CHECK(d->ResetCommandBuffer(f->command, 0));
	CHECK(d->ResetDescriptorPool(d->device, f->descriptors, 0));
	f->used = 0;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CHECK(d->BeginCommandBuffer(f->command, &begin));
	scene.recording = 1;
	f->query_count = f->batch_count = 0;
	d->CmdResetQueryPool(f->command, f->queries, 0, 1024);
	barrier();
	return 1;
}
static unsigned upload(const void *bytes, unsigned size, unsigned alignment)
{
	struct scene_frame *f = &scene.frames[scene.slot];
	unsigned offset = (f->used + alignment - 1) & ~(alignment - 1);
	if (offset > SCENE_ARENA || size > SCENE_ARENA - offset)
	{
		halo_vk_scene_error("frame upload arena exhausted");
		return UINT32_MAX;
	}
	memcpy(f->mapped + offset, bytes, size);
	f->used = offset + size;
	return offset;
}
static int make_buffer(struct scene_frame *f)
{
	struct halo_vk_device *d = scene.d;
	VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	VkMemoryRequirements req;
	VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	info.size = SCENE_ARENA;
	info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	CHECK(d->CreateBuffer(d->device, &info, NULL, &f->buffer));
	d->GetBufferMemoryRequirements(d->device, f->buffer, &req);
	allocation.allocationSize = req.size;
	if (!halo_vk_memory_type(d, req.memoryTypeBits,
							 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							 &allocation.memoryTypeIndex))
	{
		halo_vk_scene_error("coherent upload memory unavailable");
		return 0;
	}
	CHECK(d->AllocateMemory(d->device, &allocation, NULL, &f->memory));
	CHECK(d->BindBufferMemory(d->device, f->buffer, f->memory, 0));
	CHECK(d->MapMemory(d->device, f->memory, 0, SCENE_ARENA, 0, (void **)&f->mapped));
	return 1;
}
static int make_pass(unsigned depth)
{
	struct halo_vk_device *d = scene.d;
	VkAttachmentDescription a[2];
	VkAttachmentReference color = {0, VK_IMAGE_LAYOUT_GENERAL}, z = {1, VK_IMAGE_LAYOUT_GENERAL};
	VkSubpassDescription sub = {0};
	VkRenderPassCreateInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	unsigned i;
	memset(a, 0, sizeof(a));
	for (i = 0; i < 2; i++)
	{
		a[i].format = i ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_R8G8B8A8_UNORM;
		a[i].samples = VK_SAMPLE_COUNT_1_BIT;
		a[i].loadOp = a[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		a[i].storeOp = a[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		a[i].initialLayout = a[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &color;
	sub.pDepthStencilAttachment = depth ? &z : NULL;
	info.attachmentCount = depth ? 2 : 1;
	info.pAttachments = a;
	info.subpassCount = 1;
	info.pSubpasses = &sub;
	CHECK(d->CreateRenderPass(d->device, &info, NULL, &scene.passes[depth]));
	return 1;
}
int halo_vk_scene_open(struct halo_vk_device *d)
{
	VkDescriptorSetLayoutBinding bindings[5];
	VkDescriptorSetLayoutCreateInfo set = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	VkPipelineLayoutCreateInfo layout = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VkCommandBufferAllocateInfo command = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkQueryPoolCreateInfo queries = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SCENE_DRAWS},
									 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SCENE_DRAWS * 4}};
	VkDescriptorPoolCreateInfo pool = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	char path[1024];
	unsigned i;
	memset(&scene, 0, sizeof(scene));
	scene.d = d;
	snprintf(path, sizeof(path), "%s%s", SDL_GetBasePath(),
#ifdef _WIN32
			 "halo_vulkan_compiler.dll"
#else
			 "libhalo_vulkan_compiler.so"
#endif
	);
	scene.compiler = SDL_LoadObject(path);
	if (!scene.compiler)
	{
		halo_vk_scene_error(SDL_GetError());
		return 0;
	}
	scene.compile = (int (*)(int, const char *, unsigned *, unsigned, char *, unsigned))SDL_LoadFunction(
		scene.compiler, "halo_compile_spirv");
	scene.compiler_close = (void (*)(void))SDL_LoadFunction(scene.compiler, "halo_compiler_shutdown");
	if (!scene.compile || !scene.compiler_close)
	{
		halo_vk_scene_error("shader compiler exports unavailable");
		return 0;
	}
	memset(bindings, 0, sizeof(bindings));
	for (i = 0; i < 5; i++)
	{
		bindings[i].binding = i;
		bindings[i].descriptorCount = 1;
		bindings[i].descriptorType =
			i ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[i].stageFlags =
			i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	set.bindingCount = 5;
	set.pBindings = bindings;
	CHECK(d->CreateDescriptorSetLayout(d->device, &set, NULL, &scene.bindings));
	layout.setLayoutCount = 1;
	layout.pSetLayouts = &scene.bindings;
	CHECK(d->CreatePipelineLayout(d->device, &layout, NULL, &scene.layout));
	if (!make_pass(0) || !make_pass(1))
		return 0;
	command.commandPool = d->commands;
	command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command.commandBufferCount = 1;
	queries.queryType = VK_QUERY_TYPE_OCCLUSION;
	queries.queryCount = 1024;
	pool.maxSets = SCENE_DRAWS;
	pool.poolSizeCount = 2;
	pool.pPoolSizes = sizes;
	for (i = 0; i < 3; i++)
	{
		struct scene_frame *f = &scene.frames[i];
		CHECK(d->CreateQueryPool(d->device, &queries, NULL, &f->queries));
		CHECK(d->AllocateCommandBuffers(d->device, &command, &f->command));
		CHECK(d->CreateFence(d->device, &fence, NULL, &f->fence));
		CHECK(d->CreateDescriptorPool(d->device, &pool, NULL, &f->descriptors));
		if (!make_buffer(f))
			return 0;
	}
	return 1;
}
uint32_t halo_vk_scene_shader(const char *glsl, int fragment)
{
	VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	char error[4096] = {0}, *source = halo_vk_shader_interface(glsl, fragment);
	unsigned *words = malloc(1024 * 1024);
	int count;
	unsigned id = scene.shader_count + 1;
	if (!source || !words || id >= SCENE_SHADERS)
	{
		free(source);
		free(words);
		halo_vk_scene_error("shader capacity/allocation");
		return 0;
	}
	count = scene.compile(fragment, source, words, 256 * 1024, error, sizeof(error));
	if (!count)
	{
		FILE *file = fopen("vulkan-failed-shader.glsl", "wb");
		if (file)
		{
			fputs(source, file);
			fclose(file);
		}
		fprintf(stderr, "vulkan scene: shader compilation: %s\n", error);
		halo_vk_scene_error("shader translation failed; source saved as vulkan-failed-shader.glsl");
	}
	free(source);
	info.pCode = words;
	info.codeSize = count > 0 ? (size_t)count * 4 : 0;
	if (count && !check(scene.d->CreateShaderModule(scene.d->device, &info, NULL, &scene.shaders[id]),
						"shader module"))
		count = 0;
	free(words);
	if (!count)
		return 0;
	scene.shader_count = id;
	return id;
}
static unsigned make_image(unsigned width, unsigned height, unsigned depth, unsigned levels, unsigned faces,
						   VkFormat format, int target, int is_depth)
{
	struct halo_vk_device *d = scene.d;
	struct scene_image *image;
	VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	VkMemoryAllocateInfo memory = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkMemoryRequirements req;
	VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	VkFormatProperties properties;
	unsigned maximum, max_levels = 1;
	unsigned id = scene.image_count + 1;
	if (!width || !height || width > 4096 || height > 4096 || !depth || depth > 512 || levels > 16 ||
		!levels || (faces != 1 && faces != 6) || id >= SCENE_IMAGES)
	{
		halo_vk_scene_error("invalid image extent or image capacity");
		return 0;
	}
	maximum = width > height ? width : height;
	if (depth > maximum)
		maximum = depth;
	while (maximum > 1)
	{
		maximum >>= 1;
		max_levels++;
	}
	if (levels > max_levels || (faces == 6 && (width != height || depth != 1)))
	{
		halo_vk_scene_error("invalid mip or cube extent");
		return 0;
	}
	d->GetPhysicalDeviceFormatProperties(d->physical, format, &properties);
	if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
	{
		halo_vk_scene_error("unsupported sampled image format");
		return 0;
	}
	image = &scene.images[id];
	scene.image_count = id;
	info.flags = faces == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	info.imageType = depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = depth;
	info.mipLevels = levels;
	info.arrayLayers = faces;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.usage =
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (target)
		info.usage |=
			is_depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	CHECK(d->CreateImage(d->device, &info, NULL, &image->image));
	d->GetImageMemoryRequirements(d->device, image->image, &req);
	if (req.size > 512u * 1024u * 1024u - scene.image_bytes)
	{
		halo_vk_scene_error("scene image memory budget");
		return 0;
	}
	scene.image_bytes += req.size;
	memory.allocationSize = req.size;
	if (!halo_vk_memory_type(d, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 &memory.memoryTypeIndex))
	{
		halo_vk_scene_error("device image memory unavailable");
		return 0;
	}
	CHECK(d->AllocateMemory(d->device, &memory, NULL, &image->memory));
	CHECK(d->BindImageMemory(d->device, image->image, image->memory, 0));
	view.image = image->image;
	view.format = format;
	view.viewType = faces == 6	? VK_IMAGE_VIEW_TYPE_CUBE
					: depth > 1 ? VK_IMAGE_VIEW_TYPE_3D
								: VK_IMAGE_VIEW_TYPE_2D;
	view.subresourceRange.aspectMask =
		is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.levelCount = levels;
	view.subresourceRange.layerCount = faces;
	CHECK(d->CreateImageView(d->device, &view, NULL, &image->view));
	b.image = image->image;
	b.subresourceRange = view.subresourceRange;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	d->CmdPipelineBarrier(scene.frames[scene.slot].command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
	image->width = width;
	image->height = height;
	image->volume = depth;
	image->format = (unsigned)format;
	image->depth = is_depth;
	image->levels = levels;
	image->faces = faces;
	return id;
}
/* Validate the complete byte span before copying any caller-owned storage. */
static int texture_upload_valid(const struct halo_vk_texture_upload *u, unsigned existing)
{
	unsigned face, level, maximum, levels = 1, block;
	if (!u || !u->pixels || !u->bytes || u->bytes > 32u * 1024u * 1024u || !u->width || !u->height ||
		!u->depth || u->width > 4096 || u->height > 4096 || u->depth > 512 || !u->levels || u->levels > 16 ||
		(u->faces != 1 && u->faces != 6) || (u->faces == 6 && (u->width != u->height || u->depth != 1)))
		return 0;
	maximum = u->width > u->height ? u->width : u->height;
	if (u->depth > maximum)
		maximum = u->depth;
	while (maximum > 1)
	{
		maximum >>= 1;
		levels++;
	}
	if (u->levels > levels)
		return 0;
	if (u->format == VK_FORMAT_R8G8B8A8_UNORM || u->format == VK_FORMAT_B8G8R8A8_UNORM)
		block = 4;
	else if (u->format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
		block = 8;
	else if (u->format == VK_FORMAT_BC2_UNORM_BLOCK || u->format == VK_FORMAT_BC3_UNORM_BLOCK)
		block = 16;
	else
		return 0;
	if (existing)
	{
		const struct scene_image *image;
		if (existing > scene.image_count || existing >= SCENE_IMAGES)
			return 0;
		image = &scene.images[existing];
		if (!image->image || image->identity || image->depth || image->width != u->width ||
			image->height != u->height || image->volume != u->depth || image->levels != u->levels ||
			image->faces != u->faces || image->format != u->format)
			return 0;
	}
	for (face = 0; face < u->faces; face++)
		for (level = 0; level < u->levels; level++)
		{
			unsigned w = u->width >> level, h = u->height >> level, d = u->depth >> level;
			uint64_t bytes;
			if (!w)
				w = 1;
			if (!h)
				h = 1;
			if (!d)
				d = 1;
			if (block != 4)
			{
				w = (w + 3) / 4;
				h = (h + 3) / 4;
			}
			bytes = (uint64_t)w * h * d * block;
			if (u->offsets[face][level] % block || (uint64_t)u->offsets[face][level] + bytes > u->bytes)
				return 0;
		}
	return 1;
}
uint32_t halo_vk_scene_texture_update(const struct halo_vk_texture_upload *u, uint32_t existing)
{
	VkBufferImageCopy copies[96];
	unsigned image, offset, face, level, count = 0;
	if (!texture_upload_valid(u, existing))
	{
		halo_vk_scene_error("invalid texture upload");
		return 0;
	}
	if (!halo_vk_scene_begin())
		return 0;
	offset = upload(u->pixels, u->bytes, 16);
	if (offset == UINT32_MAX)
		return 0;
	image = existing
				? existing
				: make_image(u->width, u->height, u->depth, u->levels, u->faces, (VkFormat)u->format, 0, 0);
	barrier();
	if (!image)
		return 0;
	memset(copies, 0, sizeof(copies));
	for (face = 0; face < u->faces; face++)
		for (level = 0; level < u->levels; level++)
		{
			VkBufferImageCopy *c = &copies[count++];
			if (u->offsets[face][level] >= u->bytes)
			{
				halo_vk_scene_error("texture offset outside upload");
				return 0;
			}
			c->bufferOffset = offset + u->offsets[face][level];
			c->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			c->imageSubresource.mipLevel = level;
			c->imageSubresource.baseArrayLayer = face;
			c->imageSubresource.layerCount = 1;
			c->imageExtent.width = u->width >> level;
			if (!c->imageExtent.width)
				c->imageExtent.width = 1;
			c->imageExtent.height = u->height >> level;
			if (!c->imageExtent.height)
				c->imageExtent.height = 1;
			c->imageExtent.depth = u->depth >> level;
			if (!c->imageExtent.depth)
				c->imageExtent.depth = 1;
		}
	scene.d->CmdCopyBufferToImage(scene.frames[scene.slot].command, scene.frames[scene.slot].buffer,
								  scene.images[image].image, VK_IMAGE_LAYOUT_GENERAL, count, copies);
	barrier();
	return image;
}
uint32_t halo_vk_scene_target(uint32_t identity, unsigned width, unsigned height, int depth)
{
	unsigned i;
	if (!identity || !halo_vk_scene_begin())
		return 0;
	for (i = 1; i <= scene.image_count; i++)
		if (scene.images[i].identity == identity && scene.images[i].width == width &&
			scene.images[i].height == height && scene.images[i].depth == (unsigned)depth)
			return i;
	i = make_image(width, height, 1, 1, 1, depth ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_R8G8B8A8_UNORM, 1,
				   depth);
	if (i)
		scene.images[i].identity = identity;
	return i;
}
static VkFramebuffer framebuffer(unsigned color, unsigned depth)
{
	VkFramebufferCreateInfo info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	VkImageView views[2];
	unsigned i;
	for (i = 0; i < scene.framebuffer_count; i++)
		if (scene.framebuffers[i].color == color && scene.framebuffers[i].depth == depth)
			return scene.framebuffers[i].framebuffer;
	if (i >= 256)
	{
		halo_vk_scene_error("framebuffer capacity");
		return VK_NULL_HANDLE;
	}
	views[0] = scene.images[color].view;
	views[1] = scene.images[depth].view;
	info.renderPass = scene.passes[depth != 0];
	info.attachmentCount = depth ? 2 : 1;
	info.pAttachments = views;
	info.width = scene.images[color].width;
	info.height = scene.images[color].height;
	info.layers = 1;
	if (depth && (scene.images[depth].width != info.width || scene.images[depth].height != info.height))
	{
		halo_vk_scene_error("target size mismatch");
		return VK_NULL_HANDLE;
	}
	if (!check(scene.d->CreateFramebuffer(scene.d->device, &info, NULL, &scene.framebuffers[i].framebuffer),
			   "framebuffer"))
		return VK_NULL_HANDLE;
	scene.framebuffers[i].color = color;
	scene.framebuffers[i].depth = depth;
	scene.framebuffer_count++;
	return scene.framebuffers[i].framebuffer;
}
static int begin_pass(unsigned color, unsigned depth)
{
	VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	if (!color || color > scene.image_count || depth > scene.image_count)
	{
		halo_vk_scene_error("missing draw target");
		return 0;
	}
	info.framebuffer = framebuffer(color, depth);
	if (!info.framebuffer)
		return 0;
	info.renderPass = scene.passes[depth != 0];
	info.renderArea.extent.width = scene.images[color].width;
	info.renderArea.extent.height = scene.images[color].height;
	barrier();
	scene.d->CmdBeginRenderPass(scene.frames[scene.slot].command, &info, VK_SUBPASS_CONTENTS_INLINE);
	return 1;
}
int halo_vk_scene_clear(uint32_t color, uint32_t depth, unsigned flags, const float rgba[4], float z,
						unsigned stencil, const int *rectangles, unsigned count)
{
	VkClearAttachment attachments[2];
	VkClearRect rect;
	unsigned n = 0, i;
	if (!halo_vk_scene_begin())
		return !scene.failed;
	memset(attachments, 0, sizeof(attachments));
	if (flags & 1)
	{
		attachments[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		memcpy(attachments[n++].clearValue.color.float32, rgba, 16);
	}
	if (depth && (flags & 6))
	{
		attachments[n].aspectMask =
			((flags & 2) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | ((flags & 4) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		attachments[n].clearValue.depthStencil.depth = z;
		attachments[n++].clearValue.depthStencil.stencil = stencil;
	}
	if (!n)
		return 1;
	if (!begin_pass(color, depth))
		return 0;
	memset(&rect, 0, sizeof(rect));
	rect.layerCount = 1;
	for (i = 0; i < (count ? count : 1); i++)
	{
		int x0 = count ? rectangles[4 * i] : 0, y0 = count ? rectangles[4 * i + 1] : 0;
		int x1 = count ? rectangles[4 * i + 2] : (int)scene.images[color].width,
			y1 = count ? rectangles[4 * i + 3] : (int)scene.images[color].height;
		if (x0 < 0)
			x0 = 0;
		if (y0 < 0)
			y0 = 0;
		if (x1 > (int)scene.images[color].width)
			x1 = scene.images[color].width;
		if (y1 > (int)scene.images[color].height)
			y1 = scene.images[color].height;
		if (x1 <= x0 || y1 <= y0)
			continue;
		rect.rect.offset.x = x0;
		rect.rect.offset.y = y0;
		rect.rect.extent.width = x1 - x0;
		rect.rect.extent.height = y1 - y0;
		scene.d->CmdClearAttachments(scene.frames[scene.slot].command, n, attachments, 1, &rect);
	}
	scene.d->CmdEndRenderPass(scene.frames[scene.slot].command);
	return 1;
}

static VkPipeline pipeline(const struct halo_vk_draw *draw)
{
	struct halo_vk_device *d = scene.d;
	const struct halo_vk_raster *r = &draw->raster;
	struct scene_pipeline *entry;
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription binding = {0, 256, VK_VERTEX_INPUT_RATE_VERTEX};
	VkVertexInputAttributeDescription attributes[16];
	VkPipelineVertexInputStateCreateInfo vertex = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	VkPipelineInputAssemblyStateCreateInfo assembly = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	VkPipelineViewportStateCreateInfo viewport = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	VkPipelineRasterizationStateCreateInfo raster = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	VkPipelineMultisampleStateCreateInfo samples = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	VkPipelineDepthStencilStateCreateInfo depth = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	VkPipelineColorBlendAttachmentState attachment = {0};
	VkPipelineColorBlendStateCreateInfo blend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
							   VK_DYNAMIC_STATE_BLEND_CONSTANTS};
	VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	VkGraphicsPipelineCreateInfo info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	unsigned i;
	for (i = 0; i < scene.pipeline_count; i++)
	{
		entry = &scene.pipelines[i];
		if (entry->vs == draw->vertex_shader && entry->ps == draw->pixel_shader &&
			entry->depth == (draw->depth != 0) && !memcmp(&entry->state, r, sizeof(*r)))
			return entry->pipeline;
	}
	if (i >= SCENE_PIPELINES || !draw->vertex_shader || !draw->pixel_shader ||
		draw->vertex_shader > scene.shader_count || draw->pixel_shader > scene.shader_count)
	{
		halo_vk_scene_error("pipeline capacity or shader identity");
		return VK_NULL_HANDLE;
	}
	entry = &scene.pipelines[i];
	entry->vs = draw->vertex_shader;
	entry->ps = draw->pixel_shader;
	entry->depth = draw->depth != 0;
	entry->state = *r;
	memset(stages, 0, sizeof(stages));
	for (i = 0; i < 2; i++)
	{
		stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
		stages[i].module = scene.shaders[i ? draw->pixel_shader : draw->vertex_shader];
		stages[i].pName = "main";
	}
	for (i = 0; i < 16; i++)
	{
		attributes[i].location = i;
		attributes[i].binding = 0;
		attributes[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributes[i].offset = i * 16;
	}
	vertex.vertexBindingDescriptionCount = 1;
	vertex.pVertexBindingDescriptions = &binding;
	vertex.vertexAttributeDescriptionCount = 16;
	vertex.pVertexAttributeDescriptions = attributes;
	assembly.topology = (VkPrimitiveTopology)r->topology;
	viewport.viewportCount = viewport.scissorCount = 1;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.lineWidth = 1;
	/* The negative viewport preserves D3D clip-space Y; cull names refer to
	   the screen winding to discard, independent of two-sided shading. */
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.cullMode = (VkCullModeFlags)r->cull;
	raster.depthBiasEnable = r->bias;
	raster.depthBiasSlopeFactor = r->bias_slope;
	raster.depthBiasConstantFactor = r->bias_units;
	samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	depth.depthTestEnable = draw->depth && r->depth_test;
	depth.depthWriteEnable = depth.depthTestEnable && r->depth_write;
	depth.depthCompareOp = (VkCompareOp)r->depth_function;
	depth.stencilTestEnable = draw->depth && r->stencil;
	depth.front.compareOp = (VkCompareOp)r->stencil_function;
	depth.front.failOp = (VkStencilOp)r->stencil_fail;
	depth.front.depthFailOp = (VkStencilOp)r->stencil_zfail;
	depth.front.passOp = (VkStencilOp)r->stencil_pass;
	depth.front.reference = r->stencil_ref;
	depth.front.compareMask = r->stencil_read;
	depth.front.writeMask = r->stencil_write;
	depth.back = depth.front;
	attachment.blendEnable = r->blend;
	attachment.colorWriteMask = r->color_mask;
	attachment.srcColorBlendFactor = attachment.srcAlphaBlendFactor = (VkBlendFactor)r->source_blend;
	attachment.dstColorBlendFactor = attachment.dstAlphaBlendFactor = (VkBlendFactor)r->dest_blend;
	attachment.colorBlendOp = attachment.alphaBlendOp = (VkBlendOp)r->blend_op;
	blend.attachmentCount = 1;
	blend.pAttachments = &attachment;
	dynamic.dynamicStateCount = 3;
	dynamic.pDynamicStates = states;
	info.stageCount = 2;
	info.pStages = stages;
	info.pVertexInputState = &vertex;
	info.pInputAssemblyState = &assembly;
	info.pViewportState = &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &samples;
	info.pDepthStencilState = &depth;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = scene.layout;
	info.renderPass = scene.passes[draw->depth != 0];
	if (!check(d->CreateGraphicsPipelines(d->device, VK_NULL_HANDLE, 1, &info, NULL, &entry->pipeline),
			   "graphics pipeline"))
		return VK_NULL_HANDLE;
	scene.pipeline_count++;
	return entry->pipeline;
}
static VkSampler sampler(const struct halo_vk_draw *draw, unsigned stage)
{
	VkSamplerCreateInfo info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	struct scene_sampler *s;
	VkSamplerAddressMode address[3];
	unsigned i;
	for (i = 0; i < scene.sampler_count; i++)
	{
		s = &scene.samplers[i];
		if (!memcmp(s->address, draw->address[stage], sizeof(s->address)) &&
			!memcmp(s->filter, draw->filter[stage], sizeof(s->filter)) && s->bias == draw->lod_bias[stage])
			return s->sampler;
	}
	if (i >= 256)
	{
		halo_vk_scene_error("sampler capacity");
		return VK_NULL_HANDLE;
	}
	s = &scene.samplers[i];
	memcpy(s->address, draw->address[stage], sizeof(s->address));
	memcpy(s->filter, draw->filter[stage], sizeof(s->filter));
	s->bias = draw->lod_bias[stage];
	for (i = 0; i < 3; i++)
		address[i] = s->address[i] == 2	  ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
					 : s->address[i] == 3 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
					 : s->address[i] == 4 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
										  : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeU = address[0];
	info.addressModeV = address[1];
	info.addressModeW = address[2];
	info.magFilter = s->filter[0] > 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.minFilter = s->filter[1] > 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = s->filter[2] > 1 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.maxLod = s->filter[2] ? 16 : 0;
	info.mipLodBias = s->bias;
	if (info.mipLodBias > scene.d->properties.limits.maxSamplerLodBias)
		info.mipLodBias = scene.d->properties.limits.maxSamplerLodBias;
	if (info.mipLodBias < -scene.d->properties.limits.maxSamplerLodBias)
		info.mipLodBias = -scene.d->properties.limits.maxSamplerLodBias;
	info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	if (!check(scene.d->CreateSampler(scene.d->device, &info, NULL, &s->sampler), "sampler"))
		return VK_NULL_HANDLE;
	scene.sampler_count++;
	return s->sampler;
}
int halo_vk_scene_draw_indexed(const struct halo_vk_draw *draw, const float *vertices, unsigned count,
							   const unsigned *indices, unsigned index_count)
{
	struct halo_vk_device *d = scene.d;
	struct scene_frame *f = &scene.frames[scene.slot];
	VkDescriptorSetAllocateInfo allocation = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	VkDescriptorSet set;
	VkDescriptorBufferInfo uniform;
	VkDescriptorImageInfo textures[4];
	VkWriteDescriptorSet writes[5];
	VkViewport viewport;
	VkRect2D scissor;
	VkDeviceSize offset;
	VkPipeline p;
	unsigned i, vertex_offset, uniform_offset, index_offset = 0;
	if (!count || !halo_vk_scene_begin())
		return !scene.failed;
	if (count > SCENE_ARENA / 256)
	{
		halo_vk_scene_error("draw vertex limit");
		return 0;
	}
	vertex_offset = upload(vertices, count * 256, 16);
	if (indices)
		index_offset = upload(indices, index_count * 4, 4);
	uniform_offset = upload(&draw->uniforms, sizeof(draw->uniforms),
							(unsigned)d->properties.limits.minUniformBufferOffsetAlignment);
	if (scene.failed)
		return 0;
	p = pipeline(draw);
	if (!p)
		return 0;
	memset(writes, 0, sizeof(writes));
	memset(textures, 0, sizeof(textures));
	allocation.descriptorPool = f->descriptors;
	allocation.descriptorSetCount = 1;
	allocation.pSetLayouts = &scene.bindings;
	CHECK(d->AllocateDescriptorSets(d->device, &allocation, &set));
	uniform.buffer = f->buffer;
	uniform.offset = uniform_offset;
	uniform.range = sizeof(draw->uniforms);
	for (i = 0; i < 5; i++)
	{
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType =
			i ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		if (i)
		{
			unsigned id = draw->textures[i - 1];
			if (!id || id > scene.image_count || id == draw->depth)
			{
				halo_vk_scene_error("invalid texture or depth feedback");
				return 0;
			}
			if (id == draw->color)
			{
				struct scene_image *source = &scene.images[id];
				VkImageCopy copy = {0};
				if (!source->snapshot)
					source->snapshot =
						make_image(source->width, source->height, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, 0, 0);
				if (!source->snapshot)
					return 0;
				id = source->snapshot;
				copy.srcSubresource.aspectMask = copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copy.srcSubresource.layerCount = copy.dstSubresource.layerCount = 1;
				copy.extent.width = source->width;
				copy.extent.height = source->height;
				copy.extent.depth = 1;
				barrier();
				d->CmdCopyImage(f->command, source->image, VK_IMAGE_LAYOUT_GENERAL, scene.images[id].image,
								VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
				barrier();
			}
			textures[i - 1].imageView = scene.images[id].view;
			textures[i - 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			textures[i - 1].sampler = sampler(draw, i - 1);
			if (!textures[i - 1].sampler)
				return 0;
			writes[i].pImageInfo = &textures[i - 1];
		}
		else
			writes[i].pBufferInfo = &uniform;
	}
	d->UpdateDescriptorSets(d->device, 5, writes, 0, NULL);
	if (!begin_pass(draw->color, draw->depth))
		return 0;
	viewport.x = draw->viewport[0];
	viewport.y = draw->viewport[1] + draw->viewport[3];
	viewport.width = draw->viewport[2];
	viewport.height = -draw->viewport[3];
	viewport.minDepth = draw->viewport[4];
	viewport.maxDepth = draw->viewport[5];
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = scene.images[draw->color].width;
	scissor.extent.height = scene.images[draw->color].height;
	d->CmdBindPipeline(f->command, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
	d->CmdSetViewport(f->command, 0, 1, &viewport);
	d->CmdSetScissor(f->command, 0, 1, &scissor);
	d->CmdSetBlendConstants(f->command, draw->blend_color);
	d->CmdBindDescriptorSets(f->command, VK_PIPELINE_BIND_POINT_GRAPHICS, scene.layout, 0, 1, &set, 0, NULL);
	offset = vertex_offset;
	d->CmdBindVertexBuffers(f->command, 0, 1, &f->buffer, &offset);
	if (scene.query_active)
	{
		if (f->query_count == 1024)
		{
			d->CmdEndRenderPass(f->command);
			halo_vk_scene_error("query capacity");
			return 0;
		}
		d->CmdBeginQuery(f->command, f->queries, f->query_count, VK_QUERY_CONTROL_PRECISE_BIT);
	}
	if (indices)
	{
		d->CmdBindIndexBuffer(f->command, f->buffer, index_offset, VK_INDEX_TYPE_UINT32);
		d->CmdDrawIndexed(f->command, index_count, 1, 0, 0, 0);
	}
	else
		d->CmdDraw(f->command, count, 1, 0, 0);
	if (scene.query_active)
		d->CmdEndQuery(f->command, f->queries, f->query_count++);
	d->CmdEndRenderPass(f->command);
	scene.draws++;
	return 1;
}
int halo_vk_scene_finish(uint32_t back, struct halo_vk_present *presentation, const char *capture)
{
	struct halo_vk_device *d = scene.d;
	struct scene_frame *f = &scene.frames[scene.slot];
	VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	(void)capture;
	if (scene.query_active)
	{
		halo_vk_scene_error("visibility query crosses presentation");
		return 0;
	}
	if (scene.failed)
		return 0;
	if (scene.recording)
	{
		barrier();
		CHECK(d->EndCommandBuffer(f->command));
		CHECK(d->ResetFences(d->device, 1, &f->fence));
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &f->command;
		CHECK(d->QueueSubmit(d->queue, 1, &submit, f->fence));
		f->pending = 1;
		if (back && back <= scene.image_count)
		{
			presentation->source_image = scene.images[back].image;
			presentation->source_extent.width = scene.images[back].width;
			presentation->source_extent.height = scene.images[back].height;
		}
		scene.slot = (scene.slot + 1) % 3;
		scene.submitted++;
		if (!(scene.submitted % 120))
			fprintf(stderr, "vulkan scene: frames=%u draws=%u shaders=%u pipelines=%u images=%u\n",
					scene.submitted, scene.draws, scene.shader_count, scene.pipeline_count,
					scene.image_count);
	}
	scene.attempted = scene.recording = 0;
	return 1;
}
int halo_vk_scene_close(void)
{
	struct halo_vk_device *d = scene.d;
	unsigned i;
	int ok = !scene.failed;
	if (!d)
		return ok;
	for (i = 0; i < 3; i++)
		if (scene.frames[i].pending)
		{
			VkResult result = d->WaitForFences(d->device, 1, &scene.frames[i].fence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
				return 0;
		}
	fprintf(stderr, "vulkan scene: closed frames=%u draws=%u shaders=%u pipelines=%u images=%u result=%s\n",
			scene.submitted, scene.draws, scene.shader_count, scene.pipeline_count, scene.image_count,
			ok ? "PASS" : "FAIL");
	for (i = 0; i < scene.pipeline_count; i++)
		d->DestroyPipeline(d->device, scene.pipelines[i].pipeline, NULL);
	for (i = 0; i < scene.framebuffer_count; i++)
		d->DestroyFramebuffer(d->device, scene.framebuffers[i].framebuffer, NULL);
	for (i = 0; i < scene.sampler_count; i++)
		d->DestroySampler(d->device, scene.samplers[i].sampler, NULL);
	for (i = 1; i <= scene.shader_count; i++)
		d->DestroyShaderModule(d->device, scene.shaders[i], NULL);
	for (i = 1; i <= scene.image_count; i++)
	{
		if (scene.images[i].view)
			d->DestroyImageView(d->device, scene.images[i].view, NULL);
		if (scene.images[i].image)
			d->DestroyImage(d->device, scene.images[i].image, NULL);
		if (scene.images[i].memory)
			d->FreeMemory(d->device, scene.images[i].memory, NULL);
	}
	for (i = 0; i < 3; i++)
	{
		struct scene_frame *f = &scene.frames[i];
		if (f->command)
			d->FreeCommandBuffers(d->device, d->commands, 1, &f->command);
		if (f->mapped)
			d->UnmapMemory(d->device, f->memory);
		if (f->buffer)
			d->DestroyBuffer(d->device, f->buffer, NULL);
		if (f->memory)
			d->FreeMemory(d->device, f->memory, NULL);
		if (f->descriptors)
			d->DestroyDescriptorPool(d->device, f->descriptors, NULL);
		if (f->queries)
			d->DestroyQueryPool(d->device, f->queries, NULL);
		if (f->fence)
			d->DestroyFence(d->device, f->fence, NULL);
	}
	if (scene.layout)
		d->DestroyPipelineLayout(d->device, scene.layout, NULL);
	if (scene.bindings)
		d->DestroyDescriptorSetLayout(d->device, scene.bindings, NULL);
	for (i = 0; i < 2; i++)
		if (scene.passes[i])
			d->DestroyRenderPass(d->device, scene.passes[i], NULL);
	if (scene.compiler_close)
		scene.compiler_close();
	if (scene.compiler)
		SDL_UnloadObject(scene.compiler);
	memset(&scene, 0, sizeof(scene));
	return ok;
}

uint32_t halo_vk_scene_texture(const struct halo_vk_texture_upload *u)
{
	return halo_vk_scene_texture_update(u, 0);
}

int halo_vk_scene_draw(const struct halo_vk_draw *draw, const float *vertices, unsigned count)
{
	return halo_vk_scene_draw_indexed(draw, vertices, count, NULL, 0);
}
