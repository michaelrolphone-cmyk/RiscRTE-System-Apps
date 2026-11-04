/* A pure semantic deployment descriptor. The generated header is REQUIRED;
 * there is deliberately no product-name switch, hardware probe or fallback. */
#include "PortableSettingsRuntime.h"
#include "RiscProviderV2.h"
#include "settings_policy_config.h"
static bool active;
static const portable_settings_policy_v1 policy=SETTINGS_POLICY_INITIALIZER;
static bool start(const risc_provider_dependency_v1 *deps,size_t count){
 (void)deps;if(count||active)return false;active=true;return true;
}
static void stop(void){active=false;}
static bool quiesce(void){stop();return true;}
static const risc_driver_v2 driver={2,sizeof(driver),"settings-policy",PORTABLE_SETTINGS_POLICY_CAPABILITY,1,&policy,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t version){return version==2?&driver:NULL;}
