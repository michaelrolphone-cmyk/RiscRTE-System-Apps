#pragma once
#include "InstalledAppsFeedV1.h"
#define INSTALLED_APPS_AUTOMATIC_TAG UINT32_C(0x49415531)
/* The foreground owner starts after its first frame, supplies trusted UTC and
 * keeps its network/volume leases until cancel drains. The selected profile is
 * a list of package identities in the configured product feed, never authority.
 * Reconciliation preserves unlisted installed apps and all private namespaces. */
typedef struct {
 installed_apps_feed_v1 base;
 uint32_t automatic_tag,automatic_version;
 int32_t (*begin)(void*,const char*profile,uint64_t utc);
 int32_t (*step)(void*,const risc_storage_volume_api_v1*);
 int32_t (*cancel)(void*);
} installed_apps_automatic_v1;
static inline const installed_apps_automatic_v1*installed_apps_automatic(const installed_apps_v1*a){
 if(!installed_apps_feed(a)||a->struct_size<sizeof(installed_apps_automatic_v1))return NULL;
 const installed_apps_automatic_v1*p=(const installed_apps_automatic_v1*)a;
 return p->automatic_tag==INSTALLED_APPS_AUTOMATIC_TAG&&p->automatic_version==1&&p->begin&&p->step&&p->cancel?p:NULL;
}
