#include "PortableAppSleep.h"
extern int portable_home_light_sleep_with_ui(const risc_runtime_api_v1*,const risc_display_output_api_v1*,const risc_battery_gauge_api_v1*,const alarm_service_v1*,const portable_idle_sleep_ui*);
int portable_app_idle_sleep_with_ui(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a,const portable_idle_sleep_ui*ui){return portable_home_light_sleep_with_ui(rt,d,g,a,ui);}
int portable_app_idle_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){return portable_home_light_sleep_with_ui(rt,d,g,a,NULL);}
