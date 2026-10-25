#include "harness.h"
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned long DWORD;
enum { GL_ACTIVE_TEXTURE, GL_TEXTURE0 = 100, GL_TEXTURE_3D = 200,
 GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S,
 GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R, GL_LINEAR, GL_CLAMP_TO_EDGE,
 GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
struct xgpu_texture_description { int unused; };
static int active = GL_TEXTURE0 + 3, lookups, sampler_parameters, samplers, selected = 3;
static int sampler_uniforms, enabled_location, transform_location[2];
static float enabled_value[2];
static int fail_sampler = 1, fallback_parameters;
static DWORD resources[2];
static GLint glGetUniformLocation(GLuint program, const char *name)
{
 lookups++;
 if (!strcmp(name,"plasma_occ_enabled")) return program * 10;
 if (!strcmp(name,"plasma_occ_transform0")) return program * 10 + 1;
 if (!strcmp(name,"plasma_occ_transform1")) return program * 10 + 2;
 return program * 10 + 3;
}
static void glUniform1i(GLint location, int value)
{ CHECK(value==6 || value==7,"feature samplers use only reserved units"); sampler_uniforms++; }
static void glGetIntegerv(GLenum name, GLint *value) { *value=active; }
static void glActiveTexture(GLenum value) { active=value; }
static void glGenSamplers(int count, GLuint *value) { samplers++; *value=fail_sampler ? 0 : 99; }
static void glSamplerParameteri(GLuint sampler, GLenum name, GLint value)
{
 CHECK(sampler==99,"only owned sampler is configured");
 CHECK(value==((name==GL_TEXTURE_MIN_FILTER || name==GL_TEXTURE_MAG_FILTER) ? GL_LINEAR : GL_CLAMP_TO_EDGE),
       "sampling preserves linear filtering and clamped edges");
 sampler_parameters++;
}
static void glBindSampler(GLuint unit, GLuint sampler)
{ CHECK((unit==6 || unit==7) && sampler==(fail_sampler ? 0 : 99),"stock sampler units are untouched"); }
static void glTexParameteri(GLenum target, GLenum name, GLint value)
{
 CHECK(fail_sampler && (active==GL_TEXTURE0+6 || active==GL_TEXTURE0+7),"fallback configures only feature textures");
 CHECK(value==((name==GL_TEXTURE_MIN_FILTER || name==GL_TEXTURE_MAG_FILTER) ? GL_LINEAR : GL_CLAMP_TO_EDGE),
       "allocation fallback preserves sampling");
 fallback_parameters++;
}
static void glBindTexture(GLenum target, GLuint texture)
{ CHECK(active==GL_TEXTURE0+6 || active==GL_TEXTURE0+7,"only reserved texture units are changed"); }
static void glGenTextures(int count, GLuint *texture) { *texture=88; }
static void glTexImage3D(GLenum a,int b,int c,int d,int e,int f,int g,GLenum h,GLenum i,const void *p) {}
static void glUniform4fv(GLint location, int count, const float *values)
{ transform_location[active-GL_TEXTURE0-6]=location; }
static void glUniform2f(GLint location,float a,float b)
{ enabled_location=location; enabled_value[0]=a; enabled_value[1]=b; }
static void *plasma_occlusion_model_texture(int slot,float transform[4])
{ int i; for(i=0;i<4;i++) transform[i]=(float)i; return selected&(1<<slot) ? &resources[slot] : NULL; }
static GLuint xgpu_texture_get(const DWORD *resource,void *unused,GLenum *target,struct xgpu_texture_description *description)
{ return 11+(resource==&resources[1]); }
#include "under_test.inc"
int main(int argc,char **argv)
{
 struct plasma_occlusion_uniforms a,b;
 int n;
 plasma_occlusion_model_initialize(1,&a);
 plasma_occlusion_model_initialize(2,&b);
 CHECK(lookups==10 && sampler_uniforms==4,"uniform lookup and sampler assignments occur at link time");
 plasma_occlusion_bind_model(&a);
 CHECK(fallback_parameters==10 && !sampler_parameters,"failed sampler allocation uses previous texture parameters");
 fail_sampler=0;
 for(n=0;n<100;n++) plasma_occlusion_bind_model(n&1 ? &a : &b);
 CHECK(lookups==10 && sampler_uniforms==4,"draws do not repeat program lookup or sampler assignment");
 CHECK(samplers==2 && sampler_parameters==5 && fallback_parameters==10,"immutable sampling state is created once after recovery");
 CHECK(active==GL_TEXTURE0+3,"caller active unit restored");
 CHECK(enabled_location==10 && transform_location[0]==11 && transform_location[1]==12,
       "uniform locations follow the linked program, not the last bound program");
 selected=0; plasma_occlusion_bind_model(&b);
 CHECK(enabled_location==20 && !enabled_value[0] && !enabled_value[1],"disabled slots clear both uniforms");
 selected=2; plasma_occlusion_bind_model(&a);
 CHECK(!enabled_value[0] && enabled_value[1],"activation change keeps independent slots");
 a.enabled=-1; n=sampler_parameters; plasma_occlusion_bind_model(&a);
 CHECK(sampler_parameters==n && active==GL_TEXTURE0+3,"unrelated shader requires no binding work");
 return 0;
}
