#include "RiscProviderV2.h"
#include "RiscInputNavigationV1.h"
#include <string.h>
extern void canvas_runtime_input_event(bool);
static bool poll(void*c,risc_input_navigation_frame_v1*out){(void)c;memset(out,0,sizeof(*out));return true;}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;return true;}
static bool reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 api={.api_version=1,.struct_size=sizeof(api),.poll=poll,.foreground=foreground,.reset=reset};
static bool start(const risc_provider_dependency_v1*d,size_t n){(void)d;if(n)return false;canvas_runtime_input_event(true);return true;}
static void stop(void){canvas_runtime_input_event(false);}
static bool quiet(void){return true;}
static const risc_driver_v2 driver={2,sizeof(driver),"navigation","input.navigation",1,&api,start,stop,quiet};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t v){return v==2?&driver:0;}
