/* Foreground-only input dependencies. Raster/TIMER Canvas has no dependency
 * edge to this provider, touch or navigation. Acquisition is explicit by Home. */
#include "RiscCanvasInputV1.h"
#include "RiscProviderV2.h"
#include "RiscTouchPowerV1.h"
#include "RiscRuntimeV1.h"
#include <string.h>
static const risc_canvas_api_v1*core;
static const risc_canvas_binding_v1*canvas;
static const risc_touch_api_v1*touch;
static const risc_input_navigation_api_v1*navigation;
static bool started,retained,preparing;
static uint64_t active;
static risc_canvas_input_v1 api;
static int32_t input_result(int32_t rc){if(rc<RISC_CANVAS_UNSUPPORTED||rc>RISC_CANVAS_AGAIN){retained=true;return RISC_CANVAS_RETAINED;}return rc;}
static int32_t activate(void*c,uint64_t session){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(!started||!session)return RISC_CANVAS_INVALID;
 if(preparing)return RISC_CANVAS_BUSY;
 if(active)return active==session?RISC_CANVAS_OK:RISC_CANVAS_BUSY;
 risc_canvas_open_input_binding_v1 request={.base={.struct_size=sizeof(request)},.binding_tag=RISC_CANVAS_BINDING_TAG,.binding_out=&canvas,.session=session};
 uint64_t queried=0;int rc=input_result(core->open(core->context,&request.base,&queried));
 if(rc)return rc;
 if(queried!=session||!risc_canvas_binding_get_v1(canvas)){retained=true;return RISC_CANVAS_RETAINED;}
 rc=input_result(canvas->bind_inputs(canvas->context,session,touch,navigation));
 if(rc==RISC_CANVAS_RETAINED)retained=true;
 if(rc==RISC_CANVAS_OK)active=session;
 return rc;
}
static int32_t deactivate(void*c,uint64_t session){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(preparing)return RISC_CANVAS_BUSY;
 if(!active)return RISC_CANVAS_OK;
 if(session!=active)return RISC_CANVAS_STALE;
 int rc=input_result(canvas->unbind_inputs(canvas->context,session));
 if(rc==RISC_CANVAS_RETAINED)retained=true;
 if(rc==RISC_CANVAS_OK)active=0;
 return rc;
}
static int32_t power_result(int32_t rc){
 if(!risc_runtime_get_api(1)||rc==RISC_TOUCH_POWER_RETAINED||rc<RISC_TOUCH_POWER_INVALID||rc>RISC_TOUCH_POWER_OK){retained=true;return RISC_CANVAS_RETAINED;}
 return rc==RISC_TOUCH_POWER_OK?RISC_CANVAS_OK:RISC_CANVAS_UNAVAILABLE;
}
static int32_t prepare_sleep(void*c,uint64_t session,uint32_t budget){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(!active||active!=session||budget>RISC_TOUCH_POWER_MAX_BUDGET_MS)return RISC_CANVAS_INVALID;
 const risc_touch_power_api_v1*power=risc_touch_power(touch);
 if(touch&&!power)return RISC_CANVAS_UNSUPPORTED;
 if(!preparing){int rc=input_result(canvas->unbind_inputs(canvas->context,session));if(rc){if(rc==RISC_CANVAS_RETAINED)retained=true;return rc;}}
 preparing=true;return power?power_result(power->prepare(touch->context,budget)):RISC_CANVAS_OK;
}
static int32_t resume_sleep(void*c,uint64_t session,uint32_t budget){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(!active||active!=session||budget>RISC_TOUCH_POWER_MAX_BUDGET_MS)return RISC_CANVAS_INVALID;
 if(!preparing)return RISC_CANVAS_OK;
 const risc_touch_power_api_v1*power=risc_touch_power(touch);
 if(touch&&!power)return RISC_CANVAS_UNSUPPORTED;
 int rc=power?power_result(power->resume(touch->context,budget)):RISC_CANVAS_OK;if(rc)return rc;
 rc=input_result(canvas->bind_inputs(canvas->context,session,touch,navigation));
 if(rc==RISC_CANVAS_RETAINED)retained=true;
 if(!rc)preparing=false;
 return rc;
}
static bool start(const risc_provider_dependency_v1*deps,size_t count){
 if(started||active||retained||!deps)return false;
 core=NULL;
 for(size_t i=0;i<count;i++){
  if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
  for(size_t j=0;j<i;j++)if(!strcmp(deps[i].capability_id,deps[j].capability_id))return false;
  if(!strcmp(deps[i].capability_id,RISC_CANVAS_RASTER_CAPABILITY))core=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.touch.raw"))touch=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.navigation"))navigation=deps[i].api;
  else return false;
 }
 if(!core||!risc_canvas_get_v1(core,core->struct_size)||(!touch&&!navigation))return false;
 api.flags=(!touch||risc_touch_power(touch))?RISC_CANVAS_INPUT_SLEEP_SUPPORTED:0;
 started=true;return true;
}
static bool quiesce(void){return !active&&!retained&&!preparing;}
static void stop(void){if(!quiesce())return;started=false;core=NULL;canvas=NULL;touch=NULL;navigation=NULL;}
static risc_canvas_input_v1 api={1,sizeof(api),RISC_CANVAS_INPUT_TAG,NULL,activate,deactivate,prepare_sleep,resume_sleep,0};
static const risc_driver_v2 driver={2,sizeof(driver),"canvas-input",RISC_CANVAS_INPUT_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
