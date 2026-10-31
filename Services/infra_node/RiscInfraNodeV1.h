#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RISC_INFRA_NODE_CAPABILITY "infra.node"
enum {INFRA_OK=0,INFRA_AGAIN=1,INFRA_BLOCKED=-1,INFRA_RETAINED=-2,INFRA_INVALID=-3};
typedef struct {uint32_t struct_size;uint64_t internal_free,internal_largest,dma_free,dma_largest,psram_free,psram_largest;uint32_t stack_headroom;} risc_infra_memory_v1;
typedef struct {uint32_t struct_size,state,pending;uint64_t uptime_ms,last_sync_ms,storage_writes,storage_bytes;uint64_t minimum_internal,minimum_dma,minimum_psram,maximum_lag_ms;uint32_t minimum_stack;uint32_t requested,completed,timed_out,unknown,coalesced,gaps,diagnostic_events;char reason[128],participant_id[128],boot_id[65];uint32_t ready_for_boot_confirmation;uint32_t pending_action,rejected_action;char pending_operation_id[128],rejected_operation_id[128],rejection_code[64];} risc_infra_status_v1;
#define RISC_INFRA_STATUS_V1_SIZE offsetof(risc_infra_status_v1,pending_action)
/* Owner task only; no borrowed arguments retained. Bootstrap and advance each
 * perform one bounded state transition. shutdown returns AGAIN until all copied
 * requests and writes have settled; RETAINED forbids teardown and further I/O.
 * Work/control inputs come only from authenticated, pinned service responses. */
typedef struct {uint32_t api_version,struct_size;void* context;
 int32_t (*bootstrap)(void*);
 int32_t (*advance)(void*);
 int32_t (*memory)(void*,const risc_infra_memory_v1*);
 int32_t (*snapshot)(void*,risc_infra_status_v1*);
 int32_t (*shutdown)(void*);
 int32_t (*inventory)(void*,char*,uint32_t,uint32_t*);
 int32_t (*cancel)(void*); /* drain; never erases result/effect custody */
} risc_infra_node_v1;
#ifdef __cplusplus
}
#endif
