/* Pure validation around an app-owned closed-sample callback. */
#include "PortableNativeTimeToolbar.h"
static bool native_toolbar_local(twatch_rtc_time_v1 *out) {
  if(!out || failed || native_custody_retained)return false;
  twatch_rtc_time_v1 local={0};
  bool available=portable_app_native_local_time(&local);
  /* The callback may have fenced custody even when it reports success. No
   * fallback, health/battery read, rendering or cleanup may follow that fence. */
  if(native_custody_retained || failed || !available)return false;
  static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned year=local.year,month=local.month;
  if(year<1600 || year>9999 || month<1 || month>12 || local.hour>23 ||
     local.minute>59 || local.second>59 || local.weekday>6)return false;
  bool leap=year%4==0 && (year%100!=0 || year%400==0);
  unsigned limit=days[month-1]+(month==2 && leap);
  if(local.day<1 || local.day>limit)return false;
  unsigned prior=year-1,day_index=prior*365+prior/4-prior/100+prior/400;
  for(unsigned m=1;m<month;++m)day_index+=days[m-1]+(m==2 && leap);
  day_index+=local.day;
  if(day_index%7!=local.weekday)return false; /* 0001-01-01 was Monday. */
  *out=local;return true;
}
