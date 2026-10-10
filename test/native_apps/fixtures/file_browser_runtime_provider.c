/* A mapped ABI-v2 provider. Runtime owns admission/leases; only media is fake. */
#include <RiscProviderV2.h>
#include <RiscHardwareConfigV1.h>
#include <RiscStorageVolumeV1.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern bool file_browser_runtime_fault(const char *name);
extern const char *file_browser_runtime_mode(void);
extern void file_browser_runtime_event(const char *name);
static unsigned directory, file, cursor, offset;
static bool in_books;
static bool nested(void) {return !strncmp(file_browser_runtime_mode(),"nested-",7);}
static void volume_path(const char *path) {
    /* /sd belongs only to the broker. The provider accepts volume-relative
     * paths and must catch an accidental client-side VFS prefix. */
    assert(path && path[0]=='/' && strcmp(path,"/sd") && strncmp(path,"/sd/",4));
}
static bool ready(void *context) {(void)context;return !file_browser_runtime_fault("absent");}
static bool refresh(void *context) {(void)context;return !file_browser_runtime_fault("refresh-error");}
static bool label(void *context,char *out,size_t cap) {(void)context;snprintf(out,cap,"Fixture SD");return true;}
static bool stat_file(void *context,const char *path,uint64_t *size,bool *dir) {
    (void)context;volume_path(path);
    *dir=!strcmp(path,"/") || (nested() && !strcmp(path,"/Books"));
    if(!*dir && strcmp(path,nested()?"/Books/read.txt":"/read.txt"))return false;
    if(nested() && !*dir)file_browser_runtime_event("nested-stat");
    *size=*dir?0:5;return true;
}
static uint32_t dir_open(void *context,const char *path) {
    (void)context;volume_path(path);assert(!directory);
    in_books=nested() && !strcmp(path,"/Books");
    if(strcmp(path,"/") && !in_books)return 0;
    if(in_books)file_browser_runtime_event("nested-dir-open");
    directory=88;cursor=0;file_browser_runtime_event("dir-open");return directory;
}
static bool dir_next(void *context,uint32_t handle,risc_storage_dirent_v1 *entry) {
    (void)context;assert(handle==directory && directory);
    if(cursor++)return false;
    memset(entry,0,sizeof(*entry));
    entry->is_directory=nested() && !in_books;
    strcpy(entry->name,entry->is_directory?"Books":"read.txt");entry->size=entry->is_directory?0:5;return true;
}
static bool dir_checked(void *context,uint32_t handle) {
    (void)context;assert(handle==directory && directory);
    if(file_browser_runtime_fault("dir-close"))return false;
    directory=0;file_browser_runtime_event("dir-close");return true;
}
static void dir_close(void *context,uint32_t handle) {assert(dir_checked(context,handle));}
static uint32_t handle_error(void *context,uint32_t handle,bool is_directory) {
    (void)context;assert(handle==(is_directory?directory:file));
    return is_directory && file_browser_runtime_fault("listing-error");
}
static uint32_t file_open(void *context,const char *path,uint64_t *size) {
    (void)context;volume_path(path);assert(!file);
    if(strcmp(path,nested()?"/Books/read.txt":"/read.txt"))return 0;
    if(nested())file_browser_runtime_event("nested-file-open");
    file=77;offset=0;*size=5;file_browser_runtime_event("file-open");return file;
}
static size_t file_read(void *context,uint32_t handle,void *out,size_t size) {
    (void)context;assert(handle==file && file);if(size>5-offset)size=5-offset;
    memcpy(out,"hello"+offset,size);offset+=(unsigned)size;return size;
}
static bool file_close(void *context,uint32_t handle,bool commit) {
    (void)context;assert(handle==file && file && commit);
    if(file_browser_runtime_fault("file-close"))return false;
    file=0;file_browser_runtime_event("file-close");return true;
}
static bool remove_file(void *context,const char *path) {(void)context;volume_path(path);assert(!"unexpected mutation");return false;}
static bool error_text(void *context,char *out,size_t cap) {
    (void)context;snprintf(out,cap,"%s",file_browser_runtime_fault("absent")?"SD absent":file_browser_runtime_fault("refresh-error")?"SD refresh error":"");return true;
}
static bool start(const risc_provider_dependency_v1 *deps,size_t count) {
    assert(count==1 && !strcmp(deps[0].capability_id,"hardware.device"));
    const risc_hardware_device_v1 *hardware=deps[0].api;assert(hardware && hardware->instance_id==9);
    assert(!directory && !file);file_browser_runtime_event("provider-start");return true;
}
static bool quiesce(void) {assert(!directory && !file);file_browser_runtime_event("provider-quiesce");return true;}
static void stop(void) {assert(!directory && !file);file_browser_runtime_event("provider-stop");}
static const risc_storage_volume_api_v1_ext api={
    .base={1,sizeof(api),NULL,refresh,ready,label,stat_file,dir_open,dir_next,dir_close,
           file_open,file_read,NULL,NULL,file_close,remove_file,error_text},
    .dir_close_checked=dir_checked,.handle_error=handle_error
};
static const risc_driver_v2 driver={2,sizeof(driver),"files-runtime-volume","storage.volume",1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {return abi==2?&driver:NULL;}
