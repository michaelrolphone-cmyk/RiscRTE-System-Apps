#pragma once
#include "PortableRtcClock.h"
/* A failed/missing RTC disables aging. Uptime is never persisted as a date. */
static inline bool portable_fingerprint_clock(const risc_runtime_api_v1*runtime,uint64_t*out){
    *out=0;risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    if(!runtime->acquire("rtc.clock",2,0,&grant))return true;
    const twatch_rtc_api_v1*rtc=grant.api;twatch_rtc_time_v1 t={0};
    bool read=rtc&&rtc->api_version==2&&rtc->struct_size>=sizeof(*rtc)&&rtc->read&&rtc->read(rtc->context,&t);
    if(!runtime->release(&grant))return false;
    static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(!read||t.year<2000||t.year>2099||t.month<1||t.month>12||!t.day||t.day>days[t.month-1]+(t.month==2&&t.year%4==0)||t.hour>23||t.minute>59||t.second>59)return true;
    uint64_t d=0;for(unsigned y=2000;y<t.year;y++)d+=365+(y%4==0);
    for(unsigned m=1;m<t.month;m++)d+=days[m-1]+(m==2&&t.year%4==0);
    *out=946684800u+(d+t.day-1)*86400u+t.hour*3600u+t.minute*60u+t.second;
#ifdef PORTABLE_RTC_UTC8_DENVER
    *out-=8u*3600u;
#endif
    return true;
}
