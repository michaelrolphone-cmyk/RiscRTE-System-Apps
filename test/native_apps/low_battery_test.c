/* Real shared policy + saved settings + Quick Controls hardware projection. */
#include "PortableLowBattery.h"
#include "PortableQuickSession.h"
#include "PortableQuickRadios.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *keys[]={PORTABLE_LOW_BATTERY_KEY,PORTABLE_SLEEP_IDLE_KEY,PORTABLE_SLEEP_DEEP_KEY,PQA_BRIGHTNESS_KEY,PORTABLE_RADIO_KEY};
static struct {bool exists;uint8_t data[8];uint32_t size;} records[5];
static unsigned writes[5],reads,live,brightness,hardware_calls;
static int fail_get=-1,fail_put=-1,commit_io=-1,unconfirmed=-1;
static bool acquire_fail,release_fail,display_fail,wifi_fail,ble_fail,wifi_on;
static uint8_t bluetooth;
static int index_of(const char *key){for(unsigned i=0;i<5;i++)if(!strcmp(key,keys[i]))return (int)i;return -1;}
static int32_t get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size){
 (void)c;reads++;*size=0;int i=index_of(key);if(i<0)return RISC_KEY_VALUE_NOT_FOUND;
 if(i==fail_get||(i==unconfirmed&&writes[i]))return RISC_KEY_VALUE_IO;
 if(!records[i].exists)return RISC_KEY_VALUE_NOT_FOUND;
 *size=records[i].size;if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(data,records[i].data,*size);return RISC_KEY_VALUE_OK;
}
static int32_t put(void *c,const char *key,const void *data,uint32_t size){
 (void)c;int i=index_of(key);assert(i>=0&&size<=8);writes[i]++;
 if(i==fail_put)return RISC_KEY_VALUE_IO;
 records[i].exists=true;records[i].size=size;memcpy(records[i].data,data,size);
 return i==commit_io?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool brightness_set(void *c,uint16_t b,uint16_t max){(void)c;assert(b<=100&&max==100);hardware_calls++;if(display_fail)return false;brightness=b;return true;}
static bool wifi_off(void *c){(void)c;hardware_calls++;if(wifi_fail)return false;wifi_on=false;return true;}
static wifi_link_t wifi_status(void *c){(void)c;return wifi_on?WIFI_LINK_UP:WIFI_LINK_DOWN;}
static bool ble_set(void *c,bool enabled){(void)c;hardware_calls++;if(ble_fail)return false;bluetooth=enabled?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;}
static bool ble_status(void *c,uint8_t *out){(void)c;*out=bluetooth;return true;}
static const risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.set_brightness=brightness_set};
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.disconnect_checked=wifi_off,.status=wifi_status};
static const portable_bluetooth_control_v1 ble={.api_version=1,.struct_size=sizeof(ble),.set_enabled=ble_set,.status=ble_status};
static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g){
 assert(version==1);if(acquire_fail)return false;
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(id==1);g->api=&kv;}
 else if(!strcmp(name,"net.wifi")){assert(id==15);g->api=&wifi;}
 else {assert(!strcmp(name,"bluetooth.hci")&&id==16);g->api=&ble;}
 live++;return true;
}
static bool release(risc_runtime_capability_v1 *g){assert(live&&g->api);if(release_fail)return false;live--;g->api=NULL;return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
static void reset(void){
 memset(records,0,sizeof(records));memset(writes,0,sizeof(writes));reads=live=hardware_calls=0;brightness=40;
 fail_get=fail_put=commit_io=unconfirmed=-1;acquire_fail=release_fail=display_fail=wifi_fail=ble_fail=false;
 wifi_on=true;bluetooth=PORTABLE_BLUETOOTH_ON;
}
static unsigned observe(portable_low_battery *state,unsigned percent,unsigned flags){
 risc_battery_sample_v1 sample={3700,(uint8_t)percent,(uint8_t)flags};
 unsigned result=portable_low_battery_update(state,&rt,&sample);if(!release_fail)assert(!live);return result;
}
static void expected(uint32_t idle,uint32_t deep,unsigned b,uint8_t radios){
 pqa_session s;pqa_session_init(&s);assert(pqa_session_load(&s,&rt));
 assert(s.idle_ms==idle&&s.deep_ms==deep&&s.brightness==b&&s.ui.brightness==b);
 pqa_radios r;assert(pqa_radios_load(&r,&s.ui,&rt));assert(r.flags==radios);
 assert(pqa_session_restore(&s,&display)&&brightness==b&&!live);
}
static void manual(void){
 assert(portable_sleep_timer_save(&kv,false,120000));assert(portable_sleep_timer_save(&kv,true,600000));
 pqa_session s;pqa_session_init(&s);assert(pqa_session_load(&s,&rt));s.ui.action_brightness=70;bool changed;
 assert(pqa_session_apply(&s,&rt,&display,PQA_BRIGHTNESS_COMMIT,&changed));
 pqa_radios r;assert(pqa_radios_load(&r,&s.ui,&rt));
 if(!(r.flags&PORTABLE_RADIO_WIFI))assert(pqa_radios_apply(&r,&s.ui,&rt,PQA_WIFI));
 if(!(r.flags&PORTABLE_RADIO_BLUETOOTH))assert(pqa_radios_apply(&r,&s.ui,&rt,PQA_BLUETOOTH));
}
static void crossings(void){
 reset();portable_low_battery state={0};assert(observe(&state,11,0)==0);assert(observe(&state,10,0)==0);
 for(unsigned i=0;i<5;i++)assert(!writes[i]);
 assert(observe(&state,9,0)==PORTABLE_LOW_BATTERY_ENTERED);expected(20000,60000,15,0);
 for(unsigned i=0;i<5;i++)assert(writes[i]==1);
 assert(!wifi_on&&bluetooth==PORTABLE_BLUETOOTH_OFF);
 manual();unsigned saved[5];memcpy(saved,writes,sizeof(saved));unsigned got=reads,hardware=hardware_calls;
 for(unsigned p=0;p<10;p++)assert(observe(&state,p,0)==0);
 assert(reads==got&&hardware_calls==hardware&&!memcmp(saved,writes,sizeof(saved)));
 expected(120000,600000,70,3);
 state=(portable_low_battery){0};assert(observe(&state,3,0)==0);expected(120000,600000,70,3);assert(!memcmp(saved,writes,sizeof(saved)));
 assert(observe(&state,8,RISC_BATTERY_CHARGING|PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_INPUT_READY|PORTABLE_POWER_BATTERY_PRESENT)==0);
 assert(observe(&state,9,0)==0);expected(120000,600000,70,3);
 assert(observe(&state,10,RISC_BATTERY_CHARGING)==0);assert(writes[0]==saved[0]+1);
 assert(observe(&state,9,RISC_BATTERY_CHARGING)==PORTABLE_LOW_BATTERY_ENTERED);expected(20000,60000,15,0);
 reset();state=(portable_low_battery){0};assert(observe(&state,0,0)==1);manual();state=(portable_low_battery){0};assert(observe(&state,0,0)==0);expected(120000,600000,70,3);
}
static void invalid_samples(void){
 reset();portable_low_battery state={0};
 assert(observe(&state,255,0)==0);assert(observe(&state,0,RISC_BATTERY_PROFILE_MISSING)==0);
 assert(observe(&state,0,PORTABLE_POWER_STATUS_VALID)==0);assert(!state.observed&&!reads);
 assert(observe(&state,9,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT)==1);manual();
 unsigned saved[5];memcpy(saved,writes,sizeof(saved));
 assert(observe(&state,100,RISC_BATTERY_PROFILE_MISSING)==0);assert(observe(&state,255,0)==0);
 state=(portable_low_battery){0};assert(observe(&state,9,0)==0);assert(!memcmp(saved,writes,sizeof(saved)));
}
static void failures(void){
 for(int key=0;key<5;key++)for(unsigned mode=0;mode<3;mode++){
  reset();portable_low_battery state={0};if(mode==0)fail_put=key;else if(mode==1)commit_io=key;else unconfirmed=key;
  unsigned result=observe(&state,9,0);assert((result&PORTABLE_LOW_BATTERY_ERROR)==(mode==1?0:PORTABLE_LOW_BATTERY_ERROR));
  assert(!!(result&1)==(key!=0||mode==1));unsigned saved[5];memcpy(saved,writes,sizeof(saved));
  fail_put=commit_io=unconfirmed=-1;
  assert(observe(&state,8,0)==0&&!memcmp(saved,writes,sizeof(saved)));
  if(key!=0||mode!=0){state=(portable_low_battery){0};assert(observe(&state,8,0)==0&&!memcmp(saved,writes,sizeof(saved)));}
 }
 reset();portable_low_battery state={0};fail_get=0;assert(observe(&state,9,0)==2);fail_get=-1;assert(observe(&state,8,0)==0);assert(!writes[0]);
 reset();state=(portable_low_battery){0};acquire_fail=true;assert(observe(&state,9,0)==2);acquire_fail=false;assert(observe(&state,8,0)==0&&!reads);
 reset();state=(portable_low_battery){0};release_fail=true;assert(observe(&state,9,0)==(1|4)&&live==1);release_fail=false;live=0;
 for(unsigned n=0;n<7;n++){
  reset();records[0].exists=true;records[0].size=n;memset(records[0].data,0xff,8);state=(portable_low_battery){0};
  assert(observe(&state,9,0)==2);for(unsigned i=0;i<5;i++)assert(!writes[i]);
 }
 for(unsigned mode=0;mode<3;mode++){
  reset();state=(portable_low_battery){0};assert(observe(&state,9,0)==1);
  pqa_session s;pqa_session_init(&s);assert(pqa_session_load(&s,&rt));pqa_radios r;
  if(mode==0)display_fail=true;else if(mode==1)wifi_fail=true;else ble_fail=true;
  assert(mode==0?!pqa_session_restore(&s,&display):!pqa_radios_load(&r,&s.ui,&rt));
  assert(observe(&state,8,0)==0);for(unsigned i=0;i<5;i++)assert(writes[i]==1);
 }
}
static void timer_records(void){
 reset();uint32_t ms;assert(portable_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_MISSING&&ms==60000);
 assert(portable_sleep_timer_load(&kv,true,&ms)==PORTABLE_SLEEP_MISSING&&ms==300000);
 assert(!portable_sleep_timer_save(&kv,false,0)&&!portable_sleep_timer_save(&kv,false,4999)&&!portable_sleep_timer_save(&kv,true,3600001));
 for(unsigned seconds=5;seconds<=3600;seconds++){
  assert(portable_sleep_timer_save(&kv,false,seconds*1000));assert(portable_sleep_timer_load(&kv,false,&ms)==0&&ms==seconds*1000);
 }
 unsigned n=writes[1];assert(portable_sleep_timer_save(&kv,false,3600000)&&writes[1]==n);
 for(unsigned i=0;i<5;i++)for(unsigned bit=0;bit<8;bit++){
  uint8_t before=records[1].data[i];records[1].data[i]^=(uint8_t)(1u<<bit);
  assert(portable_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_INVALID&&ms==60000);records[1].data[i]=before;
 }
}
int main(void){crossings();invalid_samples();failures();timer_records();puts("Low battery: crossing/reboot/manual overrides/charging/unknown/error and shared settings parity passed");}
