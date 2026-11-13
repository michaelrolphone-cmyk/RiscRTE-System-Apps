#include "RiscRtcUtcCalendarV1.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 risc_rtc_calendar_v2 t={2000,1,1,6,0,0,0};int64_t e=-1;
 assert(risc_rtc_calendar_utc_v1(&t,28800,&e)&&e==INT64_C(946656000));
 t=(risc_rtc_calendar_v2){2099,12,31,4,23,59,59};
 assert(risc_rtc_calendar_utc_v1(&t,28800,&e)&&e==INT64_C(4102415999));
 t=(risc_rtc_calendar_v2){2000,2,29,2,0,0,0};
 assert(risc_rtc_calendar_utc_v1(&t,0,&e)&&e==INT64_C(951782400));
 t.year=2001;e=123;assert(!risc_rtc_calendar_utc_v1(&t,28800,&e)&&e==123);
 t=(risc_rtc_calendar_v2){2038,1,19,2,3,14,8};
 assert(risc_rtc_calendar_utc_v1(&t,0,&e)&&e==INT64_C(2147483648));
 t.year=2100;e=123;assert(!risc_rtc_calendar_utc_v1(&t,0,&e)&&e==123);
 t.year=2000;t.month=0;assert(!risc_rtc_calendar_utc_v1(&t,0,&e));
 t.month=1;t.weekday=7;assert(!risc_rtc_calendar_utc_v1(&t,0,&e));
 t.weekday=0;t.second=60;assert(!risc_rtc_calendar_utc_v1(&t,0,&e));
 t.second=0;assert(!risc_rtc_calendar_utc_v1(&t,86401,&e));
 risc_rtc_utc_client_v1 client={0};assert(!risc_rtc_utc_bind_v1(NULL,&client));
 risc_rtc_utc_api_v1 a={0};a.base.api_version=2;a.base.struct_size=sizeof(a.base);
 assert(!risc_rtc_utc_bind_v1(&a,&client));a.base.struct_size=sizeof(a);
 assert(!risc_rtc_utc_bind_v1(&a,&client));
 puts("RTC UTC calendar and suffix negative checks passed");return 0;
}
