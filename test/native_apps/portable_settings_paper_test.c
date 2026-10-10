#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_INPUT_NAVIGATION
#define TEST_PAPER_SETTINGS
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_NOVA_UI
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_ALARM_SETTINGS
#define main legacy_fixture_main
#include "portable_settings_test.c"
#undef main
int main(int argc,char **argv) {
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);scenario=200+test;
 assert(app_module_init()==0);assert(width()==480&&height()==800&&pp_enabled());settings_render(0,0);
 if(test==0){assert(displays==1);}
 else if(test==1||test==2||test==3||test==8||test==9) {
  tap(3,200,300);tap(7,test==2?100:350,730);
  if(test==3){format_write_error=true;tap(11,100,730);}
  if(test==8)gap_at=4;
  if(test==9)fail_poll_at=4;
  uint8_t result=settings_activate(0,2);
  assert((result==T5_APP_SETTING_UPDATED)==(test==1||test==8||test==9));
  assert(format_writes==(test==1||test==3?1u:0u));assert(!writes);
  if(test==1){unsigned mode;assert(portable_time_format_load(&kv_api,&mode)==PORTABLE_TIME_FORMAT_LOADED&&mode==PORTABLE_TIME_FORMAT_24);}
 } else if(test==4||test==5) {
  tap(3,100,150);tap(7,380,280);tap(11,240,730);tap(15,test==5?100:350,730);
  uint8_t result=settings_activate(0,0);assert((result==T5_APP_SETTING_UPDATED)==(test==4));
  assert(writes==(test==4?1u:0u));assert(!format_writes);
  if(test==4)assert(stored.year==2025&&stored.month==2&&stored.day==28);
 } else if(test==6) {
  tap(1,100,150);tap(2,100,150);tap(3,100,150);tap(7,100,730);
  assert(settings_activate(0,0)==T5_APP_SETTING_NO_CHANGE);assert(!writes&&!format_writes);
 } else if(test==7) {
  tap(3,100,140);tap(4,100,280);tap(8,100,730);app_main();assert(!writes&&!format_writes);
 } else if(test==10) {
  tap(3,240,730);assert(settings_activate(0,SETTINGS_ABOUT_ROW)==T5_APP_SETTING_NO_CHANGE);assert(!writes&&!format_writes);
 } else if(test==12||test==13) {
  tap(3,200,300);tap(7,test==13?100:350,730);
  uint8_t result=settings_activate(0,4);assert((result==T5_APP_SETTING_UPDATED)==(test==12));assert(kv_writes==(test==12?1u:0u));assert(!writes&&!format_writes);
 } else if(test==16){tap(3,100,600);tap(4,100,180);tap(8,100,730);app_main();assert(sp_first==6&&!writes&&!format_writes);}
 else if(test==17){tap(3,200,300);tap(7,240,730);tap(11,100,730);assert(settings_activate(0,2)==T5_APP_SETTING_NO_CHANGE);assert(!format_writes&&!writes);}
 else if(test==18||test==19){
  tap(3,100,140);app_main();assert(return_launches==1&&polls==9&&!writes&&!format_writes);
  unsigned before=displays;t5_app_input_t terminal={0};assert(poll(&terminal,20)&&terminal.exit_requested);
  settings_render(0,0);assert(displays==before&&return_launches==1);
 }
 else if(test==15){assert(failed);}
 else assert(!"Unknown paper scenario");
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
 printf("Paper Settings scenario %u: explicit saves, draft cancellation and clean teardown pass\n",test);
}
