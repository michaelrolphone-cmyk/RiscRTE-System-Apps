/* Actual adapter boundaries with local-bridge custody spies. The separate
 * Watch bridge matrix must link its real implementation before deployment. */
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define PORTABLE_NAVIGATION_RETENTION
#define PORTABLE_SETTINGS_NAVIGATION_OVERRIDE
#define OPTIONAL_SERVICE_FIXTURE_MAIN unused_optional_main
#include "optional_service_adapter_test.c"
static unsigned hook_fault,hook_calls;
static bool hook_retained,hook_closing,hook_ready;
static bool hook_poll(void*c,risc_input_navigation_frame_v1*out){
 (void)c;assert(!fenced);hook_calls++;*out=(risc_input_navigation_frame_v1){0};
 if(hook_fault==4){hook_retained=true;return false;}return true;
}
static bool hook_foreground(void*c,const risc_input_foreground_v1*f,size_t n){
 (void)c;(void)f;assert(!fenced);hook_calls++;hook_closing=!n;
 if((hook_fault==2&&n)||(hook_fault==5&&!n)){hook_retained=true;return false;}return true;
}
static bool hook_reset(void*c){
 (void)c;assert(!fenced);hook_calls++;
 if(hook_fault==3||(hook_fault==6&&hook_closing)||((hook_fault==8||hook_fault==9)&&hook_ready)){hook_retained=true;return false;}return true;
}
static const risc_input_navigation_api_v1 hook_api={1,sizeof(hook_api),NULL,hook_poll,hook_foreground,hook_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*runtime){
 assert(runtime&&!fenced);hook_calls++;hook_closing=false;
 if(hook_fault==1){hook_retained=true;return NULL;}return &hook_api;
}
void portable_input_navigation_close(const risc_runtime_api_v1*runtime){
 assert(runtime&&!fenced);hook_calls++;if(hook_fault==7)hook_retained=true;
}
bool portable_input_navigation_retained(void){return hook_retained;}
int main(int argc,char**argv){
 assert(argc==2);hook_fault=(unsigned)atoi(argv[1]);configure();
 int result=app_module_init();
 if(hook_fault>=1&&hook_fault<=3)assert(result==-1);
 else {
  assert(result==0);hook_ready=true;
  if(hook_fault==4)input_service();
  else if(hook_fault==8)input_navigation_reset();
  else if(hook_fault==9){clear();present(false);alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};bool consumed=false;assert(!alarm_foreground(&consumed));}
  else app_module_fini();
 }
 if(hook_fault){
  assert(hook_retained);check_fenced();unsigned calls=hook_calls;
  input_navigation_reset();input_navigation_close();assert(!input_navigation_open());
  assert(hook_calls==calls);
 }else{
  assert(!fenced&&!grants&&!subscriptions&&!frame_count);
  for(unsigned i=0;i<2;i++){assert(app_module_init()==0);app_module_fini();assert(!grants&&!subscriptions&&!frame_count);}
 }
 printf("Local navigation retained-query adapter boundary %u PASS\n",hook_fault);return 0;
}
