#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "PortableHomeBoot.h"
#include "PortableHomeSelection.h"
#include "PortableHomeBindings.h"
#include <string.h>
extern void home_faces_app_main(void);
extern void home_overview_app_main(void);
extern int home_overview_app_module_init(void);
extern void home_overview_app_module_fini(void);
static const risc_runtime_api_v1*runtime;
static risc_home_presentation_v1 policy;
static bool policy_loaded,terminal,overview_initialized;
static void retain(void){
 terminal=true;
 if(runtime&&runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&runtime->retain_invocation)(void)runtime->retain_invocation();
}
static bool bindings(void){
 if(terminal)return false;
 if(policy_loaded)return true;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!runtime->acquire(RISC_HOME_PRESENTATION_CAPABILITY,1,0,&grant))return false;
 const risc_home_presentation_v1*p=grant.api;
 p=p?risc_home_presentation_get_v1(p,p->struct_size):NULL;
 if(!p){if(!runtime->release(&grant))retain();return false;}
 memset(&policy,0,sizeof(policy));memcpy(&policy,p,p->struct_size<sizeof(policy)?p->struct_size:sizeof(policy));
 if(!runtime->release(&grant)){retain();return false;}
 policy_loaded=true;return true;
}
uint64_t portable_home_crash_namespace(void){return bindings()?policy.crash_namespace:0;}
unsigned portable_home_points_schema(void){return bindings()?risc_home_points_schema(&policy):RISC_HOME_POINTS_UNSPECIFIED;}
uint64_t portable_home_catalog_volume(void){return bindings()?policy.catalog_volume:0;}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_HOST,0};
__attribute__((visibility("default"))) int app_module_init(void){
 runtime=risc_runtime_get_api(1);
 return !runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release?-1:0;
}
__attribute__((visibility("default"))) void app_main(void){
 if(!runtime||terminal||portable_home_boot_begin(runtime)<0){terminal=true;return;}
 unsigned view=portable_home_boot_view();
 /* A recovered record chooses its owning semantic controller without loading
  * any presentation, storage, input or radio provider on the TIMER path. */
 if(!view){
  if(!bindings()){if(!terminal&&runtime->diagnostic)runtime->diagnostic("HOME presentation policy unavailable");return;}
  unsigned saved=0;int state=PORTABLE_HOME_VIEW_UNAVAILABLE;
  risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
  if(runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_HOME_VIEW_INSTANCE,&grant)){
   state=portable_home_view_load(grant.api,&saved);
   if(!runtime->release(&grant)){retain();return;}
  }
  view=portable_home_view_choose(&policy,state,saved);
 }
 if(view==RISC_HOME_VIEW_FACES)home_faces_app_main();
 else if(view==RISC_HOME_VIEW_OVERVIEW){
  if(home_overview_app_module_init())return;
  overview_initialized=true;home_overview_app_main();
 }
}
__attribute__((visibility("default"))) void app_module_fini(void){
 /* Runtime suppresses fini on retention; also keep our own bootstrap fence. */
 if(!terminal&&overview_initialized)home_overview_app_module_fini();
}
