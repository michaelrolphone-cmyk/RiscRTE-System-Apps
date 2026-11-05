#define PORTABLE_FRONTLIGHT_TONE
#define PORTABLE_SETTINGS_AVAILABILITY
#define main automatic_fixture_main
#include "home_automatic_test.c"
#undef main
static uint32_t status=RISC_KEY_VALUE_UNAVAILABLE;
static uint64_t status_generation=1;
static unsigned status_acquires,status_releases;
static int32_t snapshot(void *ctx,risc_key_value_availability_state_v1 *out) {
 (void)ctx;io();*out=(risc_key_value_availability_state_v1){sizeof(*out),1,status,0,status_generation};
 if(is("bad-status"))out->namespace_id=6;
 return 0;
}
static const risc_key_value_availability_v1 api={1,sizeof(api),NULL,snapshot};
static bool status_acquire(const char *name,uint32_t version,uint64_t ns,risc_runtime_capability_v1 *out) {
 if(strcmp(name,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY))return acquire(name,version,ns,out);
 io();assert(version==1&&ns==1);++status_acquires;
 *out=(risc_runtime_capability_v1){sizeof(*out),3,1,&api};return true;
}
static bool status_release(risc_runtime_capability_v1 *grant) {
 if(grant->slot==3) {
  ++status_releases;if(is("release-failure"))return false;
  if(is("dirty-release"))return true;
 }
 return release(grant);
}
static const risc_runtime_api_v1 status_runtime={.api_version=1,.struct_size=sizeof(status_runtime),.acquire=status_acquire,.release=status_release,.yield_ms=yield};
int main(int argc,char **argv) {
 assert(argc==2);test=argv[1];rt=&status_runtime;
 if(is("pending"))status=RISC_KEY_VALUE_RECONCILIATION_PENDING;
 if(is("conflict"))status=RISC_KEY_VALUE_CONFLICT;
 if(is("available"))status=RISC_KEY_VALUE_AVAILABLE;
 bool ok=portable_home_settings_poll();
 if(is("bad-status")||is("release-failure")||is("dirty-release")) {
  assert(!ok&&native_custody_retained&&home_settings_status.grant.api&&!submits);
  unsigned before=calls;assert(!portable_home_settings_poll()&&before==calls);puts(test);return 0;
 }
 assert(ok&&!home_settings_status.grant.api&&status_acquires==status_releases);
 if(is("regression")) {
  status_generation=0;assert(!portable_home_settings_poll()&&native_custody_retained);puts(test);return 0;
 }
 if(is("available")) {
  assert(portable_home_settings_available()&&!*portable_home_settings_notice());
  assert(home_automatic_begin(PORTABLE_AUTOMATIC_LOW_BATTERY)&&acquires==1);
  assert(home_automatic_close());
 } else {
  assert(!portable_home_settings_available()&&*portable_home_settings_notice());
  quick.ui.brightness_valid=quick.ui.volume_valid=quick.ui.tone_valid=true;
  quick.ui.dnd_valid=quick.ui.radios_valid=quick.ui.contexts_valid=true;
  home_settings_readonly_controls();assert(!quick.ui.brightness_valid&&!quick.ui.tone_valid&&!quick.ui.radios_valid);
  assert(home_settings_filter_actions(PQA_BRIGHTNESS_PREVIEW|PQA_TONE_COMMIT|PQA_DND|PQA_USB_TRANSFER|PQA_CLEAN_REFRESH|PQA_TORCH)==(PQA_USB_TRANSFER|PQA_CLEAN_REFRESH|PQA_TORCH));
  assert(!home_automatic_begin(PORTABLE_AUTOMATIC_LOW_BATTERY)&&!acquires&&!submits&&!legacy_writes);
  assert(home_automatic_close());
  if(is("recovery")) {
   status=RISC_KEY_VALUE_AVAILABLE;++status_generation;assert(portable_home_settings_poll());
   assert(portable_home_settings_available()&&portable_home_settings_generation()==2);
   assert(home_automatic_begin(PORTABLE_AUTOMATIC_LOW_BATTERY)&&acquires==1);assert(home_automatic_close());
  }
 }
 assert(!native_custody_retained&&!home_automatic_owned()&&!home_settings_status.grant.api);
 printf("Home settings availability %s passed\n",test);return 0;
}
