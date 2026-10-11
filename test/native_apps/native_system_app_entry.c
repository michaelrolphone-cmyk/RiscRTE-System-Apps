/* Compile the unchanged app controller separately from the adapter fixture.
 * The test-only opening hook permits Runtime-finalization coverage with live
 * app-owned grants, without replacing the production opening/closing logic. */
#ifdef PORTABLE_FILE_BROWSER_APP
#include "../../Apps/file_browser.c"
bool native_system_test_open(void){return fb_open();}
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
#include "../../Apps/wifi_settings.c"
bool native_system_test_open(void){return wifi_open();}
void native_system_test_wifi_request(bool scan){
 strcpy(credentials.ssid,"Fixture network");strcpy(credentials.password,"testpass123");
 if(scan)wifi_scan_start();else wifi_connect();
}
#elif defined(PORTABLE_UPDATE_APP)
#if PORTABLE_UPDATE_FIRMWARE
#include "../../Apps/ota_update.c"
#else
#include "../../Apps/app_store.c"
#endif
bool native_system_test_open(void){return open_update();}
bool native_update_test_utc(uint64_t *out){return utc_now(out);}
bool native_update_test_check(void){return connect_saved(false);}
#else
#include "../../Apps/springboard.c"
bool native_system_test_open(void){return true;}
#endif
#ifdef TEST_NATIVE_APP_CUSTODY
bool native_custody_test_close(void){
#ifdef PORTABLE_FILE_BROWSER_APP
 return portable_file_browser_close();
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
 return portable_wifi_close();
#else
 return portable_update_close();
#endif
}
void native_custody_test_action(unsigned action){
#ifdef PORTABLE_FILE_BROWSER_APP
 if(action==0)(void)fbx_copy(fb_volume,"/source",fb_volume,"/target");
 if(action==5){(void)portable_file_browser_safe();fb_error("retained");}
 if(action==6){if(!fbx_handlers())return;fb_handler_count=1;fb_handler_offset=0;fb_handlers[0].kind=T5_FILE_HANDLER_APP;strcpy(fb_handlers[0].app_id,"fixture.elf");strcpy(fb_handler_path,"/sd/file");fbx_dispatch(0);}
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
 if(action==0)native_system_test_wifi_request(false);
 if(action==2)wifi_activate(6);
 if(action==3)native_system_test_wifi_request(true);
 if(action==4)(void)wifi_cancel_scan();
 if(action==5){portable_wifi_resume();(void)portable_wifi_services_safe();wifi_forget();}
#else
 if(action==0)(void)connect_saved(false);
 if(action==1)update_tick();
 if(action==5){portable_update_resume();(void)portable_update_services_safe();(void)connect_saved(false);}
#endif
}
#endif
