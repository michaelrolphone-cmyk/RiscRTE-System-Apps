/* Link-only native harness: not a manifest app or product image. Exercise all
 * public surfaces, with no process/libc timezone or additional native ABI. */
#include "PortableTimeZone.h"
#include "PortableTimeZonePreference.h"
static volatile int result;
__attribute__((visibility("default"))) void app_main(void) {
    portable_timezone_rule rule;
    portable_timezone_civil civil={2026,3,8,1,30,0,0};
    portable_timezone_candidate details;
    portable_timezone_inverse inverse;
    portable_timezone_rtc_result rtc;
    int64_t epoch; char id[40];
    result=portable_timezone_resolve("America/New_York",17,&rule);
    result=portable_timezone_parse("UTC0",5,&rule);
    result=portable_timezone_find("Etc/UTC",8);
    result=(int)portable_timezone_count();
    result=(int)portable_timezone_region_count(PORTABLE_TIMEZONE_AMERICA);
    result=portable_timezone_city_index(PORTABLE_TIMEZONE_AMERICA,100);
    result=portable_timezone_region_name(PORTABLE_TIMEZONE_UTC)!=0;
    result=portable_timezone_get(0)!=0;
    result=portable_timezone_display_name(0,id,sizeof(id));
    result=portable_timezone_civil_to_epoch(&civil,&epoch);
    result=portable_timezone_epoch_to_civil(epoch,&civil);
    result=portable_timezone_utc_to_local(&rule,epoch,&civil,&details);
    result=portable_timezone_local_to_utc(&rule,&civil,&inverse);
    result=portable_timezone_interpret_rtc(&rule,&civil,0,0,-1,&rtc);
    result=portable_timezone_preference_load(0,id);
    result=portable_timezone_preference_save(0,"UTC",4);
}
