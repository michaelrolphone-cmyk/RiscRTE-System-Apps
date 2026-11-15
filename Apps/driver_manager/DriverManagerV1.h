#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <RiscStorageVolumeV1.h>
#ifdef __cplusplus
extern "C" {
#endif
#define DRIVER_MANAGER_CAPABILITY "software.driver-manager"
#define DRIVER_MANAGER_API_VERSION 1u
#define DRIVER_MANAGER_ROWS_MAX 65u
enum { DRIVER_MANAGER_ONLINE=0, DRIVER_MANAGER_INBOX=1, DRIVER_MANAGER_RECOVERY=2 };
enum { DRIVER_MANAGER_NONE=0, DRIVER_MANAGER_INSTALL=1, DRIVER_MANAGER_UNINSTALL=2, DRIVER_MANAGER_RETRY=3, DRIVER_MANAGER_DISCARD=4 };
enum { DRIVER_MANAGER_VALID=1u, DRIVER_MANAGER_CAN_INSTALL=2u, DRIVER_MANAGER_CAN_UPDATE=4u,
 DRIVER_MANAGER_CAN_UNINSTALL=8u, DRIVER_MANAGER_CAN_RETRY=16u, DRIVER_MANAGER_CAN_DISCARD=32u };
enum { DRIVER_MANAGER_OK=0, DRIVER_MANAGER_WORKING=1, DRIVER_MANAGER_NEEDS_NETWORK=2,
 DRIVER_MANAGER_UNAVAILABLE=-1, DRIVER_MANAGER_INVALID=-2, DRIVER_MANAGER_RETAINED=-3,
 DRIVER_MANAGER_STALE=-4 };
enum { DRIVER_MANAGER_CUSTODY_NONE=0, DRIVER_MANAGER_CUSTODY_PENDING=1, DRIVER_MANAGER_CUSTODY_RETAINED=2 };
typedef struct {
 uint64_t token;
 uint32_t flags;
 char id[64],version[32],installed_version[32],label[96],detail[160];
} driver_manager_row_v1;
typedef struct {
 uint32_t generation,index,source;
 uint64_t token;
 char id[64],version[32];
} driver_manager_selection_v1;
typedef struct {
 uint32_t generation,count,source,available_sources;
 int32_t result;
 uint32_t operation,custody;
 char status[160];
} driver_manager_snapshot_v1;
/* A view implemented by the existing package owner, never an independent
 * installed.apps cache/journal owner. Metadata calls only copy bounded RAM.
 * Rows/tokens are provider-owned and immutable until generation changes.
 * Every refresh/source change/action invalidates prior selections. Generation
 * and tokens must not wrap/reuse during an invocation: exhaustion fails closed.
 * Empty ID is permitted for an interrupted-download recovery row.
 *
 * start validates the complete selection again and copies it before returning.
 * confirmed is a UI assertion, not authority: the existing native installation
 * policy and custody transactions remain mandatory. UNINSTALL and DISCARD
 * require a separate explicit confirmation; INSTALL never activates an ELF.
 * All mutation paths revalidate immutable source identity at execution time.
 *
 * WORKING and RETAINED preserve all borrowed grants. Only successful cancel
 * with custody NONE permits caller teardown. Cancellation never means discard.
 * A failed call is not evidence that asynchronous work did not execute.
 * step submits/collects copied worker operations; it must not do synchronous
 * filesystem/network work on the UI thread. network_ready uses the caller's
 * existing Wi-Fi/UTC flow and grants no additional authority.
 */
typedef struct {
 uint32_t api_version,struct_size;void *context;
 bool (*snapshot)(void*,driver_manager_snapshot_v1*);
 bool (*row_get)(void*,uint32_t generation,uint32_t index,driver_manager_row_v1*);
 int32_t (*refresh)(void*,uint32_t source);
 int32_t (*start)(void*,uint32_t operation,const driver_manager_selection_v1*,bool confirmed);
 int32_t (*step)(void*,const risc_storage_volume_api_v1*);
 int32_t (*cancel)(void*);
 int32_t (*network_ready)(void*,uint64_t utc_seconds);
} driver_manager_v1;
#define DRIVER_MANAGER_V1_SIZE (offsetof(driver_manager_v1,network_ready)+sizeof(((driver_manager_v1*)0)->network_ready))
#ifdef __cplusplus
}
#endif
