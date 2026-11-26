#include "harness.h"
static struct { unsigned int vertex_array; } device = {17};
static struct { unsigned int framebuffer, vertex_array; float depth_range[2]; } gl_state;
static unsigned int bound_array;
static int binds;
static void glBindVertexArray(unsigned int array) { binds++;bound_array=array; }
struct nv2a_pixel_shader_key { int modern_shadow; };
struct draw_uniforms { float texture_scale[4]; };
static int texture_binds;
static void bind_textures(struct nv2a_pixel_shader_key *key, float *scales) { texture_binds++; }
#include "under_test.inc"
int main(int argc,char **argv)
{
 unsigned char *bytes=(unsigned char *)&gl_state;
 unsigned int i;
 memset(&gl_state,0,sizeof(gl_state));
 xgpu_gl_end_external_pass();
 CHECK(binds==1 && bound_array==device.vertex_array,"external pass releases its own vertex array");
 for(i=0;i<sizeof(gl_state);i++) CHECK(bytes[i]==255,"next draw must reapply every cached state");
 fallback_binding(0,0); fallback_binding(2,2);
 CHECK(!texture_binds,"disabled and successful shadows add no texture rebind");
 fallback_binding(2,0); fallback_binding(1,0);
 CHECK(texture_binds==2,"failed diffuse and specular draws bind restored collision attenuation");
 return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
