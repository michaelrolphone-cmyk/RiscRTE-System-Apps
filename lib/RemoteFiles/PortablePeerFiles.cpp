#include <RiscProtectedSettingsPath.h>
#include "PortablePeerFiles.h"
#include "PortableNetworkSession.h"
#include "WebDavRemoteVolume.h"
#include <RiscEntropySourceV1.h>
#include <cstring>
#include <cstdio>
#include <new>
using namespace RiscWebDav;
static_assert(uint32_t(PEER_CAP_LIST)==Client::List&&uint32_t(PEER_CAP_READ)==Client::Read&&uint32_t(PEER_CAP_HEAD)==Client::Head&&uint32_t(PEER_CAP_PUT)==Client::Put&&uint32_t(PEER_CAP_DELETE)==Client::Delete&&uint32_t(PEER_CAP_MKCOL)==Client::Mkcol&&uint32_t(PEER_CAP_MOVE)==Client::Move&&uint32_t(PEER_CAP_COPY)==Client::Copy,"Peer capability ABI");
static_assert(PORTABLE_PEER_PREVIEW_MAX==RemoteVolume::PreviewMax,"Peer preview bound");
struct portable_peer_files {
 const risc_runtime_api_v1*runtime;
portable_peer_hooks hooks;
Client client;
RemoteVolume remote;
 risc_runtime_capability_v1 tcp{},entropy{},ble{},wifi{},store{};
risc_bluetooth_session_setup_v1 broker{};
 risc_bluetooth_session_descriptor_v1 descriptor{};
risc_bluetooth_network_offer_v1 offer{};
portable_network_session network{};
bool networkInitialized=false,networkReady=false,leaseEnding=false;
risc_bluetooth_session_setup_status_v1 setup{};
 risc_bluetooth_session_peer_v1 peers[RISC_SESSION_PEERS_MAX]{};
 portable_peer_status status{};
uint8_t random[16]{};
uint64_t token=0,now=0,copiedAt=0,startedAt=0;
 enum class Step{Off,Tcp,Ble,Discover,Entropy,Random,ReleaseEntropy,Scan,ReadDescriptor,ReadNetwork,CloseBle,ReleaseBle,Wifi,Store,Join,Network,Authenticate,Handshake,Capabilities,Browse,Operation,StopRemote,StopClient,StopBle,CloseNetwork,StopGrants}step=Step::Off;
 enum class Command{None,Select,Confirm,List,Preview,Download,Upload,Mutate}command=Command::None;
 uint32_t selected=0,generation=0,number=0;
bool replace=false,accept=false,stopping=false,cancelRequested=false;size_t previewLimit=0;uint64_t previewOffset=0;
unsigned scanIndex=0;
 Step tracedStep=Step::Off;
 unsigned tracedState=~0u,tracedBroker=~0u;
 int tracedResult=0;
 const char*tracedOperation=nullptr;
 static const char*stepName(Step s){
  static const char*const names[]={"off","tcp","ble","discover","entropy","random","release-entropy","scan","read-descriptor","read-network","close-ble","release-ble","wifi","store","join","network","authenticate","handshake","capabilities","browse","operation","stop-remote","stop-client","stop-ble","close-network","stop-grants"};
  return names[static_cast<unsigned>(s)];
 }
 void trace(const char*event,int rc=0){
  /* Fixed stage names and numeric results only: never session material, peer
   * names, paths, pairing numbers or network credentials. No new clock I/O. */
  if(!runtime->diagnostic||!owned(this))return;
  char line[192];
  snprintf(line,sizeof(line),"FILES t_ms=%llu role=nearby event=%s stage=%s state=%u broker=%u result=%d",(unsigned long long)now,event,stepName(step),unsigned(status.state),unsigned(setup.state),rc);
  (void)runtime->diagnostic(line);
 }
 void traceState(){
  if(tracedStep==step&&tracedState==unsigned(status.state)&&tracedBroker==unsigned(setup.state))return;
  tracedStep=step;tracedState=unsigned(status.state);tracedBroker=unsigned(setup.state);
  trace("state");
 }
 void traceResult(const char*operation,int rc){
  if(tracedOperation==operation&&tracedResult==rc)return;
  tracedOperation=operation;tracedResult=rc;trace(operation,rc);
 }
 const risc_storage_volume_api_v1*local=nullptr;
char first[513]{},second[513]{},method[12]{};
 portable_peer_files(const risc_runtime_api_v1*r,const portable_peer_hooks&h):runtime(r),hooks(h),remote(client,{this,owned}){}
 static bool owned(void*c){auto&p=*static_cast<portable_peer_files*>(c);
return p.status.state!=PEER_RETAINED&&p.hooks.owner(p.hooks.context);
}
 void wipe(){volatile unsigned char*p=reinterpret_cast<volatile unsigned char*>(&descriptor);
for(size_t i=0;i<sizeof(descriptor);i++)p[i]=0;
memset(random,0,sizeof(random));
volatile unsigned char*n=reinterpret_cast<volatile unsigned char*>(&offer);for(size_t i=0;i<sizeof(offer);i++)n[i]=0;
}
 void retain(){status.state=PEER_RETAINED;status.operation_ok=false;status.completed_generation=status.operation_generation;
client.tick(now);
 /* owned() is false; wipes session material without I/O. */
snprintf(status.message,sizeof(status.message),"Resource custody retained; restart required");
wipe();
}
 bool owner(){if(owned(this))return true;
retain();
return false;
}
 void fail(const char*s){trace("failed");snprintf(status.message,sizeof(status.message),"%s",s);
status.state=PEER_ERROR;status.operation_ok=false;status.completed_generation=status.operation_generation;
stopping=true;
step=Step::StopRemote;
wipe();
}
 bool acquire(const char*cap,risc_runtime_capability_v1&g,uint64_t instance=0){g={};
g.struct_size=sizeof(g);
bool ok=runtime->acquire(cap,1,instance,&g);
if(!owner())return false;
if(!ok||!g.api){if(g.api)retain();
else fail("Peer service is unavailable");
return false;
}return true;
}
 bool release(risc_runtime_capability_v1&g){if(!g.api)return true;
if(!owner()||!runtime->release(&g)||!owner()){retain();
return false;
}g={};
return true;
}
 bool brokerResult(int32_t rc){if(!owner())return false;
if(rc<0&&rc!=RISC_SETUP_BUSY)traceResult("broker-result",rc);
if(rc==RISC_SETUP_RETAINED||rc==RISC_SETUP_CONTEXT){retain();
return false;
}if(rc==RISC_SETUP_CLEANUP_PENDING){fail("Bluetooth cleanup pending");
return false;
}
if(rc==RISC_SETUP_BUSY)return false;
if(rc<0){fail(rc==RISC_SETUP_EXPIRED?"Peer setup expired":"Peer setup failed");
return false;
}return true;
}
 bool closeBroker(){if(!token)return true;
int rc=broker.close(broker.context,token);
if(!owner())return false;
traceResult("close-result",rc);
if(rc==RISC_SETUP_CLEANUP_PENDING||rc==RISC_SETUP_BUSY)return false;
if(rc!=RISC_SETUP_OK){retain();
return false;
}token=0;
return true;
}
};
extern "C" portable_peer_files*portable_peer_create(const risc_runtime_api_v1*r,const portable_peer_hooks*h){if(!r||r->api_version!=1||r->struct_size<sizeof(*r)||!r->acquire||!r->release||!h||!h->owner||!h->allocate||!h->deallocate||!h->owner(h->context))return nullptr;
void*p=h->allocate(sizeof(portable_peer_files));
return p?new(p)portable_peer_files(r,*h):nullptr;
}
extern "C" bool portable_peer_start(portable_peer_files*p,uint64_t now){if(!p||p->step!=portable_peer_files::Step::Off||!p->owner())return false;
p->now=p->startedAt=now;
const uint32_t generation=p->status.operation_generation;
p->status={};p->status.operation_generation=p->status.completed_generation=generation;
p->status.state=PEER_STARTING;
p->stopping=p->cancelRequested=false;
p->command=portable_peer_files::Command::None;p->scanIndex=0;p->selected=p->generation=p->number=0;
memset(p->peers,0,sizeof(p->peers));
p->step=portable_peer_files::Step::Tcp;
p->tracedState=~0u;p->tracedBroker=~0u;p->tracedOperation=nullptr;p->traceState();
return true;
}
extern "C" void portable_peer_tick(portable_peer_files*p,uint64_t now){
 if(!p||p->step==portable_peer_files::Step::Off||!p->owner())return;
if(now<p->now){p->fail("Peer clock moved backwards");
}p->now=now;
p->traceState();
using S=portable_peer_files::Step;
using C=portable_peer_files::Command;
 if(p->step>=S::Tcp&&p->step<=S::ReleaseEntropy&&now-p->startedAt>=10000u)p->fail("Peer startup timed out");
 if(p->leaseEnding){int rc=portable_network_session_service_end(&p->network);if(rc==PORTABLE_NETWORK_SERVICE_RETAINED)p->retain();else if(rc==PORTABLE_NETWORK_SERVICE_OK)p->leaseEnding=false;return;}
 bool lease=false;
 if(p->networkReady && p->step>=S::Authenticate && p->step<=S::StopClient){
   int rc=portable_network_session_service_begin(&p->network);if(rc==PORTABLE_NETWORK_SERVICE_RETAINED){p->retain();return;}if(rc!=PORTABLE_NETWORK_SERVICE_OK)return;lease=true;
 }
 switch(p->step){
 case S::Tcp:if(p->acquire(RISC_TCP_CONNECTION_CAPABILITY,p->tcp)){auto*t=static_cast<const risc_tcp_connection_v1*>(p->tcp.api);
if(t->api_version!=1||t->struct_size<sizeof(*t)||t->connect_tag!=RISC_TCP_CONNECTION_CONNECT_TAG||!t->connect||!t->connected)p->fail("This Runtime lacks peer TCP support");
else p->step=S::Ble;
}break;
 case S::Entropy:if(p->acquire(RISC_ENTROPY_SOURCE_CAPABILITY,p->entropy))p->step=S::Random;
break;
 case S::Random:{auto*e=static_cast<const risc_entropy_source_v1*>(p->entropy.api);
if(e->api_version!=1||e->struct_size<sizeof(*e)||!e->fill){p->fail("Invalid entropy service");
break;
}int rc=e->fill(e->context,p->random,sizeof(p->random));
if(!p->owner())break;
if(rc==RISC_ENTROPY_SOURCE_BUSY)break;
if(rc==RISC_ENTROPY_SOURCE_UNAVAILABLE||rc==RISC_ENTROPY_SOURCE_INVALID){p->fail("Secure entropy unavailable");
break;
}
if(rc){p->retain();
break;
}p->step=S::ReleaseEntropy;
break;
}
 case S::ReleaseEntropy:if(p->release(p->entropy)){p->step=S::Scan;p->status.state=PEER_DISCOVERING;}
break;
 case S::Ble:if(p->acquire(RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY,p->ble)){auto*b=static_cast<const risc_bluetooth_session_setup_v1*>(p->ble.api);
if(b->api_version!=1||b->struct_size<offsetof(risc_bluetooth_session_setup_v1,network_tag)||b->peer_tag!=RISC_SESSION_PEER_TAG||!b->discover||!b->peer||!b->select_peer||!b->peer_descriptor||!b->poll||!b->status||!b->close||!b->confirm_pairing){p->fail("This Bluetooth provider lacks peer discovery");
break;
}memcpy(&p->broker,b,b->struct_size<sizeof(p->broker)?b->struct_size:sizeof(p->broker));
p->step=S::Discover;
}break;
 case S::Discover:{int rc=p->broker.discover(p->broker.context,300000,&p->token);
if(!p->brokerResult(rc))break;
if(!p->token){p->retain();
break;
}// Discovery claims/enables BLE before entropy; no peer or router is required.
p->step=S::Entropy;
break;
}
 case S::Scan:{
  if(p->command==C::Select){p->command=C::None;
if(p->brokerResult(p->broker.select_peer(p->broker.context,p->token,p->selected)))p->status.state=PEER_CONNECTING;
break;
}
  if(p->command==C::Confirm){p->command=C::None;
p->trace("confirm");
if(!p->brokerResult(p->broker.confirm_pairing(p->broker.context,p->token,p->generation,p->number,p->accept)))break;
p->status.state=PEER_CONNECTING;
break;
}
  if(!p->brokerResult(p->broker.poll(p->broker.context,p->token,2)))break;
  p->setup={};
p->setup.struct_size=sizeof(p->setup);
if(!p->brokerResult(p->broker.status(p->broker.context,p->token,&p->setup)))break;
  if(p->setup.state==RISC_SETUP_DISCOVERING){p->status.state=PEER_DISCOVERING;
risc_bluetooth_session_peer_v1 row{};
row.struct_size=sizeof(row);
int rc=p->broker.peer(p->broker.context,p->token,p->scanIndex,&row);
if(rc==0){p->peers[p->scanIndex]=row;
p->status.count=p->scanIndex+1;
p->scanIndex=(p->scanIndex+1)%RISC_SESSION_PEERS_MAX;
}else if(rc==RISC_SETUP_INVALID)p->scanIndex=0;
else p->brokerResult(rc);
}
  else if(p->setup.state==RISC_SETUP_PAIR_CONFIRM){p->status.state=PEER_PAIRING;
p->status.pairing_generation=p->setup.pairing_generation;
p->status.pairing_number=p->setup.pairing_number;
}
  else if(p->setup.state==RISC_SETUP_READY)p->step=S::ReadDescriptor;
  else if(p->setup.state==RISC_SETUP_FAILED||p->setup.state==RISC_SETUP_ENDED)p->fail("Peer setup ended");
break;
}
 case S::ReadDescriptor:p->descriptor={};
p->descriptor.struct_size=sizeof(p->descriptor);
if(p->brokerResult(p->broker.peer_descriptor(p->broker.context,p->token,&p->descriptor))){p->copiedAt=now;
p->step=S::ReadNetwork;
}break;
 case S::ReadNetwork:
 p->offer={};p->offer.struct_size=sizeof(p->offer);
 if(p->broker.network_tag==RISC_SESSION_NETWORK_TAG&&p->broker.peer_network){
   int rc=p->broker.peer_network(p->broker.context,p->token,&p->offer);
   if(rc==RISC_SETUP_INVALID)p->offer={};else {
    if(!p->brokerResult(rc))break;
    if(p->offer.struct_size!=sizeof(p->offer)||!memchr(p->offer.ssid,0,sizeof(p->offer.ssid))||!p->offer.ssid[0]||!memchr(p->offer.password,0,sizeof(p->offer.password))||strlen(p->offer.password)<8||memcmp(p->offer.address,"\xc0\xa8\x04\x01",4)||memcmp(p->offer.endpoint.url,p->descriptor.url,sizeof(p->descriptor.url))||memcmp(p->offer.endpoint.username,p->descriptor.username,sizeof(p->descriptor.username))||memcmp(p->offer.endpoint.password,p->descriptor.password,sizeof(p->descriptor.password))){p->fail("Invalid private network offer");break;}
   }
 }else p->offer={};
 p->step=S::CloseBle;break;
 case S::CloseBle:if(p->closeBroker())p->step=S::ReleaseBle;
break;
 case S::ReleaseBle:if(p->release(p->ble))p->step=S::Wifi;
break;
 case S::Wifi:if(p->acquire("net.wifi",p->wifi,15))p->step=p->offer.struct_size?S::Join:S::Store;break;
 case S::Store:if(p->acquire("storage.key-value",p->store,6))p->step=S::Join;break;
 case S::Join:{
  portable_network_session_init(&p->network,static_cast<const wifi_api_v1*>(p->wifi.api),static_cast<const risc_key_value_v1*>(p->store.api),portable_peer_files::owned,p);p->networkInitialized=true;
  int rc=p->offer.struct_size?portable_network_session_start_private(&p->network,false,p->offer.ssid,p->offer.password,uint32_t(now)):portable_network_session_start(&p->network,uint32_t(now));
  memset(&p->offer,0,sizeof(p->offer));
  if(rc==PORTABLE_NETWORK_RETAINED){p->retain();break;}
  if(rc!=PORTABLE_NETWORK_PENDING){p->fail("Peer network setup unavailable");break;}p->step=S::Network;break;
 }
 case S::Network:{
  if(now-p->copiedAt>=p->descriptor.remaining_lifetime_ms){p->fail("Peer session expired");break;}
  int rc=portable_network_session_step(&p->network,uint32_t(now));
  if(rc==PORTABLE_NETWORK_RETAINED){p->retain();break;}
  if(rc<0){p->fail("Peer Wi-Fi connection failed");break;}
  if(rc==PORTABLE_NETWORK_READY){p->networkReady=true;p->step=S::Authenticate;}break;
 }
 case S::Authenticate:{uint64_t elapsed=now-p->copiedAt;
if(elapsed>=p->descriptor.remaining_lifetime_ms){p->fail("Peer session expired");
break;
}p->descriptor.remaining_lifetime_ms-=uint32_t(elapsed);
bool ok=p->client.begin(static_cast<const risc_tcp_connection_v1*>(p->tcp.api),p->descriptor,p->random,{p,portable_peer_files::owned,nullptr,nullptr},now);
p->wipe();
if(!ok){p->fail("Unsupported peer endpoint or session");
break;
}p->step=S::Handshake;
p->status.state=PEER_CONNECTING;
break;
}
 case S::Handshake:{auto state=p->client.tick(now);
if(state==Client::State::Retained){p->retain();
break;
}if(state==Client::State::Error){p->fail("Peer authentication failed");
break;
}if(state==Client::State::Ready){if(!p->client.request("OPTIONS","/",nullptr,0,{p,portable_peer_files::owned,nullptr,nullptr},now)){p->fail("Could not query peer capabilities");break;}p->step=S::Capabilities;}
break;
}
 case S::Capabilities:{auto state=p->client.tick(now);
if(state==Client::State::Retained){p->retain();break;}
if(state==Client::State::Error){p->fail("Could not query peer capabilities");break;}
if(state==Client::State::Ready){p->status.capabilities=p->client.capabilities();if(!p->remote.list("/",now)){p->fail("Could not list peer files");
break;
}p->step=S::Operation;
p->status.state=PEER_WORKING;
}break;
}
 case S::Browse:{auto state=p->client.tick(now);
if(state==Client::State::Retained){p->retain();
break;
}if(state==Client::State::Error){p->fail("Peer session expired or ended");
break;
}if(p->cancelRequested){p->cancelRequested=false;p->command=C::None;p->status.operation_ok=false;p->status.completed_generation=p->status.operation_generation;snprintf(p->status.message,sizeof(p->status.message),"Operation cancelled");break;}
if(p->command==C::None)break;
bool ok=false;
switch(p->command){case C::List:ok=p->remote.list(p->first,now);
break;
case C::Preview:ok=p->remote.preview(p->first,p->previewLimit,now,p->previewOffset);
break;
case C::Download:ok=p->remote.download(p->first,p->local,p->second,now);
break;
case C::Upload:ok=p->remote.upload(p->local,p->first,p->second,now,p->replace);
break;
case C::Mutate:ok=p->remote.mutate(p->method,p->first,p->second[0]?p->second:nullptr,now);
break;
default:break;
}p->command=C::None;
if(!ok&&p->remote.state()!=RemoteVolume::State::Close){
if(p->remote.state()==RemoteVolume::State::Retained){p->retain();break;}
p->status.completed_generation=p->status.operation_generation;
snprintf(p->status.message,sizeof(p->status.message),"%s",p->remote.error()[0]?p->remote.error():"Could not start remote operation");
break;
}p->status.published_path[0]=0;
p->step=S::Operation;
p->status.state=PEER_WORKING;
break;
}
 case S::Operation:{if(p->cancelRequested){p->cancelRequested=false;p->remote.cancel();}
auto state=p->remote.tick(now);
p->status.progress=p->remote.progress();
p->status.uncertain=p->remote.uncertain();
if(state==RemoteVolume::State::Retained){p->retain();
break;
}if(state==RemoteVolume::State::Done||state==RemoteVolume::State::Failed){snprintf(p->status.message,sizeof(p->status.message),"%s",state==RemoteVolume::State::Done?"Operation complete":p->remote.error());
p->status.operation_ok=state==RemoteVolume::State::Done;p->status.completed_generation=p->status.operation_generation;
p->status.preview_size=p->remote.previewSize();p->status.preview_offset=p->status.operation_ok?p->previewOffset:0;p->status.preview_total=p->remote.previewTotal();p->status.preview_truncated=p->remote.previewTruncated();
if(const char*path=p->remote.publishedPath())snprintf(p->status.published_path,sizeof(p->status.published_path),"%s",path);
snprintf(p->status.path,sizeof(p->status.path),"%s",p->remote.directory());
p->status.count=p->remote.count();
p->status.state=PEER_READY;
p->step=S::Browse;
}break;
}
 case S::StopRemote:p->remote.cancel();
if(p->remote.state()==RemoteVolume::State::Network||p->remote.state()==RemoteVolume::State::Close||p->remote.state()==RemoteVolume::State::Sync||p->remote.state()==RemoteVolume::State::Publish)p->remote.tick(now);
if(p->remote.state()==RemoteVolume::State::Retained){p->retain();
break;
}if(p->remote.close())p->step=S::StopClient;
else {if(p->remote.state()!=RemoteVolume::State::Network)p->client.tick(now);
if(p->client.state()==Client::State::Retained)p->retain();
}break;
 case S::StopClient:p->client.cancel();
if(p->client.tick(now)==Client::State::Retained){p->retain();
break;
}if(p->client.state()==Client::State::Off)p->step=S::StopBle;
break;
 case S::StopBle:if(p->closeBroker())p->step=S::CloseNetwork;
break;
 case S::CloseNetwork:
  if(p->networkInitialized){int rc=portable_network_session_close(&p->network);if(rc==PORTABLE_NETWORK_CLOSE_RETAINED){p->retain();break;}if(rc!=PORTABLE_NETWORK_CLOSED)break;}
  p->networkReady=p->networkInitialized=false;p->step=S::StopGrants;break;
 case S::StopGrants:
 if(p->wifi.api){p->release(p->wifi);break;}if(p->store.api){p->release(p->store);break;}
if(p->ble.api){p->release(p->ble);
break;
}if(p->entropy.api){p->release(p->entropy);
break;
}if(p->tcp.api){p->release(p->tcp);
break;
}p->wipe();
p->status.state=PEER_OFF;
p->step=S::Off;
break;
 default:break;
}
 if(lease && p->owner()){
   int rc=portable_network_session_service_end(&p->network);
   if(rc==PORTABLE_NETWORK_SERVICE_RETAINED)p->retain();else p->leaseEnding=rc==PORTABLE_NETWORK_SERVICE_PENDING;
 }
 p->traceState();
}
extern "C" void portable_peer_status_get(const portable_peer_files*p,portable_peer_status*out){if(out)*out=p?p->status:portable_peer_status{};
}
extern "C" bool portable_peer_network_owned(const portable_peer_files*p){
 if(!p)return false;
 if(p->status.state==PEER_RETAINED)return true;
 if(p->networkInitialized&&(!p->network.view.borrowed||p->network.operation||
    p->network.service_lease||p->network.service_depth||p->leaseEnding))return true;
 /* A borrowed LAN can still own a TCP request or pending socket close. Only
  * these terminal/idle client states prove that no request holds a socket. */
 const auto state=p->client.state();
 return state!=Client::State::Off&&state!=Client::State::Ready&&state!=Client::State::Error;
}
extern "C" bool portable_peer_row_get(const portable_peer_files*p,unsigned i,portable_peer_row*out){if(!p||!out||i>=p->status.count)return false;
*out={};
if(p->status.state==PEER_DISCOVERING){if(i>=RISC_SESSION_PEERS_MAX)return false;
out->peer_id=p->peers[i].peer_id;
snprintf(out->name,sizeof(out->name),"%s",p->peers[i].name);
return true;
}if(p->status.state!=PEER_READY||p->status.operation_generation!=p->status.completed_generation)return false;
const auto*r=p->remote.entry(i);
if(!r||risc_protected_settings_path(r->path))return false;
out->directory=r->directory;
out->size=r->size;
strcpy(out->name,r->name);
strcpy(out->path,r->path);
return true;
}
static bool queue(portable_peer_files*p,portable_peer_files::Command c,const char*a,const char*b){if(!p||p->status.state!=PEER_READY||p->command!=portable_peer_files::Command::None||p->status.operation_generation==UINT32_MAX||!a||!memchr(a,0,sizeof(p->first))||(b&&!memchr(b,0,sizeof(p->second))))return false;
char encoded[1537];
if(risc_protected_settings_path(a)||risc_protected_settings_path(b)||!Client::encodePath(a,encoded,sizeof(encoded))||(b&&!Client::encodePath(b,encoded,sizeof(encoded))))return false;
strcpy(p->first,a);
p->second[0]=0;
if(b)strcpy(p->second,b);
p->command=c;p->previewOffset=0;++p->status.operation_generation;
p->status.operation_ok=false;p->status.uncertain=false;p->status.progress=0;p->status.preview_size=0;p->status.preview_total=0;p->status.preview_offset=0;p->status.preview_truncated=false;p->status.published_path[0]=0;p->status.message[0]=0;
return true;
}
extern "C" bool portable_peer_select(portable_peer_files*p,uint32_t id){if(!p||p->status.state!=PEER_DISCOVERING||p->command!=portable_peer_files::Command::None)return false;
p->selected=id;
p->command=portable_peer_files::Command::Select;
return true;
}
extern "C" bool portable_peer_confirm(portable_peer_files*p,uint32_t generation,uint32_t number,bool accept){if(!p||p->status.state!=PEER_PAIRING||p->command!=portable_peer_files::Command::None||generation!=p->status.pairing_generation||number!=p->status.pairing_number)return false;
p->generation=generation;
p->number=number;
p->accept=accept;
p->command=portable_peer_files::Command::Confirm;
return true;
}
extern "C" bool portable_peer_list(portable_peer_files*p,const char*path){return p&&(p->status.capabilities&PEER_CAP_LIST)&&queue(p,portable_peer_files::Command::List,path,nullptr);
}
extern "C" bool portable_peer_preview(portable_peer_files*p,const char*path,size_t limit){return portable_peer_preview_range(p,path,0,limit);}
extern "C" bool portable_peer_preview_range(portable_peer_files*p,const char*path,uint64_t offset,size_t limit){if(offset>64u*1024u*1024u||!p||!(p->status.capabilities&PEER_CAP_READ)||!limit||limit>PORTABLE_PEER_PREVIEW_MAX||!queue(p,portable_peer_files::Command::Preview,path,nullptr))return false;
p->previewLimit=limit;p->previewOffset=offset;return true;
}
extern "C" bool portable_peer_preview_get(const portable_peer_files*p,void*out,size_t capacity,size_t*size){if(size)*size=0;
return p&&p->status.state==PEER_READY&&p->command==portable_peer_files::Command::None&&!p->cancelRequested&&p->status.operation_ok&&p->status.operation_generation==p->status.completed_generation&&p->remote.previewGet(out,capacity,size);
}
extern "C" void portable_peer_cancel_operation(portable_peer_files*p){if(!p||p->stopping||p->status.state==PEER_RETAINED)return;
if(p->step==portable_peer_files::Step::Operation||(p->step==portable_peer_files::Step::Browse&&p->command!=portable_peer_files::Command::None))p->cancelRequested=true;
}
extern "C" bool portable_peer_download(portable_peer_files*p,const char*remote,const risc_storage_volume_api_v1*local,const char*dest){if(!p||!(p->status.capabilities&PEER_CAP_READ)||!local||!queue(p,portable_peer_files::Command::Download,remote,dest))return false;
p->local=local;
return true;
}
extern "C" bool portable_peer_upload(portable_peer_files*p,const risc_storage_volume_api_v1*local,const char*source,const char*remote){if(!p||!(p->status.capabilities&PEER_CAP_PUT)||!local||!queue(p,portable_peer_files::Command::Upload,source,remote))return false;
p->local=local;
p->replace=false;
return true;
}
extern "C" bool portable_peer_replace(portable_peer_files*p,const risc_storage_volume_api_v1*local,const char*source,const char*remote){if(!p||!(p->status.capabilities&PEER_CAP_HEAD)||!portable_peer_upload(p,local,source,remote))return false;
p->replace=true;
return true;
}
extern "C" bool portable_peer_mutate(portable_peer_files*p,const char*method,const char*path,const char*dest){if(!p||!method||!memchr(method,0,12))return false;
uint32_t capability=!strcmp(method,"MKCOL")?PEER_CAP_MKCOL:!strcmp(method,"DELETE")?PEER_CAP_DELETE:!strcmp(method,"MOVE")?PEER_CAP_MOVE:!strcmp(method,"COPY")?PEER_CAP_COPY:0;
if(((!strcmp(method,"MOVE")||!strcmp(method,"COPY"))&&!dest)||!(p->status.capabilities&capability)||!queue(p,portable_peer_files::Command::Mutate,path,dest))return false;
strcpy(p->method,method);
return true;
}
extern "C" void portable_peer_stop(portable_peer_files*p){if(!p||p->stopping||p->status.state==PEER_RETAINED||p->step==portable_peer_files::Step::Off)return;
p->status.state=PEER_STOPPING;p->status.operation_ok=false;p->status.completed_generation=p->status.operation_generation;
p->stopping=true;
p->command=portable_peer_files::Command::None;
p->wipe();
p->remote.cancel();
p->client.cancel();
p->step=portable_peer_files::Step::StopRemote;
}
extern "C" bool portable_peer_destroy(portable_peer_files*p){if(!p)return true;
if(p->step!=portable_peer_files::Step::Off||p->status.state==PEER_RETAINED)return false;
auto hooks=p->hooks;
p->wipe();
p->~portable_peer_files();
hooks.deallocate(p);
return true;
}
