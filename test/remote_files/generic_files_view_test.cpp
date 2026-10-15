#include "GenericFilesFixture.h"
#include "PortableFilesVolume.h"
#include <cstdio>
int main(){
 GenericFilesFixture f;portable_files_volume v{};assert(portable_files_volume_init(&v,&f.api,"Work"));
 auto*a=portable_files_volume_api(&v);auto*e=risc_storage_volume_extension(a);auto*fs=risc_storage_volume_fs(a);
 assert(a->refresh(a->context));uint64_t size=0;uint32_t h=a->file_open_read(a->context,"/book.txt",&size);assert(h&&size==15);char b[32]{};assert(a->file_read(a->context,h,b,sizeof(b))==15&&!strcmp(b,"generic content"));assert(e->file_seek(a->context,h,8));assert(a->file_read(a->context,h,b,7)==7);assert(a->file_close(a->context,h,true));
 h=a->file_open_write(a->context,"/new");assert(h);assert(a->file_write(a->context,h,"new",3)==3);assert(e->file_sync(a->context,h));assert(a->file_close(a->context,h,true));assert(f.nodes.at("/new").bytes=="new");assert(!a->file_open_write(a->context,"/new"));
 h=a->file_open_write(a->context,"/abort");assert(h);assert(a->file_write(a->context,h,"discard",7)==7);assert(a->file_close(a->context,h,false));assert(!f.nodes.count("/abort"));assert(e->rename(a->context,"/new","/renamed"));assert(a->remove(a->context,"/renamed"));assert(e->mkdir(a->context,"/created"));
 h=a->dir_open(a->context,"/");assert(h);risc_storage_dirent_v1 row{};assert(a->dir_next(a->context,h,&row));uint64_t at=0;assert(fs->dir_tell(a->context,h,&at)&&at==1);assert(fs->dir_seek(a->context,h,0));assert(a->dir_next(a->context,h,&row));while(a->dir_next(a->context,h,&row)){}assert(!e->handle_error(a->context,h,true));assert(e->dir_close_checked(a->context,h));
 f.fault=RISC_FILES_IO;risc_storage_metadata_v1 meta{};meta.struct_size=sizeof(meta);assert(fs->metadata(a->context,"/not-missing",&meta)==RISC_STORAGE_FS_IO);f.fault=RISC_FILES_DENIED;assert(!a->file_open_write(a->context,"/denied")&&!f.nodes.count("/denied"));f.fault=RISC_FILES_UNAVAILABLE;assert(!a->refresh(a->context));f.fault=0;assert(a->refresh(a->context));assert(fs->metadata(a->context,"/missing",&meta)==RISC_STORAGE_FS_NOT_FOUND);
 h=a->file_open_read(a->context,"/book.txt",&size);assert(h);f.closeFails=true;assert(!a->file_close(a->context,h,true));unsigned calls=f.calls;assert(v.retained);assert(!a->refresh(a->context));assert(!a->file_read(a->context,h,b,sizeof(b)));assert(!a->file_close(a->context,h,true));assert(f.calls==calls&&f.handles.count(h));
 puts("Generic files view: CRUD, cursors, EOF vs IO, denied, unavailable recovery, and terminal close custody passed");
}
