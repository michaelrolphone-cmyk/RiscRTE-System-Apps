#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static int modal_test_fail_next;
static unsigned memory_attempts,memory_fail_at,memory_live_count,memory_peak_count;
static size_t memory_live_bytes,memory_peak_bytes,memory_requests[32];
static struct {void *pointer;size_t bytes;} memory_blocks[5];
static void *modal_test_malloc(size_t size){
 assert(memory_attempts<32);memory_requests[memory_attempts++]=size;
 if(modal_test_fail_next||memory_attempts==memory_fail_at){modal_test_fail_next=0;return NULL;}
 void *p=malloc(size);assert(p);
 unsigned i;for(i=0;i<5;i++)if(!memory_blocks[i].pointer)break;assert(i<5);
 memory_blocks[i].pointer=p;memory_blocks[i].bytes=size;
 memory_live_bytes+=size;++memory_live_count;
 if(memory_live_bytes>memory_peak_bytes)memory_peak_bytes=memory_live_bytes;
 if(memory_live_count>memory_peak_count)memory_peak_count=memory_live_count;
 return p;
}
static void modal_test_free(void *p){
 if(!p)return;
 unsigned i;for(i=0;i<5;i++)if(memory_blocks[i].pointer==p)break;assert(i<5);
 memory_live_bytes-=memory_blocks[i].bytes;--memory_live_count;
 memory_blocks[i].pointer=NULL;memory_blocks[i].bytes=0;free(p);
}
#define free modal_test_free
#define malloc modal_test_malloc
#include "telemetry_probe.c"
#undef malloc
#undef free
static uint64_t modal_test_subscription;
static bool modal_test_navigation;
void modal_test_allocation_failure(void){
 modal_test_subscription=subscription;modal_test_navigation=nav_claimed;modal_test_fail_next=1;
}
static scene_model modal_test_expected;
static uint8_t *modal_test_live,*modal_test_frozen;
void modal_test_capture(void){
 assert(paused&&!pending&&!rasterizing);
 /* Exercise ownership transfer independently of page presentation geometry. */
 if(!page_live&&!page_frozen){page_capacity=32;page_live=modal_test_malloc(32);page_frozen=modal_test_malloc(32);}
 assert(page_live&&page_frozen&&page_capacity==32);
 memset(page_live,0x73,32);memset(page_frozen,0x28,32);
 modal_test_live=page_live;modal_test_frozen=page_frozen;
 modal_test_expected=logical_model;
}
void modal_test_restored(void){
 assert(paused&&active&&!modal_parent&&!pending&&!rasterizing&&!subscription&&!nav_claimed);
 assert(!memcmp(&modal_test_expected,&logical_model,sizeof(logical_model)));
 assert(page_live==modal_test_live&&page_frozen==modal_test_frozen&&page_capacity==32);
 for(unsigned i=0;i<32;i++){assert(page_live[i]==0x73);assert(page_frozen[i]==0x28);}
 assert(modal_focus_changed);
 assert(view_current()); /* Includes ordinary child-open rollback. */
 memset(&modal_test_expected,0,sizeof(modal_test_expected));
}

uint32_t modal_test_logical_digest(void){
 const unsigned char *p=(const void*)&logical_model;uint32_t hash=2166136261u;
 for(size_t i=0;i<sizeof(logical_model);i++)hash=(hash^p[i])*16777619u;
 return hash;
}

void modal_test_allocation_unchanged(void){
 assert(!modal_test_fail_next&&!modal_parent&&paused&&!pausing&&active);
 assert(subscription==modal_test_subscription&&nav_claimed==modal_test_navigation);
 assert(!memcmp(&modal_test_expected,&logical_model,sizeof(logical_model)));
 assert(page_live==modal_test_live&&page_frozen==modal_test_frozen&&page_capacity==32);
}

void modal_memory_begin(unsigned fail_at){
 assert(!memory_live_count&&!memory_live_bytes);
 memory_attempts=memory_peak_count=0;memory_peak_bytes=0;memory_fail_at=fail_at;
 memset(memory_requests,0,sizeof(memory_requests));
}
size_t modal_memory_checkpoint_bytes(void){return sizeof(modal_checkpoint);}
void modal_memory_live(unsigned count,size_t bytes){
 assert(memory_live_count==count&&memory_live_bytes==bytes);
}
void modal_memory_end(unsigned attempts,size_t pixel,unsigned parent_page,unsigned child_page){
 assert(!memory_live_count&&!memory_live_bytes&&memory_attempts==attempts);
 if(parent_page){assert(memory_requests[0]==pixel&&memory_requests[1]==pixel);}
 unsigned checkpoint=parent_page?2:0;
 if(attempts>checkpoint)assert(memory_requests[checkpoint]==sizeof(modal_checkpoint));
 if(child_page&&attempts>checkpoint+1){
  assert(memory_requests[checkpoint+1]==pixel&&memory_requests[checkpoint+2]==pixel);
 }
 if(!memory_fail_at){
  assert(memory_peak_count==1+2*parent_page+2*child_page);
  assert(memory_peak_bytes==sizeof(modal_checkpoint)+2*pixel*(parent_page+child_page));
 }
 printf("Modal allocation sequence host_checkpoint=%zu pixel=%zu parent_page=%u child_page=%u fail_at=%u attempts=%u peak_payload=%zu peak_blocks=%u final_live=0 PASS\n",
        sizeof(modal_checkpoint),pixel,parent_page,child_page,memory_fail_at,attempts,memory_peak_bytes,memory_peak_count);
}
