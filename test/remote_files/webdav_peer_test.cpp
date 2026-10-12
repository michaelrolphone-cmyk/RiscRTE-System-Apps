#include "WebDavClient.h"
#include "WebDavRemoteVolume.h"
#include "WebDavSharing.h"
#include "runtime/storage/AppDataExport.h"
#include <map>
#include <vector>
#include "UserFilesVolume.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
using namespace RiscWebDav;
struct Store {
 std::string bytes;uint64_t revision=0;bool available=true,retained=false;unsigned calls=0;
 risc_app_data_v1 api{1,sizeof(api),this,
 [](void*c,const char*n,uint32_t*size,uint64_t*rev)->int32_t{auto&s=*static_cast<Store*>(c);assert(!strcmp(n,"documents.ruf"));s.calls++;*size=0;*rev=0;if(s.retained)return -9;if(!s.available)return -5;if(!s.revision)return -1;*size=s.bytes.size();*rev=s.revision;return 0;},
 [](void*c,const char*n,uint64_t rev,void*out,uint32_t cap,uint32_t*size,uint64_t*next)->int32_t{auto&s=*static_cast<Store*>(c);assert(!strcmp(n,"documents.ruf"));s.calls++;*size=0;*next=0;if(rev!=s.revision)return -10;if(cap<s.bytes.size())return -2;memcpy(out,s.bytes.data(),s.bytes.size());*size=s.bytes.size();*next=s.revision;return 0;},
 [](void*c,const char*n,uint64_t rev,const void*in,uint32_t size)->int32_t{auto&s=*static_cast<Store*>(c);assert(!strcmp(n,"documents.ruf"));s.calls++;if(!s.available)return -5;if(rev!=s.revision)return -10;s.bytes.assign(static_cast<const char*>(in),size);s.revision++;return 0;}};
};
struct Link {
 bool owner=true,clientLive=false,serverLive=false,accepted=false,drop=false,closeFault=false;uint64_t serial=10;std::string up,down;size_t upAt=0,downAt=0;unsigned calls=0,refuseConnect=0,readChunk=31,writeChunk=41;size_t clientBytes=0;
 risc_tcp_connection_v1 tcp{1,sizeof(tcp),this,
 [](void*,const risc_tcp_connection_listen_v1*,uint64_t*out)->int32_t{*out=1;return 0;},
 [](void*c,uint64_t,uint64_t*out)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*out=0;if(!s.clientLive||s.accepted)return 1;s.accepted=true;s.serverLive=true;*out=s.serial+1000;return 0;},
 [](void*c,uint64_t h,void*out,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*n=0;if(s.drop)return -4;bool server=h>1000;auto&b=server?s.up:s.down;auto&at=server?s.upAt:s.downAt;*n=std::min<size_t>({cap,s.readChunk,b.size()-at});if(!*n)return server?(!s.clientLive?2:1):(!s.serverLive&&s.accepted?2:1);memcpy(out,b.data()+at,*n);at+=*n;if(!server)s.clientBytes+=*n;return 0;},
 [](void*c,uint64_t h,const void*in,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*n=0;if(s.drop||(h>1000&&!s.clientLive))return -4;*n=std::min(cap,s.writeChunk);(h>1000?s.down:s.up).append(static_cast<const char*>(in),*n);return 0;},
 [](void*c,uint64_t h)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;if(s.closeFault)return -6;if(h>1000)s.serverLive=false;else if(h>1)s.clientLive=false;return 0;},RISC_TCP_CONNECTION_CONNECT_TAG,
 [](void*c,const risc_tcp_connection_listen_v1*r,uint64_t*out)->int32_t{auto&s=*static_cast<Link*>(c);assert(r->port==8080&&r->address[0]==192);assert(!s.clientLive&&!s.serverLive);s.calls++;if(s.refuseConnect){s.refuseConnect--;*out=0;return -4;}s.up.clear();s.down.clear();s.upAt=s.downAt=0;s.clientBytes=0;s.accepted=false;s.clientLive=true;*out=++s.serial;return 0;},
 [](void*c,uint64_t)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;return s.drop?-4:0;}};
};
struct Fixture {
 Store store;RiscUserFiles::Volume volume;Link link;DigestSession digest;Sharing server;Client client;uint64_t now=1;std::string upload,download;size_t at=0;
 Client::Io io(){return {this,[](void*c){return static_cast<Fixture*>(c)->link.owner;},[](void*c,void*out,size_t cap,size_t*n){auto&f=*static_cast<Fixture*>(c);*n=std::min(cap,f.upload.size()-f.at);memcpy(out,f.upload.data()+f.at,*n);f.at+=*n;return true;},[](void*c,const void*in,size_t n){static_cast<Fixture*>(c)->download.append(static_cast<const char*>(in),n);return true;}};}
 Fixture(const risc_storage_volume_api_v1*external=nullptr){assert(volume.begin(&store.api,{this,[](void*c){return static_cast<Fixture*>(c)->link.owner;},malloc,free}));
 char password[]="test-password";uint8_t nonce[32];for(unsigned i=0;i<32;i++)nonce[i]=uint8_t(i+1);assert(digest.begin("files",password,strlen(password),"Saved files",nonce,32,0,300000));
 risc_tcp_connection_listen_v1 listen{sizeof(listen),{192,168,2,7},8080,0};assert(server.begin(external?external:volume.api(),&link.tcp,&digest,{this,[](void*c){return static_cast<Fixture*>(c)->link.owner;},malloc,free},listen,0,300000));server.tick(0);
 risc_bluetooth_session_descriptor_v1 descriptor{sizeof(descriptor),RISC_SESSION_HTTP,300000,"http://192.168.2.7:8080/","files","test-password","one"};if(const char*path=getenv("PEER_BLE_DESCRIPTOR")){FILE*in=fopen(path,"rb");assert(in);assert(fread(&descriptor,1,sizeof(descriptor),in)==sizeof(descriptor));assert(fgetc(in)==EOF);fclose(in);}assert(client.begin(&link.tcp,descriptor,nonce,io(),0));run();assert(client.ready());}
 void run(){for(unsigned i=0;i<25000&&client.state()!=Client::State::Ready&&client.state()!=Client::State::Error&&client.state()!=Client::State::Off&&client.state()!=Client::State::Retained;i++){unsigned before=link.calls;client.tick(now);assert(link.calls-before<=1);server.tick(now++);}if(client.state()!=Client::State::Ready&&client.state()!=Client::State::Off)fprintf(stderr,"state=%u error=%u status=%u \n",unsigned(client.state()),unsigned(client.error()),client.status());assert(client.state()==Client::State::Ready||client.state()==Client::State::Error||client.state()==Client::State::Off||client.state()==Client::State::Retained);}
 void inject(const std::string&reply){for(unsigned i=0;i<10000&&client.state()!=Client::State::Reply;i++){unsigned before=link.calls;client.tick(now++);assert(link.calls-before<=1);}assert(client.state()==Client::State::Reply);link.down=reply;link.accepted=true;}
 void list(RemoteVolume& remote,const char*path){assert(remote.list(path,now));for(unsigned i=0;i<25000;i++){unsigned before=link.calls;auto state=remote.tick(now);assert(link.calls-before<=1);server.tick(now++);if(state==RemoteVolume::State::Done||state==RemoteVolume::State::Failed||state==RemoteVolume::State::Retained)break;}if(remote.state()!=RemoteVolume::State::Done)fprintf(stderr,"list %s: %s\n",path,remote.error());assert(remote.state()==RemoteVolume::State::Done);}
 unsigned request(const char*m,const char*p,const char*d=nullptr,const std::string&body="",const char*match=nullptr){upload=body;at=0;download.clear();assert(client.request(m,p,d,body.size(),io(),now,match));run();assert(client.ready());return client.status();}
 ~Fixture(){if(!link.owner||client.state()==Client::State::Retained)return;client.end();run();server.requestStop();for(unsigned i=0;i<20&&server.status().state!=SharingState::Off;i++)server.tick(now++);assert(volume.end());}
};
#include "peer_controller_fixture.inc"
struct ExistingFiles {
 struct Record{std::string bytes;uint64_t revision;};
 std::map<std::pair<uint32_t,std::string>,Record> records;unsigned calls=0,replacements=0;int32_t replaceResult=0;bool publishUnknown=false;
 RiscBoot::AppDataBackend backend{this,
 [](void*c,uint32_t ns,const char*n,uint32_t*z,uint64_t*r)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);assert(ns==1||ns==2||ns==3||ns==62||ns==64);s.calls++;auto i=s.records.find({ns,n});*z=0;*r=0;if(i==s.records.end())return -1;*z=i->second.bytes.size();*r=i->second.revision;return 0;},
 [](void*c,uint32_t ns,const char*n,uint64_t rev,void*b,uint32_t cap,uint32_t*z,uint64_t*r)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);auto i=s.records.find({ns,n});if(i==s.records.end())return -1;if(rev!=i->second.revision)return -10;if(cap<i->second.bytes.size())return -2;memcpy(b,i->second.bytes.data(),i->second.bytes.size());*z=i->second.bytes.size();*r=i->second.revision;return 0;},
 [](void*c,uint32_t ns,const char*n,uint64_t rev,const void*b,uint32_t z)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);++s.replacements;auto i=s.records.find({ns,n});if((i==s.records.end()?0:i->second.revision)!=rev)return -10;if(s.replaceResult&&!(s.replaceResult==RISC_APP_DATA_COMMIT_UNKNOWN&&s.publishUnknown))return s.replaceResult;s.records[{ns,n}]={std::string(static_cast<const char*>(b),z),rev+1};return s.replaceResult;},[](void*){return true;}};
};
struct AtomicLocal {
 ExistingFiles files;bool owned=true,available=true;risc_app_data_export_v1 api{};
 RiscStorage::AppDataExport exported;
 AtomicLocal():exported({this,[](void*c){return static_cast<AtomicLocal*>(c)->owned;},[](void*c){return static_cast<AtomicLocal*>(c)->available;},malloc,free}){
  RiscStorage::AppDataExport::Entry rows[]={{"model_viewer",64,"model.obj","/models/model.obj",true},{"model_viewer",64,"model.stl","/models/model.stl",false}};
  files.records[{64,"model.stl"}]={"readonly source",1};assert(exported.configure(&files.backend,rows,2,"Watch user files")&&exported.begin(&api));
 }
 const risc_storage_volume_api_v1*volume(){return &api.volume.terminal.power.volume.base;}
 ~AtomicLocal(){if(owned&&!exported.retained()){available=true;assert(exported.end());}}
};
[[maybe_unused]] static void atomicTransferTests(){
 auto pump=[](Fixture&f,RemoteVolume&r){for(unsigned i=0;i<30000;i++){unsigned before=f.link.calls;auto state=r.tick(f.now);assert(f.link.calls-before<=1);f.server.tick(f.now++);if(state==RemoteVolume::State::Done||state==RemoteVolume::State::Failed||state==RemoteVolume::State::Retained)return;}assert(!"atomic transfer timeout");};
 {Fixture f;AtomicLocal local;std::string bytes(10000,'a');bytes[3000]=0;assert(f.request("PUT","/model.obj",nullptr,bytes)==201);f.link.readChunk=f.link.writeChunk=2048;
  RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
  assert(remote.download("/model.obj",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Done&&remote.publishedPath()&&!strcmp(remote.publishedPath(),"/models/model.obj")&&local.files.records.at({64,"model.obj"}).bytes==bytes&&local.files.replacements==1);
  assert(remote.download("/model.obj",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&local.files.replacements==1&&!remote.publishedPath());
  unsigned before=local.files.calls;assert(remote.download("/model.obj",local.volume(),"/models/other.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&local.files.calls==before);
  assert(remote.download("/model.obj",local.volume(),"/models/model.stl",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&local.files.records.at({64,"model.stl"}).bytes=="readonly source");
  assert(remote.upload(local.volume(),"/models/model.obj","/copied.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Done);assert(f.request("GET","/copied.obj")==200&&f.download==bytes);
  assert(remote.upload(local.volume(),"/models/model.stl","/readonly.stl",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Done);assert(f.request("GET","/readonly.stl")==200&&f.download=="readonly source");
  // Generic SD has no strong revision: replacement refuses, preserving bytes.
  assert(remote.upload(local.volume(),"/models/model.obj","/copied.obj",f.now,true));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&strstr(remote.error(),"revision"));assert(f.request("GET","/copied.obj")==200&&f.download==bytes);
  local.files.records.erase({64,"model.obj"});before=f.link.serial;assert(remote.upload(local.volume(),"/models/model.obj","/absent.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&f.link.serial==before);
  local.available=false;assert(remote.download("/model.obj",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&f.link.serial==before);local.available=true;
  assert(remote.download("/model.obj",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Done);
  local.files.records.erase({64,"model.obj"});assert(f.request("PUT","/empty")==201);assert(remote.download("/empty",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Done&&local.files.records.at({64,"model.obj"}).bytes.empty());assert(remote.close());
 }
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/source",nullptr,"incoming")==201);RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/source",local.volume(),"/models/model.obj",f.now));remote.tick(f.now++);assert(remote.state()==RemoteVolume::State::Network);local.files.records[{64,"model.obj"}]={"concurrent create",1};pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!remote.publishedPath()&&local.files.replacements==1&&local.files.records.at({64,"model.obj"}).bytes=="concurrent create");assert(remote.close());}
 for(int32_t result:{RISC_APP_DATA_IO,RISC_APP_DATA_STALE,RISC_APP_DATA_COMMIT_UNKNOWN})for(bool published:{false,true}){
  if(result!=RISC_APP_DATA_COMMIT_UNKNOWN&&published)continue;Fixture f;AtomicLocal local;assert(f.request("PUT","/source",nullptr,"saved bytes")==201);local.files.replaceResult=result;local.files.publishUnknown=published;
  RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/source",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!remote.publishedPath()&&local.files.replacements==1&&remote.uncertain()==(result==RISC_APP_DATA_COMMIT_UNKNOWN));assert(local.files.records.count({64,"model.obj"})==unsigned(published));assert(remote.close());
 }
 for(bool afterOpen:{false,true}){Fixture f;AtomicLocal local;assert(f.request("PUT","/source",nullptr,std::string(2000,'x'))==201);RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/source",local.volume(),"/models/model.obj",f.now));if(afterOpen){remote.tick(f.now++);assert(remote.state()==RemoteVolume::State::Network);}remote.cancel();pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!local.files.replacements&&!local.files.records.count({64,"model.obj"}));assert(remote.close());}
 // Temporary unavailability after a writer opens keeps custody until abort can close.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/source",nullptr,std::string(2000,'x'))==201);RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/source",local.volume(),"/models/model.obj",f.now));remote.tick(f.now++);local.available=false;remote.cancel();for(unsigned i=0;i<5;++i)remote.tick(f.now++);assert(remote.state()==RemoteVolume::State::Close&&!remote.close());local.available=true;pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!local.files.replacements&&remote.close());}
 // Source admission, read-close cancellation and pending cleanup use the same exact export contract.
 {Fixture f;AtomicLocal local;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});local.available=false;assert(remote.upload(local.volume(),"/models/model.stl","/out",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!f.link.clientLive);local.available=true;assert(remote.upload(local.volume(),"/models/model.stl","/out",f.now));for(unsigned i=0;i<3&&remote.state()==RemoteVolume::State::Prepare;++i)remote.tick(f.now++);assert(remote.state()==RemoteVolume::State::Network);remote.cancel();pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&remote.close());}
 {Fixture f;AtomicLocal local;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.upload(local.volume(),"/models/model.stl","/out",f.now));for(unsigned i=0;i<3&&remote.state()==RemoteVolume::State::Prepare;++i)remote.tick(f.now++);local.available=false;remote.cancel();for(unsigned i=0;i<5;++i)remote.tick(f.now++);assert(remote.state()==RemoteVolume::State::Close&&!remote.close());local.available=true;pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&remote.close());}
 {Fixture f;AtomicLocal local;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/too-big",local.volume(),"/models/model.obj",f.now));remote.tick(f.now++);f.inject("HTTP/1.1 200 OK\r\nContent-Length: 65537\r\n\r\n"+std::string(512,'x'));pump(f,remote);assert(remote.state()==RemoteVolume::State::Failed&&!local.files.replacements&&!local.files.records.count({64,"model.obj"})&&remote.close());}
 // A retained atomic publication must never be retried or aborted.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/source",nullptr,"bytes")==201);local.files.replaceResult=RISC_APP_DATA_RETAINED;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.download("/source",local.volume(),"/models/model.obj",f.now));pump(f,remote);assert(remote.state()==RemoteVolume::State::Retained&&local.exported.retained()&&local.files.replacements==1);unsigned before=f.link.calls;remote.tick(f.now++);remote.cancel();assert(!remote.close()&&f.link.calls==before&&local.files.replacements==1);}
 // Loss of exact export custody fences further network/storage and cleanup.
 {Fixture f;AtomicLocal local;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(remote.upload(local.volume(),"/models/model.stl","/out",f.now));local.owned=false;unsigned before=f.link.calls;assert(remote.tick(f.now++)==RemoteVolume::State::Retained&&!remote.close()&&f.link.calls==before);}
 puts("Atomic Watch peer transfer: exclusive mapped creates, bounded reads/writes, source snapshots, cancellation, unavailable gate and unknown-commit preservation PASS");
}
struct RawPeer {
 Link link;Client client;RemoteVolume remote;uint64_t now=1;
 static bool owner(void*c){return static_cast<Link*>(c)->owner;}
 RawPeer():remote(client,{&link,owner}){
  uint8_t entropy[16]{};risc_bluetooth_session_descriptor_v1 descriptor{sizeof(descriptor),RISC_SESSION_HTTP,300000,"http://192.168.2.7:8080/","files","test-password","fixture"};
  assert(client.begin(&link.tcp,descriptor,entropy,{&link,owner,nullptr,nullptr},now));
  until(Client::State::Reply);link.down="HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nWWW-Authenticate: Digest realm=\"files\", nonce=\"nonce\", algorithm=SHA-256, qop=\"auth\"\r\n\r\n";link.accepted=true;until(Client::State::Ready);
 }
 void tick(){unsigned before=link.calls;remote.tick(now++);assert(link.calls-before<=1);}
 void until(Client::State expected){for(unsigned i=0;i<10000&&client.state()!=expected;i++){unsigned before=link.calls;client.tick(now++);assert(link.calls-before<=1);}assert(client.state()==expected);}
 void preview(){assert(remote.preview("/large.txt",2048,now));}
 void reply(){until(Client::State::Reply);link.down="HTTP/1.1 200 OK\r\nContent-Length: 10000\r\n\r\n"+std::string(10000,'p');link.accepted=true;}
 void done(){for(unsigned i=0;i<10000&&remote.state()==RemoteVolume::State::Network;i++)tick();}
};
[[maybe_unused]] static void previewFailureTests(){
 // Cancellation at each request phase closes once, hides partial bytes, and permits a later request.
 for(auto phase:{Client::State::Connect,Client::State::Connecting,Client::State::Header,Client::State::Reply,Client::State::Body,Client::State::Closing}){
  RawPeer f;f.preview();if(phase==Client::State::Body||phase==Client::State::Closing)f.reply();f.until(phase);f.remote.cancel();f.done();assert(f.remote.state()==RemoteVolume::State::Failed&&f.client.ready()&&!f.link.clientLive);char bytes[2048];size_t size=99;assert(!f.remote.previewGet(bytes,sizeof(bytes),&size)&&!size);
  f.preview();f.reply();f.done();assert(f.remote.state()==RemoteVolume::State::Done&&f.remote.previewGet(bytes,sizeof(bytes),&size)&&size==sizeof(bytes));assert(f.remote.close());
 }
 for(auto phase:{Client::State::Connect,Client::State::Header,Client::State::Body}){
  RawPeer f;f.preview();if(phase==Client::State::Body)f.reply();f.until(phase);unsigned before=f.link.calls;f.link.owner=false;assert(f.remote.tick(f.now++)==RemoteVolume::State::Retained);f.remote.cancel();assert(!f.remote.close());assert(f.remote.tick(f.now++)==RemoteVolume::State::Retained&&f.link.calls==before);
 }
 {RawPeer f;f.preview();f.reply();f.until(Client::State::Closing);f.link.closeFault=true;f.tick();assert(f.remote.state()==RemoteVolume::State::Retained&&f.client.state()==Client::State::Retained);unsigned before=f.link.calls;f.tick();f.remote.cancel();assert(!f.remote.close()&&f.link.calls==before);}
 {RawPeer f;f.preview();f.reply();f.until(Client::State::Body);f.now=300001;f.done();assert(f.remote.state()==RemoteVolume::State::Failed&&f.client.error()==Client::Expired&&!f.remote.previewSize()&&!f.link.clientLive);assert(f.remote.close());}
}
#ifndef PEER_UI_BACKEND
int main(){
 controllerTests();previewFailureTests();atomicTransferTests();
 {ExistingFiles files;RiscStorage::AppDataExport exported({nullptr,[](void*){return true;},[](void*){return true;},malloc,free});
  RiscStorage::AppDataExport::Entry rows[]={
   {"timecard",1,"timecard.json","/timecard/timecard.json",true},
   {"audio_spectrum",2,"spectrum-events-a.sqt","/audio-spectrum/spectrum-events-a.sqt",true},
   {"audio_spectrum",2,"spectrum-events-b.sqt","/audio-spectrum/spectrum-events-b.sqt",true},
   {"waterfall",3,"rf-events-a.rft","/waterfall/rf-events-a.rft",true},
   {"waterfall",3,"rf-events-b.rft","/waterfall/rf-events-b.rft",true},
   {"lists",62,"lists.bin","/lists/lists.bin",true},
   {"model_viewer",64,"model.obj","/models/model.obj",true},
   {"model_viewer",64,"model.stl","/models/model.stl",true}};
  for(const auto&r:rows)files.records[{r.nameSpace,r.name}]={std::string("existing ")+r.name,1};
  risc_app_data_export_v1 api{};assert(exported.configure(&files.backend,rows,8,"Watch user files")&&exported.begin(&api));
  {Fixture f(&api.volume.terminal.power.volume.base);
   assert(f.client.capabilities()==0);assert(f.request("OPTIONS","/")==200);assert(f.client.capabilities()==(Client::List|Client::Read|Client::Head|Client::Put));
   RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
   f.list(remote,"/");assert(remote.count()==5);for(unsigned i=0;i<remote.count();++i)assert(remote.entry(i)->directory);
   f.list(remote,"/audio-spectrum");assert(remote.count()==2);for(unsigned i=0;i<remote.count();++i)assert(!remote.entry(i)->directory&&remote.entry(i)->size==std::string("existing spectrum-events-a.sqt").size());
   f.list(remote,"/models");assert(remote.count()==2);
   // A mapped directory remains navigable with no existing mapped files.
   files.records.erase({64,"model.obj"});files.records.erase({64,"model.stl"});f.list(remote,"/models");assert(remote.count()==0);
   for(const auto&r:rows)files.records[{r.nameSpace,r.name}]={std::string("existing ")+r.name,1};
   assert(f.request("PROPFIND","/")==207&&f.download.find("models")!=std::string::npos&&f.download.find("lists")!=std::string::npos);
   for(const auto&r:rows){assert(f.request("GET",r.path)==200&&f.download==std::string("existing ")+r.name);assert(f.request("HEAD",r.path)==200);std::string revision=f.client.etag();assert(!revision.empty());assert(f.request("PUT",r.path,nullptr,"replacement",revision.c_str())==204);assert(files.records.at({r.nameSpace,r.name}).bytes=="replacement");assert(f.request("PUT",r.path,nullptr,"stale",revision.c_str())==412);assert(f.request("DELETE",r.path)==405);}
   unsigned before=files.calls;assert(f.request("GET","/credentials/wifi")>=400&&files.calls==before);
  }
  {PeerControllerFixture p(&api.volume.terminal.power.volume.base);p.connect();assert(p.status.capabilities==(PEER_CAP_LIST|PEER_CAP_READ|PEER_CAP_HEAD|PEER_CAP_PUT));unsigned before=p.f.link.calls;
   assert(!portable_peer_mutate(p.peer,"MKCOL","/New",nullptr));assert(!portable_peer_mutate(p.peer,"DELETE","/models/model.obj",nullptr));assert(!portable_peer_mutate(p.peer,"MOVE","/models/model.obj","/models/other.obj"));assert(!portable_peer_mutate(p.peer,"COPY","/models/model.obj","/models/other.obj"));assert(p.f.link.calls==before);p.stop();}
  assert(exported.end());
 }

 {Store backing;RiscUserFiles::Volume volume;assert(volume.begin(&backing.api,{nullptr,[](void*){return true;},malloc,free}));
  auto readonly=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(volume.api());auto&ext=readonly.state.prepared.base.sleep.terminal.power.volume;auto&base=ext.base;base.file_open_write=nullptr;base.file_write=nullptr;base.remove=nullptr;ext.rename=nullptr;ext.mkdir=nullptr;ext.file_sync=nullptr;
  {PeerControllerFixture p(&base);p.connect();assert(p.status.capabilities==(PEER_CAP_LIST|PEER_CAP_READ|PEER_CAP_HEAD));unsigned before=p.f.link.calls;assert(!portable_peer_mutate(p.peer,"MKCOL","/New",nullptr)&&!portable_peer_upload(p.peer,&base,"/x","/x")&&p.f.link.calls==before);p.stop();}
  assert(volume.end());
 }
 {Fixture f;assert(!f.client.capabilities());assert(f.request("OPTIONS","/")==200&&f.client.capabilities()==255u);assert(f.request("MKCOL","/Documents")==201);std::string text(12000,'x');text.replace(40,10,"watch data");assert(f.request("PUT","/Documents/read me.txt",nullptr,text)==201);assert(f.request("GET","/Documents/read me.txt")==200&&f.download==text);assert(f.request("PROPFIND","/Documents")==207&&f.download.find("read%20me.txt")!=std::string::npos);assert(f.request("PUT","/Documents/read me.txt",nullptr,"overwrite")==412);assert(f.request("MOVE","/Documents/read me.txt","/Documents/renamed.txt")==201);assert(f.request("COPY","/Documents/renamed.txt","/copy.txt")==201);assert(f.request("GET","/copy.txt")==200&&f.download==text);assert(f.request("DELETE","/copy.txt")==204);assert(f.request("GET","/copy.txt")==404);}
 {Fixture f;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
  f.list(remote,"/");assert(!remote.count());assert(f.request("MKCOL","/Documents")==201);assert(f.request("MKCOL","/Empty")==201);
  assert(f.request("PUT","/root & file.txt",nullptr,"root bytes")==201);assert(f.request("PUT","/Documents/read me & <this>.txt",nullptr,"nested bytes")==201);
  f.list(remote,"/");assert(remote.count()==3);f.list(remote,"/Documents");assert(remote.count()==1&&!remote.entry(0)->directory&&!strcmp(remote.entry(0)->name,"read me & <this>.txt")&&remote.entry(0)->size==12);
  f.list(remote,"/Empty");assert(!remote.count());
 }
 {Fixture f;assert(f.request("PUT","/empty.txt")==201);assert(f.request("GET","/empty.txt")==200&&f.download.empty());assert(f.request("GET","/secret")==404);}
 {Fixture f;assert(!f.client.request("GET","/../private",nullptr,0,f.io(),f.now));assert(!f.client.request("GET","/bad\\path",nullptr,0,f.io(),f.now));}
 {Fixture f;Store localStore;RiscUserFiles::Volume local;assert(local.begin(&localStore.api,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;},malloc,free}));
  auto api=local.api();auto ext=risc_storage_volume_extension(api);assert(ext->mkdir(api->context,"/Downloads"));
  uint64_t now=f.now;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
  auto pump=[&](){for(unsigned tick=0;tick<20000;tick++){auto state=remote.tick(now);f.server.tick(now++);if(state==RemoteVolume::State::Done||state==RemoteVolume::State::Failed||state==RemoteVolume::State::Retained)break;}f.now=now;};
  auto file=api->file_open_write(api->context,"/local.txt");assert(file);std::string bytes(10000,'z');for(size_t at=0;at<bytes.size();){size_t n=std::min<size_t>(2048,bytes.size()-at);assert(api->file_write(api->context,file,bytes.data()+at,n)==n);at+=n;}assert(api->file_close(api->context,file,true));
  assert(remote.upload(api,"/local.txt","/from-x4.txt",now));pump();assert(remote.state()==RemoteVolume::State::Done);
  assert(remote.list("/",now));pump();assert(remote.state()==RemoteVolume::State::Done&&remote.count()==1&&!strcmp(remote.entry(0)->name,"from-x4.txt"));
  assert(remote.download("/from-x4.txt",api,"/Downloads/from-watch.txt",now));pump();assert(remote.state()==RemoteVolume::State::Done&&remote.publishedPath());
  uint64_t size=0;file=api->file_open_read(api->context,"/Downloads/from-watch.txt",&size);assert(file&&size==bytes.size());std::string got;char chunk[2048];for(;;){size_t n=api->file_read(api->context,file,chunk,sizeof(chunk));if(!n)break;got.append(chunk,n);}assert(got==bytes&&api->file_close(api->context,file,true));
  // Declared-file-type dispatch receives only the successfully published local path.
  assert(!strcmp(remote.publishedPath(),"/Downloads/from-watch.txt"));
  assert(remote.close());assert(local.end());RiscUserFiles::Volume reopened;assert(reopened.begin(&localStore.api,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;},malloc,free}));bool directory=false;assert(reopened.api()->stat(reopened.api()->context,"/Downloads/from-watch.txt",&size,&directory)&&size==bytes.size()&&!directory);assert(reopened.end());
 }

 // A malformed later row must not publish partial rows from a failed folder.
 {Fixture f;RemoteVolume remote(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
  assert(remote.list("/",f.now));
  std::string body="<D:multistatus xmlns:D=\"DAV:\"><D:response><D:href>/good.txt</D:href><D:propstat><D:prop><D:getcontentlength>4</D:getcontentlength></D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response><D:response><D:href>/bad.txt</D:href><D:propstat><D:prop><D:getcontentlength>bad</D:getcontentlength></D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response></D:multistatus>";
  f.inject("HTTP/1.1 207 Multi-Status\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
  for(unsigned i=0;i<10000&&remote.state()==RemoteVolume::State::Network;i++)remote.tick(f.now++);
  assert(remote.state()==RemoteVolume::State::Failed&&remote.count()==0&&!remote.entry(0));
 }
 // Only authenticated, successful OPTIONS advertisements grant operation flags.
 for(const auto&test:std::vector<std::pair<std::string,uint32_t>>{{"",0},{"Allow: OPTIONS, GET, HEAD, PROPFIND, PUT\r\n",15},{"aLLoW: GET, UNKNOWN, GETLY\r\nAllow: \tMKCOL , COPY\r\n",Client::Read|Client::Mkcol|Client::Copy},{"Allow: get, head, propfind\r\n",0}}){
  Fixture f;assert(!f.client.capabilities());assert(f.client.request("OPTIONS","/",nullptr,0,f.io(),f.now));f.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"+test.first+"\r\n");f.run();assert(f.client.ready()&&f.client.capabilities()==test.second);
 }
 {Fixture f;assert(f.client.request("OPTIONS","/",nullptr,0,f.io(),f.now));f.inject("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nAllow: GET/PUT\r\n\r\n");f.run();assert(f.client.state()==Client::State::Error&&f.client.error()==Client::Protocol&&!f.client.capabilities());}

 {Fixture f;f.client.end();f.run();uint8_t random[16]{};risc_bluetooth_session_descriptor_v1 bad{sizeof(bad),RISC_SESSION_HTTP,10000,"http://192.168.2.7:8080/","files","wrong-password","bad"};assert(f.client.begin(&f.link.tcp,bad,random,f.io(),f.now));f.run();unsigned before=f.store.calls;assert(f.client.request("GET","/",nullptr,0,f.io(),f.now));f.run();assert(f.client.state()==Client::State::Error&&f.store.calls==before);}
 {Fixture f;assert(f.client.request("GET","/",nullptr,0,f.io(),f.now));f.client.tick(f.now++);f.now=300001;f.run();assert(f.client.state()==Client::State::Error&&f.client.error()==Client::Expired&&!f.link.clientLive);}
 {Fixture f;f.upload=std::string(10000,'q');assert(f.client.request("PUT","/cancelled.txt",nullptr,f.upload.size(),f.io(),f.now));for(unsigned i=0;i<10000&&f.client.state()!=Client::State::UploadRead;i++){f.client.tick(f.now);f.server.tick(f.now++);}assert(f.client.state()==Client::State::UploadRead);f.client.cancel();f.run();for(unsigned i=0;i<20;i++)f.server.tick(f.now++);assert(f.client.uncertain());uint64_t n;bool dir;assert(!f.volume.api()->stat(f.volume.api()->context,"/cancelled.txt",&n,&dir));}
 {Fixture f;Store backing;RiscUserFiles::Volume local;assert(local.begin(&backing.api,{nullptr,[](void*){return true;},malloc,free}));auto api=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(local.api());static int observed=RISC_STORAGE_STATE_UNAVAILABLE;api.state.observe=[](void*){return observed;};auto base=&api.state.prepared.base.sleep.terminal.power.volume.base;RemoteVolume remote(f.client,{nullptr,[](void*){return true;}});assert(!remote.download("/missing",base,"/download.txt",f.now));assert(remote.state()==RemoteVolume::State::Failed&&backing.revision==0);observed=RISC_STORAGE_STATE_READY;assert(remote.download("/missing",base,"/download.txt",f.now));observed=RISC_STORAGE_STATE_UNAVAILABLE;for(unsigned i=0;i<20000;i++){auto state=remote.tick(f.now);f.server.tick(f.now++);if(state==RemoteVolume::State::Failed)break;}assert(remote.state()==RemoteVolume::State::Failed&&backing.revision==0);assert(remote.close());assert(local.end());}
 puts("Peer WebDAV: 8 existing Watch mappings, revision replacement/stale rejection, actual Digest/client/server/UserFiles volume, fragmented nonblocking transport, PUT/GET both directions, PROPFIND, MOVE/COPY/DELETE, exclusive creation and confinement PASS");
}

#endif
