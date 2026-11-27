#pragma once
#include "RiscRtcUtcV1.h"
/* Pure calendar codec. The offset is explicit provider recipe authority. */
static inline bool risc_rtc_calendar_utc_v1(const risc_rtc_calendar_v2*t,int32_t offset,int64_t*out){
 static const uint8_t mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(!t||!out||offset < -86400||offset > 86400||t->year<2000||t->year>2099||t->month<1||t->month>12||t->weekday>6||t->hour>23||t->minute>59||t->second>59)return false;
 unsigned leap=(t->year%4u)==0;
 if(t->day<1||t->day>mdays[t->month-1]+(t->month==2&&leap))return false;
 uint32_t days=10957;
 for(unsigned y=2000;y<t->year;y++)days+=365u+(y%4u==0);
 for(unsigned m=1;m<t->month;m++)days+=mdays[m-1]+(m==2&&leap);
 days+=t->day-1u;
 *out=(int64_t)days*86400+t->hour*3600+t->minute*60+t->second-offset;
 return true;
}
