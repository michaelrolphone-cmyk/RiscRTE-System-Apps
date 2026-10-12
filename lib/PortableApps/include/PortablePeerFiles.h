#pragma once
#include "RiscRuntimeV1.h"
#include "RiscStorageVolumeV1.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct portable_peer_files portable_peer_files;
enum {PEER_OFF,PEER_STARTING,PEER_DISCOVERING,PEER_PAIRING,PEER_CONNECTING,PEER_READY,PEER_WORKING,PEER_STOPPING,PEER_ERROR,PEER_RETAINED};
enum {PEER_CAP_LIST=1u<<0,PEER_CAP_READ=1u<<1,PEER_CAP_HEAD=1u<<2,PEER_CAP_PUT=1u<<3,PEER_CAP_DELETE=1u<<4,PEER_CAP_MKCOL=1u<<5,PEER_CAP_MOVE=1u<<6,PEER_CAP_COPY=1u<<7};
enum {PORTABLE_PEER_PREVIEW_MAX=2048};
typedef struct {void*context;bool(*owner)(void*);void*(*allocate)(size_t);void(*deallocate)(void*);} portable_peer_hooks;
typedef struct {unsigned state,count;uint32_t pairing_generation,pairing_number;uint64_t progress;bool uncertain;char path[513],message[96],published_path[513];uint32_t capabilities;bool operation_ok;size_t preview_size;uint64_t preview_total,preview_offset;bool preview_truncated;uint32_t operation_generation,completed_generation;} portable_peer_status;
typedef struct {uint32_t peer_id;bool directory;uint64_t size;char name[128],path[513];} portable_peer_row;
portable_peer_files*portable_peer_create(const risc_runtime_api_v1*,const portable_peer_hooks*);
bool portable_peer_start(portable_peer_files*,uint64_t);
void portable_peer_tick(portable_peer_files*,uint64_t);
void portable_peer_status_get(const portable_peer_files*,portable_peer_status*);
/* Copied-state query: native network/transport custody blocks resident POLL
 * handoff until checked quiescence. No provider calls or cleanup. */
bool portable_peer_network_owned(const portable_peer_files*);
bool portable_peer_row_get(const portable_peer_files*,unsigned,portable_peer_row*);
/* Commands only queue copied input; provider/storage I/O runs on later ticks.
 * An accepted file command increments operation_generation immediately. It is
 * pending until completed_generation matches, even while state is PEER_READY
 * and a network lease is busy. Completion includes rejection/cancel/failure. */
bool portable_peer_select(portable_peer_files*,uint32_t);
bool portable_peer_confirm(portable_peer_files*,uint32_t,uint32_t,bool);
bool portable_peer_list(portable_peer_files*,const char*);
/* Queue a bounded raw-byte preview. No caller buffer is retained. Bytes are
 * copied only after successful completion; capacity must cover preview_size. */
bool portable_peer_preview(portable_peer_files*,const char*,size_t limit);
/* Offset previews skip the prefix cooperatively when the server has no Range
 * support. Offset is bounded to 64 MiB; normal request/session deadlines apply. */
bool portable_peer_preview_range(portable_peer_files*,const char*,uint64_t offset,size_t limit);
bool portable_peer_preview_get(const portable_peer_files*,void*,size_t capacity,size_t*size);
/* Queue request cancellation; cleanup remains tick-driven and keeps the mount. */
void portable_peer_cancel_operation(portable_peer_files*);
bool portable_peer_download(portable_peer_files*,const char*,const risc_storage_volume_api_v1*,const char*);
bool portable_peer_upload(portable_peer_files*,const risc_storage_volume_api_v1*,const char*,const char*);
bool portable_peer_mutate(portable_peer_files*,const char*,const char*,const char*);
bool portable_peer_replace(portable_peer_files*,const risc_storage_volume_api_v1*,const char*,const char*);
void portable_peer_stop(portable_peer_files*);
bool portable_peer_destroy(portable_peer_files*);
#ifdef __cplusplus
}
#endif
