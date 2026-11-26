#include "harness.h"
typedef int GLint;
enum { GL_FALSE, GL_TEXTURE_2D };
static struct { int array; } meshes[2];
static struct { int mesh,nodes,alpha; float matrices[32],uv_transform[8]; } draws[6];
static int array, texture, tested, node_count, pose_uploads, uv_uploads;
static float pose[32], transform[8];
static void glBindVertexArray(int value) { array=value; }
static void glUniform1i(int loc,int value) { tested=value; }
static void glBindTexture(int target,int value) { texture=value; }
static void glUniform4fv(int loc,int count,const float *p) { memcpy(transform,p,32); uv_uploads++; }
static void glUniformMatrix4fv(int loc,int count,int transpose,const float *p) {
    memcpy(pose,p,count*64); node_count=count; pose_uploads++;
}
#include "under_test.inc"

int main(void)
{
    int last=-1;
    meshes[0].array=10; meshes[1].array=11;
    for (int i=0;i<6;i++) {
        draws[i].nodes=i<3 ? 1 : 2; draws[i].mesh=i%2;
        draws[i].matrices[0]=i<4 ? 1.f : 2.f;
        draws[i].alpha=(i==0 || i==4) ? 0 : (i==3 ? 30 : 20);
        draws[i].uv_transform[5]=i>=3 ? 4.f : 2.f;
        bind_caster_draw(i,&last,0,1,2);
        CHECK(array==meshes[draws[i].mesh].array,"mesh VAO");
        CHECK(node_count==draws[i].nodes && !memcmp(pose,draws[i].matrices,node_count*64),"skinned pose");
        CHECK(tested==!!draws[i].alpha,"alpha-test state");
        CHECK(!tested || (texture==draws[i].alpha && !memcmp(transform,draws[i].uv_transform,32)),"animated cutout");
    }
    CHECK(pose_uploads==3 && uv_uploads==3,"redundant uniform uploads omitted");
    return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
