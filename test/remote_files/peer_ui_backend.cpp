#define PEER_UI_BACKEND
#include "webdav_peer_test.cpp"
static PeerControllerFixture*backend;static uint64_t clockBase;
static ExistingFiles watchFiles;static RiscStorage::AppDataExport*watchExport;static risc_app_data_export_v1 watchApi;
static Store localStore;static int localState=RISC_STORAGE_STATE_READY;static risc_storage_volume_api_v1_fs localApi;
static RiscUserFiles::Volume localVolume;
static const char bytes[]="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
static void put(const risc_storage_volume_api_v1*a,const char*p,const char*b){auto f=a->file_open_write(a->context,p);assert(f&&a->file_write(a->context,f,b,strlen(b))==strlen(b)&&a->file_close(a->context,f,true));}
extern "C" void peer_backend_begin(){backend=new PeerControllerFixture;backend->stop();clockBase=backend->f.now;backend->acquiredBle=true;assert(localVolume.begin(&localStore.api,{nullptr,[](void*){return true;},malloc,free}));auto a=localVolume.api();assert(risc_storage_volume_extension(a)->mkdir(a->context,"/Downloads"));put(a,"/Downloads/local.txt","local upload");put(backend->f.volume.api(),"/incoming.obj",bytes);localApi=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(a);localApi.state.observe=[](void*){return localState;};}
extern "C" void peer_backend_direct(){backend->privateNetwork=true;}
extern "C" const void*peer_backend_capability(const char*n){if(!strcmp(n,"net.wifi")&&backend->privateNetwork)return &backend->wifi;if(!strcmp(n,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY))return &backend->ble;if(!strcmp(n,RISC_TCP_CONNECTION_CAPABILITY))return &backend->f.link.tcp;if(!strcmp(n,RISC_ENTROPY_SOURCE_CAPABILITY))return &backend->entropy;if(!strcmp(n,"storage.installed-files"))return &localApi;return nullptr;}
extern "C" void peer_backend_tick(uint64_t now){backend->f.now=now+clockBase;backend->f.server.tick(now+clockBase);}
extern "C" void peer_backend_verify(const char*path){auto a=localVolume.api();uint64_t n=0;auto f=a->file_open_read(a->context,path,&n);assert(f&&n==strlen(bytes));char out[128]{};assert(a->file_read(a->context,f,out,sizeof(out))==n&&!strcmp(out,bytes));assert(a->file_close(a->context,f,true));assert(!backend->token&&!backend->f.link.clientLive);}
extern "C" void peer_backend_end(){assert(localVolume.end());delete backend;backend=nullptr;if(watchExport){assert(watchExport->end());delete watchExport;watchExport=nullptr;}}

extern "C" void peer_backend_local_state(int state){localState=state;}
extern "C" void peer_backend_uploaded_contents(const char*expected){auto a=backend->f.volume.api();uint64_t n=0;auto h=a->file_open_read(a->context,"/local.txt",&n);char b[128]{};assert(h&&n==strlen(expected)&&n<sizeof(b));assert(a->file_read(a->context,h,b,sizeof(b))==n&&!strcmp(b,expected));assert(a->file_close(a->context,h,true));}
extern "C" void peer_backend_uploaded(){peer_backend_uploaded_contents("local upload");}
extern "C" void peer_backend_change_source(const char*contents){auto a=localVolume.api();assert(a->remove(a->context,"/Downloads/local.txt"));put(a,"/Downloads/local.txt",contents);}
extern "C" uint64_t peer_backend_connections(){return backend->f.link.serial;}
extern "C" void peer_backend_folders(){auto a=backend->f.volume.api();auto ext=risc_storage_volume_extension(a);assert(ext->mkdir(a->context,"/Documents"));assert(ext->mkdir(a->context,"/Documents/Nested"));put(a,"/Documents/Nested/note.txt","nested bytes");}
extern "C" bool peer_backend_remote_exists(const char*path){uint64_t n=0;bool directory=false;auto a=backend->f.volume.api();return a->stat(a->context,path,&n,&directory);}
extern "C" bool peer_backend_local_exists(const char*path){uint64_t n=0;bool directory=false;auto a=localVolume.api();return a->stat(a->context,path,&n,&directory);}

extern "C" void peer_backend_watch(){
 delete backend;backend=nullptr;
 watchExport=new RiscStorage::AppDataExport({nullptr,[](void*){return true;},[](void*){return true;},malloc,free});
 const RiscStorage::AppDataExport::Entry rows[]={
 {"timecard",1,"timecard.json","/timecard/timecard.json",true},
 {"audio_spectrum",2,"spectrum-events-a.sqt","/audio-spectrum/spectrum-events-a.sqt",true},
 {"audio_spectrum",2,"spectrum-events-b.sqt","/audio-spectrum/spectrum-events-b.sqt",true},
 {"waterfall",3,"rf-events-a.rft","/waterfall/rf-events-a.rft",true},
 {"waterfall",3,"rf-events-b.rft","/waterfall/rf-events-b.rft",true},
 {"lists",62,"lists.bin","/lists/lists.bin",true},
 {"model_viewer",64,"model.obj","/models/model.obj",true},
 {"model_viewer",64,"model.stl","/models/model.stl",true}};
 watchFiles.records[{64,"model.obj"}]={bytes,1};
 assert(watchExport->configure(&watchFiles.backend,rows,8,"Watch user files")&&watchExport->begin(&watchApi));
 backend=new PeerControllerFixture(&watchApi.volume.terminal.power.volume.base);backend->stop();clockBase=backend->f.now;backend->acquiredBle=true;
}
extern "C" void peer_backend_watch_replaced(){assert(watchFiles.records.at({64,"model.obj"}).revision==2&&watchFiles.records.at({64,"model.obj"}).bytes==bytes);}

extern "C" void peer_backend_collision(){put(localVolume.api(),"/Downloads/incoming.obj","keep local bytes");}
extern "C" void peer_backend_collision_preserved(){auto a=localVolume.api();uint64_t size;auto h=a->file_open_read(a->context,"/Downloads/incoming.obj",&size);assert(h&&size==16);char b[32]{};assert(a->file_read(a->context,h,b,sizeof(b))==size&&!strcmp(b,"keep local bytes"));assert(a->file_close(a->context,h,true));}
