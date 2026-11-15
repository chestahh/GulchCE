#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "posix.h"
#define READ_BOUNCE_SIZE 16
#define O_RDONLY 0
#define O_CLOEXEC 0
#define SDL_IO_SEEK_SET 0
typedef long ssize_t;
typedef long long Sint64;
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static void pthread_mutex_lock(int *m){assert(!*m);*m=1;}
static void pthread_mutex_unlock(int *m){assert(*m);*m=0;}
static struct {long file_count;struct {char *path;} files[12];} port_mods;
static struct {unsigned char data[64];struct posix_file_information info;int exists;} disk[12];
static struct {int active,file;unsigned offset;unsigned char data[64];} handles[32];
static int opens,closes,stats,reads,fail_open,fail_read,fail_seek,fail_stat,unsupported,bounce,writes;
static int path_index(char const *p){return atoi(p);}
static int file_open(char const *p,int flags){
    int i,f=path_index(p);(void)flags;opens++;
    if(fail_open || !disk[f].exists)return -1;
    for(i=0;i<32;i++)if(!handles[i].active)break;
    assert(i<32);handles[i].active=1;handles[i].file=f;handles[i].offset=0;
    memcpy(handles[i].data,disk[f].data,64);return i;
}
static int file_close(int fd){assert(handles[fd].active);handles[fd].active=0;closes++;return 0;}
static ssize_t file_read(int fd,void *p,unsigned long n){
    unsigned left;int f=handles[fd].file;assert(handles[fd].active);reads++;
    if(fail_read)return -1;
    left=disk[f].info.size_low-handles[fd].offset;
    if(n>left)n=left;
    memcpy(p,handles[fd].data+handles[fd].offset,n);handles[fd].offset+=(unsigned)n;return (ssize_t)n;
}
int posix_stat(char const *p,struct posix_file_information *out){
    int f=path_index(p);stats++;if(fail_stat || !disk[f].exists)return -1;
    *out=disk[f].info;if(unsupported)out->flags=0;return 0;
}
#ifndef HALO_WINDOWS
int posix_fstat(int fd,struct posix_file_information *out){*out=disk[handles[fd].file].info;return 0;}
int posix_seek(int fd,posix_long low,posix_long high,int whence,posix_ulong *ol,posix_ulong *oh){
    (void)whence;(void)high;if(fail_seek || (posix_ulong)low>64)return -1;
    handles[fd].offset=(unsigned)low;*ol=(posix_ulong)low;*oh=0;return 0;
}
#define open file_open
#define close file_close
#define read file_read
#else
typedef struct {int fd;} SDL_IOStream;
static SDL_IOStream streams[32];
static SDL_IOStream *SDL_IOFromFile(char const *p,char const *mode){int fd=file_open(p,0);(void)mode;if(fd<0)return NULL;streams[fd].fd=fd;return &streams[fd];}
static Sint64 SDL_SeekIO(SDL_IOStream *s,Sint64 offset,int whence){(void)whence;if(fail_seek || offset>64)return -1;handles[s->fd].offset=(unsigned)offset;return offset;}
static unsigned long SDL_ReadIO(SDL_IOStream *s,void *p,unsigned long n){ssize_t r=file_read(s->fd,p,n);return r<0 ? 0 : (unsigned long)r;}
static void SDL_CloseIO(SDL_IOStream *s){file_close(s->fd);}
#endif
static int platform_is_contiguous(void *p){(void)p;return bounce;}
static void memory_watch_prepare_write(void *p,unsigned long n){(void)p;(void)n;writes++;}
#include "under_test.inc"
static int active(void){int i,n=0;for(i=0;i<32;i++)n+=handles[i].active;return n;}
int main(void){
    char names[12][4];unsigned char out[64];int i,n;
    port_mods.file_count=12;
    for(i=0;i<12;i++){
        sprintf(names[i],"%d",i);port_mods.files[i].path=names[i];disk[i].exists=1;
        disk[i].info.flags=_posix_file_has_identity;disk[i].info.inode_low=i+1;
        disk[i].info.size_low=64;disk[i].info.modification_seconds=1;disk[i].info.creation_seconds=1;
        memset(disk[i].data,'a'+i,64);
    }
    for(i=0;i<300;i++){assert(port_mods_file_read(0,0,out,16));assert(out[0]=='a');}
#ifdef HALO_WINDOWS
    assert(opens==300 && closes==300 && active()==0);puts("Windows: original 300 opens/300 closes retained");
#else
    assert(opens==1 && closes==0 && stats==300 && active()==1);
    puts("POSIX: 300 reads -> 1 open, 300 fresh metadata checks");
#endif
    /* Atomic replacement with the same size and modification time. */
    disk[0].info.inode_low=100;memset(disk[0].data,'R',64);
    assert(port_mods_file_read(0,0,out,16) && out[0]=='R');
    disk[0].info.creation_nanoseconds++;memset(disk[0].data,'E',64);
    assert(port_mods_file_read(0,0,out,16) && out[0]=='E');
    disk[0].exists=0;assert(!port_mods_file_read(0,0,out,16));disk[0].exists=1;
    assert(port_mods_file_read(0,0,out,16));
    fail_read=1;assert(!port_mods_file_read(0,0,out,16));fail_read=0;
    assert(port_mods_file_read(0,0,out,16));
    fail_seek=1;assert(!port_mods_file_read(0,0,out,16));fail_seek=0;
    port_mods_files_close();assert(active()==0);
    fail_open=1;assert(!port_mods_file_read(0,0,out,16));fail_open=0;
    fail_stat=1;assert(port_mods_file_read(0,0,out,16));assert(active()==0);fail_stat=0;
    unsupported=1;assert(port_mods_file_read(0,0,out,16));assert(active()==0);unsupported=0;
    bounce=1;assert(port_mods_file_read(0,0,out,64) && writes==4);bounce=0;
    disk[0].info.size_low=8;assert(!port_mods_file_read(0,0,out,16));disk[0].info.size_low=64;
    for(i=0;i<12;i++)assert(port_mods_file_read(i,0,out,16));
#ifndef HALO_WINDOWS
    assert(active()==8);n=opens;assert(port_mods_file_read(0,0,out,16) && opens==n+1);
    open_files_clock=~0ul;assert(port_mods_file_read(0,0,out,16));
#else
    n=opens;assert(active()==0 && n>0);
#endif
    port_mods_files_close();assert(active()==0);
    assert(!port_mods_file_read(-1,0,out,16));assert(!port_mods_file_read(12,0,out,16));
    port_mods.file_count=0;assert(!port_mods_file_read(0,0,out,16));
    assert(!open_files_lock);return 0;
}
