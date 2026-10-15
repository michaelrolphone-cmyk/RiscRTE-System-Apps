#pragma once
/* Transitional internal compatibility view for the shared browser/WebDAV algorithms. The
 * only external contract is storage.files: no provider lifecycle, hardware,
 * mount, power or export operation is forwarded by this view. */
#include "RiscStorageFilesV1.h"
#include "RiscStorageVolumeFsV1.h"
#include <string.h>
#include <stdio.h>
#define PORTABLE_FILES_HANDLES 4u
typedef struct {
 uint32_t token; int32_t error; bool directory,abort_created;
 char created_path[RISC_STORAGE_FILES_PATH_MAX];
} portable_files_handle;
typedef struct {
 risc_storage_volume_api_v1_fs view;
 const risc_storage_files_v1 *files;
 portable_files_handle handles[PORTABLE_FILES_HANDLES];
 int32_t error; bool retained;
 char name[64];
} portable_files_volume;
static inline bool pfv_result(portable_files_volume*p,int32_t rc){
 p->error=rc;if(rc==RISC_FILES_RETAINED||rc==RISC_FILES_CONTEXT)p->retained=true;
 return rc==RISC_FILES_OK;
}
static inline portable_files_handle* pfv_handle(portable_files_volume*p,uint32_t token,bool dir){
 for(unsigned i=0;i<PORTABLE_FILES_HANDLES;i++)if(p->handles[i].token==token&&p->handles[i].directory==dir)return &p->handles[i];
 p->error=RISC_FILES_INVALID;return NULL;
}
static inline portable_files_handle* pfv_slot(portable_files_volume*p){
 for(unsigned i=0;i<PORTABLE_FILES_HANDLES;i++)if(!p->handles[i].token)return &p->handles[i];
 p->error=RISC_FILES_BUSY;return NULL;
}
static inline bool pfv_available(portable_files_volume*p){return p&&p->files&&!p->retained;}
static inline bool pfv_refresh(void*c){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p))return false;risc_storage_files_stat_v1 s;memset(&s,0,sizeof(s));s.struct_size=sizeof(s);return pfv_result(p,p->files->stat(p->files->context,"/",&s))&&(s.flags&RISC_FILES_DIRECTORY);}
static inline bool pfv_ready(void*c){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&p->error!=RISC_FILES_UNAVAILABLE&&p->error!=RISC_FILES_BUSY;}
static inline bool pfv_label(void*c,char*out,size_t n){portable_files_volume*p=(portable_files_volume*)c;if(!out||!n)return false;snprintf(out,n,"%s",p->name);return true;}
static inline int32_t pfv_metadata(void*c,const char*path,risc_storage_metadata_v1*out){
 portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p)||!out)return RISC_STORAGE_FS_IO;
 risc_storage_files_stat_v1 s;memset(&s,0,sizeof(s));s.struct_size=sizeof(s);int32_t rc=p->files->stat(p->files->context,path,&s);pfv_result(p,rc);
 if(rc==RISC_FILES_NOT_FOUND)return RISC_STORAGE_FS_NOT_FOUND;
 if(rc)return RISC_STORAGE_FS_IO;
 out->flags=s.flags;out->size=s.size;out->modified_seconds=s.modified_seconds;return RISC_STORAGE_FS_OK;
}
static inline bool pfv_stat(void*c,const char*path,uint64_t*size,bool*dir){risc_storage_metadata_v1 s;memset(&s,0,sizeof(s));s.struct_size=sizeof(s);if(!size||!dir||pfv_metadata(c,path,&s))return false;*size=s.size;*dir=(s.flags&RISC_STORAGE_DIRECTORY)!=0;return true;}
static inline uint32_t pfv_open(void*c,const char*path,uint32_t flags){
 portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p))return 0;portable_files_handle*h=pfv_slot(p);if(!h)return 0;
 uint32_t token=0;int32_t rc=p->files->open(p->files->context,path,flags,&token);
 if((!rc&&!token)||(rc&&token)){pfv_result(p,RISC_FILES_RETAINED);return 0;}if(!pfv_result(p,rc))return 0;
 memset(h,0,sizeof(*h));h->token=token;return token;
}
static inline bool pfv_info(void*c,uint32_t token,uint64_t*size,uint64_t*offset){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,false)&&pfv_result(p,p->files->info(p->files->context,token,size,offset));}
static inline bool pfv_close(void*c,uint32_t token,bool commit){
 portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p))return false;portable_files_handle*h=pfv_handle(p,token,false);if(!h)return false;
 if(!pfv_result(p,p->files->close(p->files->context,token))){pfv_result(p,RISC_FILES_RETAINED);return false;}
 if(!commit&&h->abort_created){int32_t rc=p->files->remove(p->files->context,h->created_path);if(rc!=RISC_FILES_OK&&rc!=RISC_FILES_NOT_FOUND){pfv_result(p,RISC_FILES_RETAINED);return false;}}
 memset(h,0,sizeof(*h));p->error=0;return true;
}
static inline uint32_t pfv_open_read(void*c,const char*path,uint64_t*size){portable_files_volume*p=(portable_files_volume*)c;if(!size)return 0;uint32_t token=pfv_open(c,path,RISC_FILES_READ);if(!token)return 0;uint64_t offset=0;if(!pfv_info(c,token,size,&offset)){int32_t rc=p->error;if(pfv_close(c,token,true))p->error=rc;return 0;}return token;}
static inline uint32_t pfv_open_write(void*c,const char*path){portable_files_volume*p=(portable_files_volume*)c;if(!path||strlen(path)>=RISC_STORAGE_FILES_PATH_MAX)return 0;uint32_t token=pfv_open(c,path,RISC_FILES_WRITE|RISC_FILES_CREATE|RISC_FILES_EXCLUSIVE);if(token){portable_files_handle*h=pfv_handle(p,token,false);h->abort_created=true;strcpy(h->created_path,path);}return token;}
static inline size_t pfv_read(void*c,uint32_t token,void*out,size_t n){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p)||!n)return 0;portable_files_handle*h=pfv_handle(p,token,false);if(!h)return 0;if(n>RISC_STORAGE_FILES_IO_MAX)n=RISC_STORAGE_FILES_IO_MAX;uint32_t done=0;int32_t rc=p->files->read(p->files->context,token,out,(uint32_t)n,&done);if(done>n){pfv_result(p,RISC_FILES_RETAINED);return 0;}h->error=rc;pfv_result(p,rc);return done;}
static inline size_t pfv_write(void*c,uint32_t token,const void*in,size_t n){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p)||!n)return 0;portable_files_handle*h=pfv_handle(p,token,false);if(!h)return 0;if(n>RISC_STORAGE_FILES_IO_MAX)n=RISC_STORAGE_FILES_IO_MAX;uint32_t done=0;int32_t rc=p->files->write(p->files->context,token,in,(uint32_t)n,&done);if(done>n){pfv_result(p,RISC_FILES_RETAINED);return 0;}h->error=rc;pfv_result(p,rc);return done;}
static inline bool pfv_seek(void*c,uint32_t token,uint64_t pos){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,false)&&pfv_result(p,p->files->seek(p->files->context,token,pos));}
static inline bool pfv_sync(void*c,uint32_t token){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,false)&&pfv_result(p,p->files->sync(p->files->context,token));}
static inline bool pfv_truncate(void*c,uint32_t token,uint64_t size){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,false)&&pfv_result(p,p->files->truncate(p->files->context,token,size));}
static inline bool pfv_remove(void*c,const char*path){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_result(p,p->files->remove(p->files->context,path));}
static inline bool pfv_mkdir(void*c,const char*path){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_result(p,p->files->mkdir(p->files->context,path));}
static inline bool pfv_rename(void*c,const char*from,const char*to){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_result(p,p->files->rename(p->files->context,from,to));}
static inline uint32_t pfv_dir_open(void*c,const char*path){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p))return 0;portable_files_handle*h=pfv_slot(p);if(!h)return 0;uint32_t token=0;int32_t rc=p->files->dir_open(p->files->context,path,&token);if((!rc&&!token)||(rc&&token)){pfv_result(p,RISC_FILES_RETAINED);return 0;}if(!pfv_result(p,rc))return 0;memset(h,0,sizeof(*h));h->token=token;h->directory=true;return token;}
static inline bool pfv_dir_next(void*c,uint32_t token,risc_storage_dirent_v1*out){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p)||!out)return false;portable_files_handle*h=pfv_handle(p,token,true);if(!h)return false;risc_storage_files_entry_v1 e;memset(&e,0,sizeof(e));e.struct_size=sizeof(e);int32_t rc=p->files->dir_next(p->files->context,token,&e);h->error=rc==RISC_FILES_END?0:rc;pfv_result(p,rc);if(rc)return false;if(!memchr(e.name,0,sizeof(e.name))){h->error=RISC_FILES_IO;p->error=RISC_FILES_IO;return false;}memset(out,0,sizeof(*out));memcpy(out->name,e.name,sizeof(e.name));out->size=e.size;out->is_directory=(e.flags&RISC_FILES_DIRECTORY)!=0;return true;}
static inline bool pfv_dir_close_checked(void*c,uint32_t token){portable_files_volume*p=(portable_files_volume*)c;if(!pfv_available(p))return false;portable_files_handle*h=pfv_handle(p,token,true);if(!h)return false;if(!pfv_result(p,p->files->dir_close(p->files->context,token))){pfv_result(p,RISC_FILES_RETAINED);return false;}memset(h,0,sizeof(*h));return true;}
static inline void pfv_dir_close(void*c,uint32_t token){(void)pfv_dir_close_checked(c,token);}
static inline bool pfv_dir_rewind(void*c,uint32_t token){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,true)&&pfv_result(p,p->files->dir_rewind(p->files->context,token));}
static inline bool pfv_dir_tell(void*c,uint32_t token,uint64_t*pos){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,true)&&pfv_result(p,p->files->dir_tell(p->files->context,token,pos));}
static inline bool pfv_dir_seek(void*c,uint32_t token,uint64_t pos){portable_files_volume*p=(portable_files_volume*)c;return pfv_available(p)&&pfv_handle(p,token,true)&&pfv_result(p,p->files->dir_seek(p->files->context,token,pos));}
static inline uint32_t pfv_error(void*c,uint32_t token,bool dir){portable_files_volume*p=(portable_files_volume*)c;portable_files_handle*h=pfv_handle(p,token,dir);return h?(uint32_t)(h->error<0?-h->error:h->error):1;}
static inline int32_t pfv_observe(void*c){portable_files_volume*p=(portable_files_volume*)c;return p->retained?RISC_STORAGE_STATE_RETAINED:(p->error==RISC_FILES_UNAVAILABLE||p->error==RISC_FILES_BUSY)?RISC_STORAGE_STATE_UNAVAILABLE:RISC_STORAGE_STATE_READY;}
static inline bool pfv_last_error(void*c,char*out,size_t n){portable_files_volume*p=(portable_files_volume*)c;if(!out||!n)return false;const char*s=p->retained?"File custody retained; restart required":p->error==RISC_FILES_UNAVAILABLE?"Files unavailable":p->error==RISC_FILES_BUSY?"Files busy; retry":p->error==RISC_FILES_DENIED?"File operation denied":p->error==RISC_FILES_NOT_FOUND?"File not found":p->error==RISC_FILES_EXISTS?"Destination exists":p->error&&p->error!=RISC_FILES_END?"File operation failed":"";snprintf(out,n,"%s",s);return true;}
/* The legacy fs suffix gates cursor/metadata access on replacement callbacks.
 * These intentionally reject unsupported atomic replacement, never emulate it
 * by deleting a destination. Current browser/WebDAV use rename staging. */
