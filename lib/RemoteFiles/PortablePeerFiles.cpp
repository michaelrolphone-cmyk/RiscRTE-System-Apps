#include "PortablePeerFiles.h"
#include "WebDavRemoteVolume.h"
#include <RiscEntropySourceV1.h>
#include <cstring>
#include <cstdio>
#include <new>
using namespace RiscWebDav;
struct portable_peer_files {
 const risc_runtime_api_v1*runtime;
portable_peer_hooks hooks;
Client client;
RemoteVolume remote;
 risc_runtime_capability_v1 tcp{},entropy{},ble{};
risc_bluetooth_session_setup_v1 broker{};
 risc_bluetooth_session_descriptor_v1 descriptor{};
risc_bluetooth_session_setup_status_v1 setup{};
 risc_bluetooth_session_peer_v1 peers[RISC_SESSION_PEERS_MAX]{};
 portable_peer_status status{};
uint8_t random[16]{};
uint64_t token=0,now=0,copiedAt=0;
 enum class Step{Off,Tcp,Entropy,Random,ReleaseEntropy,Ble,Discover,Scan,ReadDescriptor,CloseBle,ReleaseBle,Authenticate,Handshake,Browse,Operation,StopRemote,StopClient,StopBle,StopGrants}step=Step::Off;
 enum class Command{None,Select,Confirm,List,Download,Upload,Mutate}command=Command::None;
 uint32_t selected=0,generation=0,number=0;
bool replace=false,accept=false,stopping=false;
unsigned scanIndex=0;
 const risc_storage_volume_api_v1*local=nullptr;
char first[513]{},second[513]{},method[12]{};
 portable_peer_files(const risc_runtime_api_v1*r,const portable_peer_hooks&h):runtime(r),hooks(h),remote(client,{this,owned}){}
 static bool owned(void*c){auto&p=*static_cast<portable_peer_files*>(c);
return p.status.state!=PEER_RETAINED&&p.hooks.owner(p.hooks.context);
}
 void wipe(){volatile unsigned char*p=reinterpret_cast<volatile unsigned char*>(&descriptor);
for(size_t i=0;i<sizeof(descriptor);i++)p[i]=0;
memset(random,0,sizeof(random));
}
 void retain(){status.state=PEER_RETAINED;
client.tick(now);
 /* owned() is false; wipes session material without I/O. */
snprintf(status.message,sizeof(status.message),"Resource custody retained; restart required");
wipe();
}
 bool owner(){if(owned(this))return true;
retain();
return false;
}
 void fail(const char*s){snprintf(status.message,sizeof(status.message),"%s",s);
status.state=PEER_ERROR;
stopping=true;
step=Step::StopRemote;
wipe();
}
 bool acquire(const char*cap,risc_runtime_capability_v1&g){g={};
g.struct_size=sizeof(g);
bool ok=runtime->acquire(cap,1,0,&g);
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
p->now=now;
p->status={};
p->status.state=PEER_STARTING;
p->stopping=false;
p->step=portable_peer_files::Step::Tcp;
return true;
}
extern "C" void portable_peer_tick(portable_peer_files*p,uint64_t now){
 if(!p||p->step==portable_peer_files::Step::Off||!p->owner())return;
if(now<p->now){p->fail("Peer clock moved backwards");
}p->now=now;
using S=portable_peer_files::Step;
using C=portable_peer_files::Command;
 switch(p->step){
 case S::Tcp:if(p->acquire(RISC_TCP_CONNECTION_CAPABILITY,p->tcp)){auto*t=static_cast<const risc_tcp_connection_v1*>(p->tcp.api);
if(t->api_version!=1||t->struct_size<sizeof(*t)||t->connect_tag!=RISC_TCP_CONNECTION_CONNECT_TAG||!t->connect||!t->connected)p->fail("This Runtime lacks peer TCP support");
else p->step=S::Entropy;
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
 case S::ReleaseEntropy:if(p->release(p->entropy))p->step=S::Ble;
break;
 case S::Ble:if(p->acquire(RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY,p->ble)){auto*b=static_cast<const risc_bluetooth_session_setup_v1*>(p->ble.api);
if(b->api_version!=1||b->struct_size<sizeof(*b)||b->peer_tag!=RISC_SESSION_PEER_TAG||!b->discover||!b->peer||!b->select_peer||!b->peer_descriptor||!b->poll||!b->status||!b->close||!b->confirm_pairing){p->fail("This Bluetooth provider lacks peer discovery");
break;
}p->broker=*b;
p->step=S::Discover;
}break;
 case S::Discover:{int rc=p->broker.discover(p->broker.context,300000,&p->token);
if(!p->brokerResult(rc))break;
if(!p->token){p->retain();
break;
}p->step=S::Scan;
p->status.state=PEER_DISCOVERING;
break;
}
 case S::Scan:{
  if(p->command==C::Select){p->command=C::None;
if(p->brokerResult(p->broker.select_peer(p->broker.context,p->token,p->selected)))p->status.state=PEER_CONNECTING;
break;
}
  if(p->command==C::Confirm){p->command=C::None;
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
p->step=S::CloseBle;
}break;
 case S::CloseBle:if(p->closeBroker())p->step=S::ReleaseBle;
break;
 case S::ReleaseBle:if(p->release(p->ble))p->step=S::Authenticate;
break;
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
}if(state==Client::State::Ready){if(!p->remote.list("/",now)){p->fail("Could not list peer files");
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
}if(p->command==C::None)break;
bool ok=false;
switch(p->command){case C::List:ok=p->remote.list(p->first,now);
break;
case C::Download:ok=p->remote.download(p->first,p->local,p->second,now);
break;
case C::Upload:ok=p->remote.upload(p->local,p->first,p->second,now,p->replace);
break;
case C::Mutate:ok=p->remote.mutate(p->method,p->first,p->second[0]?p->second:nullptr,now);
break;
default:break;
}p->command=C::None;
if(!ok&&p->remote.state()!=RemoteVolume::State::Close){snprintf(p->status.message,sizeof(p->status.message),"%s",p->remote.error());
break;
}p->status.published_path[0]=0;
p->step=S::Operation;
p->status.state=PEER_WORKING;
break;
}
 case S::Operation:{auto state=p->remote.tick(now);
p->status.progress=p->remote.progress();
p->status.uncertain=p->remote.uncertain();
if(state==RemoteVolume::State::Retained){p->retain();
break;
}if(state==RemoteVolume::State::Done||state==RemoteVolume::State::Failed){snprintf(p->status.message,sizeof(p->status.message),"%s",state==RemoteVolume::State::Done?"Operation complete":p->remote.error());
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
 case S::StopBle:if(p->closeBroker())p->step=S::StopGrants;
break;
 case S::StopGrants:if(p->ble.api){p->release(p->ble);
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
}
extern "C" void portable_peer_status_get(const portable_peer_files*p,portable_peer_status*out){if(out)*out=p?p->status:portable_peer_status{};
}
extern "C" bool portable_peer_row_get(const portable_peer_files*p,unsigned i,portable_peer_row*out){if(!p||!out||i>=p->status.count)return false;
*out={};
if(p->status.state==PEER_DISCOVERING){if(i>=RISC_SESSION_PEERS_MAX)return false;
out->peer_id=p->peers[i].peer_id;
snprintf(out->name,sizeof(out->name),"%s",p->peers[i].name);
return true;
}const auto*r=p->remote.entry(i);
if(!r)return false;
out->directory=r->directory;
out->size=r->size;
strcpy(out->name,r->name);
strcpy(out->path,r->path);
return true;
}
static bool queue(portable_peer_files*p,portable_peer_files::Command c,const char*a,const char*b){if(!p||p->status.state!=PEER_READY||p->command!=portable_peer_files::Command::None||!a||!memchr(a,0,sizeof(p->first))||(b&&!memchr(b,0,sizeof(p->second))))return false;
strcpy(p->first,a);
p->second[0]=0;
if(b)strcpy(p->second,b);
p->command=c;
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
extern "C" bool portable_peer_list(portable_peer_files*p,const char*path){return queue(p,portable_peer_files::Command::List,path,nullptr);
}
extern "C" bool portable_peer_download(portable_peer_files*p,const char*remote,const risc_storage_volume_api_v1*local,const char*dest){if(!local||!queue(p,portable_peer_files::Command::Download,remote,dest))return false;
p->local=local;
return true;
}
extern "C" bool portable_peer_upload(portable_peer_files*p,const risc_storage_volume_api_v1*local,const char*source,const char*remote){if(!local||!queue(p,portable_peer_files::Command::Upload,source,remote))return false;
p->local=local;
p->replace=false;
return true;
}
extern "C" bool portable_peer_replace(portable_peer_files*p,const risc_storage_volume_api_v1*local,const char*source,const char*remote){if(!portable_peer_upload(p,local,source,remote))return false;
p->replace=true;
return true;
}
extern "C" bool portable_peer_mutate(portable_peer_files*p,const char*method,const char*path,const char*dest){if(!method||!memchr(method,0,12)||!queue(p,portable_peer_files::Command::Mutate,path,dest))return false;
strcpy(p->method,method);
return true;
}
extern "C" void portable_peer_stop(portable_peer_files*p){if(!p||p->stopping||p->status.state==PEER_RETAINED||p->step==portable_peer_files::Step::Off)return;
p->status.state=PEER_STOPPING;
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
