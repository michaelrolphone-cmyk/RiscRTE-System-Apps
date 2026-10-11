#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <RiscStorageVolumeV1.h>
#ifdef __cplusplus
extern "C" {
#endif
#define INSTALLED_APPS_CAPABILITY "software.installed-apps"
#define INSTALLED_APPS_MAX 40u
#define INSTALLED_APPS_OK 0
#define INSTALLED_APPS_WORKING 1
#define INSTALLED_APPS_UNAVAILABLE (-1)
#define INSTALLED_APPS_INVALID (-2)
#define INSTALLED_APPS_RETAINED (-3)
typedef struct {
 char id[96],name[96],version[64],icon[24],launch_path[193];
 bool available;
} installed_app_v1;
typedef struct {
 uint32_t api_version,struct_size;void*context;
 /* Refresh loads only internal cached metadata and requests later discovery.
  * count/get never access a filesystem. */
 int32_t (*refresh)(void*);
 uint32_t (*generation)(void*);
 uint32_t (*count)(void*);
 bool (*get)(void*,uint32_t,installed_app_v1*);
 /* Nonblocking owner progress: copied submissions and result collection only.
  * The client owns the live SD grant through cancel or scan completion.
  * The volume worker executes filesystem I/O independently of input/display. */
 int32_t (*step)(void*,const risc_storage_volume_api_v1*);
 /* WORKING keeps the borrowed volume grant live; poll until OK. */
 int32_t (*cancel)(void*);
 int32_t (*remove)(void*,const char*id);
 /* Cached staged descriptors from Apps/<id>/install.json. Their get result's
  * launch_path is the descriptor identity, not an executable launch path.
  * Installation rereads it and validates the selected image exactly once. */
 uint32_t (*staged_count)(void*);
 bool (*staged_get)(void*,uint32_t,installed_app_v1*);
 int32_t (*install)(void*,const char*id,const char*version);

} installed_apps_v1;
#ifdef __cplusplus
}
#endif
