#include "PortableTime.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  portable_time_candidate candidates[2];
  twatch_rtc_time_v1 raw={2026,10,4,0,0,40,0},local;
  assert(portable_time_forward(&raw,&local));
#ifdef PORTABLE_RTC_UTC8_DENVER
  assert(local.year==2026 && local.month==10 && local.day==3 && local.hour==10 && local.minute==40 && local.weekday==6);
  local=(twatch_rtc_time_v1){2026,3,8,0,2,30,0};
  assert(portable_time_inverse(&local,candidates)==0);
  local=(twatch_rtc_time_v1){2026,11,1,0,1,30,0};
  assert(portable_time_inverse(&local,candidates)==2);
  assert(candidates[0].rtc.hour==15 && candidates[0].utc_offset_minutes==-360);
  assert(candidates[1].rtc.hour==16 && candidates[1].utc_offset_minutes==-420);
  local=(twatch_rtc_time_v1){2006,4,2,0,2,30,0};assert(portable_time_inverse(&local,candidates)==0);
  local=(twatch_rtc_time_v1){2006,10,29,0,1,30,0};assert(portable_time_inverse(&local,candidates)==2);
  local=(twatch_rtc_time_v1){2099,12,31,0,23,59,59};assert(portable_time_inverse(&local,candidates)==0);
#else
  assert(portable_time_same(&raw,&local));
  assert(portable_time_inverse(&local,candidates)==1 && portable_time_same(&raw,&candidates[0].rtc));
#endif
  unsigned checked=0;
  for(unsigned y=2000;y<=2099;++y)for(unsigned m=1;m<=12;++m)
    for(unsigned d=1;d<=watch_month_days(y,m);++d)for(unsigned h=0;h<24;++h) {
      raw=(twatch_rtc_time_v1){(uint16_t)y,(uint8_t)m,(uint8_t)d,0,(uint8_t)h,30,17};
      raw.weekday=(uint8_t)watch_weekday(y,m,d);
      if(!portable_time_forward(&raw,&local))continue;
      unsigned n=portable_time_inverse(&local,candidates);assert(n==1 || n==2);
      bool found=false;for(unsigned i=0;i<n;++i)if(portable_time_same(&raw,&candidates[i].rtc))found=true;
      assert(found);++checked;
    }
  printf("portable time policy: %u hourly round trips plus gap/fold/range fixtures passed\n",checked);
  return 0;
}
