#define PEER_UI_BACKEND
#include "webdav_peer_test.cpp"
static PeerControllerFixture*backend;static uint64_t clockBase;
static Store localStore;static int localState=RISC_STORAGE_STATE_READY;static risc_storage_volume_api_v1_fs localApi;
static RiscUserFiles::Volume localVolume;
static const char bytes[]="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
static void put(const risc_storage_volume_api_v1*a,const char*p,const char*b){auto f=a->file_open_write(a->context,p);assert(f&&a->file_write(a->context,f,b,strlen(b))==strlen(b)&&a->file_close(a->context,f,true));}
extern "C" void peer_backend_begin(){backend=new PeerControllerFixture;backend->stop();clockBase=backend->f.now;backend->acquiredBle=true;assert(localVolume.begin(&localStore.api,{nullptr,[](void*){return true;},malloc,free}));auto a=localVolume.api();assert(risc_storage_volume_extension(a)->mkdir(a->context,"/Downloads"));put(a,"/Downloads/local.txt","local upload");put(backend->f.volume.api(),"/incoming.obj",bytes);localApi=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(a);localApi.state.observe=[](void*){return localState;};}
extern "C" void peer_backend_direct(){backend->privateNetwork=true;}
extern "C" const void*peer_backend_capability(const char*n){if(!strcmp(n,"net.wifi")&&backend->privateNetwork)return &backend->wifi;if(!strcmp(n,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY))return &backend->ble;if(!strcmp(n,RISC_TCP_CONNECTION_CAPABILITY))return &backend->f.link.tcp;if(!strcmp(n,RISC_ENTROPY_SOURCE_CAPABILITY))return &backend->entropy;if(!strcmp(n,"storage.installed-files"))return &localApi;return nullptr;}
extern "C" void peer_backend_tick(uint64_t now){backend->f.now=now+clockBase;backend->f.server.tick(now+clockBase);}
extern "C" void peer_backend_verify(const char*path){auto a=localVolume.api();uint64_t n=0;auto f=a->file_open_read(a->context,path,&n);assert(f&&n==strlen(bytes));char out[128]{};assert(a->file_read(a->context,f,out,sizeof(out))==n&&!strcmp(out,bytes));assert(a->file_close(a->context,f,true));assert(!backend->token&&!backend->f.link.clientLive);}
extern "C" void peer_backend_end(){assert(localVolume.end());delete backend;backend=nullptr;}

extern "C" void peer_backend_local_state(int state){localState=state;}
extern "C" void peer_backend_uploaded(){auto a=backend->f.volume.api();uint64_t n;auto h=a->file_open_read(a->context,"/local.txt",&n);assert(h&&n==12);char b[16]{};assert(a->file_read(a->context,h,b,sizeof(b))==12&&!strcmp(b,"local upload"));assert(a->file_close(a->context,h,true));}
