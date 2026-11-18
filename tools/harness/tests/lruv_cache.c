#include "harness.h"
typedef void (*lruv_delete_block_proc)(long);
typedef boolean (*lruv_locked_block_proc)(long);
#include "types.inc"
#define actual_count count
#define data_verify(a) ((void)0)
#undef match_assert
#define match_assert(f,l,c) CHECK(c,"LRU invariant at %d",l)
#define NEXT_HOLE_INDEX(i) ((i)==MAXIMUM_LRUV_CACHE_HOLES-1?0:(i)+1)
#define lruv_cache_block_get(c,i) ((struct lruv_cache_block *)datum_get((c)->blocks,i))
static struct lruv_cache *active;
static int deleting_manually,deletions;
static boolean locked(long index) {return lruv_cache_block_get(active,index)->user_data!=0;}
static void deleted(long index) {
 struct lruv_cache_block *b=lruv_cache_block_get(active,index);
 CHECK(deleting_manually || (!locked(index)&&b->last_used_tick!=active->tick),"live/locked block evicted");deletions++;
}
#include "under_test.inc"
static unsigned seed=12345;
static unsigned random_next(void) {seed=seed*1664525u+1013904223u;return seed;}
static void validate(struct lruv_cache *c) {
 long index=c->first_block_index,previous=NONE,end=0,count=0;
 while(index!=NONE) {
  struct lruv_cache_block *b=lruv_cache_block_get(c,index);
  CHECK(b->previous_block_index==previous&&b->first_page_index>=end&&b->page_count>0,"ordered list");
  end=b->first_page_index+b->page_count;CHECK(end<=c->page_count,"pages bounded");
  previous=index;index=b->next_block_index;CHECK(++count<=c->blocks->maximum_count,"no cycle");
 }
 CHECK(count==c->blocks->count&&previous==c->last_block_index,"all live entries in list");
}
int main(void) {
 struct lruv_cache c;long blocks[8];
 memset(&c,0,sizeof(c));c.blocks=game_state_data_new("test",8,sizeof(struct lruv_cache_block));
 c.signature=LRUV_CACHE_SIGNATURE;c.page_count=64;c.tick=10;c.first_block_index=c.last_block_index=NONE;
 c.delete_block_proc=deleted;c.locked_block_proc=locked;active=&c;
 for(int i=0;i<8;i++) {blocks[i]=lruv_block_new(&c,8);CHECK(blocks[i]!=NONE,"fill cache");}
 CHECK(lruv_block_new(&c,8)==NONE,"current tick blocks protected");
 c.tick++;lruv_cache_block_get(&c,blocks[2])->user_data=1;
 deleting_manually=1;lruv_block_delete(&c,blocks[0]);lruv_block_delete(&c,blocks[4]);deleting_manually=0;
 CHECK(lruv_block_new(&c,16)!=NONE,"evict across fragmented region");validate(&c);
 CHECK(locked(blocks[2]),"locked block survived");
 for(int i=0;i<2000;i++) {
  c.tick++;long id=c.first_block_index;
  while(id!=NONE) {struct lruv_cache_block *b=lruv_cache_block_get(&c,id);b->user_data=(random_next()%5)==0;id=b->next_block_index;}
  id=lruv_block_new(&c,1+(random_next()%24));
  if(id!=NONE)CHECK(lruv_cache_block_get(&c,id)->last_used_tick==c.tick,"new block timestamp");
  validate(&c);
 }
 CHECK(deletions>100,"capacity and eviction exercised");free(c.blocks->data);free(c.blocks);return 0;
}
