#include "RiscHomePresentationV1.h"
#include "RiscProviderV2.h"
#ifndef HOME_POLICY_VIEW
#error "Choose an explicit semantic default view for this presentation provider"
#endif
#ifndef HOME_POLICY_CRASH_NAMESPACE
#error "Preserve the deployed Home crash-data binding"
#endif
#ifndef HOME_POLICY_ID
#error "Choose a provider package identity"
#endif
static bool started;
static bool start(const risc_provider_dependency_v1*deps,size_t count){(void)deps;if(started||count)return false;started=true;return true;}
static bool quiesce(void){return true;}
static void stop(void){started=false;}
static const risc_home_presentation_v1 policy={1,sizeof(policy),RISC_HOME_PRESENTATION_TAG,HOME_POLICY_VIEW,HOME_POLICY_CRASH_NAMESPACE,0};
static const risc_driver_v2 driver={2,sizeof(driver),HOME_POLICY_ID,RISC_HOME_PRESENTATION_CAPABILITY,1,&policy,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t version){return version==2?&driver:NULL;}
