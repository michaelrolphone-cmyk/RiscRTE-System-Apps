#include "RiscRuntimeV1.h"
#include "RiscTextEntryV1.h"
#include "RiscProviderPromotionV1.h"
#include <assert.h>
#include <string.h>
extern unsigned text_runtime_entry(void);
extern bool text_runtime_arm_retention(void);
extern const char *text_runtime_mode(void);
extern void text_runtime_control(unsigned);
extern void text_runtime_app_event(unsigned,const void*);
extern void text_runtime_check_token(uint64_t);
static unsigned witness;
__attribute__((constructor)) static void load(void){text_runtime_app_event(1,&witness);}
__attribute__((destructor)) static void unload(void){text_runtime_app_event(2,&witness);}
static risc_text_entry_state_v1 poll(const risc_text_entry_api_v1*a,uint64_t s){risc_text_entry_state_v1 v={.struct_size=sizeof(v)};assert(a->poll(a->context,s,&v)==0);return v;}
__attribute__((visibility("default"))) void app_main(void){
 unsigned invocation=text_runtime_entry();if(invocation>2)return;
 const char*mode=text_runtime_mode();const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);
 if(invocation==1&&text_runtime_arm_retention()){
  risc_runtime_capability_v1 promotion={.struct_size=sizeof(promotion)};
  assert(r->acquire(RISC_PROVIDER_PROMOTION_CAPABILITY,1,0,&promotion));
  const risc_provider_promotion_api_v1*p=promotion.api;
  assert(p&&p->promote(p->context)==RISC_PROVIDER_PROMOTION_OK);
  assert(r->release(&promotion)&&!promotion.api&&!promotion.slot&&!promotion.generation);
 }
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};assert(r->acquire(RISC_TEXT_ENTRY_CAPABILITY,1,0,&g));
 const risc_text_entry_api_v1*a=g.api;assert(a&&a->api_version==1&&a->struct_size==sizeof(*a));
 risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=16,.label="Name"};strcpy(q.text,invocation==1?"first":"fresh");
 uint64_t s=0;int opened=a->open(a->context,&q,&s);
 if(!strncmp(mode,"native-",7)){
  if(!risc_runtime_get_api(1)){assert(opened==RISC_TEXT_ENTRY_RETAINED);return;}
  assert(opened==RISC_TEXT_ENTRY_OK&&s);
  for(unsigned i=0;i<4;i++){
   risc_text_entry_state_v1 v={.struct_size=sizeof(v)};int rc=a->poll(a->context,s,&v);
   if(!risc_runtime_get_api(1)){assert(rc==RISC_TEXT_ENTRY_RETAINED);return;}
   assert(rc==RISC_TEXT_ENTRY_OK);
  }
  int rc=a->close(a->context,s);assert(!risc_runtime_get_api(1)&&rc==RISC_TEXT_ENTRY_RETAINED);return;
 }
 assert(opened==0&&s);memset(&q,0xa5,sizeof(q));text_runtime_check_token(s);
 for(unsigned i=0;i<4;i++)assert(!strcmp(poll(a,s).text,invocation==1?"first":"fresh"));
 if(!strcmp(mode,"unclosed"))return; /* Eager pins preserve hosts; this pointer-free app may unload. */
 if(!strcmp(mode,"retained")){
  text_runtime_control(9);assert(a->close(a->context,s)==RISC_TEXT_ENTRY_RETAINED);
  assert(r->retain_invocation());return;
 }
 if(invocation==1&&!strcmp(mode,"hardware")){
  text_runtime_control(1);risc_text_entry_state_v1 v=poll(a,s);assert(v.flags&RISC_TEXT_ENTRY_HARDWARE);assert(!strcmp(v.text,"first"));
  text_runtime_control(2);v=poll(a,s);assert(!strcmp(v.text,"firsta"));
  text_runtime_control(3);v=poll(a,s);assert(!(v.flags&RISC_TEXT_ENTRY_HARDWARE));assert(!strcmp(v.text,"firsta"));
 }
 if(!strcmp(mode,"pending")){
  text_runtime_control(4);poll(a,s);assert(a->close(a->context,s)==RISC_TEXT_ENTRY_AGAIN);text_runtime_control(5);
 }else{
  for(unsigned i=0;i<5;i++)poll(a,s);
  text_runtime_control(6);assert(poll(a,s).state==RISC_TEXT_ENTRY_CANCELLED);
 }
 int rc;unsigned attempts=0;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN){r->yield_ms(1);assert(++attempts<10);}assert(rc==0);
 assert(r->release(&g)&&!g.api&&!g.slot&&!g.generation);
 if(invocation==1)assert(r->request_launch("child.elf"));
}
