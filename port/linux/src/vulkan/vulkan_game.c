/* Native game-side state and D3D8 draw translation. No GL context or commands. */
#include "xgpu.h"
#include "sdl_platform.h"
#include "port_config.h"
#include "vulkan_game.h"
#include "vulkan_scene.h"
#include "vulkan_window.h"
#include <stdlib.h>
#include <string.h>

void d3d8_surface_initialize(
	D3DSurface *surface, D3DFORMAT format, unsigned long width, unsigned long height);

#define GAME_SHADERS 512
#define GAME_TEXTURES 2048
#define GAME_VERTICES 65536
struct element
{
	unsigned stream, reg, type, offset, bytes;
};
struct program
{
	DWORD words[256 * 4];
	unsigned count, elements, shader;
	struct element layout[16];
};
struct texture
{
	DWORD data, format, size, palette, hash;
	unsigned image, serial, generation;
};
struct target
{
	DWORD data;
	unsigned image;
};
struct pixel_program
{
	struct nv2a_pixel_shader_key key;
	unsigned shader;
};
static struct
{
	D3DSurface back, z;
	D3DSurface *color, *depth;
	D3DVIEWPORT8 viewport;
	D3DMATRIX transforms[D3DTS_MAX];
	struct halo_vk_uniforms uniforms;
	D3DBaseTexture *textures[4];
	D3DPalette *palettes[4];
	D3DVertexBuffer *streams[16];
	unsigned strides[16], base;
	struct program programs[GAME_SHADERS];
	unsigned program_count, declaration, program_address, slots[256], fixed, fixed_shader, constant_mode;
	struct pixel_program pixels[GAME_SHADERS];
	unsigned pixel_count;
	struct texture images[GAME_TEXTURES];
	unsigned image_count, dummy;
	struct target targets[256];
	unsigned target_count;
	float attributes[16][4], *immediate;
	unsigned immediate_count, immediate_type, immediate_active;
	unsigned frame, created;
} game;

