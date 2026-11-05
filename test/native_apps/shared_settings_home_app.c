/* Real Home controller globals checked after a foreground BUSY->ready boot. */
#include <assert.h>
#include "../../Apps/paper_clock.c"
extern bool shared_settings_force_busy;
void shared_settings_assert(void) {
 assert(!desk_retained && !desk_config_pending && desk_zone_known && desk_basis_known);
 assert(format==PORTABLE_TIME_FORMAT_24 && desk_configuration.time_format==PORTABLE_TIME_FORMAT_24);
 assert(!strcmp(desk_configuration.time_zone,"America/Denver"));
 assert(desk_configuration.face==2 && desk_configuration.rtc_stores_utc==1);
#ifdef PORTABLE_DESK_LOCK_HOME
 assert(desk_direction_known && desk_foreground_flip==1 && desk_configuration.flip_ui==1);
#endif
 portable_desk_config saved=desk_configuration;unsigned saved_mode=desk_sleep_mode;
 portable_rtc_basis saved_basis=desk_basis;
 shared_settings_force_busy=true;
 assert(!desk_config(&desk_configuration,&desk_sleep_mode) && desk_config_pending);
 assert(!memcmp(&saved,&desk_configuration,sizeof(saved)) && saved_mode==desk_sleep_mode);
 assert(!memcmp(&saved_basis,&desk_basis,sizeof(saved_basis)));
 assert(desk_zone_known && desk_basis_known && portable_desk_clock_mode()==-3);
 shared_settings_force_busy=false;
 assert(desk_config(&desk_configuration,&desk_sleep_mode) && !desk_config_pending);
}
