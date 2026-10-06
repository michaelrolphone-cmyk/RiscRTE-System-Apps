#include <assert.h>
#include <stdio.h>
#include "PortableTapSettings.h"
#include "PortableTapCalibration.h"
static uint8_t record[8];static uint32_t length;static int get_result,put_result;static bool mismatch;
static int32_t get(void*c,const char*key,void*out,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(key,PORTABLE_TAP_KEY)&&cap==8);*n=length;if(get_result==0)memcpy(out,record,8);return get_result;}
static int32_t put(void*c,const char*key,const void*in,uint32_t n){(void)c;assert(!strcmp(key,PORTABLE_TAP_KEY)&&n==8);memcpy(record,in,8);length=8;if(mismatch)record[7]^=1;return put_result;}
static risc_key_value_v1 store={1,sizeof(store),NULL,get,put};
static portable_tap_calibration cal;static uint32_t now;
static void feed(unsigned ms,bool tap,bool moving,bool sampled){
 for(unsigned i=0;i<ms;i+=10){now+=10;twatch_tap_observation_v1 s={.struct_size=sizeof(s),.double_tap=tap&&i==0,.sample_ready=sampled,.z_mg=1000,.x_mg=moving?(int16_t)(i%100<50?0:300):0};portable_tap_calibration_feed(&cal,now,&s);}
}
static void start(unsigned profile){twatch_tap_info_v1 info={sizeof(info),profile,0,profile==1?7:15,profile==1?3:12};assert(portable_tap_calibration_start(&cal,&info,now));}
static void three_pairs(void){feed(1500,false,false,true);assert(cal.phase==TAP_CAL_PAIR);for(unsigned i=0;i<3;i++){feed(10,true,false,true);assert(cal.phase==TAP_CAL_QUIET&&cal.pairs==i+1);feed(1000,false,false,true);}assert(cal.phase==TAP_CAL_MOVE);}
int main(void){
 portable_tap_settings p;
 get_result=RISC_KEY_VALUE_NOT_FOUND;assert(portable_tap_load(&store,&p)==PORTABLE_TAP_MISSING&&p.enabled&&p.bma423==3&&p.bma456h==12);
 assert(portable_tap_load(NULL,&p)==PORTABLE_TAP_UNAVAILABLE&&!p.enabled);assert(portable_tap_load(&store,NULL)==PORTABLE_TAP_INVALID);
 get_result=RISC_KEY_VALUE_OK;
 for(unsigned enabled=0;enabled<2;enabled++)for(unsigned x=0;x<8;x++)for(unsigned y=0;y<16;y++)for(unsigned measured=0;measured<4;measured++){
  p=(portable_tap_settings){enabled,x,y,measured};assert(portable_tap_save(&store,&p));portable_tap_settings q;assert(portable_tap_load(&store,&q)==PORTABLE_TAP_LOADED&&!memcmp(&p,&q,sizeof(p)));
 }
 uint8_t good[8];memcpy(good,record,8);
 for(unsigned i=0;i<64;i++){memcpy(record,good,8);record[i/8]^=(uint8_t)(1u<<(i%8));assert(portable_tap_load(&store,&p)==PORTABLE_TAP_INVALID&&!p.enabled);}
 memcpy(record,good,8);length=7;assert(portable_tap_load(&store,&p)==PORTABLE_TAP_INVALID&&!p.enabled);length=8;
 get_result=RISC_KEY_VALUE_IO;assert(portable_tap_load(&store,&p)==PORTABLE_TAP_UNAVAILABLE&&!p.enabled);
 get_result=RISC_KEY_VALUE_BUFFER_SMALL;assert(portable_tap_load(&store,&p)==PORTABLE_TAP_INVALID&&!p.enabled);
 get_result=0;p=portable_tap_defaults();put_result=RISC_KEY_VALUE_IO;assert(!portable_tap_save(&store,&p));put_result=0;mismatch=true;assert(!portable_tap_save(&store,&p));mismatch=false;
 p.enabled=2;assert(!portable_tap_save(&store,&p));p=portable_tap_defaults();p.bma423=8;assert(!portable_tap_save(&store,&p));p=portable_tap_defaults();p.bma456h=16;assert(!portable_tap_save(&store,&p));p=portable_tap_defaults();p.measured=4;assert(!portable_tap_save(&store,&p));
 for(unsigned profile=1;profile<=2;profile++){
  now=UINT32_MAX-1000;start(profile);unsigned maximum=cal.value;three_pairs();feed(5000,false,true,true);assert(cal.phase==TAP_CAL_READY&&cal.value==maximum&&cal.pairs==3);
  start(profile);feed(1500,false,false,true);feed(5000,false,false,true);assert(cal.phase==TAP_CAL_NEXT&&cal.value==maximum-1);portable_tap_calibration_candidate(&cal,now);three_pairs();feed(5000,false,true,true);assert(cal.phase==TAP_CAL_READY&&cal.value==maximum-1);
  start(profile);three_pairs();feed(5000,false,false,true);assert(cal.phase==TAP_CAL_INSUFFICIENT);
  start(profile);three_pairs();feed(10,true,true,true);assert(cal.phase==TAP_CAL_NOISY);
  start(profile);feed(1500,false,false,true);feed(10,true,false,true);feed(10,true,false,true);assert(cal.phase==TAP_CAL_NOISY);
  start(profile);feed(510,false,false,false);assert(cal.phase==TAP_CAL_INSUFFICIENT);
  start(profile);now+=501;twatch_tap_observation_v1 s={.struct_size=sizeof(s),.sample_ready=true};portable_tap_calibration_feed(&cal,now,&s);assert(cal.phase==TAP_CAL_ERROR);
  start(profile);s=(twatch_tap_observation_v1){.struct_size=sizeof(s),.sample_ready=true};
  for(unsigned n=0;n<100;n++)portable_tap_calibration_feed(&cal,now,&s);
  assert(cal.phase==TAP_CAL_ERROR);
  start(profile);cal.value=0;feed(1500,false,false,true);feed(5000,false,false,true);assert(cal.phase==TAP_CAL_INSUFFICIENT);
 }
 puts("Tap policy: 1024 exact records, all 64 corruption bits, uncertain storage, real-status calibration state transitions and timer wrap PASS");
}
