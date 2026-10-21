/* Compile the real scene implementation against a deliberately delayed GPU. */
#include "scene_under_test.c"
#include <assert.h>
static unsigned ready, calls, resets, waits, unavailable;
static VkResult VKAPI_PTR fence_status(VkDevice d, VkFence f) { (void)d; return ready & (1u << (unsigned)(uintptr_t)f) ? VK_SUCCESS : VK_NOT_READY; }
static VkResult VKAPI_PTR query_results(VkDevice d,VkQueryPool p,uint32_t first,uint32_t count,size_t bytes,void *out,VkDeviceSize stride,VkQueryResultFlags flags)
{
    uint64_t *v=out;unsigned i; (void)d;
    assert(first==0 && count==2 && bytes==32 && stride==16);
    assert(flags==(VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    calls++;
    for(i=0;i<count;i++){v[i*2]=(uintptr_t)p==101?i+3:0;v[i*2+1]=!(unavailable && i==1);}
    return VK_SUCCESS;
}
static VkResult VKAPI_PTR reset_command(VkCommandBuffer c,VkCommandBufferResetFlags f){(void)c;(void)f;resets++;return VK_SUCCESS;}
static VkResult VKAPI_PTR reset_descriptors(VkDevice d,VkDescriptorPool p,VkDescriptorPoolResetFlags f){(void)d;(void)p;(void)f;return VK_SUCCESS;}
static VkResult VKAPI_PTR begin_command(VkCommandBuffer c,const VkCommandBufferBeginInfo *b){(void)c;(void)b;return VK_SUCCESS;}
static void VKAPI_PTR reset_queries(VkCommandBuffer c,VkQueryPool p,uint32_t a,uint32_t b){(void)c;(void)p;assert(a==0&&b==1024);}
static void VKAPI_PTR pipeline_barrier(VkCommandBuffer c,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags dep,uint32_t n,const VkMemoryBarrier *m,uint32_t bn,const VkBufferMemoryBarrier *bm,uint32_t in,const VkImageMemoryBarrier *im){(void)c;(void)a;(void)b;(void)dep;(void)n;(void)m;(void)bn;(void)bm;(void)in;(void)im;}
static VkResult VKAPI_PTR wait_failure(VkDevice d,uint32_t n,const VkFence *f,VkBool32 all,uint64_t time){(void)d;(void)n;(void)f;(void)all;(void)time;waits++;return VK_TIMEOUT;}
int halo_vk_memory_type(struct halo_vk_device *d,uint32_t b,VkMemoryPropertyFlags f,uint32_t *i){(void)d;(void)b;(void)f;(void)i;return 0;}
const char *SDL_GetBasePath(void){return "";}
const char *SDL_GetError(void){return "mock";}
SDL_SharedObject *SDL_LoadObject(const char *path){(void)path;return NULL;}
void *SDL_LoadFunction(SDL_SharedObject *o,const char *n){(void)o;(void)n;return NULL;}
void SDL_UnloadObject(SDL_SharedObject *o){(void)o;}
int main(void)
{
    struct halo_vk_device d={0};struct halo_vk_present presentation={0};unsigned pixels=99;
    struct scene_frame *f;
    struct halo_vk_texture_upload u={0};
    unsigned texels[4]={0};
    d.GetFenceStatus=fence_status;d.GetQueryPoolResults=query_results;d.ResetCommandBuffer=reset_command;
    d.ResetDescriptorPool=reset_descriptors;d.BeginCommandBuffer=begin_command;d.CmdResetQueryPool=reset_queries;
    d.CmdPipelineBarrier=pipeline_barrier;d.WaitForFences=wait_failure;scene.d=&d;
    f=&scene.frames[0];f->pending=1;f->fence=(VkFence)(uintptr_t)1;f->queries=(VkQueryPool)(uintptr_t)101;
    f->query_count=2;f->batch_count=1;f->batches[0].count=2;f->batches[0].serial=1;
    assert(!halo_vk_scene_begin());assert(!resets && !calls && f->pending);
    assert(!halo_vk_scene_query_result(0,&pixels));assert(pixels==99 && !calls);
    assert(halo_vk_scene_finish(0,&presentation,NULL));
    ready=2;
    assert(halo_vk_scene_query_result(0,&pixels));assert(pixels==7 && calls==1 && !f->pending);
    assert(halo_vk_scene_begin());assert(resets==1 && !f->query_count && !f->batch_count);
    scene.recording=scene.attempted=0;
    f=&scene.frames[1];f->pending=1;f->fence=(VkFence)(uintptr_t)2;f->queries=(VkQueryPool)(uintptr_t)102;
    f->query_count=2;f->batch_count=1;f->batches[0].count=2;f->batches[0].serial=2;
    assert(halo_vk_scene_query_result(0,&pixels));assert(pixels==7 && calls==1);
    ready|=4;unavailable=1;
    assert(!halo_vk_scene_query_result(0,&pixels));assert(pixels==7 && f->pending && scene.failed);
    scene.failed=0;unavailable=0;
    assert(halo_vk_scene_query_result(0,&pixels));assert(pixels==0 && !f->pending);
    /* A late completion from an older batch must not overwrite the zero. */
    f=&scene.frames[0];f->pending=1;f->query_count=2;f->batch_count=1;f->batches[0].count=2;f->batches[0].serial=1;
    assert(halo_vk_scene_query_result(0,&pixels));assert(pixels==0);
    f->pending=1;
    assert(!halo_vk_scene_close());assert(waits==1 && halo_vk_scene_owned() && f->pending);
    u.width=u.height=2;u.depth=u.faces=u.levels=1;u.format=VK_FORMAT_R8G8B8A8_UNORM;u.pixels=texels;u.bytes=sizeof(texels);
    assert(texture_upload_valid(&u,0));
    u.bytes--;assert(!texture_upload_valid(&u,0));u.bytes++;
    u.offsets[0][0]=4;assert(!texture_upload_valid(&u,0));u.offsets[0][0]=0;
    u.faces=7;assert(!texture_upload_valid(&u,0));u.faces=1;
    u.levels=17;assert(!texture_upload_valid(&u,0));u.levels=1;
    assert(!texture_upload_valid(&u,SCENE_IMAGES));
    puts("scene completion ownership and texture bounds PASS");return 0;
}
