/* Real masked text owner; every 60th synthetic touch explicitly clears the
 * bounded draft. No test bypasses text ownership or expands the password ABI. */
#include "RiscRuntimeV1.h"
#include "RiscTextEntryV1.h"
#include "SceneKeyboardV1.h"
#include <assert.h>
#include <string.h>
extern void scene_timing_begin(void);
extern unsigned scene_timing_mode(void);
extern bool scene_timing_finished(void);
extern void scene_timing_action(unsigned);
extern void scene_timing_complete(unsigned);
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};assert(r->acquire("ui.text-input",1,0,&g));
 const risc_text_entry_api_v1*a=g.api;assert(risc_text_entry_masked(a));
 risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=64,.reserved=RISC_TEXT_ENTRY_REQUEST_MASKED,.label="Wi-Fi password"};
 uint64_t s=0;assert(!a->open(a->context,&q,&s));scene_timing_begin();unsigned count=0,previous=0;
 while(!scene_timing_finished()){
  risc_text_entry_state_v1 e={.struct_size=sizeof(e)};int rc=a->poll(a->context,s,&e);assert(rc==RISC_TEXT_ENTRY_OK);
  unsigned n=(unsigned)strlen(e.text);
  if(n<previous){assert(n==0);scene_timing_action(RISC_SCENE_KEY_CLEAR);++count;}
  else for(unsigned i=previous;i<n;++i){if(scene_timing_mode()==1)assert(e.text[i]=='Q');scene_timing_action((unsigned char)e.text[i]);++count;}
  previous=n;
  r->yield_ms(1);
 }
 scene_timing_complete(count);int rc;while((rc=a->close(a->context,s))==RISC_TEXT_ENTRY_AGAIN)r->yield_ms(1);
 assert(rc==RISC_TEXT_ENTRY_OK);assert(r->release(&g));
}
