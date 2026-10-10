#include "RiscRuntimeV1.h"
#include "RiscTextEntryV1.h"
#include "RiscProviderPromotionV1.h"
#include <assert.h>
#include <string.h>
extern unsigned text_runtime_entry(void);
extern bool text_runtime_paper(void);
extern bool text_runtime_arm_retention(void);
extern const char *text_runtime_mode(void);
extern void text_runtime_control(unsigned);
extern unsigned text_runtime_frames(void);
extern void text_runtime_app_event(unsigned,const void*);
extern void text_runtime_check_token(uint64_t);
static unsigned witness;
__attribute__((constructor)) static void load(void){text_runtime_app_event(1,&witness);}
__attribute__((destructor)) static void unload(void){text_runtime_app_event(2,&witness);}
static risc_text_entry_state_v1 poll(const risc_text_entry_api_v1*a,uint64_t s){risc_text_entry_state_v1 v={.struct_size=sizeof(v)};assert(a->poll(a->context,s,&v)==0);return v;}
static void accepted_pending(const risc_text_entry_api_v1*a,uint64_t s){
 unsigned before=text_runtime_frames(),waits=0;text_runtime_control(4);
 do{risc_text_entry_state_v1 v=poll(a,s);assert(v.flags&RISC_TEXT_ENTRY_PRESENTING);assert(++waits<2048);}while(text_runtime_frames()==before);
}
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
 const risc_text_entry_api_v1*a=g.api;assert(a&&a->api_version==1&&a->struct_size>=sizeof(*a));
 risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=16,.label="Name"};strcpy(q.text,invocation==1?"first":"fresh");
 if(!strncmp(mode,"fast",4)){q.capacity=72;q.text[0]=0;}
 bool home_reason=!strncmp(mode,"reason-",7);
 if(home_reason){assert(risc_text_entry_home_reason(a));q.reserved=strcmp(mode,"reason-legacy-home")?RISC_TEXT_ENTRY_REQUEST_HOME_REASON:0;}
 uint64_t s=0;int opened=a->open(a->context,&q,&s);
 if(!strncmp(mode,"native-",7)){
  if(!risc_runtime_get_api(1)){assert(opened==RISC_TEXT_ENTRY_RETAINED);return;}
  assert(opened==RISC_TEXT_ENTRY_OK&&s);
  for(unsigned i=0;i<256;i++){
   risc_text_entry_state_v1 v={.struct_size=sizeof(v)};int rc=a->poll(a->context,s,&v);
   if(!risc_runtime_get_api(1)){assert(rc==RISC_TEXT_ENTRY_RETAINED);return;}
   assert(rc==RISC_TEXT_ENTRY_OK);
  }
  int rc=a->close(a->context,s);assert(!risc_runtime_get_api(1)&&rc==RISC_TEXT_ENTRY_RETAINED);return;
 }
 assert(opened==0&&s);memset(&q,0xa5,sizeof(q));text_runtime_check_token(s);
 for(unsigned i=0;i<4;i++)assert(!strcmp(poll(a,s).text,!strncmp(mode,"fast",4)?"":invocation==1?"first":"fresh"));
 if(!strncmp(mode,"fast",4)){
  char expected[72]="Q";text_runtime_control(10);assert(!strcmp(poll(a,s).text,expected));
  text_runtime_control(11);
  const char *burst="QQWEWQ";
  for(unsigned k=0;k<6;k++){size_t n=strlen(expected);expected[n]=burst[k];expected[n+1]=0;risc_text_entry_state_v1 v=poll(a,s);assert(!strcmp(v.text,expected));assert(v.flags&RISC_TEXT_ENTRY_PRESENTING);}
  for(unsigned k=0;k<40;k++){text_runtime_control(100+k%2);size_t n=strlen(expected);expected[n]=k%2?'W':'Q';expected[n+1]=0;assert(!strcmp(poll(a,s).text,expected));}
  text_runtime_control(12);assert(!strcmp(poll(a,s).text,expected));
  text_runtime_control(5);assert(!strcmp(poll(a,s).text,expected));
  text_runtime_control(13);strcat(expected,"Q");assert(!strcmp(poll(a,s).text,expected));
  for(unsigned k=0;k<4;k++)poll(a,s);
  text_runtime_control(14);assert(!strcmp(poll(a,s).text,expected));
  text_runtime_control(15);strcat(expected,"1");assert(!strcmp(poll(a,s).text,expected));
  for(unsigned k=0;k<15;k++){text_runtime_control(15);strcat(expected,"1");assert(!strcmp(poll(a,s).text,expected));}
  text_runtime_control(5);for(unsigned k=0;k<4;k++)poll(a,s);
  text_runtime_control(15);strcat(expected,"1");assert(!strcmp(poll(a,s).text,expected));
  if(text_runtime_paper()){
   text_runtime_control(19);expected[0]=0;assert(!strcmp(poll(a,s).text,expected));
   text_runtime_control(15);strcpy(expected,"1");assert(!strcmp(poll(a,s).text,expected));
  }
  if(!strcmp(mode,"fast-cancel")){text_runtime_control(6);assert(poll(a,s).state==RISC_TEXT_ENTRY_CANCELLED);}
  else {text_runtime_control(16);assert(poll(a,s).state==RISC_TEXT_ENTRY_ACCEPTED);}
  text_runtime_control(100);risc_text_entry_state_v1 final=poll(a,s);assert(!strcmp(final.text,expected));
  text_runtime_control(5);int rc;unsigned tries=0;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN){r->yield_ms(1);assert(++tries<10);}assert(rc==0);
  assert(r->release(&g));if(invocation==1)assert(r->request_launch("child.elf"));return;
 }
 if(!strncmp(mode,"reason-race-",12)){
  text_runtime_control(1);poll(a,s);for(unsigned i=0;i<4;i++)poll(a,s);
  bool accept=!strcmp(mode,"reason-race-accept-home");text_runtime_control(accept?20:21);
  risc_text_entry_state_v1 v=poll(a,s);assert(v.state==(accept?RISC_TEXT_ENTRY_ACCEPTED:RISC_TEXT_ENTRY_CANCELLED)&&!(v.flags&RISC_TEXT_ENTRY_HOME_CANCEL));
  int rc;unsigned tries=0;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN){r->yield_ms(1);assert(++tries<10);}assert(rc==0);
  assert(r->release(&g));if(invocation==1)assert(r->request_launch("child.elf"));return;
 }
 if(home_reason){
  bool home=strstr(mode,"home")!=NULL,busy=strstr(mode,"pending")!=NULL;
  if(busy)accepted_pending(a,s);
  text_runtime_control(home?7:6);risc_text_entry_state_v1 v=poll(a,s);
  assert(v.state==RISC_TEXT_ENTRY_CANCELLED&&!strcmp(v.text,invocation==1?"first":"fresh"));
  assert(!!(v.flags&RISC_TEXT_ENTRY_HOME_CANCEL)==(home&&strcmp(mode,"reason-legacy-home")));
  if(busy){assert(v.flags&RISC_TEXT_ENTRY_PRESENTING);assert(a->close(a->context,s)==RISC_TEXT_ENTRY_AGAIN);risc_text_entry_state_v1 blocked={.struct_size=sizeof(blocked)};assert(a->poll(a->context,s,&blocked)==RISC_TEXT_ENTRY_BUSY);text_runtime_control(5);}
  else {risc_text_entry_state_v1 frozen=poll(a,s);assert(frozen.state==v.state&&!strcmp(frozen.text,v.text)&&!!(frozen.flags&RISC_TEXT_ENTRY_HOME_CANCEL)==!!(v.flags&RISC_TEXT_ENTRY_HOME_CANCEL));}
  int rc;unsigned tries=0;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN){r->yield_ms(1);assert(++tries<10);}assert(rc==0);
  assert(r->release(&g)&&!g.api&&!g.slot&&!g.generation);if(invocation==1)assert(r->request_launch("child.elf"));return;
 }
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
  accepted_pending(a,s);assert(a->close(a->context,s)==RISC_TEXT_ENTRY_AGAIN);text_runtime_control(5);
 }else{
  for(unsigned i=0;i<5;i++)poll(a,s);
  text_runtime_control(6);assert(poll(a,s).state==RISC_TEXT_ENTRY_CANCELLED);
 }
 int rc;unsigned attempts=0;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN){r->yield_ms(1);assert(++attempts<10);}assert(rc==0);
 assert(r->release(&g)&&!g.api&&!g.slot&&!g.generation);
 if(invocation==1)assert(r->request_launch("child.elf"));
}
