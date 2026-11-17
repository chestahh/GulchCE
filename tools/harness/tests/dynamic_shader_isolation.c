#include "harness.h"
typedef unsigned long DWORD;
typedef unsigned int GLuint;
typedef int BOOL;
#define XGPU_VERTEX_ATTRIBUTE_COUNT 16
#define D3DRS_PS_MAX 64
#define GL_VERTEX_SHADER 1
struct nv2a_vertex_lighting;
struct nv2a_pixel_shader_key;
struct plasma_occlusion_uniforms { int enabled; };
struct program_entry { GLuint program; int plasma_model_slots; struct plasma_occlusion_uniforms plasma_occlusion; };
static struct { const char *dump_shaders; } debug_settings;
static int compiles, hashes, pp = 1, fail_compile, fail_link, last_lit, last_optional;
static int gpu_calls, bound, looked_up;
static float mask0, mask1;
static char *nv2a_vertex_shader_to_glsl(const DWORD *words, unsigned long count,
 unsigned long packed, const struct nv2a_vertex_lighting *lighting, BOOL plasma)
{
 char *result = malloc(3);
 result[0] = plasma ? '1' : '0'; result[1] = lighting ? '1' : '0'; result[2] = 0;
 return result;
}
static GLuint xgpu_compile_shader(int kind, const char *source, const char *label)
{ compiles++; return fail_compile && source[0]=='1' ? 0 : compiles; }
static void shader_hash_set(GLuint shader, const char *source) { hashes++; }
static BOOL per_pixel_lighting(void) { return pp; }
static GLuint fragment_shader_get(const struct nv2a_pixel_shader_key *key);
static struct program_entry *program_get(GLuint vs, GLuint fs, BOOL optional)
{
 static struct program_entry entries[64];
 last_optional = optional;
 if (!vs || (fail_link && last_lit && optional)) return NULL;
 CHECK(vs < 64, "bounded lazy variants"); entries[vs].program = vs;
 return &entries[vs];
}
static void platform_log(const char *format, ...) {}
static int glGetUniformLocation(GLuint program, const char *name)
{ gpu_calls++; looked_up++; return 42; }
static void plasma_occlusion_model_initialize(GLuint program, struct plasma_occlusion_uniforms *uniforms)
{ gpu_calls++; }
static void plasma_occlusion_bind_model(const struct plasma_occlusion_uniforms *uniforms)
{ gpu_calls++; bound++; }
static void glUniform2f(int location, float a, float b)
{ gpu_calls++; mask0=a; mask1=b; }
#include "under_test.inc"
static GLuint fragment_shader_get(const struct nv2a_pixel_shader_key *key)
{
 last_lit = key->per_pixel_lighting != 0;
 CHECK(!key->plasma_model || key->per_pixel_lighting==2, "only feature per-pixel sources need a different key");
 return 1 + key->per_pixel_lighting + key->plasma_model*3;
}
int main(int argc, char **argv)
{
 struct vertex_shader_object program = {0};
 struct nv2a_pixel_shader_key key = {0};
 struct program_entry *stock, *enhanced;
 struct program_entry entry = {1, -1, {0}};
 int n, base, immediate, lit;
 program.lighting.lights=2; device.vertex_shader=&program;
#ifndef HALO_ANDROID
 program.plasma_model=TRUE;
#endif
 if (!strcmp(argv[1], "gpu"))
 {
  initialize(&entry, FALSE); bind_model(&entry, 0);
  CHECK(!gpu_calls, "stock path makes no feature GPU calls, including uniform lookups");
  initialize(&entry, TRUE);
#ifndef HALO_ANDROID
  CHECK(looked_up==1 && gpu_calls==2, "enhanced initialization is lazy");
  bind_model(&entry, 2); CHECK(bound==1 && mask0==0 && mask1==1, "selected slots uploaded unchanged");
#else
  bind_model(&entry, 2);
  CHECK(!gpu_calls && !looked_up && !bound && !mask0 && !mask1, "Android excludes all optional GPU setup even with a selected mask");
#endif
  base=gpu_calls; bind_model(&entry, 0);
  CHECK(gpu_calls==base, "disable skips even a previously enhanced program's GPU setup");
  return 0;
 }
 if (!strcmp(argv[1], "failure"))
 {
  fail_link=1;
  enhanced=draw_program_get(&program,FALSE,&key,1);
  CHECK(enhanced, "per-vertex fallback remains available");
#ifndef HALO_ANDROID
  CHECK(program.plasma_lighting_failed && !program.lighting_failed && !key.per_pixel_lighting && !key.plasma_model,
   "enhanced link failure is isolated and clears the fragment key");
#endif
  stock=draw_program_get(&program,FALSE,&key,0);
  CHECK(stock && key.per_pixel_lighting==2 && !last_optional, "stock per-pixel path survives an enhanced failure");
  memset(&program,0,sizeof(program)); program.lighting.lights=2;
#ifndef HALO_ANDROID
  program.plasma_model=TRUE;
#endif
  fail_link=0; fail_compile=1;
  draw_program_get(&program,FALSE,&key,1); base=compiles;
  draw_program_get(&program,FALSE,&key,1);
  CHECK(compiles==base, "failed variants are not recompiled each draw");
  CHECK(draw_program_get(&program,FALSE,&key,0) && key.per_pixel_lighting==2, "enhanced compile failure leaves stock usable");
  return 0;
 }
 if (!strcmp(argv[1], "unmarked"))
 {
  program.lighting.lights=0;
  CHECK(draw_program_get(&program,FALSE,&key,1) && !key.plasma_model && !key.per_pixel_lighting,
   "unmarked shaders retain per-vertex lighting and a stock fragment");
#ifndef HALO_ANDROID
  CHECK(last_optional, "instruction-recognized lighting does not depend on per-pixel registration");
  program.plasma_model=FALSE;
#endif
  draw_program_get(&program,FALSE,&key,1);
  CHECK(!last_optional, "unrelated shaders stay stock even with a selected mask");
  return 0;
 }
 for (lit=0;lit<2;lit++) for (immediate=0;immediate<2;immediate++)
 {
  pp=lit; base=compiles;
  stock=draw_program_get(&program,immediate,&key,0);
  CHECK(stock && !last_optional && !key.plasma_model && compiles==base+1, "off compiles only the stock variant");
  enhanced=draw_program_get(&program,immediate,&key,1);
#ifndef HALO_ANDROID
  CHECK(enhanced && enhanced!=stock && last_optional && compiles==base+2, "on creates a distinct lazy variant");
#else
  CHECK(enhanced==stock && !last_optional && compiles==base+1, "Android always reuses stock");
#endif
  base=compiles;
  for(n=0;n<20;n++)
  {
   CHECK(draw_program_get(&program,immediate,&key,0)==stock && !key.plasma_model, "off restores the exact cached stock program");
   CHECK(draw_program_get(&program,immediate,&key,(n&1)?2:3)==enhanced, "slot changes reuse one enhanced variant");
  }
  CHECK(compiles==base && hashes==compiles, "toggles do not recompile or bypass binary-cache registration");
 }
 return 0;
}
