#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "posix.h"
#define READ_BOUNCE_SIZE 16
static struct {long file_count;struct {char *path;} files[12];} port_mods;
static int opens,closes,bounce;
static int counted_open(char const *p,int f){opens++;return open(p,f);}
static int counted_close(int fd){closes++;return close(fd);}
#define open counted_open
#define close counted_close
static int platform_is_contiguous(void *p){(void)p;return bounce;}
static void memory_watch_prepare_write(void *p,unsigned long n){(void)p;(void)n;}
#include "under_test.inc"
static void write_file(char const *path,char value){FILE *f=fopen(path,"wb");int i;assert(f);for(i=0;i<64;i++)assert(fputc(value,f)!=EOF);assert(!fclose(f));}
static void *reader(void *unused){int i;char bytes[16];(void)unused;for(i=0;i<2000;i++){assert(port_mods_file_read(0,7,bytes,16));assert(bytes[0]=='C');}return NULL;}
static void *closer(void *unused){int i;(void)unused;for(i=0;i<2000;i++)port_mods_files_close();return NULL;}
int main(void){
    char out[64];int i,previous;struct stat st;struct timespec times[2];pthread_t a,b;
    struct posix_file_information info;
    port_mods.file_count=1;port_mods.files[0].path="caf\303\251.bitmap";
    write_file(port_mods.files[0].path,'A');
    for(i=0;i<300;i++){assert(port_mods_file_read(0,7,out,16));assert(out[0]=='A');}
    assert(opens==1 && closes==0);previous=opens;
    assert(!stat(port_mods.files[0].path,&st));times[0]=st.st_atim;times[1]=st.st_mtim;
    write_file("replacement",'B');assert(!utimensat(AT_FDCWD,"replacement",times,0));
    assert(!rename("replacement",port_mods.files[0].path));
    assert(port_mods_file_read(0,7,out,16) && out[0]=='B' && opens==previous+1);
    write_file(port_mods.files[0].path,'C');assert(!utimensat(AT_FDCWD,port_mods.files[0].path,times,0));
    assert(port_mods_file_read(0,7,out,16) && out[0]=='C');
    bounce=1;assert(port_mods_file_read(0,0,out,64) && out[63]=='C');bounce=0;
    assert(!posix_stat(port_mods.files[0].path,&info) && (info.flags&_posix_file_has_identity));
    assert(!pthread_create(&a,NULL,reader,NULL));assert(!pthread_create(&b,NULL,closer,NULL));
    assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));
    assert(!unlink(port_mods.files[0].path));assert(!port_mods_file_read(0,0,out,16));
    write_file(port_mods.files[0].path,'D');assert(port_mods_file_read(0,0,out,16) && out[0]=='D');
    assert(!truncate(port_mods.files[0].path,4));assert(!port_mods_file_read(0,0,out,16));
    port_mods_files_close();assert(opens==closes+1); /* one failed open after unlink */
    assert(!unlink(port_mods.files[0].path));puts("Real POSIX replacement, edits, short reads and concurrent cleanup passed");return 0;
}
