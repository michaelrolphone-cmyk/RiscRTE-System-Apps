#include "PortableDeskClock.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
static portable_desk_config cfg(void){portable_desk_config c={0};c.face=PORTABLE_DESK_RAILWAY;c.time_format=1;c.rtc_stores_utc=1;c.rtc_reference_epoch=123456;memcpy(c.time_zone,"America/Denver",15);return c;}
int main(void){
 for(uint64_t n=0;n<1000000;n+=113){int64_t value=(int64_t)(n*UINT64_C(8174286451881));assert(portable_desk_second_in_minute(value)==(uint64_t)value%60u);}
 portable_desk_config config=cfg();portable_desk_cycle c;portable_desk_frame f;portable_desk_record r={0},out={0};uint32_t delay=0;
 assert(portable_desk_begin(&c,&config,NULL,false));assert(portable_desk_plan_frame(&c,120,&f));assert(f.full&&!f.has_previous);assert(!portable_desk_plan_frame(&c,120,&f));
 assert(portable_desk_presented(&c,true));assert(portable_desk_prepare_record(&c,120,0,&r,&delay)==PORTABLE_DESK_READY);assert(delay==60000&&r.displayed_minute==120&&r.refresh_modulo==1);
 for(unsigned n=1;n<=60;++n){assert(portable_desk_begin(&c,&config,&r,true));assert(portable_desk_plan_frame(&c,120+60*n,&f));assert(f.full==(n%30==0));assert(f.has_previous==!f.full);assert(portable_desk_presented(&c,true));assert(portable_desk_prepare_record(&c,120+60*n,999999,&r,&delay)==PORTABLE_DESK_READY);assert(delay==59001);}
 for(unsigned sec=0;sec<60;++sec)for(unsigned us=0;us<1000000;us+=997){
  assert(portable_desk_begin(&c,&config,NULL,false));assert(portable_desk_plan_frame(&c,sec,&f));assert(portable_desk_presented(&c,true));assert(portable_desk_prepare_record(&c,sec,(int32_t)us,&out,&delay)==PORTABLE_DESK_READY);uint64_t exact=(60-sec)*1000000u-us;assert(delay>=1&&delay<=60000&&delay*1000u>=exact&&delay*1000u-exact<1000u);
 }
 /* Slow render or teardown: bounded catch-up, no stale minute committed. */
 assert(portable_desk_begin(&c,&config,&r,true));
 for(unsigned n=0;n<3;++n){assert(portable_desk_plan_frame(&c,600+n*60,&f));assert(portable_desk_presented(&c,true));out.displayed_minute=-1;delay=123;int rc=portable_desk_prepare_record(&c,660+n*60,0,&out,&delay);assert(rc==(n==2?PORTABLE_DESK_STOP:PORTABLE_DESK_REPAINT));assert(out.displayed_minute==-1&&delay==123);}
 assert(!portable_desk_plan_frame(&c,999,&f));
 assert(portable_desk_begin(&c,&config,&r,true));assert(portable_desk_plan_frame(&c,900,&f));int64_t prior=c.current.displayed_minute;assert(!portable_desk_presented(&c,false));assert(c.current.displayed_minute==prior);assert(portable_desk_prepare_record(&c,900,0,&out,&delay)==PORTABLE_DESK_STOP);
 /* Button/cold/config changes never reuse an incompatible retained image. */
 assert(portable_desk_begin(&c,&config,&r,false));assert(c.full&&!c.current.has_image);
 for(unsigned face=0;face<PORTABLE_DESK_FACE_COUNT;++face){portable_desk_config changed=config;changed.face=(uint8_t)face;assert(portable_desk_begin(&c,&changed,&r,true));assert(c.current.has_image==(face==config.face));}
 config.flip_ui=1;assert(portable_desk_begin(&c,&config,&r,true));assert(c.full&&!c.current.has_image);config=cfg();
 assert(portable_desk_begin(&c,&config,&r,true));portable_desk_cancel(&c);assert(!portable_desk_plan_frame(&c,900,&f));
 uint8_t bytes[80],copy[80];assert(portable_desk_encode(&r,bytes,sizeof(bytes)));assert(portable_desk_decode(bytes,sizeof(bytes),&out));assert(out.displayed_minute==r.displayed_minute&&portable_desk_same_config(&out.config,&r.config));
 for(unsigned i=0;i<80;++i){bool reserved=(i>=25&&i<32)||i>=72; if(!reserved)continue;memcpy(copy,bytes,80);copy[i]=1;out.displayed_minute=-1;assert(!portable_desk_decode(copy,80,&out));assert(out.displayed_minute==-1);}
 for(unsigned i=0;i<80;++i)assert(!portable_desk_decode(bytes,i,&out));
 for(unsigned field=0;field<6;++field){memcpy(copy,bytes,80);unsigned indexes[]={4,5,6,7,9,10};copy[indexes[field]]=255;assert(!portable_desk_decode(copy,80,&out));}
 memcpy(copy,bytes,80);copy[24]=30;assert(!portable_desk_decode(copy,80,&out));memcpy(copy,bytes,80);copy[23]=128;assert(!portable_desk_decode(copy,80,&out));memcpy(copy,bytes,80);copy[16]|=1;assert(!portable_desk_decode(copy,80,&out));
 memcpy(copy,bytes,80);memset(copy+32,'A',40);assert(!portable_desk_decode(copy,80,&out));memcpy(copy,bytes,80);copy[71]='A';assert(!portable_desk_decode(copy,80,&out));
 memset(copy,0xa5,80);portable_desk_record invalid=r;invalid.has_image=false;assert(!portable_desk_encode(&invalid,copy,80));for(unsigned i=0;i<80;++i)assert(copy[i]==0xa5);
 for(int usec=-1;usec<=1000000;usec+=1000001){assert(portable_desk_begin(&c,&config,NULL,false));assert(portable_desk_plan_frame(&c,0,&f));assert(portable_desk_presented(&c,true));assert(portable_desk_prepare_record(&c,0,usec,&out,&delay)==PORTABLE_DESK_STOP);}
 assert(portable_desk_begin(&c,&config,NULL,false));assert(portable_desk_plan_frame(&c,INT64_MAX,&f));assert(portable_desk_presented(&c,true));assert(portable_desk_prepare_record(&c,INT64_MAX,999999,&out,&delay)==PORTABLE_DESK_READY);assert(delay>=1&&delay<=60000);
 puts("Desk clock policy: 60 refresh cycles, 60,240 deadline boundaries, serialization/rendering-schema and interrupted-cycle cases pass");return 0;
}
