#include <assert.h>
#include <stdio.h>
#include "PortableQuickActions.h"
#include "RiscRuntimeV1.h"
static bool native_custody_retained,failed;
static struct {pqa_state ui;} quick;
static const risc_runtime_api_v1 *rt;
static void portable_adapter_retain(void){native_custody_retained=true;}
#define PORTABLE_SHARED_HOME_CANVAS 1
#include "../../lib/PortableApps/src/home_settings_availability.inc"
static unsigned calls,releases;static bool missing,dirty,release_fail;static int snapshot_rc;
static uint64_t generation=1;static uint32_t available=RISC_KEY_VALUE_AVAILABLE;
static int32_t snapshot(void*c,risc_key_value_availability_state_v1*out){
 (void)c;++calls;*out=(risc_key_value_availability_state_v1){.struct_size=sizeof(*out),.namespace_id=1,.generation=generation,.availability=available};return snapshot_rc;
}
static const risc_key_value_availability_v1 api={.api_version=1,.struct_size=sizeof(api),.snapshot=snapshot};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(!strcmp(n,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY)&&v==1&&i==1);
 if(missing){if(dirty)g->slot=1;return false;}
 g->slot=g->generation=1;g->api=&api;return true;
}
static bool release(risc_runtime_capability_v1*g){++releases;if(release_fail)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static void reset(void){home_settings_status=(PortableSettingsAvailability){0};home_settings_state=RISC_KEY_VALUE_UNAVAILABLE;home_settings_generation=0;home_settings_status_missing=false;calls=releases=0;native_custody_retained=failed=missing=dirty=release_fail=false;snapshot_rc=0;generation=1;available=RISC_KEY_VALUE_AVAILABLE;rt=&runtime;}
int main(void){
 reset();missing=true;assert(portable_home_settings_poll()&&!native_custody_retained&&!calls&&!releases&&!portable_home_settings_available());
 assert(strstr(portable_home_settings_notice(),"UNAVAILABLE"));
 quick.ui.brightness_valid=quick.ui.volume_valid=quick.ui.dnd_valid=quick.ui.radios_valid=quick.ui.contexts_valid=true;
 assert(home_settings_filter_actions(PQA_BRIGHTNESS_COMMIT|PQA_WIFI|PQA_USB_TRANSFER)==PQA_USB_TRANSFER);
 assert(!quick.ui.brightness_valid&&!quick.ui.radios_valid);
 missing=false;assert(portable_home_settings_poll()&&portable_home_settings_available()&&portable_home_settings_generation()==1&&releases==1);
 missing=true;assert(portable_home_settings_poll()&&!portable_home_settings_available()&&home_settings_generation==1);
 missing=false;assert(portable_home_settings_poll()&&portable_home_settings_available()&&!native_custody_retained);
 generation=0;assert(!portable_home_settings_poll()&&native_custody_retained);
 reset();missing=dirty=true;assert(!portable_home_settings_poll()&&native_custody_retained&&!calls&&!releases);
 reset();snapshot_rc=-9;assert(!portable_home_settings_poll()&&native_custody_retained&&!releases);
 reset();release_fail=true;assert(!portable_home_settings_poll()&&native_custody_retained&&releases==1);
 puts("Home optional settings: clean absence, no writes, readonly controls, reappearance, monotonic generation and custody fences PASS");
}
