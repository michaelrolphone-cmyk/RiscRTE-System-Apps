/* ui.canvas provider: application content stays outside this provider.
 * Uses existing native display/touch/navigation ownership and presentation policy. */
#include "canvas_provider.h"
#include "RiscRuntimeV1.h"
#include "SceneProfileV1.h"
static canvas_provider provider;
static bool started;
static bool runtime_alive(void*unused){(void)unused;return risc_runtime_get_api(1)!=NULL;}
static bool runtime_cooperate(void*unused){
 (void)unused;const risc_runtime_api_v1*rt=risc_runtime_get_api(1);
 if(!rt||!rt->yield_ms)return false;
 rt->yield_ms(1);return risc_runtime_get_api(1)!=NULL;
}
static bool start(const risc_provider_dependency_v1*deps,size_t count){
 if(started||provider.frames.session||provider.frames.retained||!deps)return false;
 const risc_display_output_api_v1*display=NULL;const risc_touch_api_v1*touch=NULL;
 const risc_input_navigation_api_v1*navigation=NULL;const risc_scene_profile_v1*profile=NULL;
 for(size_t i=0;i<count;i++){
  if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
  for(size_t j=0;j<i;j++)if(!strcmp(deps[i].capability_id,deps[j].capability_id))return false;
  if(!strcmp(deps[i].capability_id,"display.output"))display=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.touch.raw"))touch=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.navigation"))navigation=deps[i].api;
  else if(!strcmp(deps[i].capability_id,RISC_SCENE_PROFILE_CAPABILITY))profile=deps[i].api;
  else return false;
 }
 if(!display||!profile||profile->api_version!=1||profile->struct_size!=sizeof(*profile)||profile->reserved||profile->display_rotation>270||profile->display_rotation%90||profile->touch_rotation>270||profile->touch_rotation%90)return false;
 if(display->api_version!=1||display->struct_size<sizeof(*display)||!display->get_info||!display->acquire||!display->release||!display->submit||!display->present_status)return false;
 if(touch&&(touch->api_version!=1||touch->struct_size<sizeof(*touch)||!touch->subscribe||!touch->unsubscribe||!touch->poll||!touch->next||!touch->snapshot))return false;
 if(navigation&&(navigation->api_version!=1||navigation->struct_size<sizeof(*navigation)||!navigation->poll||!navigation->foreground||!navigation->reset))return false;
 /* Provider startup binds dependencies before an application invocation exists.
  * API calls still require live Runtime authority through the installed hook. */
 canvas_provider_init(&provider,display,touch,profile->display_rotation/90);
 provider.preferred_format=profile->preferred_format;provider.navigation=navigation;provider.touch_rotation=profile->touch_rotation/90;
 provider.frames.alive=runtime_alive;provider.frames.cooperate=runtime_cooperate;
 started=true;return true;
}
static bool quiesce(void){return !provider.frames.retained&&!provider.frames.session&&!provider.frames.lease&&!provider.frames.receipts&&!provider.subscription&&!provider.claimed;}
static void stop(void){if(!quiesce())return;started=false;provider.frames.display=NULL;provider.touch=NULL;provider.navigation=NULL;}
static const risc_driver_v2 driver={2,sizeof(driver),"canvas-host",RISC_CANVAS_CAPABILITY,1,&provider.api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
