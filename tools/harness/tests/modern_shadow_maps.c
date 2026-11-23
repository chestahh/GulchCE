#include "harness.h"
static struct { int model,soft; unsigned static_depth,object_depth; float tint[3],frame[16],radius; } sun;
static int sun_draw_batch(void) {return 0;}
static unsigned long long spot_window;
struct modern_shadow_uniforms;
static void spot_bind(const struct modern_shadow_uniforms *u) { (void)u; }
static void modern_environment_dispose(void) {}
#include <stddef.h>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
enum { GL_TEXTURE0,GL_TEXTURE_2D,GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
 GL_TEXTURE_CUBE_MAP_POSITIVE_X,GL_FRAMEBUFFER_COMPLETE,GL_NO_ERROR,
 GL_LOWER_LEFT,GL_NEGATIVE_ONE_TO_ONE,GL_DEPTH_TEST,GL_GREATER,GL_TRUE,
 GL_FALSE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_CULL_FACE,
 GL_POLYGON_OFFSET_FILL,GL_FRONT_AND_BACK,GL_FILL,GL_DEPTH_BUFFER_BIT,
 GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER,GL_FLOAT,GL_TRIANGLES,
 GL_UNSIGNED_SHORT,GL_UPPER_LEFT,GL_ZERO_TO_ONE,GL_TEXTURE_CUBE_MAP };
