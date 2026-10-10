/* Controller observations only: all input/actions run through app_main. */
#include "../../Apps/wifi_settings.c"
bool native_system_test_open(void){return wifi_open();}
unsigned scroll_wifi_page(void){return page;}
const char *scroll_wifi_ssid(void){return credentials.ssid;}
bool scroll_wifi_cleanup(void){return cleanup_pending;}
