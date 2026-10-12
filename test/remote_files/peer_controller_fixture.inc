#include "PortablePeerFiles.h"
#include "PortableNetworkSession.h"
#include "RiscEntropySourceV1.h"
struct PeerControllerFixture {
 static PeerControllerFixture*active;
 Fixture f;unsigned grants=0,bleState=RISC_SETUP_OFF,closePending=2;bool acquiredBle=false,token=false,cleanupOnly=false;
 wifi_async_v1 wifi{};risc_key_value_v1 kv{1,sizeof(kv),nullptr,nullptr,nullptr};
 bool privateNetwork=false,networkLive=false,networkCancelled=false,noPeers=false,entropyBusy=false;unsigned joins=0,entropyCalls=0,serviceBeginBusy=0,serviceEndBusy=0;uint64_t discoveryDeadline=0;
 risc_runtime_api_v1 runtime{};risc_bluetooth_session_setup_v1 ble{};
 risc_entropy_source_v1 entropy{1,sizeof(entropy),this,[](void*c,void*out,uint32_t n)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(n==16&&p.token&&p.acquiredBle);++p.entropyCalls;if(p.entropyBusy)return RISC_ENTROPY_SOURCE_BUSY;memset(out,55+p.entropyCalls,n);return 0;}};
 portable_peer_files*peer=nullptr;portable_peer_status status{};
 PeerControllerFixture(const risc_storage_volume_api_v1*external=nullptr):f(external){active=this;f.client.end();f.run();runtime.api_version=1;runtime.struct_size=sizeof(runtime);
 runtime.acquire=[](const char*c,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){auto&p=*active;assert(v==1);if(!strcmp(c,"net.wifi")){assert(i==15);g->api=&p.wifi;}else if(!strcmp(c,"storage.key-value")){assert(i==6);g->api=&p.kv;}else if(!strcmp(c,RISC_TCP_CONNECTION_CAPABILITY))g->api=&p.f.link.tcp;else if(!strcmp(c,RISC_ENTROPY_SOURCE_CAPABILITY))g->api=&p.entropy;else if(!strcmp(c,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY)){g->api=&p.ble;p.acquiredBle=true;}else return false;p.grants++;return true;};
 runtime.release=[](risc_runtime_capability_v1*g){auto&p=*active;assert(p.grants);if(g->api==&p.ble){assert(!p.token);p.acquiredBle=false;}if(g->api==&p.f.link.tcp)assert(!p.f.link.clientLive);p.grants--;return true;};
 wifi.base={};wifi.base.api_version=1;wifi.base.struct_size=sizeof(wifi);wifi.base.context=this;
 wifi.base.status=[](void*){return WIFI_LINK_UP;};wifi.base.rssi=[](void*)->int8_t{return -40;};
 wifi.base.addresses=[](void*,wifi_ipv4_v1*s,wifi_ipv4_v1*a){*s={{192,168,2,8},{192,168,2,1},{255,255,255,0}};*a={};return true;};
 wifi.async_tag=RISC_RADIO_ASYNC_TAG;wifi.async_version=1;
 wifi.begin=[](void*c,const risc_radio_request_v1*r,uint32_t*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.privateNetwork&&!p.token&&r->kind==RISC_RADIO_REQUEST_JOIN&&r->flags==RISC_RADIO_TEMPORARY&&!strcmp(r->ssid,"Files-fixture"));p.networkLive=true;p.networkCancelled=false;p.joins++;*out=51;return 0;};
 wifi.poll=[](void*c,uint32_t id,risc_radio_progress_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(id==51);*out={};out->struct_size=sizeof(*out);out->operation_id=id;out->scan.struct_size=sizeof(out->scan);out->phase=p.networkCancelled?RISC_RADIO_IDLE:RISC_RADIO_CONNECTED;out->quiescent=p.networkCancelled;out->owned=!p.networkCancelled;out->station_state=p.networkCancelled?0:2;out->station[0]=192;out->station[1]=168;out->station[2]=4;out->station[3]=2;return p.networkCancelled?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;};
 wifi.cancel=[](void*c,uint32_t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.f.link.clientLive);p.networkLive=false;p.networkCancelled=true;return RISC_RADIO_PENDING;};
 wifi.service_begin=[](void*c,uint32_t*t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);*t=0;if(p.serviceBeginBusy){--p.serviceBeginBusy;return RISC_RADIO_AGAIN;}*t=9;return 0;};wifi.service_end=[](void*c,uint32_t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);if(p.serviceEndBusy){--p.serviceEndBusy;return RISC_RADIO_AGAIN;}return 0;};
 ble.network_tag=RISC_SESSION_NETWORK_TAG;
 ble.peer_network=[](void*c,uint64_t,risc_bluetooth_network_offer_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);if(!p.privateNetwork)return RISC_SETUP_INVALID;out->struct_size=sizeof(*out);p.ble.peer_descriptor(c,7,&out->endpoint);strcpy(out->ssid,"Files-fixture");strcpy(out->password,"private-password-fixture");memcpy(out->address,"\xc0\xa8\x04\x01",4);return 0;};
 ble.api_version=1;ble.struct_size=sizeof(ble);ble.context=this;ble.peer_tag=RISC_SESSION_PEER_TAG;
 ble.discover=[](void*c,uint32_t n,uint64_t*t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(n==300000&&p.acquiredBle);p.token=true;p.discoveryDeadline=p.f.now+n;*t=7;p.bleState=RISC_SETUP_DISCOVERING;return p.cleanupOnly?RISC_SETUP_CLEANUP_PENDING:0;};
 ble.poll=[](void*c,uint64_t t,uint32_t n)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.cleanupOnly&&p.token&&t==7&&n<=2);return p.f.now>=p.discoveryDeadline?RISC_SETUP_EXPIRED:0;};
 ble.status=[](void*c,uint64_t t,risc_bluetooth_session_setup_status_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.cleanupOnly&&p.token&&t==7);out->state=p.bleState;out->pairing_generation=9;out->pairing_number=123456;return 0;};
 ble.peer=[](void*c,uint64_t t,uint32_t i,risc_bluetooth_session_peer_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7);if(i||p.noPeers)return -1;out->peer_id=123;strcpy(out->name,"Watch Files");return 0;};
 ble.select_peer=[](void*c,uint64_t t,uint32_t id)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&id==123);p.bleState=RISC_SETUP_PAIR_CONFIRM;return 0;};
 ble.confirm_pairing=[](void*c,uint64_t t,uint32_t gen,uint32_t number,bool accept)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&gen==9&&number==123456);p.bleState=accept?RISC_SETUP_READY:RISC_SETUP_ENDED;return 0;};
 ble.peer_descriptor=[](void*c,uint64_t t,risc_bluetooth_session_descriptor_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&p.bleState==RISC_SETUP_READY);*out={sizeof(*out),RISC_SESSION_HTTP,250000,"http://192.168.2.7:8080/","files","test-password","fixture"};if(p.privateNetwork)strcpy(out->url,"http://192.168.4.1:8080/");if(const char*path=getenv("PEER_BLE_DESCRIPTOR")){FILE*in=fopen(path,"rb");assert(in&&fread(out,1,sizeof(*out),in)==sizeof(*out));fclose(in);}return 0;};
 ble.close=[](void*c,uint64_t t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7);if(p.closePending){p.closePending--;return RISC_SETUP_CLEANUP_PENDING;}p.token=false;p.bleState=RISC_SETUP_OFF;return 0;};
 portable_peer_hooks hooks{this,[](void*c){return static_cast<PeerControllerFixture*>(c)->f.link.owner;},malloc,free};peer=portable_peer_create(&runtime,&hooks);assert(peer&&portable_peer_start(peer,f.now));
 }
 void tick(){unsigned before=f.link.calls;portable_peer_tick(peer,f.now);assert(f.link.calls-before<=1);f.server.tick(f.now++);portable_peer_status_get(peer,&status);}
 void until(unsigned expected){for(unsigned i=0;i<30000;i++){tick();if(status.state==expected)return;assert(status.state!=PEER_RETAINED);}fprintf(stderr,"controller timeout expected=%u actual=%u now=%llu message=%s\n",expected,status.state,(unsigned long long)f.now,status.message);assert(!"controller timeout");}
 void connect(){until(PEER_DISCOVERING);tick();portable_peer_row row;assert(portable_peer_row_get(peer,0,&row)&&row.peer_id==123);assert(portable_peer_select(peer,row.peer_id));until(PEER_PAIRING);assert(!portable_peer_confirm(peer,8,123456,true));assert(portable_peer_confirm(peer,9,123456,true));until(PEER_READY);assert(status.operation_ok);assert(!token&&!acquiredBle&&grants==(privateNetwork?2u:3u));}
 void stop(){for(unsigned i=0;i<30000;i++){portable_peer_stop(peer);tick();if(status.state==PEER_OFF)break;}assert(status.state==PEER_OFF&&!token&&!grants);assert(portable_peer_destroy(peer));peer=nullptr;}
 ~PeerControllerFixture(){if(peer)stop();active=nullptr;}
};
PeerControllerFixture*PeerControllerFixture::active=nullptr;
[[maybe_unused]] static void controllerTests(){
 {PeerControllerFixture p;p.noPeers=true;p.until(PEER_DISCOVERING);
  assert(p.entropyCalls==1&&p.token&&p.acquiredBle&&!p.joins);
  for(unsigned i=0;i<100;++i)p.tick();
  assert(p.status.state==PEER_DISCOVERING&&!p.status.count&&!p.status.message[0]);
  p.f.now+=300001;p.until(PEER_OFF);assert(strstr(p.status.message,"expired")&&!p.grants);p.stop();}
 {PeerControllerFixture p;p.entropyBusy=true;p.until(PEER_OFF);
  assert(p.entropyCalls&&strstr(p.status.message,"startup timed out")&&!p.grants&&!p.token);p.stop();}

 // Every pre-authentication stop point must finish despite repeated UI stop requests.
 for(unsigned at=0;at<14;at++){PeerControllerFixture p;for(unsigned i=0;i<at;i++)p.tick();p.stop();}
 {PeerControllerFixture p;p.cleanupOnly=true;p.until(PEER_OFF);assert(!p.token&&!p.grants);}
 {PeerControllerFixture p;p.connect();assert(portable_peer_mutate(p.peer,"MKCOL","/Peer",nullptr));p.tick();p.until(PEER_READY);assert(!p.status.uncertain);assert(portable_peer_list(p.peer,"/"));p.tick();p.until(PEER_READY);assert(p.status.count==1);portable_peer_row row;assert(portable_peer_row_get(p.peer,0,&row)&&row.directory&&!strcmp(row.path,"/Peer"));p.stop();}
 {PeerControllerFixture p;p.privateNetwork=true;p.f.link.refuseConnect=3;p.connect();assert(p.joins==1&&p.networkLive&&!p.f.link.refuseConnect);p.stop();assert(!p.networkLive);}
 {PeerControllerFixture p;p.privateNetwork=true;p.f.link.refuseConnect=1000;p.until(PEER_DISCOVERING);p.tick();assert(portable_peer_select(p.peer,123));p.until(PEER_PAIRING);assert(portable_peer_confirm(p.peer,9,123456,true));p.until(PEER_OFF);assert(!p.networkLive&&!p.grants);}
 {PeerControllerFixture p;p.privateNetwork=true;p.connect();portable_peer_stop(p.peer);p.until(PEER_OFF);assert(!p.networkLive&&!p.grants);assert(portable_peer_start(p.peer,p.f.now));p.connect();assert(p.joins==2);p.stop();}
 // A command stays pending through a busy begin or a previous lease's delayed end.
 {PeerControllerFixture p;p.connect();for(bool delayedEnd:{false,true}){
  if(delayedEnd){p.serviceEndBusy=4;p.tick();}else p.serviceBeginBusy=4;
  const uint32_t previous=p.status.completed_generation;unsigned serial=p.f.link.serial;
  assert(portable_peer_list(p.peer,"/"));portable_peer_status_get(p.peer,&p.status);uint32_t command=p.status.operation_generation;assert(command==previous+1&&p.status.completed_generation==previous&&!p.status.operation_ok);
  for(unsigned i=0;i<3;++i){p.tick();assert(p.status.state==PEER_READY&&p.status.operation_generation==command&&p.status.completed_generation==previous&&!p.status.operation_ok&&p.f.link.serial==serial);}
  for(unsigned i=0;i<30000&&p.status.completed_generation!=command;++i)p.tick();assert(p.status.operation_ok&&p.status.completed_generation==command);
 }
 p.serviceEndBusy=4;p.tick();assert(portable_peer_preview(p.peer,"/missing",32));portable_peer_status_get(p.peer,&p.status);uint32_t command=p.status.operation_generation;portable_peer_cancel_operation(p.peer);
 for(unsigned i=0;i<30000&&p.status.completed_generation!=command;++i)p.tick();assert(!p.status.operation_ok&&strstr(p.status.message,"cancelled")&&p.status.completed_generation==command);
 assert(!portable_peer_list(p.peer,"/bad//path")&&!portable_peer_mutate(p.peer,"MOVE","/x",nullptr));portable_peer_status_get(p.peer,&p.status);assert(p.status.operation_generation==command);
 risc_storage_volume_api_v1 unsupported{};assert(portable_peer_download(p.peer,"/missing",&unsupported,"/copy"));portable_peer_status_get(p.peer,&p.status);command=p.status.operation_generation;p.tick();assert(p.status.completed_generation==command&&!p.status.operation_ok&&strstr(p.status.message,"Invalid local download"));
 assert(portable_peer_list(p.peer,"/missing"));p.tick();p.until(PEER_READY);assert(!p.status.operation_ok&&!p.status.count&&p.status.completed_generation==p.status.operation_generation);
 assert(portable_peer_list(p.peer,"/"));p.tick();p.until(PEER_READY);assert(p.status.operation_ok);uint32_t generation=p.status.operation_generation;portable_peer_stop(p.peer);p.until(PEER_OFF);assert(portable_peer_start(p.peer,p.f.now));p.connect();assert(p.status.operation_generation==generation&&p.status.completed_generation==generation);p.stop();}
 // Previews retain their bounded bytes, never caller buffers, and keep the mounted directory.
 {PeerControllerFixture p;auto api=p.f.volume.api();auto put=[&](const char*path,const std::string&bytes){auto file=api->file_open_write(api->context,path);assert(file);for(size_t at=0;at<bytes.size();){size_t n=std::min<size_t>(2048,bytes.size()-at);assert(api->file_write(api->context,file,bytes.data()+at,n)==n);at+=n;}assert(api->file_close(api->context,file,true));};
  std::string bytes(10000,'x');bytes[10]=0;bytes.replace(30,4,"test");put("/large.txt",bytes);put("/empty.txt","");p.connect();assert(p.status.capabilities==255u&&p.status.count==2&&!portable_peer_network_owned(p.peer));
  unsigned before=p.f.link.calls;char copied[PORTABLE_PEER_PREVIEW_MAX]{};size_t copiedSize=99;
  assert(!portable_peer_preview(p.peer,"/large.txt",0));assert(!portable_peer_preview(p.peer,"/large.txt",PORTABLE_PEER_PREVIEW_MAX+1));
  assert(portable_peer_preview(p.peer,"/large.txt",sizeof(copied)));assert(!portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize)&&copiedSize==0&&p.f.link.calls==before);
  p.tick();assert(p.status.state==PEER_WORKING);p.until(PEER_READY);assert(p.status.operation_ok&&p.status.preview_size==sizeof(copied)&&p.status.preview_total==bytes.size()&&p.status.preview_truncated&&p.status.count==2&&!strcmp(p.status.path,"/"));
  assert(p.f.link.clientBytes<4096&&!p.f.link.clientLive&&!portable_peer_network_owned(p.peer));
  assert(!portable_peer_preview_get(p.peer,copied,sizeof(copied)-1,&copiedSize));assert(portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize)&&copiedSize==sizeof(copied)&&!memcmp(copied,bytes.data(),copiedSize));
  // Let the server observe the early close before the next independent request.
  for(unsigned i=0;i<20&&p.f.link.serverLive;i++)p.tick();assert(!p.f.link.serverLive);
  assert(!portable_peer_preview_range(p.peer,"/large.txt",64u*1024u*1024u+1u,100));
  assert(portable_peer_preview_range(p.peer,"/large.txt",2048,512));p.tick();p.until(PEER_READY);assert(p.status.operation_ok&&p.status.preview_offset==2048&&p.status.preview_size==512&&p.status.preview_total==bytes.size()&&p.status.preview_truncated);assert(portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize)&&copiedSize==512&&!memcmp(copied,bytes.data()+2048,512));
  for(unsigned i=0;i<20&&p.f.link.serverLive;i++)p.tick();
  assert(portable_peer_preview_range(p.peer,"/large.txt",bytes.size()-20,32));p.tick();p.until(PEER_READY);assert(p.status.operation_ok&&p.status.preview_size==20&&!p.status.preview_truncated);assert(portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize)&&copiedSize==20&&!memcmp(copied,bytes.data()+bytes.size()-20,20));
  assert(portable_peer_preview_range(p.peer,"/large.txt",bytes.size()+10,32));p.tick();p.until(PEER_READY);assert(p.status.operation_ok&&!p.status.preview_size&&!p.status.preview_truncated);
  assert(portable_peer_preview(p.peer,"/empty.txt",32));p.tick();p.until(PEER_READY);assert(p.status.operation_ok&&!p.status.preview_size&&!p.status.preview_total&&!p.status.preview_truncated);assert(portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize)&&!copiedSize);
  before=p.f.link.serial;assert(portable_peer_preview(p.peer,"/large.txt",100));portable_peer_cancel_operation(p.peer);p.tick();assert(p.status.state==PEER_READY&&!p.status.operation_ok&&p.f.link.serial==before);
  assert(portable_peer_preview(p.peer,"/large.txt",1000));p.tick();for(unsigned i=0;i<10000&&!p.status.progress;i++)p.tick();assert(p.status.progress&&p.status.state==PEER_WORKING);portable_peer_cancel_operation(p.peer);p.until(PEER_READY);assert(!p.status.operation_ok&&!p.status.preview_size&&!portable_peer_preview_get(p.peer,copied,sizeof(copied),&copiedSize));
  for(unsigned i=0;i<20&&p.f.link.serverLive;i++)p.tick();assert(portable_peer_list(p.peer,"/"));p.tick();p.until(PEER_READY);assert(p.status.operation_ok&&p.status.count==2);p.stop();
 }
 {PeerControllerFixture p;p.privateNetwork=true;p.connect();assert(portable_peer_network_owned(p.peer));assert(portable_peer_preview(p.peer,"/missing.txt",32));p.tick();p.until(PEER_READY);assert(!p.status.operation_ok&&!p.status.preview_size&&portable_peer_network_owned(p.peer));p.stop();}
 puts("Portable peer controller: instance-zero authority, explicit pairing, BLE release before HTTP, cooperative cleanup retry, stop at every setup phase and actual authenticated server operations PASS");
}
