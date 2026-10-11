#include "PortablePeerFiles.h"
#include "PortableNetworkSession.h"
#include "RiscEntropySourceV1.h"
struct PeerControllerFixture {
 static PeerControllerFixture*active;
 Fixture f;unsigned grants=0,bleState=RISC_SETUP_OFF,closePending=2;bool acquiredBle=false,token=false,cleanupOnly=false;
 wifi_async_v1 wifi{};risc_key_value_v1 kv{1,sizeof(kv),nullptr,nullptr,nullptr};
 bool privateNetwork=false,networkLive=false,networkCancelled=false;unsigned joins=0;
 risc_runtime_api_v1 runtime{};risc_bluetooth_session_setup_v1 ble{};
 risc_entropy_source_v1 entropy{1,sizeof(entropy),this,[](void*,void*out,uint32_t n)->int32_t{assert(n==16);memset(out,55,n);return 0;}};
 portable_peer_files*peer=nullptr;portable_peer_status status{};
 PeerControllerFixture(){active=this;f.client.end();f.run();runtime.api_version=1;runtime.struct_size=sizeof(runtime);
 runtime.acquire=[](const char*c,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){auto&p=*active;assert(v==1);if(!strcmp(c,"net.wifi")){assert(i==15);g->api=&p.wifi;}else if(!strcmp(c,"storage.key-value")){assert(i==6);g->api=&p.kv;}else if(!strcmp(c,RISC_TCP_CONNECTION_CAPABILITY))g->api=&p.f.link.tcp;else if(!strcmp(c,RISC_ENTROPY_SOURCE_CAPABILITY))g->api=&p.entropy;else if(!strcmp(c,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY)){g->api=&p.ble;p.acquiredBle=true;}else return false;p.grants++;return true;};
 runtime.release=[](risc_runtime_capability_v1*g){auto&p=*active;assert(p.grants);if(g->api==&p.ble){assert(!p.token);p.acquiredBle=false;}if(g->api==&p.f.link.tcp)assert(!p.f.link.clientLive);p.grants--;return true;};
 wifi.base={};wifi.base.api_version=1;wifi.base.struct_size=sizeof(wifi);wifi.base.context=this;
 wifi.base.status=[](void*){return WIFI_LINK_UP;};wifi.base.rssi=[](void*)->int8_t{return -40;};
 wifi.base.addresses=[](void*,wifi_ipv4_v1*s,wifi_ipv4_v1*a){*s={{192,168,2,8},{192,168,2,1},{255,255,255,0}};*a={};return true;};
 wifi.async_tag=RISC_RADIO_ASYNC_TAG;wifi.async_version=1;
 wifi.begin=[](void*c,const risc_radio_request_v1*r,uint32_t*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.privateNetwork&&!p.token&&r->kind==RISC_RADIO_REQUEST_JOIN&&r->flags==RISC_RADIO_TEMPORARY&&!strcmp(r->ssid,"Files-fixture"));p.networkLive=true;p.networkCancelled=false;p.joins++;*out=51;return 0;};
 wifi.poll=[](void*c,uint32_t id,risc_radio_progress_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(id==51);*out={};out->struct_size=sizeof(*out);out->operation_id=id;out->scan.struct_size=sizeof(out->scan);out->phase=p.networkCancelled?RISC_RADIO_IDLE:RISC_RADIO_CONNECTED;out->quiescent=p.networkCancelled;out->owned=!p.networkCancelled;out->station_state=p.networkCancelled?0:2;out->station[0]=192;out->station[1]=168;out->station[2]=4;out->station[3]=2;return p.networkCancelled?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;};
 wifi.cancel=[](void*c,uint32_t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.f.link.clientLive);p.networkLive=false;p.networkCancelled=true;return RISC_RADIO_PENDING;};
 wifi.service_begin=[](void*,uint32_t*t)->int32_t{*t=9;return 0;};wifi.service_end=[](void*,uint32_t)->int32_t{return 0;};
 ble.network_tag=RISC_SESSION_NETWORK_TAG;
 ble.peer_network=[](void*c,uint64_t,risc_bluetooth_network_offer_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);if(!p.privateNetwork)return RISC_SETUP_INVALID;out->struct_size=sizeof(*out);p.ble.peer_descriptor(c,7,&out->endpoint);strcpy(out->ssid,"Files-fixture");strcpy(out->password,"private-password-fixture");memcpy(out->address,"\xc0\xa8\x04\x01",4);return 0;};
 ble.api_version=1;ble.struct_size=sizeof(ble);ble.context=this;ble.peer_tag=RISC_SESSION_PEER_TAG;
 ble.discover=[](void*c,uint32_t n,uint64_t*t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(n==300000&&p.acquiredBle);p.token=true;*t=7;p.bleState=RISC_SETUP_DISCOVERING;return p.cleanupOnly?RISC_SETUP_CLEANUP_PENDING:0;};
 ble.poll=[](void*c,uint64_t t,uint32_t n)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.cleanupOnly&&p.token&&t==7&&n<=2);return 0;};
 ble.status=[](void*c,uint64_t t,risc_bluetooth_session_setup_status_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(!p.cleanupOnly&&p.token&&t==7);out->state=p.bleState;out->pairing_generation=9;out->pairing_number=123456;return 0;};
 ble.peer=[](void*c,uint64_t t,uint32_t i,risc_bluetooth_session_peer_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7);if(i)return -1;out->peer_id=123;strcpy(out->name,"Watch Files");return 0;};
 ble.select_peer=[](void*c,uint64_t t,uint32_t id)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&id==123);p.bleState=RISC_SETUP_PAIR_CONFIRM;return 0;};
 ble.confirm_pairing=[](void*c,uint64_t t,uint32_t gen,uint32_t number,bool accept)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&gen==9&&number==123456);p.bleState=accept?RISC_SETUP_READY:RISC_SETUP_ENDED;return 0;};
 ble.peer_descriptor=[](void*c,uint64_t t,risc_bluetooth_session_descriptor_v1*out)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7&&p.bleState==RISC_SETUP_READY);*out={sizeof(*out),RISC_SESSION_HTTP,250000,"http://192.168.2.7:8080/","files","test-password","fixture"};if(p.privateNetwork)strcpy(out->url,"http://192.168.4.1:8080/");if(const char*path=getenv("PEER_BLE_DESCRIPTOR")){FILE*in=fopen(path,"rb");assert(in&&fread(out,1,sizeof(*out),in)==sizeof(*out));fclose(in);}return 0;};
 ble.close=[](void*c,uint64_t t)->int32_t{auto&p=*static_cast<PeerControllerFixture*>(c);assert(p.token&&t==7);if(p.closePending){p.closePending--;return RISC_SETUP_CLEANUP_PENDING;}p.token=false;p.bleState=RISC_SETUP_OFF;return 0;};
 portable_peer_hooks hooks{this,[](void*c){return static_cast<PeerControllerFixture*>(c)->f.link.owner;},malloc,free};peer=portable_peer_create(&runtime,&hooks);assert(peer&&portable_peer_start(peer,f.now));
 }
 void tick(){portable_peer_tick(peer,f.now);f.server.tick(f.now++);portable_peer_status_get(peer,&status);}
 void until(unsigned expected){for(unsigned i=0;i<30000;i++){tick();if(status.state==expected)return;assert(status.state!=PEER_RETAINED);}assert(!"controller timeout");}
 void connect(){until(PEER_DISCOVERING);tick();portable_peer_row row;assert(portable_peer_row_get(peer,0,&row)&&row.peer_id==123);assert(portable_peer_select(peer,row.peer_id));until(PEER_PAIRING);assert(!portable_peer_confirm(peer,8,123456,true));assert(portable_peer_confirm(peer,9,123456,true));until(PEER_READY);assert(!token&&!acquiredBle&&grants==(privateNetwork?2u:3u));}
 void stop(){for(unsigned i=0;i<30000;i++){portable_peer_stop(peer);tick();if(status.state==PEER_OFF)break;}assert(status.state==PEER_OFF&&!token&&!grants);assert(portable_peer_destroy(peer));peer=nullptr;}
 ~PeerControllerFixture(){if(peer)stop();active=nullptr;}
};
PeerControllerFixture*PeerControllerFixture::active=nullptr;
static void controllerTests(){
 // Every pre-authentication stop point must finish despite repeated UI stop requests.
 for(unsigned at=0;at<14;at++){PeerControllerFixture p;for(unsigned i=0;i<at;i++)p.tick();p.stop();}
 {PeerControllerFixture p;p.cleanupOnly=true;p.until(PEER_OFF);assert(!p.token&&!p.grants);}
 {PeerControllerFixture p;p.connect();assert(portable_peer_mutate(p.peer,"MKCOL","/Peer",nullptr));p.tick();p.until(PEER_READY);assert(!p.status.uncertain);assert(portable_peer_list(p.peer,"/"));p.tick();p.until(PEER_READY);assert(p.status.count==1);portable_peer_row row;assert(portable_peer_row_get(p.peer,0,&row)&&row.directory&&!strcmp(row.path,"/Peer"));p.stop();}
 {PeerControllerFixture p;p.privateNetwork=true;p.f.link.refuseConnect=3;p.connect();assert(p.joins==1&&p.networkLive&&!p.f.link.refuseConnect);p.stop();assert(!p.networkLive);}
 {PeerControllerFixture p;p.privateNetwork=true;p.f.link.refuseConnect=1000;p.until(PEER_DISCOVERING);p.tick();assert(portable_peer_select(p.peer,123));p.until(PEER_PAIRING);assert(portable_peer_confirm(p.peer,9,123456,true));p.until(PEER_OFF);assert(!p.networkLive&&!p.grants);}
 {PeerControllerFixture p;p.privateNetwork=true;p.connect();portable_peer_stop(p.peer);p.until(PEER_OFF);assert(!p.networkLive&&!p.grants);assert(portable_peer_start(p.peer,p.f.now));p.connect();assert(p.joins==2);p.stop();}
 puts("Portable peer controller: instance-zero authority, explicit pairing, BLE release before HTTP, cooperative cleanup retry, stop at every setup phase and actual authenticated server operations PASS");
}
