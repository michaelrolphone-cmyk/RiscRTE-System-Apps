/* Real peer/volume backend with a strict service-custody oracle. */
#include "peer_ui_backend.cpp"
static risc_storage_volume_api_v1_ext scopeOriginal;
static bool scopeLease,scopeBeginBusy,scopeEndBusy;
static unsigned scopeCalls;
extern "C" void peer_scope_assert(){
 if(backend->privateNetwork&&backend->networkLive){assert(scopeLease);++scopeCalls;}
}
extern "C" unsigned peer_scope_calls(){return scopeCalls;}
extern "C" bool peer_scope_joined(){return backend->networkLive;}
extern "C" bool peer_scope_held(){return scopeLease;}
extern "C" void peer_scope_busy(bool begin,bool end){scopeBeginBusy=begin;scopeEndBusy=end;}
extern "C" void peer_scope_enable(){
 peer_backend_direct();
 backend->wifi.service_begin=[](void*,uint32_t*out)->int32_t{*out=0;if(scopeBeginBusy)return RISC_RADIO_BUSY;assert(!scopeLease);scopeLease=true;*out=9;return RISC_RADIO_ACCEPTED;};
 backend->wifi.service_end=[](void*,uint32_t token)->int32_t{assert(token==9&&scopeLease);if(scopeEndBusy)return RISC_RADIO_AGAIN;scopeLease=false;return RISC_RADIO_ACCEPTED;};
 auto*e=reinterpret_cast<risc_storage_volume_api_v1_ext*>(&localApi);scopeOriginal=*e;
 auto&b=e->base;
 b.refresh=[](void*c){peer_scope_assert();return scopeOriginal.base.refresh(c);};
 b.ready=[](void*c){peer_scope_assert();return scopeOriginal.base.ready(c);};
 b.label=[](void*c,char*out,size_t n){peer_scope_assert();return scopeOriginal.base.label(c,out,n);};
 b.stat=[](void*c,const char*p,uint64_t*n,bool*d){peer_scope_assert();return scopeOriginal.base.stat(c,p,n,d);};
 b.dir_open=[](void*c,const char*p){peer_scope_assert();return scopeOriginal.base.dir_open(c,p);};
 b.dir_next=[](void*c,risc_storage_dir_t h,risc_storage_dirent_v1*out){peer_scope_assert();return scopeOriginal.base.dir_next(c,h,out);};
 b.dir_close=[](void*c,risc_storage_dir_t h){peer_scope_assert();scopeOriginal.base.dir_close(c,h);};
 b.file_open_read=[](void*c,const char*p,uint64_t*n){peer_scope_assert();return scopeOriginal.base.file_open_read(c,p,n);};
 b.file_read=[](void*c,risc_storage_file_t h,void*out,size_t n){peer_scope_assert();return scopeOriginal.base.file_read(c,h,out,n);};
 b.file_open_write=[](void*c,const char*p){peer_scope_assert();return scopeOriginal.base.file_open_write(c,p);};
 b.file_write=[](void*c,risc_storage_file_t h,const void*in,size_t n){peer_scope_assert();return scopeOriginal.base.file_write(c,h,in,n);};
 b.file_close=[](void*c,risc_storage_file_t h,bool commit){peer_scope_assert();return scopeOriginal.base.file_close(c,h,commit);};
 b.remove=[](void*c,const char*p){peer_scope_assert();return scopeOriginal.base.remove(c,p);};
 b.last_error=[](void*c,char*out,size_t n){peer_scope_assert();return scopeOriginal.base.last_error(c,out,n);};
 e->file_sync=[](void*c,risc_storage_file_t h){peer_scope_assert();return scopeOriginal.file_sync(c,h);};
 e->dir_close_checked=[](void*c,risc_storage_dir_t h){peer_scope_assert();return scopeOriginal.dir_close_checked(c,h);};
 e->handle_error=[](void*c,uint32_t h,bool d){peer_scope_assert();return scopeOriginal.handle_error(c,h,d);};
 e->mkdir=[](void*c,const char*p){peer_scope_assert();return scopeOriginal.mkdir(c,p);};
 e->rename=[](void*c,const char*a,const char*b){peer_scope_assert();return scopeOriginal.rename(c,a,b);};
}
