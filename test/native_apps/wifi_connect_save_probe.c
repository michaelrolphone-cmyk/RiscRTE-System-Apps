/* Baseline-compatible reproducer: a successful connection must be remembered. */
#define PORTABLE_WIFI_REMEMBER_CONNECTED
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
int main(void){
 workflow_start(true);draft();wifi_connect();
 progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;tick(250);
 for(unsigned i=0;i<10;++i)wifi_profiles_step();
 portable_wifi_credentials saved={{0},{0}};
 assert(portable_wifi_profile_load(wifi_store,0,&saved)==PORTABLE_WIFI_CREDENTIALS_LOADED);
 assert(!memcmp(&saved,&credentials,sizeof(saved)));portable_wifi_credentials_clear(&saved);
 puts("Successful connection persisted PASS");return 0;
}
