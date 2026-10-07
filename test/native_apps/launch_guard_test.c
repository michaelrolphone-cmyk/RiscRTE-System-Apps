#define PORTABLE_APP_LAUNCH_GUARD
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_QUICK_FIXTURE_MAIN base_quick_fixture_main
#include "quick_adapter_test.c"
static bool allowed;
static unsigned guard_calls;
static bool refuse_request;
static bool guarded_launch(const char *target){if(refuse_request)return false;return quick_launch(target);}
static char guarded_target[64];
bool portable_app_before_launch(const char *destination) {
 assert(destination && !failed && !surface.frame && display_settled);
 assert(!handoff_requested && !quick_launch_pending);
 assert(strlen(destination)<sizeof(guarded_target));strcpy(guarded_target,destination);
 guard_calls++;return allowed;
}
int main(int argc,char **argv) {
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);setup();quick_runtime.request_launch=guarded_launch;
 if(test<2) {
  sv_page=SV_VALUE;settings_editing=true;
  if(test==0)home_pending=true;else crown_pending=true;
  t5_app_input_t in={0};assert(poll(&in,0));
  assert(guard_calls==1&&!strcmp(guarded_target,"default.elf"));
  assert(!in.exit_requested&&!in.buttons&&!in.tapped&&!return_launches&&!failed);
  assert(!home_pending&&!crown_pending&&!handoff_requested);
  for(unsigned n=0;n<5;n++){assert(poll(&in,0));assert(!in.exit_requested);}
  assert(guard_calls==1); /* no implicit retry of a vetoed gesture */
  allowed=true;
  if(test==0)home_pending=true;else crown_pending=true;
  assert(poll(&in,0)&&in.exit_requested&&return_launches==1&&guard_calls==2);
  assert(poll(&in,0)&&in.exit_requested&&return_launches==1&&guard_calls==2);
 } else if(test==2) {
  opening(3);tap(80,51,185);drain_to(160);background_unchanged();
  assert(guard_calls==1&&!strcmp(guarded_target,"wifi_settings.elf"));
  assert(!quick_launch_pending&&!handoff_requested&&!failed);
  allowed=true;opening(180);tap(260,51,185);t5_app_input_t in={0};
  while(!in.exit_requested)assert(poll(&in,8));
  assert(guard_calls==2&&wifi_launches==1&&!return_launches&&quick_launch_pending);
  assert(poll(&in,0)&&in.exit_requested&&wifi_launches==1&&guard_calls==2);
 } else if(test==3) {
  tap(3,20,20);t5_app_input_t in={0};
  while(guard_calls==0)assert(poll(&in,8));
  assert(!strcmp(guarded_target,"springboard.elf")&&!in.exit_requested&&!in.buttons&&!failed&&!return_launches);
  allowed=true;tap(polls+2,20,20);
  while(!in.exit_requested)assert(poll(&in,8));
  assert(guard_calls==2&&return_launches==1);
 } else if(test==4) {
  sv_page=SV_VALUE;settings_editing=true;allowed=true;refuse_request=true;
  home_pending=true;t5_app_input_t in={0};assert(poll(&in,0));
  assert(guard_calls==1&&!in.exit_requested&&!failed&&!return_launches&&!handoff_requested);
  assert(poll(&in,0)&&!in.exit_requested&&guard_calls==1);
  refuse_request=false;home_pending=true;assert(poll(&in,0)&&in.exit_requested);
  assert(guard_calls==2&&return_launches==1);
 } else if(test==5) {
  allowed=true;refuse_request=true;opening(3);tap(80,51,185);drain_to(160);
  assert(guard_calls==1&&!failed&&!wifi_launches&&!handoff_requested);
  refuse_request=false;opening(180);tap(260,51,185);t5_app_input_t in={0};
  while(!in.exit_requested)assert(poll(&in,8));
  assert(guard_calls==2&&wifi_launches==1);
 } else assert(!"unknown launch guard case");
 cleanup();puts("App-owned launch guard: one-shot veto, retry and terminal handoff PASS");
}
