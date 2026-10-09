/* Compile the unchanged app controller separately from the adapter fixture.
 * The test-only opening hook permits Runtime-finalization coverage with live
 * app-owned grants, without replacing the production opening/closing logic. */
#ifdef PORTABLE_FILE_BROWSER_APP
#include "../../Apps/file_browser.c"
bool native_system_test_open(void){return fb_open();}
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
#include "../../Apps/wifi_settings.c"
bool native_system_test_open(void){return wifi_open();}
#else
#include "../../Apps/springboard.c"
bool native_system_test_open(void){return true;}
#endif
