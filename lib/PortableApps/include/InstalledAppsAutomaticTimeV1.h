#pragma once
#include "InstalledAppsBootProfileV1.h"
#define INSTALLED_APPS_AUTOMATIC_TIME_TAG UINT32_C(0x49415431)
/* Explicit additive automatic mode. The product build supplies its one SNTP
 * hostname; the foreground owner keeps the existing network lease through
 * cancel. UTC remains private to each HTTPS session: no RTC/civil-time write.
 * Ordinary automatic.begin, feed.refresh and network_ready retain their
 * configured-UTC requirements. Missing support never selects an implicit host. */
typedef struct {
 installed_apps_boot_profile_v1 base;
 uint32_t automatic_time_tag,automatic_time_version;
 int32_t (*begin_with_time)(void*,const char*profile);
} installed_apps_automatic_time_v1;
static inline const installed_apps_automatic_time_v1*installed_apps_automatic_time(const installed_apps_v1*a){
 if(!installed_apps_boot_profile(a)||a->struct_size<sizeof(installed_apps_automatic_time_v1))return NULL;
 const installed_apps_automatic_time_v1*p=(const installed_apps_automatic_time_v1*)a;
 return p->automatic_time_tag==INSTALLED_APPS_AUTOMATIC_TIME_TAG&&p->automatic_time_version==1&&p->begin_with_time?p:NULL;
}
