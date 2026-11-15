#include "harness.h"
#include <stddef.h>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
enum { GL_TEXTURE0,GL_TEXTURE_2D,GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
 GL_TEXTURE_CUBE_MAP_POSITIVE_X,GL_FRAMEBUFFER_COMPLETE,GL_NO_ERROR,
 GL_LOWER_LEFT,GL_NEGATIVE_ONE_TO_ONE,GL_DEPTH_TEST,GL_LESS,GL_TRUE,
 GL_FALSE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_CULL_FACE,
 GL_POLYGON_OFFSET_FILL,GL_FRONT_AND_BACK,GL_FILL,GL_DEPTH_BUFFER_BIT,
 GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER,GL_FLOAT,GL_TRIANGLES,
 GL_UNSIGNED_SHORT,GL_UPPER_LEFT,GL_ZERO_TO_ONE };
struct xgpu_texture_description { int unused; };
static int fbo,allocated,handoffs,receiver,texture,element,draw_counts[4][4],fail_fbo,deleted_textures;
static unsigned int bound[2];
static int map_slots[2]={0,1};
#define glViewport(...) ((void)0)
#define glClipControl(...) ((void)0)
#define glDepthRange(...) ((void)0)
#define glClearDepth(...) ((void)0)
#define glEnable(...) ((void)0)
#define glDepthFunc(...) ((void)0)
#define glDepthMask(...) ((void)0)
#define glColorMask(...) ((void)0)
#define glDisable(...) ((void)0)
#define glPolygonMode(...) ((void)0)
#define glUseProgram(...) ((void)0)
#define glBindVertexArray(...) ((void)0)
#define glActiveTexture(...) ((void)0)
#define glBindSampler(...) ((void)0)
#define glClear(...) ((void)0)
#define glUniform1i(...) ((void)0)
#define glEnableVertexAttribArray(...) ((void)0)
#define glVertexAttribPointer(...) ((void)0)
#define glUniformMatrix4fv(...) ((void)0)
#define glBindTexture(...) ((void)0)
#define glUniform2f(...) ((void)0)
#define glUniform1f(...) ((void)0)
#define glDeleteBuffers(...) ((void)0)
#define glDeleteFramebuffers(...) ((void)0)
#define glDeleteProgram(...) ((void)0)
#define glDeleteVertexArrays(...) ((void)0)
#define glDeleteSamplers(...) ((void)0)
static void glDeleteTextures(int n,const GLuint *textures) {
 CHECK(n==2,"dispose owns world/view pair");deleted_textures+=!!textures[0]+!!textures[1];
}
static void glBindFramebuffer(int target,int value) { fbo=value; }
static void create_depth_cube(GLuint *value) { allocated++;*value=90+allocated; }
static void glFramebufferTexture2D(int t,int a,int face,int value,int l) {
 CHECK(fbo==9,"depth attachment uses owned framebuffer, including lazy allocation");texture=value;
}
static int glCheckFramebufferStatus(int t) { return fail_fbo ? -1 : GL_FRAMEBUFFER_COMPLETE; }
static int glGetError(void) { return GL_NO_ERROR; }
static void glUniform4fv(int location,int count,const float *v) {
 if(count==2) CHECK(v[3]==0.25f && v[7]==0.5f,"animated UV translation survives the depth submission");
}
static void glBindBuffer(int target,int buffer) { if(target==GL_ELEMENT_ARRAY_BUFFER) element=buffer; }
static void glDrawElements(int mode,int count,int type,void *p) {
 CHECK(texture>=91 && texture<=94,"known world/view depth target");draw_counts[texture-91][element-10]++;
}
static void xgpu_gl_end_external_pass(void) { handoffs++; }
static GLuint xgpu_texture_get(const void *p,void *u,GLenum *target,struct xgpu_texture_description *d) { *target=GL_TEXTURE_2D;return 5; }
static void glBindTextures(int unit,int n,const GLuint *p) { bound[unit==10]=*p; }
static int modern_shadows_map(int slot) { return map_slots[slot]; }
static int xgpu_gl_ready(void) { return 1; }
static int create_resources(void) { return 1; }
static int modern_shadows_view_receiver(void) { return receiver; }
static float modern_shadows_world_extent(void) { return 1; }
static void platform_log(const char *format,...) {}
int modern_shadow_end(void);
#include "under_test.inc"
int main(int argc,char **argv) {
 float matrix[16]={0},uv[8]={1,0,0,.25f,0,1,0,.5f},origin[3]={0};int i;
 struct modern_shadow_uniforms uniforms={0};
 framebuffer=9;mesh_count=3;
 for(i=0;i<3;i++) { meshes[i].indices=10+i;meshes[i].count=3; }
 CHECK(!modern_shadow_begin(-1,origin,2,0) && !modern_shadow_begin(2,origin,2,0),"invalid slot never allocates");
 CHECK(modern_shadow_begin(0,origin,2,0),"first light begins");
 CHECK(!modern_shadow_begin(1,origin,2,0),"cannot overwrite an unfinished submission");
 CHECK(modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH),"shared caster");
 CHECK(modern_shadow_draw(2,matrix,1,NULL,uv,MODERN_SHADOW_WORLD),"world body/weapon");
 CHECK(modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW),"viewmodel arms/weapon");
 CHECK(modern_shadow_end(),"complete depth maps");
 CHECK(allocated==2 && handoffs==1 && !drawing,"lazy view allocation and balanced renderer handoff");
 CHECK(draw_counts[0][0]==6 && draw_counts[0][1]==6 && !draw_counts[0][2],"world retains full body, excludes viewmodel");
 CHECK(draw_counts[1][0]==6 && !draw_counts[1][1] && draw_counts[1][2]==6,"view sees environment and own pose without hidden body");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==91 && !bound[1],"only valid map binds");
 CHECK(modern_shadow_begin(1,origin,2,1),"second independent light");
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);
 modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW);
 CHECK(modern_shadow_end() && maps[0].valid && maps[1].valid,"second pass retains first light");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==91 && bound[1]==93,"two world receiver maps");
 receiver=1;modern_shadow_bind(&uniforms);CHECK(bound[0]==92 && bound[1]==94,"two first-person receiver maps");
 map_slots[0]=1;map_slots[1]=0;modern_shadow_bind(&uniforms);
 CHECK(bound[0]==94 && bound[1]==92,"model slot order independent of depth-map ownership");
 modern_shadow_clear_draws();CHECK(!maps[0].valid && !maps[1].valid,"window invalidates both transient bindings");
 modern_shadow_begin(1,origin,2,0);
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);modern_shadow_end();
 CHECK(!maps[1].view_map && allocated==4,"hidden viewmodel adds no second pass");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==93,"no stale view map after visibility change");
 modern_shadow_clear_draws();modern_shadow_begin(0,origin,2,0);
 /* Exercise allocation failure with the previous owned name released. */
 maps[0].depth_cube[1]=0;allocated=1;fail_fbo=1;
 modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW);
 CHECK(!modern_shadow_end() && resource_failed && !drawing && handoffs==4,"failed map falls back and restores renderer");
 modern_shadow_dispose();
 CHECK(deleted_textures==4 && !maps[0].depth_cube[0] && !maps[1].depth_cube[1] && !resource_failed,"disable/map reset releases all maps and retries resources");
 return 0;
}
