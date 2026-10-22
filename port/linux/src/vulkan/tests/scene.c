/* Exercises the production scene renderer on a real GPU, without game assets. */
#include "../vulkan_scene.c"
#include "../../shield_honeycomb_gl.h"
#include <assert.h>

static const char vertex_source[] =
	"#version 330 core\nlayout(location=0) in vec4 v0;\nlayout(location=0) out vec2 uv;\n"
	"void main(){gl_Position=v0*c[191];uv=v0.xy*.5+.5;}\n";
static const char pixel_source[] = "#version 330 core\nuniform sampler2D tex0;\nlayout(location=0) in vec2 "
								   "uv;\nlayout(location=0) out vec4 color;\n"
								   "void main(){color=texture(tex0,vec2(.25,.25))*ps_c0[0];}\n";

static int capture(struct halo_vk_device *d, unsigned id, int honeycomb)
{
	struct scene_image *im = &scene.images[id];
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkBufferCreateInfo b = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	VkMemoryRequirements req;
	VkMemoryAllocateInfo a = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	VkBufferImageCopy copy = {0};
	VkMemoryBarrier barrier_info = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	VkFenceCreateInfo fc = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkCommandBuffer command;
	VkFence fence;
	unsigned char *pixels;
	unsigned i, minimum_alpha = 255, maximum_alpha = 0;
	b.size = 16 * 16 * 4;
	b.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	assert(d->CreateBuffer(d->device, &b, NULL, &buffer) == VK_SUCCESS);
	d->GetBufferMemoryRequirements(d->device, buffer, &req);
	a.allocationSize = req.size;
	assert(halo_vk_memory_type(d, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &a.memoryTypeIndex));
	assert(d->AllocateMemory(d->device, &a, NULL, &memory) == VK_SUCCESS);
	assert(d->BindBufferMemory(d->device, buffer, memory, 0) == VK_SUCCESS);
	ca.commandPool = d->commands;
	ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ca.commandBufferCount = 1;
	assert(d->AllocateCommandBuffers(d->device, &ca, &command) == VK_SUCCESS);
	assert(d->CreateFence(d->device, &fc, NULL, &fence) == VK_SUCCESS);
	assert(d->BeginCommandBuffer(command, &begin) == VK_SUCCESS);
	barrier_info.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	barrier_info.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	d->CmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
		&barrier_info, 0, NULL, 0, NULL);
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = copy.imageExtent.height = 16;
	copy.imageExtent.depth = 1;
	d->CmdCopyImageToBuffer(command, im->image, VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &copy);
	barrier_info.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier_info.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	d->CmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
		&barrier_info, 0, NULL, 0, NULL);
	assert(d->EndCommandBuffer(command) == VK_SUCCESS);
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command;
	assert(d->QueueSubmit(d->queue, 1, &submit, fence) == VK_SUCCESS);
	assert(d->WaitForFences(d->device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
	assert(d->MapMemory(d->device, memory, 0, b.size, 0, (void **)&pixels) == VK_SUCCESS);
	for (i = 0; i < 256; i++)
	{
		unsigned alpha = pixels[i * 4 + 3];
		assert(pixels[i * 4] == 0 && pixels[i * 4 + 1] == 255 && pixels[i * 4 + 2] == 0);
		if (!honeycomb)
			assert(alpha == 255);
		if (alpha < minimum_alpha) minimum_alpha = alpha;
		if (alpha > maximum_alpha) maximum_alpha = alpha;
	}
	if (honeycomb)
		assert(minimum_alpha > 0 && maximum_alpha > minimum_alpha);
	d->UnmapMemory(d->device, memory);
	d->DestroyFence(d->device, fence, NULL);
	d->DestroyBuffer(d->device, buffer, NULL);
	d->FreeMemory(d->device, memory, NULL);
	return 1;
}
int main(int argc, char **argv)
{
	struct halo_vk_device d;
	struct halo_vk_present presentation = {0};
	struct halo_vk_draw draw = {0};
	struct halo_vk_texture_upload upload_info = {0};
	unsigned colors[4] = {0xff00ff00, 0xff0000ff, 0xffff0000, 0xffffffff};
	unsigned indices[6] = {0, 1, 2, 0, 2, 3}, i, j, visible, hidden;
	float vertices[4][64] = {{0}}, clear[4] = {0, 0, 0, 1};
	const float xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
	int validation = argc > 1 && !strcmp(argv[1], "--validation");
	char honeycomb_pixel[16384];
	assert(halo_vk_device_initialize(&d, validation));
	assert(halo_vk_scene_open(&d));
	/* Explicit locations test the same interface adaptation used by Halo. */
	draw.vertex_shader = halo_vk_scene_shader(vertex_source, 0);
	draw.pixel_shader = halo_vk_scene_shader(pixel_source, 1);
	assert(draw.vertex_shader && draw.pixel_shader);
	upload_info.width = upload_info.height = 2;
	upload_info.depth = upload_info.levels = upload_info.faces = 1;
	upload_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	upload_info.pixels = colors;
	upload_info.bytes = 16;
	draw.color = halo_vk_scene_target(1, 16, 16, 0);
	draw.depth = halo_vk_scene_target(2, 16, 16, 1);
	draw.textures[0] = halo_vk_scene_texture(&upload_info);
	for (i = 0; i < 4; i++)
	{
		draw.textures[i] = draw.textures[0];
		for (j = 0; j < 3; j++)
			draw.address[i][j] = draw.filter[i][j] = 1;
	}
	draw.viewport[2] = draw.viewport[3] = 16;
	draw.viewport[5] = 1;
	for (i = 0; i < 4; i++)
	{
		draw.uniforms.c[191][i] = 1;
		draw.uniforms.ps_c0[0][i] = 1;
	}
	draw.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	draw.raster.depth_test = draw.raster.depth_write = 1;
	draw.raster.depth_function = VK_COMPARE_OP_LESS;
	draw.raster.color_mask = 15;
	for (i = 0; i < 12; i++)
	{
		while (!halo_vk_scene_begin())
		{
			assert(!halo_vk_scene_failed());
			assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
			SDL_Delay(1);
		}
		assert(halo_vk_scene_clear(draw.color, draw.depth, 3, clear, 1, 0, NULL, 0));
		for (j = 0; j < 4; j++)
		{
			vertices[j][0] = xy[j][0];
			vertices[j][1] = xy[j][1];
			vertices[j][2] = .5f;
			vertices[j][3] = 1;
		}
		halo_vk_scene_query_begin();
		assert(halo_vk_scene_draw_indexed(&draw, &vertices[0][0], 4, indices, 6));
		assert(halo_vk_scene_query_end(0));
		for (j = 0; j < 4; j++)
			vertices[j][2] = .8f;
		halo_vk_scene_query_begin();
		assert(halo_vk_scene_draw_indexed(&draw, &vertices[0][0], 4, indices, 6));
		assert(halo_vk_scene_query_end(1));
		assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
	}
	assert(capture(&d, draw.color, 0));
	assert(halo_vk_scene_query_result(0, &visible));
	assert(halo_vk_scene_query_result(1, &hidden));
	assert(visible == 256 && hidden == 0);
	/* Render-target feedback uses a distinct snapshot, never an image read
	and written in the same subpass. The captured color must survive. */
	while (!halo_vk_scene_begin())
	{
		assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
		SDL_Delay(1);
	}
	draw.textures[0] = draw.color;
	draw.raster.depth_test = 0;
	assert(halo_vk_scene_draw_indexed(&draw, &vertices[0][0], 4, indices, 6));
	assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
	assert(capture(&d, draw.color, 0));
	/* Compile the actual shield surface and verify its draw-local block and
	   matching varying on the GPU, including disable after an enabled draw. */
	draw.vertex_shader = halo_vk_scene_shader(
		"layout(location=0) in vec4 v0;\nout vec4 xD0;\nout vec2 shield_uv;\n"
		"void main(){gl_Position=v0; xD0=vec4(1); shield_uv=v0.xy;}\n", 0);
	snprintf(honeycomb_pixel, sizeof(honeycomb_pixel),
		"in vec4 xD0;\n%s\nlayout(location=0) out vec4 color;\n"
		"void main(){color=shield_honeycomb_surface(vec4(0,1,0,1));}\n", shield_honeycomb_glsl);
	draw.pixel_shader = halo_vk_scene_shader(honeycomb_pixel, 1);
	assert(draw.vertex_shader && draw.pixel_shader);
	draw.uniforms.shield_honeycomb[1][3] = 3;
	draw.uniforms.shield_honeycomb[2][1] = 1;
	draw.uniforms.shield_honeycomb[2][3] = 1;
	for (i = 0; i < 3; i++)
	{
		while (!halo_vk_scene_begin())
		{
			assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
			SDL_Delay(1);
		}
		draw.uniforms.shield_honeycomb[0][3] = i == 1 ? 5 : 0;
		assert(halo_vk_scene_draw_indexed(&draw, &vertices[0][0], 4, indices, 6));
		assert(halo_vk_scene_finish(draw.color, &presentation, NULL));
		assert(capture(&d, draw.color, i == 1));
	}
	assert(halo_vk_scene_close());
	halo_vk_device_dispose(&d);
	assert(!d.validation_errors);
	puts("Vulkan scene: textures, indexed draws, depth, queries, Honeycomb on/off and GPU pixels PASS");
	return 0;
}
