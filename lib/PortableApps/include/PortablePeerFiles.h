#pragma once
#include "RiscRuntimeV1.h"
#include "RiscStorageVolumeV1.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct portable_peer_files portable_peer_files;
enum {PEER_OFF,PEER_STARTING,PEER_DISCOVERING,PEER_PAIRING,PEER_CONNECTING,PEER_READY,PEER_WORKING,PEER_STOPPING,PEER_ERROR,PEER_RETAINED};
typedef struct {void*context;bool(*owner)(void*);void*(*allocate)(size_t);void(*deallocate)(void*);} portable_peer_hooks;
typedef struct {unsigned state,count;uint32_t pairing_generation,pairing_number;uint64_t progress;bool uncertain;char path[513],message[96],published_path[513];} portable_peer_status;
typedef struct {uint32_t peer_id;bool directory;uint64_t size;char name[128],path[513];} portable_peer_row;
portable_peer_files*portable_peer_create(const risc_runtime_api_v1*,const portable_peer_hooks*);
bool portable_peer_start(portable_peer_files*,uint64_t);
void portable_peer_tick(portable_peer_files*,uint64_t);
void portable_peer_status_get(const portable_peer_files*,portable_peer_status*);
bool portable_peer_row_get(const portable_peer_files*,unsigned,portable_peer_row*);
/* Commands only queue copied input; provider/storage I/O runs on later ticks. */
bool portable_peer_select(portable_peer_files*,uint32_t);
bool portable_peer_confirm(portable_peer_files*,uint32_t,uint32_t,bool);
bool portable_peer_list(portable_peer_files*,const char*);
bool portable_peer_download(portable_peer_files*,const char*,const risc_storage_volume_api_v1*,const char*);
bool portable_peer_upload(portable_peer_files*,const risc_storage_volume_api_v1*,const char*,const char*);
bool portable_peer_mutate(portable_peer_files*,const char*,const char*,const char*);
bool portable_peer_replace(portable_peer_files*,const risc_storage_volume_api_v1*,const char*,const char*);
void portable_peer_stop(portable_peer_files*);
bool portable_peer_destroy(portable_peer_files*);
#ifdef __cplusplus
}
#endif
