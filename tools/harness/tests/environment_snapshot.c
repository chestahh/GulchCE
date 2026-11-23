/* Exercise the actual snapshot code using a driver that can refuse a copy. */
#include "harness.h"
#include <string.h>
typedef unsigned GLuint;
typedef int GLint;
typedef unsigned GLenum;
#define SHADOW_DRAWS 16
enum { GL_NO_ERROR, GL_TEXTURE10, GL_TEXTURE_2D, GL_DEPTH_STENCIL, GL_DEPTH32F_STENCIL8,
 GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_UNSIGNED_INT_24_8, GL_TEXTURE_MIN_FILTER,
 GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_NONE,
 GL_FRAMEBUFFER_COMPLETE, GL_SCISSOR_TEST, GL_READ_FRAMEBUFFER, GL_DEPTH_BUFFER_BIT };
static GLuint source_image=1, next_name=20;
enum { GL_TRIANGLES };
static int width=512, copies, blits, queries, error, refuse_copy, draws, refuse_draw;
static void glDrawArrays(GLenum mode,int first,int count) { draws++; if (refuse_draw) error=1; }
static int xgpu_gl_scene_target(GLuint *target,int size[2],GLenum *format,GLuint *source,int viewport[4],float offset[2])
{
    *target=3; size[0]=width; size[1]=512; *format=GL_DEPTH32F_STENCIL8; *source=source_image; return 1;
}
static void copy_image(GLuint src,GLenum a,int b,int c,int d,int e,GLuint dst,GLenum f,int g,int h,int i,int j,int w,int l,int n)
{
    copies++; if (refuse_copy) error=1;
}
#define glCopyImageSubData copy_image
static int glGetError(void) { int result=error; error=0; queries++; return result; }
static void glGenTextures(int n,GLuint *out) { *out=next_name++; }
#define glGenFramebuffers glGenTextures
#define glDeleteTextures(...) ((void)0)
#define glActiveTexture(...) ((void)0)
#define glBindTexture(...) ((void)0)
#define glTexImage2D(...) ((void)0)
#define glTexParameteri(...) ((void)0)
#define glBindFramebuffer(...) ((void)0)
#define glFramebufferTexture2D(...) ((void)0)
#define glDrawBuffers(...) ((void)0)
#define glReadBuffer(...) ((void)0)
#define glCheckFramebufferStatus(...) GL_FRAMEBUFFER_COMPLETE
#define glDisable(...) ((void)0)
#define glBlitFramebuffer(...) (++blits)
#include "under_test.inc"

int main(void)
{
    sun.projecting=1;
    CHECK(environment_snapshot() && copies==1 && queries==1 && !blits,"first image pair validated");
    CHECK(environment_snapshot() && copies==1,"share one snapshot across receivers");
    environment.copied=0;
    CHECK(environment_snapshot() && copies==2 && queries==1,"validated pair stays asynchronous");
    environment.copied=0; source_image++; refuse_copy=1;
    CHECK(environment_snapshot() && blits==1 && queries==3,"rejected new source blits on this frame");
    environment.copied=0; refuse_copy=0;
    CHECK(environment_snapshot() && queries==4 && blits==1,"unverified source retries");
    environment.copied=0; width*=2;
    CHECK(environment_snapshot() && queries==5,"new destination validated even with same source");
    environment.copied=0; source_image=0;
    CHECK(environment_snapshot() && blits==2 && queries==6,"multisample depth still resolves");
    environment.verified_copy_source=1;
    CHECK(environment_draw_receiver() && draws==1 && queries==8,"validate first sun receiver");
    CHECK(environment_draw_receiver() && draws==2 && queries==8,"stable sun draw stays queued");
    environment.destination++; error=1;
    CHECK(!environment_draw_receiver() && draws==2,"rejected setup does not draw");
    refuse_draw=1;
    CHECK(!environment_draw_receiver() && draws==3,"failed receiver uses fallback");
    refuse_draw=0;
    CHECK(environment_draw_receiver() && draws==4,"retry target after receiver failure");
    sun.projecting=0; refuse_draw=1;
    CHECK(!environment_draw_receiver() && draws==5,"per-object receiver retains per-draw checks");
    return 0;
}
