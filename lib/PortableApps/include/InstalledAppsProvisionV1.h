#pragma once
#include "InstalledAppsV1.h"
#define INSTALLED_APPS_PROVISION_TAG UINT32_C(0x49505631)
/* An explicitly selected catalog client may submit a bounded native install
 * descriptor obtained from its configured HTTPS feed. The provider copies it;
 * no caller pointer survives. Runtime admission, hash/import validation and
 * protected namespace/media journals remain mandatory. No feed grants power. */
typedef struct {
 installed_apps_v1 base;uint32_t provision_tag,provision_version;
 int32_t(*install_descriptor)(void*,const char*json);
} installed_apps_provision_v1;
static inline const installed_apps_provision_v1*installed_apps_provision(const installed_apps_v1*a){
 if(!a||a->api_version!=1||a->struct_size<sizeof(installed_apps_provision_v1))return NULL;
 const installed_apps_provision_v1*p=(const installed_apps_provision_v1*)a;
 return p->provision_tag==INSTALLED_APPS_PROVISION_TAG&&p->provision_version==1&&p->install_descriptor?p:NULL;
}
