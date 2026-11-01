#define main original_remote_battery_main
#include "remote_battery_test.c"
#undef main
static unsigned mode,kill_endpoint;
static bool owner_live;
static risc_bluetooth_sensor_link_api_v1 extended;
static bool guarded_alive(void *unused){(void)unused;return owner_live;}
static bool observation(void*c,uint64_t token,const uint8_t addr[6],uint8_t type,risc_ble_sensor_link_state_v1*out){
 assert(c==&f&&token==f.open_token&&owner_live);
 if(kill_endpoint==8){owner_live=false;return false;}
 if(mode==9)return false;
 *out=(risc_ble_sensor_link_state_v1){.struct_size=sizeof(*out),.state=mode==0?RISC_BLE_LINK_CONNECTED:mode==1?RISC_BLE_LINK_NOT_CONNECTED:RISC_BLE_LINK_UNKNOWN,.address_type=type};
 memcpy(out->address,addr,6);
 if(out->state){out->generation=token;out->observed_ms=100;}
 if(mode==3)out->state=7;
 if(mode==4)out->generation=token+1;
 if(mode==5)out->address[0]^=1;
 if(mode==6)out->reserved=1;
 if(mode==7)out->struct_size--;
 if(mode==8)out->observed_ms=UINT64_MAX;
 return true;
}
static bool guarded_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){bool ok=acquire(n,v,i,g);if(kill_endpoint==1)owner_live=false;return ok;}
static bool guarded_open(void*c,uint64_t*t){bool ok=open_scan(c,t);if(kill_endpoint==2)owner_live=false;return ok;}
static bool guarded_poll(void*c,uint64_t t,uint32_t n){bool ok=poll_scan(c,t,n);if(kill_endpoint==3)owner_live=false;return ok;}
static bool guarded_status(void*c,risc_ble_sensor_status_v1*s){bool ok=scan_status(c,s);if(kill_endpoint==4)owner_live=false;return ok;}
static bool guarded_device(void*c,uint32_t i,risc_ble_sensor_device_v1*d){bool ok=scan_device(c,i,d);if(kill_endpoint==5)owner_live=false;return ok;}
static bool guarded_close(void*c,uint64_t t){bool ok=close_scan(c,t);if(kill_endpoint==6)owner_live=false;return ok;}
static bool guarded_release(risc_runtime_capability_v1*g){bool ok=release(g);if(kill_endpoint==7)owner_live=false;return ok;}
static pqb_session configured(void){
 fixture();owner_live=true;kill_endpoint=0;mode=0;
 extended=(risc_bluetooth_sensor_link_api_v1){.sensors=api,.extension_tag=RISC_BLUETOOTH_SENSOR_LINK_TAG,.extension_version=1,.extension_size=sizeof(extended),.link=observation};
 extended.sensors.struct_size=sizeof(extended);
 extended.sensors.open=guarded_open;extended.sensors.poll=guarded_poll;extended.sensors.status=guarded_status;
 extended.sensors.device=guarded_device;extended.sensors.close=guarded_close;
 runtime.acquire=guarded_acquire;runtime.release=guarded_release;f.acquire_api=&extended;
 measurement(0,1,0,73);
 return (pqb_session){.alive=guarded_alive};
}
static void chosen(pqb_session*s){
 assert(pqb_start(s,&runtime,true));assert(pqb_poll(s));assert(pqb_select(s,f.devices[0].address,0));assert(pqb_poll(s));
}
int main(void){
 assert(original_remote_battery_main()==0);unsigned cases=0;
 for(unsigned wanted=0;wanted<3;wanted++){
  pqb_session s=configured();mode=wanted;chosen(&s);
  uint32_t expected=wanted==0?RISC_BLE_LINK_CONNECTED:wanted==1?RISC_BLE_LINK_NOT_CONNECTED:RISC_BLE_LINK_UNKNOWN;
  assert(s.current&&s.link_state==expected);assert(pqb_close(&s)&&s.link_state==RISC_BLE_LINK_UNKNOWN);cases++;
 }
 for(unsigned bad=3;bad<=9;bad++){
  pqb_session s=configured();chosen(&s);mode=bad;
  assert(!pqb_poll(&s)&&s.link_state==RISC_BLE_LINK_UNKNOWN&&s.status==PQB_ERROR&&!s.active);cases++;
 }
 pqb_session s=configured();f.acquire_api=&api;assert(pqb_start(&s,&runtime,true));
 assert(pqb_poll(&s));assert(pqb_select(&s,f.devices[0].address,0));assert(pqb_poll(&s)&&s.current&&s.link_state==RISC_BLE_LINK_UNKNOWN);assert(pqb_close(&s));cases++;
 s=configured();chosen(&s);measurement(1,2,1,44);assert(pqb_poll(&s));assert(pqb_select(&s,f.devices[1].address,1));assert(s.link_state==RISC_BLE_LINK_UNKNOWN);assert(pqb_close(&s));cases++;
 for(unsigned endpoint=1;endpoint<=8;endpoint++){
  s=configured();
  if(endpoint<=2){kill_endpoint=endpoint;assert(!pqb_start(&s,&runtime,true));}
  else {
   chosen(&s);kill_endpoint=endpoint;
   if(endpoint==6||endpoint==7)assert(!pqb_close(&s));else assert(!pqb_poll(&s));
  }
  assert(s.retained&&s.terminal&&s.link_state==RISC_BLE_LINK_UNKNOWN);
  unsigned before=calls();assert(!pqb_poll(&s));assert(!pqb_close(&s));assert(!pqb_start(&s,&runtime,true));assert(calls()==before);cases++;
 }
 s=configured();assert(pqb_note_time(&s,100));chosen(&s);assert(pqb_recent(&s));
 assert(pqb_close(&s)&&pqb_recent(&s)&&s.link_state==RISC_BLE_LINK_UNKNOWN);assert(pqb_note_time(&s,10100)&&pqb_recent(&s));assert(pqb_note_time(&s,10101)&&!pqb_recent(&s));cases++;
 s=configured();assert(pqb_note_time(&s,100));chosen(&s);assert(!pqb_note_time(&s,99)&&!pqb_recent(&s));cases++;
 s=configured();s.last_valid=true;s.last_percent=73;assert(pqb_note_time(&s,100)&&!pqb_recent(&s));cases++;
 s=configured();assert(pqb_note_time(&s,100));f.devices[0].measurement_age_ms=9000;chosen(&s);assert(pqb_recent(&s));assert(pqb_note_time(&s,1101)&&!pqb_recent(&s));assert(pqb_close(&s));cases++;
 puts("Link status is receiver-local; positive connection supplied only by endpoint double.");
 printf("remote battery link/identity: %u cases passed plus unchanged baseline suite\n",cases);
 return 0;
}
