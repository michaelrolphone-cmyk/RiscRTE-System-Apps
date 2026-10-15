#pragma once
#include "InstalledAppsProvisionV1.h"
#define INSTALLED_APPS_FEED_TAG UINT32_C(0x49464631)
/* A selected HTTPS catalog is data only. Installation remains subject to the
 * native graph, immutable image validation and durable media journal. */
typedef struct {
 installed_apps_provision_v1 base;
 uint32_t feed_tag,feed_version;
 int32_t (*refresh)(void*,uint64_t utc);
 int32_t (*step)(void*);
 int32_t (*cancel)(void*);
 uint32_t (*generation)(void*);
 uint32_t (*count)(void*);
 bool (*get)(void*,uint32_t,installed_app_v1*);
 int32_t (*install)(void*,uint32_t,uint32_t generation);
} installed_apps_feed_v1;
static inline const installed_apps_feed_v1*installed_apps_feed(const installed_apps_v1*a){
 if(!installed_apps_provision(a)||a->struct_size<sizeof(installed_apps_feed_v1))return NULL;
 const installed_apps_feed_v1*f=(const installed_apps_feed_v1*)a;
 return f->feed_tag==INSTALLED_APPS_FEED_TAG&&f->feed_version==1&&f->refresh&&f->step&&f->cancel&&f->generation&&f->count&&f->get&&f->install?f:NULL;
}
