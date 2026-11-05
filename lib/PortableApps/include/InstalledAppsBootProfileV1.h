#pragma once
#include "InstalledAppsAutomaticV1.h"
#define INSTALLED_APPS_BOOT_PROFILE_TAG UINT32_C(0x49425031)
#define INSTALLED_APPS_NEEDS_PROFILE_WIFI 3
#define INSTALLED_APPS_PROFILE_QUEUED 4
/* Additive suffix: only a selected product build exposes native next-boot
 * requests. Wi-Fi comes from the foreground owner's existing authorized saved
 * profile, never from the network catalog. The service does not retain the
 * caller's strings. A queued request is not an installed/activated profile. */
typedef struct {
 installed_apps_automatic_v1 base;
 uint32_t boot_profile_tag,boot_profile_version;
 int32_t (*supply_wifi)(void*,const char*ssid,const char*password);
} installed_apps_boot_profile_v1;
static inline const installed_apps_boot_profile_v1*installed_apps_boot_profile(const installed_apps_v1*a){
 if(!installed_apps_automatic(a)||a->struct_size<sizeof(installed_apps_boot_profile_v1))return NULL;
 const installed_apps_boot_profile_v1*p=(const installed_apps_boot_profile_v1*)a;
 return p->boot_profile_tag==INSTALLED_APPS_BOOT_PROFILE_TAG&&p->boot_profile_version==1&&p->supply_wifi?p:NULL;
}
