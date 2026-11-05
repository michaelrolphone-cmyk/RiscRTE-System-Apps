#include "telemetry_probe.c"
static scene_model modal_test_expected;
static uint8_t *modal_test_live,*modal_test_frozen;
void modal_test_capture(void){
 assert(paused&&!pending&&!rasterizing);
 /* Exercise ownership transfer independently of page presentation geometry. */
 assert(!page_live&&!page_frozen);page_capacity=32;
 page_live=malloc(32);page_frozen=malloc(32);assert(page_live&&page_frozen);
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
 memset(&modal_test_expected,0,sizeof(modal_test_expected));
}
