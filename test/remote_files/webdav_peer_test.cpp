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
 bool owner=true,clientLive=false,serverLive=false,accepted=false,drop=false,closeFault=false;uint64_t serial=10;std::string up,down;size_t upAt=0,downAt=0;unsigned calls=0;
 risc_tcp_connection_v1 tcp{1,sizeof(tcp),this,
 [](void*,const risc_tcp_connection_listen_v1*,uint64_t*out)->int32_t{*out=1;return 0;},
 [](void*c,uint64_t,uint64_t*out)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*out=0;if(!s.clientLive||s.accepted)return 1;s.accepted=true;s.serverLive=true;*out=s.serial+1000;return 0;},
 [](void*c,uint64_t h,void*out,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*n=0;if(s.drop)return -4;bool server=h>1000;auto&b=server?s.up:s.down;auto&at=server?s.upAt:s.downAt;*n=std::min<size_t>({cap,31,b.size()-at});if(!*n)return server?(!s.clientLive?2:1):(!s.serverLive&&s.accepted?2:1);memcpy(out,b.data()+at,*n);at+=*n;return 0;},
 [](void*c,uint64_t h,const void*in,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;*n=0;if(s.drop)return -4;*n=std::min(cap,41u);(h>1000?s.down:s.up).append(static_cast<const char*>(in),*n);return 0;},
 [](void*c,uint64_t h)->int32_t{auto&s=*static_cast<Link*>(c);s.calls++;if(s.closeFault)return -6;if(h>1000)s.serverLive=false;else if(h>1)s.clientLive=false;return 0;},RISC_TCP_CONNECTION_CONNECT_TAG,
 [](void*c,const risc_tcp_connection_listen_v1*r,uint64_t*out)->int32_t{auto&s=*static_cast<Link*>(c);assert(r->port==8080&&r->address[0]==192);assert(!s.clientLive&&!s.serverLive);s.calls++;s.up.clear();s.down.clear();s.upAt=s.downAt=0;s.accepted=false;s.clientLive=true;*out=++s.serial;return 0;},
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
 unsigned request(const char*m,const char*p,const char*d=nullptr,const std::string&body="",const char*match=nullptr){upload=body;at=0;download.clear();assert(client.request(m,p,d,body.size(),io(),now,match));run();assert(client.ready());return client.status();}
 ~Fixture(){if(!link.owner||client.state()==Client::State::Retained)return;client.end();run();server.requestStop();for(unsigned i=0;i<20&&server.status().state!=SharingState::Off;i++)server.tick(now++);assert(volume.end());}
};
#include "peer_controller_fixture.inc"
struct ExistingFiles {
 struct Record{std::string bytes;uint64_t revision;};
 std::map<std::pair<uint32_t,std::string>,Record> records;unsigned calls=0;
 RiscBoot::AppDataBackend backend{this,
 [](void*c,uint32_t ns,const char*n,uint32_t*z,uint64_t*r)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);assert(ns==1||ns==2||ns==3||ns==62||ns==64);s.calls++;auto i=s.records.find({ns,n});*z=0;*r=0;if(i==s.records.end())return -1;*z=i->second.bytes.size();*r=i->second.revision;return 0;},
 [](void*c,uint32_t ns,const char*n,uint64_t rev,void*b,uint32_t cap,uint32_t*z,uint64_t*r)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);auto i=s.records.find({ns,n});if(i==s.records.end())return -1;if(rev!=i->second.revision)return -10;if(cap<i->second.bytes.size())return -2;memcpy(b,i->second.bytes.data(),i->second.bytes.size());*z=i->second.bytes.size();*r=i->second.revision;return 0;},
 [](void*c,uint32_t ns,const char*n,uint64_t rev,const void*b,uint32_t z)->int32_t{auto&s=*static_cast<ExistingFiles*>(c);auto i=s.records.find({ns,n});if((i==s.records.end()?0:i->second.revision)!=rev)return -10;s.records[{ns,n}]={std::string(static_cast<const char*>(b),z),rev+1};return 0;},[](void*){return true;}};
};
#ifndef PEER_UI_BACKEND
int main(){
 controllerTests();
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
   assert(f.request("PROPFIND","/")==207&&f.download.find("models")!=std::string::npos&&f.download.find("lists")!=std::string::npos);
   for(const auto&r:rows){assert(f.request("GET",r.path)==200&&f.download==std::string("existing ")+r.name);assert(f.request("HEAD",r.path)==200);std::string revision=f.client.etag();assert(!revision.empty());assert(f.request("PUT",r.path,nullptr,"replacement",revision.c_str())==204);assert(files.records.at({r.nameSpace,r.name}).bytes=="replacement");assert(f.request("PUT",r.path,nullptr,"stale",revision.c_str())==412);assert(f.request("DELETE",r.path)==405);}
   unsigned before=files.calls;assert(f.request("GET","/credentials/wifi")>=400&&files.calls==before);
  }assert(exported.end());
 }

 {Fixture f;assert(f.request("MKCOL","/Documents")==201);std::string text(12000,'x');text.replace(40,10,"watch data");assert(f.request("PUT","/Documents/read me.txt",nullptr,text)==201);assert(f.request("GET","/Documents/read me.txt")==200&&f.download==text);assert(f.request("PROPFIND","/Documents")==207&&f.download.find("read%20me.txt")!=std::string::npos);assert(f.request("PUT","/Documents/read me.txt",nullptr,"overwrite")==412);assert(f.request("MOVE","/Documents/read me.txt","/Documents/renamed.txt")==201);assert(f.request("COPY","/Documents/renamed.txt","/copy.txt")==201);assert(f.request("GET","/copy.txt")==200&&f.download==text);assert(f.request("DELETE","/copy.txt")==204);assert(f.request("GET","/copy.txt")==404);}
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

 {Fixture f;f.client.end();f.run();uint8_t random[16]{};risc_bluetooth_session_descriptor_v1 bad{sizeof(bad),RISC_SESSION_HTTP,10000,"http://192.168.2.7:8080/","files","wrong-password","bad"};assert(f.client.begin(&f.link.tcp,bad,random,f.io(),f.now));f.run();unsigned before=f.store.calls;assert(f.client.request("GET","/",nullptr,0,f.io(),f.now));f.run();assert(f.client.state()==Client::State::Error&&f.store.calls==before);}
 {Fixture f;assert(f.client.request("GET","/",nullptr,0,f.io(),f.now));f.client.tick(f.now++);f.now=300001;f.run();assert(f.client.state()==Client::State::Error&&f.client.error()==Client::Expired&&!f.link.clientLive);}
 {Fixture f;f.upload=std::string(10000,'q');assert(f.client.request("PUT","/cancelled.txt",nullptr,f.upload.size(),f.io(),f.now));for(unsigned i=0;i<10000&&f.client.state()!=Client::State::UploadRead;i++){f.client.tick(f.now);f.server.tick(f.now++);}assert(f.client.state()==Client::State::UploadRead);f.client.cancel();f.run();for(unsigned i=0;i<20;i++)f.server.tick(f.now++);assert(f.client.uncertain());uint64_t n;bool dir;assert(!f.volume.api()->stat(f.volume.api()->context,"/cancelled.txt",&n,&dir));}
 {Fixture f;Store backing;RiscUserFiles::Volume local;assert(local.begin(&backing.api,{nullptr,[](void*){return true;},malloc,free}));auto api=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(local.api());static int observed=RISC_STORAGE_STATE_UNAVAILABLE;api.state.observe=[](void*){return observed;};auto base=&api.state.prepared.base.sleep.terminal.power.volume.base;RemoteVolume remote(f.client,{nullptr,[](void*){return true;}});assert(!remote.download("/missing",base,"/download.txt",f.now));assert(remote.state()==RemoteVolume::State::Failed&&backing.revision==0);observed=RISC_STORAGE_STATE_READY;assert(remote.download("/missing",base,"/download.txt",f.now));observed=RISC_STORAGE_STATE_UNAVAILABLE;for(unsigned i=0;i<20000;i++){auto state=remote.tick(f.now);f.server.tick(f.now++);if(state==RemoteVolume::State::Failed)break;}assert(remote.state()==RemoteVolume::State::Failed&&backing.revision==0);assert(remote.close());assert(local.end());}
 puts("Peer WebDAV: 8 existing Watch mappings, revision replacement/stale rejection, actual Digest/client/server/UserFiles volume, fragmented nonblocking transport, PUT/GET both directions, PROPFIND, MOVE/COPY/DELETE, exclusive creation and confinement PASS");
}

#endif
