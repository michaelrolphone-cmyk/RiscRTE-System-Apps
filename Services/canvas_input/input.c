/* Foreground-only input dependencies. Raster/TIMER Canvas has no dependency
 * edge to this provider, touch or navigation. Acquisition is explicit by Home. */
#include "RiscCanvasInputV1.h"
#include "RiscProviderV2.h"
#include <string.h>
static const risc_canvas_binding_v1*canvas;
static const risc_touch_api_v1*touch;
static const risc_input_navigation_api_v1*navigation;
static bool started,retained;
static uint64_t active;
static int32_t activate(void*c,uint64_t session){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(!started||!session)return RISC_CANVAS_INVALID;
 if(active)return active==session?RISC_CANVAS_OK:RISC_CANVAS_BUSY;
 int rc=canvas->bind_inputs(canvas->base.context,session,touch,navigation);
 if(rc==RISC_CANVAS_RETAINED)retained=true;
 if(rc==RISC_CANVAS_OK)active=session;
 return rc;
}
static int32_t deactivate(void*c,uint64_t session){
 (void)c;if(retained)return RISC_CANVAS_RETAINED;
 if(!active)return RISC_CANVAS_OK;
 if(session!=active)return RISC_CANVAS_STALE;
 int rc=canvas->unbind_inputs(canvas->base.context,session);
 if(rc==RISC_CANVAS_RETAINED)retained=true;
 if(rc==RISC_CANVAS_OK)active=0;
 return rc;
}
static bool start(const risc_provider_dependency_v1*deps,size_t count){
 if(started||active||retained||!deps)return false;
 const risc_canvas_api_v1*core=NULL;
 for(size_t i=0;i<count;i++){
  if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
  for(size_t j=0;j<i;j++)if(!strcmp(deps[i].capability_id,deps[j].capability_id))return false;
  if(!strcmp(deps[i].capability_id,RISC_CANVAS_CAPABILITY))core=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.touch.raw"))touch=deps[i].api;
  else if(!strcmp(deps[i].capability_id,"input.navigation"))navigation=deps[i].api;
  else return false;
 }
 canvas=risc_canvas_binding_get_v1(core);
 if(!canvas||(!touch&&!navigation))return false;
 started=true;return true;
}
static bool quiesce(void){return !active&&!retained;}
static void stop(void){if(!quiesce())return;started=false;canvas=NULL;touch=NULL;navigation=NULL;}
static const risc_canvas_input_v1 api={1,sizeof(api),RISC_CANVAS_INPUT_TAG,NULL,activate,deactivate};
static const risc_driver_v2 driver={2,sizeof(driver),"canvas-input",RISC_CANVAS_INPUT_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
