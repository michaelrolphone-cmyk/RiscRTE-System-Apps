#define TEST_SETTINGS_CUSTODY
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#define PORTABLE_SETTINGS_NATIVE_ONLY
#define PORTABLE_SETTINGS_BOTH_TIMERS
#define main inherited_timer_tests
#include "settings_sleep_defaults_test.c"
#undef main
int main(int argc,char**argv){
 assert(argc==3);unsigned interruption=(unsigned)atoi(argv[2]);assert(interruption<5);unsigned kind=(unsigned)atoi(argv[1]);assert(kind<3);
 begin();retain_write=true;tap(3,kind?185:100,kind?105:80);tap(7,170,213);
#ifdef TEST_SETTINGS_ASYNC
 async_fail_pending=interruption==2;async_failed_pending=interruption==4;
 if(interruption==3){tap(9,170,213);tap(11,185,105);tap(13,170,213);}
 if(interruption==1)tap(9,60,213);
#else
 assert(!interruption);
#endif
 unsigned row=kind?SETTINGS_TIMER_ROW+kind-1:4;
 assert(settings_activate(0,row)==T5_APP_SETTING_NO_CHANGE);
 if(interruption==1||interruption==2||interruption==4){
  assert(!record_writes[kind]);
  if(interruption==1)assert(!failed&&!native_custody_retained);
  else assert(failed);
  if(interruption==2)assert(native_custody_retained&&retained_barriers==1);
  if(interruption==4)assert(native_custody_retained&&retained_barriers==1);
  app_module_fini();
  printf("PASS Settings key=%u async interruption=%u; no write before completed pending frame\n",kind,interruption);return 0;
 }
 assert(native_custody_retained&&failed&&retained_barriers==1&&record_writes[kind]==1);
 /* The last display was explicitly pending before unsafe write. No subsequent
  * frame/provider operation is permitted, including app fini. */
 unsigned presented=displays,held_grants=grants,held_subscriptions=subscriptions;
 app_module_fini();assert(displays==presented&&grants==held_grants&&subscriptions==held_subscriptions);
 printf("PASS Settings key=%u retained save; pending frame before write, no later provider/cleanup callbacks\n",kind);
}
