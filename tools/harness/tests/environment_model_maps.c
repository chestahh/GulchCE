#include "harness.h"
#define SHADOW_DRAWS 256
typedef unsigned GLuint;
typedef int GLint;
typedef unsigned GLenum;
enum { GL_TEXTURE9, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,
 GL_FLOAT, GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_NEAREST,
 GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_FRAMEBUFFER,
 GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_NONE, GL_FRAMEBUFFER_COMPLETE, GL_NO_ERROR,
 GL_UPPER_LEFT, GL_ZERO_TO_ONE };
static int drawing, environment_drawing, draw_count, failed, ready=1, scale=8;
static int deletion_calls, error_queries, next_error = GL_NO_ERROR;
static int allocations, deletions, checks, attachments, draws, handoffs, fail_fbo, fail_draw;
static unsigned program=1, next_id=10;
static GLuint targets[2];
static int xgpu_gl_ready(void) { return ready; }
static float halo_shadow_resolution_scale(void) { return (float)scale; }
static void xgpu_gl_end_external_pass(void) { handoffs++; }
static void glDeleteTextures(int n, const GLuint *v) { deletion_calls++;while(n--) if(v[n]) deletions++; }
static void glDeleteFramebuffers(int n, const GLuint *v) { deletion_calls++;while(n--) if(v[n]) deletions++; }
static void glDeleteProgram(GLuint p) {}
static void glGenTextures(int n, GLuint *v) { while(n--) {v[n]=next_id++;allocations++;} }
static void glGenFramebuffers(int n, GLuint *v) { while(n--) {v[n]=next_id++;allocations++;} }
static void glBindTextures(int unit,int n,const GLuint *tex) {}
static void glBindSampler(int unit,GLuint sampler) {}
static void glActiveTexture(GLenum t) {}
static void glBindTexture(GLenum t,GLuint v) {}
static void glTexImage2D(GLenum t,int l,int f,int w,int h,int b,GLenum e,GLenum s,const void *p) {}
static void glTexParameteri(GLenum t,GLenum p,int v) {}
static void glBindFramebuffer(GLenum t,GLuint v) {}
static void glFramebufferTexture2D(GLenum t,GLenum a,GLenum d,GLuint v,int l) { attachments++; }
static void glDrawBuffers(int n,const GLenum *p) {}
static void glReadBuffer(GLenum b) {}
static int glCheckFramebufferStatus(GLenum t) { checks++;return fail_fbo?-1:GL_FRAMEBUFFER_COMPLETE; }
static int glGetError(void) { int error=next_error; next_error=GL_NO_ERROR; error_queries++; return error; }
static void glClipControl(GLenum a,GLenum b) {}
static int environment_resources(void);
static int environment_draw_depth(GLuint target) { targets[draws++%2]=target;return !fail_draw; }
#include "under_test.inc"
static int environment_resources(void) { environment.caster=1;environment.size=128*scale;return 1; }
int main(int argc,char **argv)
{
 float frames[32]={0};int allocated,checked;
 modern_environment_dispose();modern_environment_dispose();
 CHECK(!deletion_calls && !handoffs,"unused feature cleanup never calls the driver");
 ready=0;CHECK(!modern_environment_model_begin(frames,2,0) && !allocations,"unready GPU stays idle");ready=1;
 CHECK(modern_environment_model_begin(frames,2,1),"allocate pair");
 CHECK(allocations==4 && checks==2 && attachments==2,"validate each fixed framebuffer once");
 draw_count=1;modern_environment_model_end(1,3);
 CHECK(environment_model.enabled==3 && draws==2 && targets[0]!=targets[1],"independent distant depth maps");
 allocated=allocations;checked=checks;
 modern_environment_model_clear();CHECK(!modern_environment_model_active() && modern_environment_model_reuse(),"consecutive family retains maps");
 CHECK(modern_environment_model_begin(frames,2,0),"reuse storage");draw_count=1;modern_environment_model_end(1,1);
 CHECK(allocations==allocated && checks==checked && attachments==2,"steady state does not reattach or revalidate storage");
 modern_environment_window();CHECK(!modern_environment_model_reuse(),"new window invalidates previous pose");
 CHECK(modern_environment_model_begin(frames,2,0),"draw failure fixture");draw_count=1;fail_draw=1;modern_environment_model_end(1,3);
 CHECK(!modern_environment_model_active() && !drawing && handoffs,"failed depth cannot leak stale visibility");fail_draw=0;
 scale=16;fail_fbo=1;CHECK(!modern_environment_model_begin(frames,2,0),"failed resized allocation falls back");
 allocated=allocations;CHECK(!modern_environment_model_begin(frames,2,0) && allocations==allocated,"allocation failure is latched");
 fail_fbo=0;scale=8;CHECK(modern_environment_model_begin(frames,2,0),"new resolution can recover");draw_count=1;modern_environment_model_end(1,3);
 modern_environment_dispose();CHECK(!environment_model.depth[0] && !environment_model.framebuffer[1] && !modern_environment_model_active(),"dispose clears maps and receiver state");
 CHECK(deletions>=8,"old and final depth/framebuffer resources released");
 float tint[3]={.5f,.4f,.3f};
 sun.projecting=sun.writing=drawing=environment_drawing=1;
 draw_count=SHADOW_DRAWS;environment.depth_fbo=43;sun.object_fbo=44;
 CHECK(sun_draw_batch() && sun.partial && !draw_count && targets[(draws-1)%2]==43,"full BSP batch flushed into static depth");
 draw_count=2;
 CHECK(modern_environment_sun_static(1) && !sun.partial && !sun.writing,"last BSP batch keeps depth; object map starts clear");
 draw_count=0;
 next_error=GL_NO_ERROR+1;
 CHECK(!modern_environment_sun_finish(1,tint) && !sun.object_checked,"first draw failure leaves the target retryable");
 sun.projecting=drawing=environment_drawing=1;
 CHECK(modern_environment_sun_finish(1,tint) && targets[(draws-1)%2]==44 && !drawing,"empty object map still publishes static-only sunlight");
 int queries=error_queries;
 sun.projecting=drawing=environment_drawing=1;
 CHECK(modern_environment_sun_finish(1,tint) && error_queries==queries,"validated posed target draws without a driver round trip");
 modern_environment_sun_model(1);CHECK(modern_environment_model_active(),"prepared sun reaches models independently");
 modern_environment_window();modern_environment_sun_model(1);
 CHECK(!modern_environment_model_active(),"next view cannot sample stale sun");
 return 0;
}
