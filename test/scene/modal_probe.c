#include <stdlib.h>
static int modal_test_fail_next;
static void *modal_test_malloc(size_t size){
 if(modal_test_fail_next){modal_test_fail_next=0;return NULL;}return malloc(size);
}
#define malloc modal_test_malloc
#include "telemetry_probe.c"
#undef malloc
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
 if(!page_live&&!page_frozen){page_capacity=32;page_live=malloc(32);page_frozen=malloc(32);}
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
