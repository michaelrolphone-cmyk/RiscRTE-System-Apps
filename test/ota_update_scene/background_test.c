/* Reuse the verified controller endpoints; no replacement controller or stub. */
#define main firmware_generic_background_tests
#include "controller_test.c"
#undef main
#include "../../Apps/ota_update_scene_background.c"
static bool root_safe=true;
bool ota_update_scene_background_safe(void){return root_safe;}
bool ota_update_scene_background_message(const char text[33]){(void)text;return false;}
bool ota_update_scene_background_launch(const char text[33]){(void)text;return false;}
bool ota_update_scene_background_workflow_ready(void){return true;}
void ota_update_scene_background_idle_update(uint32_t ms){(void)ms;}
static int civil_result;static unsigned civil_minute;static bool civil_corrupt;
static int32_t civil_read(void*c,risc_civil_time_v1*out){
 (void)c;touch();
 if(!civil_result||civil_corrupt)*out=(risc_civil_time_v1){.struct_size=sizeof(*out),.day=19700,.minute=civil_minute,.zone="Provider-selected-zone"};
 return civil_result;
}
static risc_civil_clock_api_v1 civil_api={1,sizeof(civil_api),NULL,civil_read};
static bool civil_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(strcmp(n,RISC_CIVIL_CLOCK_CAPABILITY))return acquire(n,v,id,g);
 touch();assert(v==1&&id==0);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=&civil_api,.slot=71,.generation=1};return true;
}
int main(void){
 assert(firmware_generic_background_tests()==0);
 reset(true);root_safe=true;
 svc.base.claim_preset=claim;svc.base.preset_result=preset_result;
 assert(ota_update_scene_background_init(&rt,alive,retain,NULL));
 assert(test_step(true));
 assert(ota_update_scene_background_capture_active());
 assert(ota_update_scene_background_pause());
 unsigned before=calls;root_safe=false;uint32_t timeout=123;
 for(unsigned i=0;i<4;i++){
  assert(ota_update_scene_background_capture());
  assert(ota_update_scene_background_step(i&1));
  assert(ota_update_scene_background_pause());
  assert(!ota_update_scene_background_close());
  assert(!ota_update_scene_background_idle_timeout(&timeout));
  assert(timeout==123&&!ota_update_scene_background_capture_active());
  assert(calls==before&&!retains);
 }
 root_safe=true;assert(ota_update_scene_background_close());
 /* An unsafe root that forgot to pause cannot silently keep capture running. */
 reset(false);assert(init());assert(test_step(true));root_safe=false;before=calls;
 assert(!ota_update_scene_background_capture());
 assert(!ota_update_scene_background_step(true));
 assert(!ota_update_scene_background_pause());
 assert(!ota_update_scene_background_close());assert(calls==before);
 root_safe=true;assert(ota_update_scene_background_close());
 /* Unsafe init must neither acquire capabilities nor read persisted policy. */
 reset(true);root_safe=false;before=calls;
 assert(!ota_update_scene_background_init(&rt,alive,retain,NULL));assert(calls==before);
 /* Provider-selected hour is authoritative for any zone, independent of RTC. */
 reset(false);root_safe=true;assert(init());rt.acquire=civil_acquire;
 for(unsigned minute=0;minute<1440;minute+=59){
  observation=(files_background_observation){0};civil_minute=minute;civil_result=0;civil_corrupt=false;
  assert(sample_local_time());assert(observation.local_time_valid&&observation.local_hour==minute/60);
 }
 observation=(files_background_observation){0};civil_result=-1;
 assert(sample_local_time()&&!observation.local_time_valid&&!retains);
 civil_corrupt=true;assert(!sample_local_time()&&retains);
 reset(false);assert(init());rt.acquire=civil_acquire;civil_corrupt=false;civil_result=0;civil_minute=1440;
 assert(!sample_local_time()&&retains);
 reset(false);assert(init());rt.acquire=civil_acquire;civil_result=-9;civil_corrupt=false;
 assert(!sample_local_time()&&retains);
 puts("Firmware background unsafe-boundary zero-I/O, safe pause/resume and init fences PASS");
 return 0;
}