struct xgpu_texture_description { int unused; };
static struct { unsigned depth[2]; int enabled; float frames[32], radius; int soft; } environment_model;
static int fbo,allocated,handoffs,receiver,texture,element,draw_counts[16][4],fail_fbo,deleted_textures,delete_calls;
static unsigned int bound[2];
static int copy_count, copied_faces, submitted[4];
static int map_slots[2]={0,1};
static long resolution_scale=8;
static int viewport_size;
long halo_shadow_resolution_scale(void) { return resolution_scale; }
static void glViewport(int x,int y,int width,int height) {
 CHECK(width==height && width==128*resolution_scale,"viewport follows selected shadow resolution");viewport_size=width;
}
static void glClipControl(int origin,int depth) {
 CHECK(depth==GL_ZERO_TO_ONE,"reversed float depth retains precision through zero-to-one clipping");
}
#define glDepthRange(...) ((void)0)
static void glClearDepth(double depth) { CHECK(depth==0,"empty reversed depth is zero"); }
#define glEnable(...) ((void)0)
static void glDepthFunc(int comparison) { CHECK(comparison==GL_GREATER,"nearest reversed depth wins"); }
#define glDepthMask(...) ((void)0)
#define glColorMask(...) ((void)0)
#define glDisable(...) ((void)0)
static int offset_calls;
static void glPolygonOffset(float factor,float units) { CHECK(factor==-.5f/256 && units==0,"accepted ground slope");offset_calls++; }
#define glPolygonMode(...) ((void)0)
#define glUseProgram(...) ((void)0)
#define glBindVertexArray(array) (element=(int)(array))
#define glActiveTexture(...) ((void)0)
#define glBindSampler(...) ((void)0)
static int face_clears;
#define glClear(...) ((void)++face_clears)
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
 delete_calls++;
 CHECK(n==1 || n==2,"release failed cube or owned pair");deleted_textures+=!!textures[0]+(n==2 && !!textures[1]);
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
 CHECK(texture>=91 && texture<=106,"known world/view depth target");draw_counts[texture-91][element-10]++;submitted[element-10]++;
}
static void glCopyImageSubData(GLuint source,GLenum source_target,int sl,int sx,int sy,int sz,
 GLuint destination,GLenum target,int dl,int dx,int dy,int dz,int w,int h,int depth) {
 CHECK(source_target==GL_TEXTURE_CUBE_MAP && target==GL_TEXTURE_CUBE_MAP && depth>=1 && sz>=0 && sz+depth<=6 && dz==sz &&
       !sl && !dl && !sx && !sy && !dx && !dy,"copy aligned whole depth faces within each cube");
 CHECK(w==128*resolution_scale && h==w,"copy follows selected shadow resolution");
 if (!copy_count) CHECK(!draw_counts[source-91][1] && !draw_counts[source-91][2],"copy precedes receiver-private casters");
 copied_faces = ((1U << depth)-1) << sz;
 memcpy(draw_counts[destination-91],draw_counts[source-91],sizeof(draw_counts[0]));copy_count++;
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
/* Sample the skinned triangle surface, not just its vertices: a large
   triangle can cross a face or the light radius with every vertex outside. */
static void check_surface_bounds(void) {
 struct modern_shadow_vertex v[3]={0};
 struct shadow_mesh mesh={0};
 float bones[32],light[4]={0,0,0,8},p[3][3];
 unsigned int random=173;
 int trial,i,j,k,a,b,faces,face,culled=0,samples=0;
 for(trial=0;trial<400;trial++) {
  for(i=0;i<32;i++) {
   random=random*1664525U+1013904223U;
   bones[i]=(float)((int)((random>>16)%2001)-1000)/200.f;
  }
  for(i=0;i<2;i++) {
   bones[i*16+3]=bones[i*16+7]=bones[i*16+11]=0;bones[i*16+15]=1;
  }
  for(i=0;i<3;i++) {
   v[i].nodes[0]=0;v[i].nodes[1]=1;v[i].weight=(float)i/2;
   for(j=0;j<3;j++) {
    random=random*1664525U+1013904223U;
    v[i].position[j]=(float)((int)((random>>16)%2001)-1000)/(trial%3==0?100.f:trial%3==1?1000.f:10000.f);
   }
   for(j=0;j<3;j++) {
    float endpoints[2];
    for(k=0;k<2;k++) endpoints[k]=bones[k*16+12+j]+bones[k*16+j]*v[i].position[0]+
        bones[k*16+4+j]*v[i].position[1]+bones[k*16+8+j]*v[i].position[2];
    p[i][j]=endpoints[0]*v[i].weight+endpoints[1]*(1-v[i].weight);
   }
  }
  mesh_bounds(&mesh,v,3);faces=mesh_faces(&mesh,bones,2,light);
  culled+=faces!=63;
  for(a=0;a<=20;a++) for(b=0;b<=20-a;b++) {
   float q[3],distance=0;
   for(j=0;j<3;j++) { q[j]=(p[0][j]*a+p[1][j]*b+p[2][j]*(20-a-b))/20;distance+=q[j]*q[j]; }
   if(distance>light[3]*light[3]) continue;
   for(face=0;face<6;face++) {
    float depth=(face&1)?-q[face/2]:q[face/2];
    if(depth<.00001f) continue;
    for(j=0;j<3;j++) if(j!=face/2 && (q[j]>depth || -q[j]>depth)) break;
    if(j==3) { samples++;CHECK(faces&(1<<face),"every visible skinned surface sample retains its cube face"); }
   }
  }
 }
 CHECK(culled>50 && samples>1000,"surface sampling exercises rejected faces and visible geometry");
}
int main(int argc,char **argv) {
 float matrix[16]={0},uv[8]={1,0,0,.25f,0,1,0,.5f},origin[3]={0};int i;
 struct modern_shadow_uniforms uniforms={0};
 check_surface_bounds();
 modern_shadow_dispose();modern_shadow_dispose();
 CHECK(!delete_calls && !handoffs,"unused shadow feature never calls the driver during cleanup");
 ground_slope=-.5f/256;framebuffer=9;mesh_count=3;
 for(i=0;i<3;i++) { meshes[i].array=meshes[i].indices=10+i;meshes[i].count=3; }
 CHECK(!modern_shadow_begin(-1,origin,2,0,0) && !modern_shadow_begin(MODERN_SHADOW_LIGHTS,origin,2,0,0),"invalid slot never allocates");
 CHECK(modern_shadow_begin(0,origin,2,0,1),"first light begins");
 CHECK(!modern_shadow_begin(1,origin,2,0,0),"cannot overwrite an unfinished submission");
 CHECK(modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH),"shared caster");
 CHECK(modern_shadow_draw(2,matrix,1,NULL,uv,MODERN_SHADOW_WORLD),"world body/weapon");
 CHECK(modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW),"viewmodel arms/weapon");
 CHECK(modern_shadow_end(),"complete depth maps");
 CHECK(offset_calls==1,"ground Hard 1024 uses slope");
 CHECK(allocated==2 && handoffs==1 && !drawing,"lazy view allocation and balanced renderer handoff");
 CHECK(copy_count==1 && submitted[0]==6 && submitted[1]==6 && submitted[2]==6,"shared casters render once without mixing private geometry");
 CHECK(draw_counts[0][0]==6 && draw_counts[0][1]==6 && !draw_counts[0][2],"world retains full body, excludes viewmodel");
 CHECK(draw_counts[1][0]==6 && !draw_counts[1][1] && draw_counts[1][2]==6,"view sees environment and own pose without hidden body");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==91 && !bound[1],"only valid map binds");
 CHECK(modern_shadow_begin(1,origin,2,1,1),"second independent light");
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);
 modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW);
 CHECK(modern_shadow_end() && maps[0].valid && maps[1].valid,"second pass retains first light");
 CHECK(offset_calls==1,"Soft leaves depth unmodified");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==91 && bound[1]==93,"two world receiver maps");
 receiver=1;modern_shadow_bind(&uniforms);CHECK(bound[0]==92 && bound[1]==94,"two first-person receiver maps");
 map_slots[0]=1;map_slots[1]=0;modern_shadow_bind(&uniforms);
 CHECK(bound[0]==94 && bound[1]==92,"model slot order independent of depth-map ownership");
 modern_shadow_clear_draws();CHECK(!maps[0].valid && !maps[1].valid,"window invalidates both transient bindings");
 modern_shadow_begin(1,origin,2,0,0);
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);modern_shadow_end();
 CHECK(!maps[1].view_map && allocated==4,"hidden viewmodel adds no second pass");
 modern_shadow_bind(&uniforms);CHECK(bound[0]==93,"no stale view map after visibility change");
 modern_shadow_clear_draws();modern_shadow_begin(0,origin,2,0,0);
 /* Exercise allocation failure with the previous owned name released. */
 maps[0].depth_cube[1]=0;allocated=1;fail_fbo=1;
 modern_shadow_draw(3,matrix,1,NULL,uv,MODERN_SHADOW_VIEW);
 CHECK(!modern_shadow_end() && resource_failed && !drawing && handoffs==4,"failed map falls back and restores renderer");
 CHECK(deleted_textures==4 && !maps[0].valid && !maps[1].valid,"allocation failure immediately releases the complete map set");
 modern_shadow_dispose();
 CHECK(deleted_textures==4 && !maps[0].depth_cube[0] && !maps[1].depth_cube[1] && !resource_failed,"disable/map reset releases all maps and retries resources");
 i=delete_calls;modern_shadow_dispose();
 CHECK(delete_calls==i,"repeated cleanup does not submit empty texture deletions");
 allocated=deleted_textures=0;fail_fbo=0;framebuffer=9;mesh_count=1;meshes[0].indices=10;meshes[0].count=3;
 for(i=0;i<MODERN_SHADOW_LIGHTS;i++) {
  CHECK(modern_shadow_begin(i,origin,2,0,0),"each budget slot allocates independently");
  modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);
  modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_VIEW);
  CHECK(modern_shadow_end(),"world and first-person maps complete for every slot");
  CHECK(allocated==2*(i+1),"unused slots are not preallocated");
 }
 map_slots[0]=MODERN_SHADOW_LIGHTS-1;map_slots[1]=MODERN_SHADOW_LIGHTS-2;
 receiver=1;modern_shadow_bind(&uniforms);
 CHECK(bound[0]==90+2*MODERN_SHADOW_LIGHTS && bound[1]==88+2*MODERN_SHADOW_LIGHTS,"final slots bind their own view maps");
 modern_shadow_clear_draws();
 for(i=0;i<MODERN_SHADOW_LIGHTS;i++) CHECK(!maps[i].valid,"all slots invalidated at window boundary");
 /* A live size change releases all cubes, retaining immutable mesh handles.
    Re-selecting the same size must not churn resources. */
 resolution_scale=16;allocated=0;
 CHECK(modern_shadow_begin(0,origin,2,0,1),"2048 begins without a feature reset");
 CHECK(deleted_textures==2*MODERN_SHADOW_LIGHTS && allocated==1,"resize frees every old cube before lazy allocation");
 CHECK(mesh_count==1 && meshes[0].indices==10,"resize preserves converted geometry");
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);modern_shadow_end();
 CHECK(viewport_size==2048 && offset_calls==1,"2048 retains its original depth rasterization");
 i=delete_calls;
 modern_shadow_begin(0,origin,2,0,0);modern_shadow_end();
 CHECK(delete_calls==i && allocated==1,"same resolution reuses resources");
 resolution_scale=32;allocated=0;resource_failed=1;
 CHECK(modern_shadow_begin(0,origin,2,0,0) && !resource_failed,"resolution change retries failed resources");
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);modern_shadow_end();
 CHECK(viewport_size==4096,"4096 raster dimensions");
 resolution_scale=1;allocated=0;
 CHECK(modern_shadow_begin(0,origin,2,0,0),"return to stock resolution");
 modern_shadow_draw(1,matrix,1,NULL,uv,MODERN_SHADOW_BOTH);modern_shadow_end();
 CHECK(viewport_size==128,"128 raster dimensions");
 modern_shadow_dispose();CHECK(deleted_textures==2*MODERN_SHADOW_LIGHTS+3,"all old and replacement cubes released");
 {
  struct modern_shadow_vertex vertices[3]={0};
  float bones[32]={0},light[4]={0,0,0,20};
  int face;
  bones[0]=bones[5]=bones[10]=bones[15]=1;
  memcpy(bones+16,bones,64);
  vertices[0].position[0]=vertices[0].position[1]=vertices[0].position[2]=-.1f;
  vertices[1].position[0]=vertices[1].position[1]=vertices[1].position[2]=.1f;
  vertices[0].weight=vertices[1].weight=1;
  mesh_bounds(&meshes[0],vertices,2);
  CHECK(mesh_faces(&meshes[0],bones,1,light)==63,"a box around the light retains every face");
  {
   struct shadow_mesh close;
   vertices[0].position[0]=vertices[1].position[0]=.0024f;
   vertices[0].position[1]=vertices[0].position[2]=-.0005f;
   vertices[1].position[1]=vertices[1].position[2]=.0005f;
   mesh_bounds(&close,vertices,2);
   CHECK(mesh_faces(&close,bones,1,light)==1,"a close caster survives the near-plane face bound");
   for(face=0;face<3;face++) {
    vertices[0].position[face]=-.1f;vertices[1].position[face]=.1f;
   }
  }
  for(face=0;face<6;face++) {
   bones[12]=bones[13]=bones[14]=0;bones[12+face/2]=(face&1)?-10:10;
   CHECK(mesh_faces(&meshes[0],bones,1,light)==(1<<face),"isolated box selects the correct cube face");
  }
  bones[12]=bones[14]=10;bones[13]=0;
  CHECK(mesh_faces(&meshes[0],bones,1,light)==17,"seam-crossing geometry retains both faces");
  light[3]=2;
  CHECK(!mesh_faces(&meshes[0],bones,1,light),"distant geometry cannot occlude a receiver inside the light");
  vertices[0].nodes[1]=vertices[1].nodes[1]=1;
  vertices[0].weight=vertices[1].weight=.5f;
  mesh_bounds(&meshes[0],vertices,2);
  bones[12]=10;bones[14]=0;bones[28]=-10;
  CHECK(mesh_faces(&meshes[0],bones,2,light)==63,"blended vertices between separated bones remain conservative");
  CHECK(mesh_faces(&meshes[0],bones,1,light)==63,"unknown pose retains the original submission");
  vertices[0].weight=2;mesh_bounds(&meshes[0],vertices,2);
  CHECK(mesh_faces(&meshes[0],bones,2,light)==63,"unsupported weights disable rejection");
  vertices[0].weight=vertices[1].weight=1;mesh_bounds(&meshes[0],vertices,2);
  framebuffer=9;mesh_count=1;meshes[0].indices=10;meshes[0].count=3;
  allocated=0;resolution_scale=8;
  CHECK(modern_shadow_begin(0,origin,2,0,0),"far viewmodel test begins");
  CHECK(modern_shadow_draw(1,bones,2,NULL,uv,MODERN_SHADOW_VIEW) && !draw_count,
        "a distant viewmodel does not queue a draw or require a second cube");
  CHECK(modern_shadow_end() && !maps[0].view_map && allocated==1,"far light keeps only its world cube");
  bones[12]=.5f;
  CHECK(modern_shadow_begin(0,origin,2,0,0),"near light begins");
  CHECK(modern_shadow_draw(1,bones,2,NULL,uv,MODERN_SHADOW_VIEW),"near viewmodel remains a caster");
  CHECK(modern_shadow_end() && maps[0].view_map && allocated==2,"near light retains the independent first-person cube");
  CHECK(copied_faces==63, "a new view cube is fully initialised");
  {
   unsigned int previous_dirty = 63 & ~maps[0].view_clear_faces;
   CHECK(previous_dirty && previous_dirty != 63, "near caster covers a subset of faces");
   modern_shadow_begin(0,origin,2,0,0);
   modern_shadow_draw(1,bones,2,NULL,uv,MODERN_SHADOW_VIEW);
   modern_shadow_end();
   CHECK((copied_faces & previous_dirty)==previous_dirty && copied_faces!=63,
         "view cube clears every previous private caster without copying known empty faces");
  }
  modern_shadow_begin(0,origin,4,0,0); modern_shadow_end();
  CHECK(maps[0].clear_faces==63,"an empty world cube clears previous private casters");
  face_clears=0;
  modern_shadow_begin(0,origin,4,0,0); modern_shadow_end();
  CHECK(!face_clears,"previously empty depth faces need no attachment clear");
  modern_shadow_dispose();
 }
 return 0;
}
