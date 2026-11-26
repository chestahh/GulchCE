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
static int width=512, copies, blits, queries, error, refuse_copy, submitted, refuse_draw;
static int mesh_count=2, draw_count=1;
static struct shadow_mesh { int handoff_nodes; GLuint handoff_alpha; } meshes[2];
static struct { int mesh, nodes; GLuint alpha; } draws[2];
static void glDrawArrays(GLenum mode,int first,int count) { submitted++; if (refuse_draw) error=1; }
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
    CHECK(environment_draw_receiver(0) && submitted==1 && queries==8,"validate first sun receiver");
    CHECK(environment_draw_receiver(0) && submitted==2 && queries==8,"stable sun draw stays queued");
    environment.destination++; error=1;
    CHECK(!environment_draw_receiver(0) && submitted==2,"rejected setup does not draw");
    refuse_draw=1;
    CHECK(!environment_draw_receiver(0) && submitted==3,"failed receiver uses fallback");
    refuse_draw=0;
    CHECK(environment_draw_receiver(0) && submitted==4,"retry target after receiver failure");
    sun.projecting=0; refuse_draw=1;
    CHECK(!environment_draw_receiver(0) && submitted==5,"per-object receiver retains per-draw checks");
    refuse_draw=0;
    draws[0].nodes=3; draws[0].alpha=22;
    CHECK(environment_draw_receiver(1),"first handoff succeeds");
    { int after=queries;
      CHECK(environment_draw_receiver(1) && queries==after,"verified handoff stays queued");
      draws[0].alpha++; refuse_draw=1;
      CHECK(!environment_draw_receiver(1) && queries==after+2,"new cutout is checked and may fail");
      refuse_draw=0;
      CHECK(environment_draw_receiver(1) && queries==after+4,"retry rejected cutout");
      draws[0].nodes++;
      CHECK(environment_draw_receiver(1) && queries==after+6,"new bone count is checked");
      draws[1]=draws[0]; draws[1].mesh=1; draw_count=2;
      CHECK(environment_draw_receiver(1) && queries==after+8,"new mesh in a family is checked");
      environment.destination++; draw_count=1;
      CHECK(environment_draw_receiver(1) && queries==after+10,"changed destination is checked");
      draws[0].mesh=1; error=1;
      CHECK(!environment_draw_receiver(1) && queries==after+11,"other mesh rechecked on changed target");
      environment.verified_copy_source=0; refuse_draw=1;
      CHECK(!environment_draw_receiver(1),"multisample handoff retains ordinary fallback");
    }
    return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