static void color_vec(DWORD color, float *out)
{
	out[0] = ((color >> 16) & 255) / 255.0f;
	out[1] = ((color >> 8) & 255) / 255.0f;
	out[2] = (color & 255) / 255.0f;
	out[3] = ((color >> 24) & 255) / 255.0f;
}
static float float_word(DWORD word)
{
	float value;
	memcpy(&value, &word, 4);
	return value;
}
static void *physical(DWORD address, unsigned bytes)
{
	if (!address || address >= PLATFORM_CONTIGUOUS_SIZE || bytes > PLATFORM_CONTIGUOUS_SIZE - address)
	{
		halo_vk_scene_error("Vulkan scene requires contiguous Xbox/CE resources; invalid address or "
							"unsupported MCC resource");
		return NULL;
	}
	return PLATFORM_PHYSICAL_TO_VIRTUAL(address);
}
static unsigned target(D3DSurface *surface, int depth)
{
	struct xgpu_texture_description desc;
	unsigned image, i;
	if (!surface)
		return 0;
	xgpu_texture_describe(surface->Format, surface->Size, &desc);
	image = halo_vk_scene_target(surface->Data, desc.width, desc.height, depth);
	if (image)
	{
		for (i = 0; i < game.target_count; i++)
			if (game.targets[i].data == surface->Data)
				break;
		if (i == 256)
		{
			halo_vk_scene_error("target capacity");
			return 0;
		}
		game.targets[i].data = surface->Data;
		game.targets[i].image = image;
		if (i == game.target_count)
			game.target_count++;
	}
	return image;
}
void halo_vk_game_viewport(const D3DVIEWPORT8 *viewport)
{
	float z = 16777215.0f;
	struct halo_vk_uniforms *u = &game.uniforms;
	struct xgpu_texture_description desc;
	game.viewport = *viewport;
	if (game.depth)
	{
		xgpu_texture_describe(game.depth->Format, game.depth->Size, &desc);
		if (desc.format == D3DFMT_D16 || desc.format == D3DFMT_LIN_D16 || desc.format == D3DFMT_F16 ||
			desc.format == D3DFMT_LIN_F16)
			z = 65535.0f;
	}
	u->viewport_scale[0] = viewport->Width * .5f;
	u->viewport_scale[1] = -(float)viewport->Height * .5f;
	u->viewport_scale[2] = z * (viewport->MaxZ - viewport->MinZ);
	u->viewport_scale[3] = 0;
	u->viewport_offset[0] = viewport->X + viewport->Width * .5f;
	u->viewport_offset[1] = viewport->Y + viewport->Height * .5f;
	u->viewport_offset[2] = z * viewport->MinZ;
	u->viewport_offset[3] = 0;
	if (!(game.constant_mode & D3DSCM_NORESERVEDCONSTANTS))
	{
		memcpy(u->c[58], u->viewport_scale, 16);
		memcpy(u->c[59], u->viewport_offset, 16);
	}
}
void halo_vk_game_target(D3DSurface *color, D3DSurface *depth)
{
	struct xgpu_texture_description desc;
	D3DVIEWPORT8 viewport = {0};
	game.color = color ? color : &game.back;
	game.depth = depth;
	xgpu_texture_describe(game.color->Format, game.color->Size, &desc);
	viewport.Width = desc.width;
	viewport.Height = desc.height;
	viewport.MaxZ = 1;
	halo_vk_game_viewport(&viewport);
}
void halo_vk_game_shutdown(void)
{
	if (!game.created)
		return;
	halo_vulkan_window_close();
	if (halo_vulkan_window_handle())
		return;
	platform_video_external_window(NULL);
	if (game.back.Data)
		platform_contiguous_free(PLATFORM_PHYSICAL_TO_VIRTUAL(game.back.Data));
	if (game.z.Data)
		platform_contiguous_free(PLATFORM_PHYSICAL_TO_VIRTUAL(game.z.Data));
	free(game.immediate);
	memset(&game, 0, sizeof(game));
}
int halo_vk_game_create(unsigned width, unsigned height)
{
	unsigned i;
	DWORD *rs = D3D__RenderState;
	memset(&game, 0, sizeof(game));
	game.created = 1;
	memory_watch_initialize();
	atexit(halo_vk_game_shutdown);
	if (!platform_video_external_window(halo_vulkan_window_handle()))
		return 0;
	if (!halo_vulkan_window_scene())
		return 0;
	d3d8_surface_initialize(&game.back, D3DFMT_LIN_A8R8G8B8, width, height);
	d3d8_surface_initialize(&game.z, D3DFMT_LIN_D24S8, width, height);
	game.immediate = malloc(GAME_VERTICES * 256);
	if (!game.back.Data || !game.z.Data || !game.immediate)
		return 0;
	for (i = 0; i < D3DTS_MAX; i++)
		game.transforms[i]._11 = game.transforms[i]._22 = game.transforms[i]._33 = game.transforms[i]._44 = 1;
	for (i = 0; i < 16; i++)
		game.attributes[i][3] = 1;
	rs[D3DRS_ZENABLE] = rs[D3DRS_ZWRITEENABLE] = 1;
	rs[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
	rs[D3DRS_COLORWRITEENABLE] = D3DCOLORWRITEENABLE_ALL;
	rs[D3DRS_SRCBLEND] = D3DBLEND_ONE;
	rs[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
	rs[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
	rs[D3DRS_CULLMODE] = D3DCULL_CCW;
	rs[D3DRS_FRONTFACE] = D3DFRONT_CW;
	rs[D3DRS_FILLMODE] = D3DFILL_SOLID;
	rs[D3DRS_ALPHAFUNC] = rs[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
	rs[D3DRS_STENCILMASK] = rs[D3DRS_STENCILWRITEMASK] = 255;
	rs[D3DRS_STENCILFAIL] = rs[D3DRS_STENCILZFAIL] = rs[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
	for (i = 0; i < 4; i++)
	{
		DWORD *ts = D3D__TextureState[i];
		ts[D3DTSS_ADDRESSU] = ts[D3DTSS_ADDRESSV] = ts[D3DTSS_ADDRESSW] = D3DTADDRESS_WRAP;
		ts[D3DTSS_MAGFILTER] = ts[D3DTSS_MINFILTER] = D3DTEXF_POINT;
		ts[D3DTSS_MAXANISOTROPY] = 1;
	}
	halo_vk_game_target(&game.back, &game.z);
	platform_log("Vulkan scene: native shader/texture/draw translation; experimental Xbox scene renderer");
	return 1;
}
D3DSurface *halo_vk_game_surface(int depth)
{
	return depth ? &game.z : &game.back;
}
void halo_vk_game_transform(unsigned state, const D3DMATRIX *matrix, D3DMATRIX *result)
{
	if (state >= D3DTS_MAX)
		return;
	if (matrix)
		game.transforms[state] = *matrix;
	if (result)
		*result = game.transforms[state];
}
void halo_vk_game_constant_mode(unsigned mode)
{
	game.constant_mode = mode;
	halo_vk_game_viewport(&game.viewport);
}
void halo_vk_game_constants(int reg, const void *data, unsigned count)
{
	int first = reg + 96;
	if (first < 0 || first >= 192 || !data)
		return;
	if (count > 192 - (unsigned)first)
		count = 192 - first;
	memcpy(game.uniforms.c[first], data, count * 16);
}
void halo_vk_game_height(float a, float b, float c, float d)
{
	game.uniforms.height_fade[0] = a;
	game.uniforms.height_fade[1] = b;
	game.uniforms.height_fade[2] = c;
	game.uniforms.height_fade[3] = d;
}
void halo_vk_game_texture(unsigned stage, D3DBaseTexture *texture)
{
	if (stage < 4)
		game.textures[stage] = texture;
}
void halo_vk_game_palette(unsigned stage, D3DPalette *palette)
{
	if (stage < 4)
		game.palettes[stage] = palette;
}
void halo_vk_game_stream(unsigned stream, D3DVertexBuffer *buffer, unsigned stride)
{
	if (stream < 16)
	{
		game.streams[stream] = buffer;
		game.strides[stream] = stride;
	}
}
void halo_vk_game_base(unsigned base)
{
	game.base = base;
}
static unsigned type_bytes(unsigned type)
{
	unsigned n = type >> 4, kind = type & 15;
	if (type == D3DVSDT_D3DCOLOR || type == D3DVSDT_NORMPACKED3)
		return 4;
	if (type == D3DVSDT_FLOAT2H)
		return 12;
	if (!n || n > 4)
		return 0;
	return kind == 2 ? n * 4 : kind == 1 || kind == 5 ? n * 2 : kind == 4 ? n : 0;
}
HRESULT halo_vk_game_shader_create(const DWORD *decl, const DWORD *function, DWORD *handle)
{
	unsigned i = 0, stream = 0, offsets[16] = {0}, id = game.program_count + 1;
	struct program *p;
	if (!handle || id >= GAME_SHADERS)
		return E_FAIL;
	p = &game.programs[id];
	memset(p, 0, sizeof(*p));
	if (function)
	{
		p->count = function[0] >> 16;
		if (p->count > 256)
			return E_FAIL;
		memcpy(p->words, function + 1, p->count * 16);
	}
	while (decl && i < 1024 && decl[i] != D3DVSD_END())
	{
		DWORD token = decl[i++];
		unsigned kind = (token & D3DVSD_TOKENTYPEMASK) >> D3DVSD_TOKENTYPESHIFT;
		if (kind == D3DVSD_TOKEN_STREAM)
		{
			stream = token & D3DVSD_STREAMNUMBERMASK;
			if (stream >= 16)
				return E_FAIL;
		}
		else if (kind == D3DVSD_TOKEN_STREAMDATA)
		{
			if (token & D3DVSD_DATALOADTYPEMASK)
				offsets[stream] += ((token & D3DVSD_SKIPCOUNTMASK) >> D3DVSD_SKIPCOUNTSHIFT) *
								   ((token & 0x08000000) ? 1 : 4);
			else
			{
				struct element *e;
				if (p->elements == 16)
					return E_FAIL;
				e = &p->layout[p->elements++];
				e->stream = stream;
				e->reg = token & D3DVSD_VERTEXREGMASK;
				e->type = (token & D3DVSD_DATATYPEMASK) >> D3DVSD_DATATYPESHIFT;
				e->bytes = type_bytes(e->type);
				e->offset = offsets[stream];
				if (e->reg >= 16 || !e->bytes)
					return E_FAIL;
				offsets[stream] += e->bytes;
			}
		}
		else if (kind == D3DVSD_TOKEN_CONSTMEM)
			i += ((token & D3DVSD_CONSTCOUNTMASK) >> D3DVSD_CONSTCOUNTSHIFT) * 4;
		else if (kind == D3DVSD_TOKEN_EXT)
			i += (token & D3DVSD_EXTCOUNTMASK) >> D3DVSD_EXTCOUNTSHIFT;
	}
	if (i >= 1024)
		return E_FAIL;
	game.program_count = id;
	*handle = id * 2;
	return S_OK;
}
static unsigned shader_id(DWORD handle)
{
	unsigned id = handle / 2;
	return !(handle & 1) && id <= game.program_count ? id : 0;
}
void halo_vk_game_shader_set(DWORD handle)
{
	unsigned id = shader_id(handle);
	if (id)
	{
		game.declaration = id;
		game.program_address = 0;
		game.slots[0] = id;
		game.fixed = 0;
	}
	else if (!handle)
		game.fixed = 1;
}
void halo_vk_game_shader_load(DWORD handle, unsigned address)
{
	if (address < 256)
		game.slots[address] = shader_id(handle);
}
void halo_vk_game_shader_select(DWORD handle, unsigned address)
{
	unsigned id = shader_id(handle);
	game.fixed = 0;
	if (id)
		game.declaration = id;
	if (address < 256)
		game.program_address = address;
}
unsigned halo_vk_game_shader_size(DWORD handle)
{
	return game.programs[shader_id(handle)].count;
}
static DWORD hash_bytes(const void *data, unsigned count)
{
	const unsigned char *p = data;
	DWORD hash = 2166136261u;
	unsigned i;
	for (i = 0; i < count; i++)
		hash = (hash ^ p[i]) * 16777619u;
	return hash;
}
static unsigned texture_upload(unsigned stage, struct xgpu_texture_description *desc)
{
	D3DBaseTexture *tex = game.textures[stage];
	struct halo_vk_texture_upload u;
	struct texture *entry;
	unsigned i, face, level, faces, size, total = 0, offset, hash, palette_data = 0, serial, generation;
	unsigned char *source, *pixels;
	D3DCOLOR *palette = NULL;
	memset(desc, 0, sizeof(*desc));
	if (!tex || !tex->Data)
	{
		if (!game.dummy)
		{
			DWORD white = 0xffffffff;
			memset(&u, 0, sizeof(u));
			u.width = u.height = u.depth = u.levels = u.faces = 1;
			u.format = 44;
			u.pixels = &white;
			u.bytes = 4;
			game.dummy = halo_vk_scene_texture(&u);
		}
		return game.dummy;
	}
	xgpu_texture_describe(tex->Format, tex->Size, desc);
	for (i = 0; i < game.target_count; i++)
		if (game.targets[i].data == tex->Data)
			return game.targets[i].image;
	faces = desc->cube_map ? 6 : 1;
	if (!desc->width || !desc->height || desc->width > 4096 || desc->height > 4096 || desc->depth > 512 ||
		!desc->levels || desc->levels > 16)
	{
		halo_vk_scene_error("invalid texture description");
		return 0;
	}
	if (!desc->depth || (uint64_t)desc->width * desc->height * desc->depth > 8u * 1024u * 1024u ||
		(faces == 6 && (desc->width != desc->height || desc->depth != 1)))
	{
		halo_vk_scene_error("texture dimensions exceed the scene budget");
		return 0;
	}
	size = xgpu_texture_face_size(desc) * faces;
	if (!size || size > 32u * 1024u * 1024u)
	{
		halo_vk_scene_error("texture source size limit");
		return 0;
	}
	source = physical(tex->Data, size);
	if (!source)
		return 0;
	if (game.palettes[stage])
	{
		palette_data = game.palettes[stage]->Data;
		palette = physical(palette_data, 1024);
		if (!palette)
			return 0;
	}
	for (i = 0; i < game.image_count; i++)
		if (game.images[i].data == tex->Data && game.images[i].format == tex->Format &&
			game.images[i].size == tex->Size && game.images[i].palette == palette_data)
			break;
	if (i == GAME_TEXTURES)
	{
		halo_vk_scene_error("texture cache capacity");
		return 0;
	}
	entry = &game.images[i];
	/* The shared CPU memory-watch interface notices writes even within one
	frame. Vulkan owns its cache and uploads; no GL cache is consulted. */
	hash = palette ? hash_bytes(palette, 1024) : 0;
	serial = memory_watch_serial();
	if (entry->image && entry->serial == serial && entry->hash == hash)
		return entry->image;
	generation = memory_watch_generation((unsigned long)source, size);
	entry->serial = serial;
	if (entry->image && generation <= entry->generation && entry->hash == hash)
		return entry->image;
	memory_watch_protect((unsigned long)source, size);
	entry->generation = memory_watch_generation((unsigned long)source, size);
	if (!entry->generation)
		entry->generation = 1;
	memset(&u, 0, sizeof(u));
	u.width = desc->width;
	u.height = desc->height;
	u.depth = desc->depth;
	u.levels = desc->levels;
	u.faces = faces;
	u.format = desc->compressed ? (desc->format == D3DFMT_DXT1		? 133
									  : desc->format == D3DFMT_DXT3 ? 135
																	: 137)
								: 44;
	for (face = 0; face < faces; face++)
		for (level = 0; level < desc->levels; level++)
		{
			unsigned w = desc->width >> level, h = desc->height >> level, d = desc->depth >> level;
			if (!w)
				w = 1;
			if (!h)
				h = 1;
			if (!d)
				d = 1;
			u.offsets[face][level] = total;
			total += desc->compressed
						 ? ((w + 3) / 4) * ((h + 3) / 4) * d * (desc->format == D3DFMT_DXT1 ? 8 : 16)
						 : w * h * d * 4;
			total = (total + 15) & ~15u;
			if (total > 32 * 1024 * 1024)
			{
				halo_vk_scene_error("texture upload size limit");
				return 0;
			}
		}
	pixels = calloc(1, total);
	if (!pixels)
	{
		halo_vk_scene_error("texture allocation");
		return 0;
	}
	for (face = 0; face < faces; face++)
		for (level = 0; level < desc->levels; level++)
		{
			offset = face * xgpu_texture_face_size(desc) + xgpu_texture_level_offset(desc, level);
			if (desc->compressed)
			{
				unsigned w = desc->width >> level, h = desc->height >> level, d = desc->depth >> level, n;
				if (!w)
					w = 1;
				if (!h)
					h = 1;
				if (!d)
					d = 1;
				n = ((w + 3) / 4) * ((h + 3) / 4) * d * (desc->format == D3DFMT_DXT1 ? 8 : 16);
				if (offset > size || n > size - offset)
				{
					free(pixels);
					halo_vk_scene_error("compressed texture bounds");
					return 0;
				}
				memcpy(pixels + u.offsets[face][level], source + offset, n);
			}
			else if (!xgpu_texture_decode_level(desc, level, source + offset, palette,
						 (unsigned long *)(pixels + u.offsets[face][level])))
			{
				free(pixels);
				halo_vk_scene_error("texture decode");
				return 0;
			}
		}
	u.pixels = pixels;
	u.bytes = total;
	entry->image = halo_vk_scene_texture_update(&u, entry->image);
	free(pixels);
	entry->data = tex->Data;
	entry->format = tex->Format;
	entry->size = tex->Size;
	entry->palette = palette_data;
	entry->hash = hash;
	if (i == game.image_count)
		game.image_count++;
	return entry->image;
}
static unsigned blend(unsigned value)
{
	switch (value)
	{
	case D3DBLEND_ZERO:
		return 0;
	case D3DBLEND_ONE:
		return 1;
	case D3DBLEND_SRCCOLOR:
		return 2;
	case D3DBLEND_INVSRCCOLOR:
		return 3;
	case D3DBLEND_DESTCOLOR:
		return 4;
	case D3DBLEND_INVDESTCOLOR:
		return 5;
	case D3DBLEND_SRCALPHA:
		return 6;
	case D3DBLEND_INVSRCALPHA:
		return 7;
	case D3DBLEND_DESTALPHA:
		return 8;
	case D3DBLEND_INVDESTALPHA:
		return 9;
	case D3DBLEND_CONSTANTCOLOR:
		return 10;
	case D3DBLEND_INVCONSTANTCOLOR:
		return 11;
	case D3DBLEND_CONSTANTALPHA:
		return 12;
	case D3DBLEND_INVCONSTANTALPHA:
		return 13;
	case D3DBLEND_SRCALPHASAT:
		return 14;
	default:
		halo_vk_scene_error("unsupported blend factor");
		return 1;
	}
}
static unsigned stencil(unsigned value)
{
	switch (value)
	{
	case D3DSTENCILOP_KEEP:
		return 0;
	case D3DSTENCILOP_ZERO:
		return 1;
	case D3DSTENCILOP_REPLACE:
		return 2;
	case D3DSTENCILOP_INCRSAT:
		return 3;
	case D3DSTENCILOP_DECRSAT:
		return 4;
	case D3DSTENCILOP_INVERT:
		return 5;
	case D3DSTENCILOP_INCR:
		return 6;
	case D3DSTENCILOP_DECR:
		return 7;
	default:
		halo_vk_scene_error("unsupported stencil operation");
		return 0;
	}
}
static int draw_state(struct halo_vk_draw *draw, unsigned topology)
{
	struct nv2a_pixel_shader_key key;
	struct halo_vk_raster *r = &draw->raster;
	struct halo_vk_uniforms *u = &draw->uniforms;
	DWORD *rs = D3D__RenderState;
	unsigned i, j, mask = rs[D3DRS_COLORWRITEENABLE];
	char *glsl;
	memset(draw, 0, sizeof(*draw));
	memset(&key, 0, sizeof(key));
	draw->color = target(game.color, 0);
	draw->depth = target(game.depth, 1);
	*u = game.uniforms;
	u->point_size = float_word(rs[D3DRS_POINTSIZE]);
	if (u->point_size < 1)
		u->point_size = 1;
	u->alpha_reference = (float)(rs[D3DRS_ALPHAREF] & 255);
	draw->viewport[0] = (float)game.viewport.X;
	draw->viewport[1] = (float)game.viewport.Y;
	draw->viewport[2] = (float)game.viewport.Width;
	draw->viewport[3] = (float)game.viewport.Height;
	draw->viewport[4] = game.viewport.MinZ;
	draw->viewport[5] = game.viewport.MaxZ;
	memcpy(key.combiner_state, rs, sizeof(key.combiner_state));
	memset(&key.combiner_state[D3DRS_PSCONSTANT0_0], 0, 16 * sizeof(DWORD));
	key.combiner_state[D3DRS_PSFINALCOMBINERCONSTANT0] = key.combiner_state[D3DRS_PSFINALCOMBINERCONSTANT1] =
		0;
	key.texture_modes = rs[D3DRS_PSTEXTUREMODES];
	key.alpha_test_function = rs[D3DRS_ALPHATESTENABLE] ? rs[D3DRS_ALPHAFUNC] : 0;
	key.fog_enable = (unsigned char)rs[D3DRS_FOGENABLE];
	key.fog_table_mode = (unsigned char)rs[D3DRS_FOGTABLEMODE];
	for (i = 0; i < 4; i++)
	{
		struct xgpu_texture_description desc;
		DWORD *ts = D3D__TextureState[i];
		draw->textures[i] = texture_upload(i, &desc);
		key.sampler_type[i] = desc.cube_map ? 3 : desc.depth > 1 ? 2 : 1;
		key.alpha_kill[i] = ts[D3DTSS_ALPHAKILL] == D3DTALPHAKILL_ENABLE;
		key.color_sign[i] = (ts[D3DTSS_COLORSIGN] >> 28) & 15;
		draw->address[i][0] = ts[D3DTSS_ADDRESSU];
		draw->address[i][1] = ts[D3DTSS_ADDRESSV];
		draw->address[i][2] = ts[D3DTSS_ADDRESSW];
		draw->filter[i][0] = ts[D3DTSS_MAGFILTER];
		draw->filter[i][1] = ts[D3DTSS_MINFILTER];
		draw->filter[i][2] = ts[D3DTSS_MIPFILTER];
		draw->lod_bias[i] = float_word(ts[D3DTSS_MIPMAPLODBIAS]);
		for (j = 0; j < 4; j++)
			u->texture_scale[i][j] = 1;
		if (desc.linear)
		{
			u->texture_scale[i][0] = 1.0f / desc.width;
			u->texture_scale[i][1] = 1.0f / desc.height;
		}
		u->bump_matrix[i][0] = float_word(ts[D3DTSS_BUMPENVMAT00]);
		u->bump_matrix[i][1] = float_word(ts[D3DTSS_BUMPENVMAT01]);
		u->bump_matrix[i][2] = float_word(ts[D3DTSS_BUMPENVMAT10]);
		u->bump_matrix[i][3] = float_word(ts[D3DTSS_BUMPENVMAT11]);
		u->bump_luminance[i][0] = float_word(ts[D3DTSS_BUMPENVLSCALE]);
		u->bump_luminance[i][1] = float_word(ts[D3DTSS_BUMPENVLOFFSET]);
	}
	for (i = 0; i < 8; i++)
	{
		color_vec(rs[D3DRS_PSCONSTANT0_0 + i], u->ps_c0[i]);
		color_vec(rs[D3DRS_PSCONSTANT1_0 + i], u->ps_c1[i]);
	}
	color_vec(rs[D3DRS_PSFINALCOMBINERCONSTANT0], u->ps_final_c0);
	color_vec(rs[D3DRS_PSFINALCOMBINERCONSTANT1], u->ps_final_c1);
	color_vec(rs[D3DRS_FOGCOLOR], u->fog_color);
	u->fog_parameters[0] = float_word(rs[D3DRS_FOGSTART]);
	u->fog_parameters[1] = float_word(rs[D3DRS_FOGEND]);
	u->fog_parameters[2] = float_word(rs[D3DRS_FOGDENSITY]);
	color_vec(rs[D3DRS_BLENDCOLOR], draw->blend_color);
	for (i = 0; i < game.pixel_count; i++)
		if (!memcmp(&game.pixels[i].key, &key, sizeof(key)))
			break;
	if (i == GAME_SHADERS)
	{
		halo_vk_scene_error("pixel shader capacity");
		return 0;
	}
	if (i == game.pixel_count)
	{
		glsl = nv2a_pixel_shader_to_glsl(&key);
		game.pixels[i].shader = halo_vk_scene_shader(glsl, 1);
		free(glsl);
		game.pixels[i].key = key;
		game.pixel_count++;
	}
	draw->pixel_shader = game.pixels[i].shader;
	if (game.fixed)
	{
		if (!game.fixed_shader)
		{
			glsl = nv2a_fixed_function_glsl();
			game.fixed_shader = halo_vk_scene_shader(glsl, 0);
			free(glsl);
		}
		draw->vertex_shader = game.fixed_shader;
	}
	else
	{
		unsigned selected = game.slots[game.program_address];
		struct program *p = &game.programs[selected ? selected : game.declaration];
		if (!p->count)
		{
			halo_vk_scene_error("draw without vertex program");
			return 0;
		}
		if (!p->shader)
		{
			glsl = nv2a_vertex_shader_to_glsl(p->words, p->count, 0, NULL);
			p->shader = halo_vk_scene_shader(glsl, 0);
			free(glsl);
		}
		draw->vertex_shader = p->shader;
	}
	r->topology = topology;
	r->depth_test = rs[D3DRS_ZENABLE] != 0;
	r->depth_write = rs[D3DRS_ZWRITEENABLE] != 0;
	r->depth_function = rs[D3DRS_ZFUNC] - D3DCMP_NEVER;
	r->blend = rs[D3DRS_ALPHABLENDENABLE] != 0;
	r->source_blend = blend(rs[D3DRS_SRCBLEND]);
	r->dest_blend = blend(rs[D3DRS_DESTBLEND]);
	switch (rs[D3DRS_BLENDOP])
	{
	case D3DBLENDOP_ADD:
		r->blend_op = 0;
		break;
	case D3DBLENDOP_SUBTRACT:
		r->blend_op = 1;
		break;
	case D3DBLENDOP_REVSUBTRACT:
		r->blend_op = 2;
		break;
	case D3DBLENDOP_MIN:
		r->blend_op = 3;
		break;
	case D3DBLENDOP_MAX:
		r->blend_op = 4;
		break;
	default:
		halo_vk_scene_error("unsupported blend operation");
	}
	r->color_mask =
		((mask & D3DCOLORWRITEENABLE_RED) ? 1 : 0) | ((mask & D3DCOLORWRITEENABLE_GREEN) ? 2 : 0) |
		((mask & D3DCOLORWRITEENABLE_BLUE) ? 4 : 0) | ((mask & D3DCOLORWRITEENABLE_ALPHA) ? 8 : 0);
	r->cull = rs[D3DRS_CULLMODE] == D3DCULL_CW ? 1 : rs[D3DRS_CULLMODE] == D3DCULL_CCW ? 2 : 0;
	r->stencil = rs[D3DRS_STENCILENABLE] != 0;
	r->stencil_function = rs[D3DRS_STENCILFUNC] - D3DCMP_NEVER;
	r->stencil_fail = stencil(rs[D3DRS_STENCILFAIL]);
	r->stencil_zfail = stencil(rs[D3DRS_STENCILZFAIL]);
	r->stencil_pass = stencil(rs[D3DRS_STENCILPASS]);
	r->stencil_ref = rs[D3DRS_STENCILREF];
	r->stencil_read = rs[D3DRS_STENCILMASK];
	r->stencil_write = rs[D3DRS_STENCILWRITEMASK];
	r->bias = rs[D3DRS_SOLIDOFFSETENABLE] != 0;
	r->bias_slope = float_word(rs[D3DRS_POLYGONOFFSETZSLOPESCALE]);
	r->bias_units = float_word(rs[D3DRS_POLYGONOFFSETZOFFSET]);
	return !halo_vk_scene_failed();
}
static void decode(float *out, const unsigned char *in, unsigned type)
{
	unsigned i, n = type >> 4, kind = type & 15;
	out[0] = out[1] = out[2] = 0;
	out[3] = 1;
	if (type == D3DVSDT_D3DCOLOR)
	{
		DWORD v;
		memcpy(&v, in, 4);
		color_vec(v, out);
		return;
	}
	if (type == D3DVSDT_NORMPACKED3)
	{
		int v;
		memcpy(&v, in, 4);
		out[0] = (float)((v << 21) >> 21) / 1023;
		out[1] = (float)((v << 10) >> 21) / 1023;
		out[2] = (float)(v >> 22) / 511;
		return;
	}
	if (type == D3DVSDT_FLOAT2H)
	{
		memcpy(out, in, 12);
		return;
	}
	for (i = 0; i < n && i < 4; i++)
	{
		if (kind == 2)
			memcpy(&out[i], in + 4 * i, 4);
		else if (kind == 1 || kind == 5)
		{
			short value;
			memcpy(&value, in + 2 * i, 2);
			out[i] = (float)value;
			if (kind == 1)
			{
				out[i] /= 32767;
				if (out[i] < -1)
					out[i] = -1;
			}
		}
		else if (kind == 4)
			out[i] = in[i] / 255.0f;
	}
}
static void transform(float *position)
{
	static const unsigned order[] = {D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION};
	unsigned i, c;
	float in[4];
	for (i = 0; i < 3; i++)
	{
		D3DMATRIX *m = &game.transforms[order[i]];
		memcpy(in, position, 16);
		for (c = 0; c < 4; c++)
			position[c] = in[0] * m->m[0][c] + in[1] * m->m[1][c] + in[2] * m->m[2][c] + in[3] * m->m[3][c];
	}
}
static void draw_vertices(
	unsigned primitive, unsigned start, unsigned count, const WORD *indices, const float *immediate)
{
	struct halo_vk_draw draw;
	struct program *p = &game.programs[game.declaration];
	unsigned i, j, n = 0, topology = 3, *order, *remap, unique = 0;
	float *vertices;
	if (!count || count > GAME_VERTICES || !halo_vk_scene_begin())
		return;
	order = malloc(count * 6 * sizeof(*order));
	if (!order)
	{
		halo_vk_scene_error("index allocation");
		return;
	}
	if (primitive == D3DPT_POINTLIST)
	{
		topology = 0;
		for (i = 0; i < count; i++)
			order[n++] = i;
	}
	else if (primitive == D3DPT_LINELIST)
	{
		topology = 1;
		for (i = 0; i < count - count % 2; i++)
			order[n++] = i;
	}
	else if (primitive == D3DPT_LINESTRIP || primitive == D3DPT_LINELOOP)
	{
		topology = 1;
		for (i = 1; i < count; i++)
		{
			order[n++] = i - 1;
			order[n++] = i;
		}
		if (primitive == D3DPT_LINELOOP)
		{
			order[n++] = count - 1;
			order[n++] = 0;
		}
	}
	else if (primitive == D3DPT_TRIANGLELIST)
	{
		for (i = 0; i < count - count % 3; i++)
			order[n++] = i;
	}
	else if (primitive == D3DPT_TRIANGLESTRIP)
	{
		for (i = 2; i < count; i++)
		{
			order[n++] = i - 2 + (i & 1);
			order[n++] = i - 1 - (i & 1);
			order[n++] = i;
		}
	}
	else if (primitive == D3DPT_TRIANGLEFAN || primitive == D3DPT_POLYGON)
	{
		for (i = 2; i < count; i++)
		{
			order[n++] = 0;
			order[n++] = i - 1;
			order[n++] = i;
		}
	}
	else if (primitive == D3DPT_QUADLIST)
	{
		for (i = 0; i + 3 < count; i += 4)
		{
			order[n++] = i;
			order[n++] = i + 1;
			order[n++] = i + 2;
			order[n++] = i;
			order[n++] = i + 2;
			order[n++] = i + 3;
		}
	}
	else if (primitive == D3DPT_QUADSTRIP)
	{
		for (i = 0; i + 3 < count; i += 2)
		{
			order[n++] = i;
			order[n++] = i + 1;
			order[n++] = i + 3;
			order[n++] = i;
			order[n++] = i + 3;
			order[n++] = i + 2;
		}
	}
	else
		halo_vk_scene_error("unsupported primitive");
	if (!n || !draw_state(&draw, topology))
	{
		free(order);
		return;
	}
	vertices = malloc(count * 256);
	remap = malloc(65536 * 4);
	if (!vertices || !remap)
	{
		free(vertices);
		free(remap);
		free(order);
		halo_vk_scene_error("vertex allocation");
		return;
	}
	memset(remap, 255, 65536 * 4);
	for (i = 0; i < n; i++)
	{
		unsigned source_index = indices ? indices[order[i]] : order[i];
		unsigned index = source_index + (indices ? game.base : start);
		float *v;
		if (remap[source_index] != 0xffffffffu)
		{
			order[i] = remap[source_index];
			continue;
		}
		v = vertices + unique * 64;
		remap[source_index] = unique++;
		if (immediate)
			memcpy(v, immediate + order[i] * 64, 256);
		else
		{
			memcpy(v, game.attributes, 256);
			for (j = 0; j < p->elements; j++)
			{
				struct element *e = &p->layout[j];
				D3DVertexBuffer *b = game.streams[e->stream];
				unsigned stride = game.strides[e->stream], offset;
				unsigned char *data;
				if (!b)
					continue;
				if (stride && index > (PLATFORM_CONTIGUOUS_SIZE - e->offset - e->bytes) / stride)
				{
					halo_vk_scene_error("vertex offset overflow");
					break;
				}
				offset = index * stride + e->offset;
				data = physical(b->Data, offset + e->bytes);
				if (!data)
					break;
				decode(v + e->reg * 4, data + offset, e->type);
			}
		}
		if (game.fixed)
			transform(v);
		order[i] = remap[source_index];
	}
	if (!halo_vk_scene_failed())
		halo_vk_scene_draw_indexed(&draw, vertices, unique, order, n);
	free(remap);
	free(vertices);
	free(order);
}
void halo_vk_game_draw(unsigned primitive, unsigned start, unsigned count, const WORD *indices)
{
	draw_vertices(primitive, start, count, indices, NULL);
}
void halo_vk_game_begin(unsigned primitive)
{
	game.immediate_type = primitive;
	game.immediate_count = 0;
	game.immediate_active = 1;
}
void halo_vk_game_end(void)
{
	game.immediate_active = 0;
	draw_vertices(game.immediate_type, 0, game.immediate_count, NULL, game.immediate);
}
void halo_vk_game_attribute(int reg, float a, float b, float c, float d)
{
	if (reg == D3DVSDE_VERTEX)
		reg = 0;
	if (reg < 0 || reg >= 16)
		return;
	game.attributes[reg][0] = a;
	game.attributes[reg][1] = b;
	game.attributes[reg][2] = c;
	game.attributes[reg][3] = d;
	if (!reg && game.immediate_active)
	{
		if (game.immediate_count == GAME_VERTICES)
		{
			halo_vk_scene_error("immediate vertex capacity");
			return;
		}
		memcpy(game.immediate + game.immediate_count++ * 64, game.attributes, 256);
	}
}
void halo_vk_game_clear(
	unsigned count, const D3DRECT *rects, unsigned flags, DWORD color, float z, unsigned stencil_value)
{
	float rgba[4];
	unsigned c, d, f;
	if (!halo_vk_scene_begin())
		return;
	c = target(game.color, 0);
	d = target(game.depth, 1);
	color_vec(color, rgba);
	f = ((flags & D3DCLEAR_TARGET) ? 1 : 0) | ((flags & D3DCLEAR_ZBUFFER) ? 2 : 0) |
		((flags & D3DCLEAR_STENCIL) ? 4 : 0);
	halo_vk_scene_clear(c, d, f, rgba, z, stencil_value, (const int *)rects, count);
}
void halo_vk_game_present(void)
{
	unsigned back = target(&game.back, 0);
	if (halo_vulkan_window_scene_present(back) < 0 || halo_vk_scene_failed())
	{
		platform_log("Vulkan scene failed; stopping rather than dropping draws");
		exit(1);
	}
	game.frame++;
	memory_watch_begin_frame();
	platform_pump_events();
	halo_vulkan_window_pace(config_integer("display.max_fps"), config_boolean("display.vsync"));
}

/* Called after the game's map teardown, before its backing storage can be
reused. CPU shader programs are device-owned; only their compiled cache dies. */
void halo_vulkan_map_reset(void)
{
	unsigned i;
	if (!halo_vulkan_scene_active || !game.created)
		return;
	if (!halo_vulkan_window_scene_reset())
	{
		platform_log("Vulkan map cleanup failed");
		exit(1);
	}
	for (i = 0; i <= game.program_count; i++)
		game.programs[i].shader = 0;
	memset(game.pixels, 0, sizeof(game.pixels));
	game.pixel_count = 0;
	game.fixed_shader = 0;
	memset(game.images, 0, sizeof(game.images));
	game.image_count = game.dummy = 0;
	memset(game.targets, 0, sizeof(game.targets));
	game.target_count = 0;
	memset(game.textures, 0, sizeof(game.textures));
	memset(game.palettes, 0, sizeof(game.palettes));
	memset(game.streams, 0, sizeof(game.streams));
	game.base = 0;
	D3D__IndexData = NULL;
	memset(game.uniforms.height_fade, 0, sizeof(game.uniforms.height_fade));
	halo_vk_game_target(&game.back, &game.z);
	platform_log("Vulkan scene: map resources retired");
}