static inline bool pfv_replace(void*c,const char*a,const char*b){(void)a;(void)b;portable_files_volume*p=(portable_files_volume*)c;p->error=RISC_FILES_DENIED;return false;}
static inline bool pfv_recover(void*c,const char*a){(void)a;portable_files_volume*p=(portable_files_volume*)c;p->error=RISC_FILES_DENIED;return false;}
static inline bool portable_files_volume_init(portable_files_volume*p,const risc_storage_files_v1*f,const char*label){
 if(!p||!f||f->api_version!=1||f->struct_size<sizeof(*f)||!f->open||!f->close||!f->read||!f->write||!f->seek||!f->info||!f->sync||!f->truncate||!f->stat||!f->mkdir||!f->rename||!f->remove||!f->dir_open||!f->dir_next||!f->dir_close||!f->dir_rewind||!f->dir_tell||!f->dir_seek)return false;
 memset(p,0,sizeof(*p));p->files=f;snprintf(p->name,sizeof(p->name),"%s",label?label:"Files");
 risc_storage_volume_api_v1_ext*e=&p->view.state.prepared.base.sleep.terminal.power.volume;
 e->base.api_version=1;e->base.struct_size=sizeof(p->view);e->base.context=p;e->base.refresh=pfv_refresh;e->base.ready=pfv_ready;e->base.label=pfv_label;e->base.stat=pfv_stat;e->base.dir_open=pfv_dir_open;e->base.dir_next=pfv_dir_next;e->base.dir_close=pfv_dir_close;e->base.file_open_read=pfv_open_read;e->base.file_read=pfv_read;e->base.file_open_write=pfv_open_write;e->base.file_write=pfv_write;e->base.file_close=pfv_close;e->base.remove=pfv_remove;e->base.last_error=pfv_last_error;
 e->file_open=pfv_open;e->file_seek=pfv_seek;e->file_info=pfv_info;e->file_sync=pfv_sync;e->dir_rewind=pfv_dir_rewind;e->dir_close_checked=pfv_dir_close_checked;e->handle_error=pfv_error;e->mkdir=pfv_mkdir;e->rename=pfv_rename;
 p->view.state.state_tag=RISC_STORAGE_STATE_TAG;p->view.state.state_version=1;p->view.state.observe=pfv_observe;
 p->view.fs_tag=RISC_STORAGE_FS_TAG;p->view.fs_version=1;p->view.dir_tell=pfv_dir_tell;p->view.dir_seek=pfv_dir_seek;p->view.metadata=pfv_metadata;p->view.file_truncate=pfv_truncate;p->view.replace_file=pfv_replace;p->view.recover_replace=pfv_recover;return true;
}
static inline const risc_storage_volume_api_v1* portable_files_volume_api(portable_files_volume*p){return &p->view.state.prepared.base.sleep.terminal.power.volume.base;}

/* Pure local checks: never use cached ready/observe as proof of current media
 * availability. Each generic operation result remains authoritative. */
static inline bool portable_files_volume_retained(const portable_files_volume*p){return p&&p->retained;}
static inline bool portable_files_volume_can_release(const portable_files_volume*p){if(!p||p->retained)return false;for(unsigned i=0;i<PORTABLE_FILES_HANDLES;i++)if(p->handles[i].token)return false;return true;}
