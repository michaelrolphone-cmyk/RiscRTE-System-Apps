#include "native_system_app_entry.c"
const char *cross_app_message(void){return wifi_message;}

void cross_app_tick(void){last_status=wa->millis()-250u;wifi_tick();}
void cross_app_render(void){wifi_make_view();portable_wifi_render(&view);}
void cross_app_cancel(void){(void)wifi_cancel_scan();}
bool cross_app_scanning(void){return scanning;}
bool cross_app_scan_owned(void){return scan_owned;}
unsigned cross_app_scan_count(void){return scan_result.count;}
