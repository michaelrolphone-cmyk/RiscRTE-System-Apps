#define TEST_PAPER_UPDATE
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_NOVA_UI
#define main watch_main
#include "portable_update_app_test.c"
#undef main
int main(int argc,char **argv) {
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);
 /* Reuse faults against the real 480x800 renderer and adapter. */
 if(test<38&&test!=26&&test!=27&&test!=34&&test!=37)return watch_main(argc,argv);
 start_fixture();assert(up&&width()==480&&height()==800);if(test==43||test==44){
  if(test==43){connect_fixture(false);fail_disconnect=1;}else {restart_pending=true;ust.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;}
  render_update();event(poll_count+3,RISC_NAV_HOME,-1,0);
  for(unsigned i=0;i<5;i++){t5_app_input_t input={0};assert(poll(&input,25));}
  assert(!launches&&opened);fail_disconnect=0;restart_pending=false;mock_update.state=SOFTWARE_UPDATE_LIST;
  event(poll_count+3,RISC_NAV_HOME,-1,0);
  for(unsigned i=0;i<5;i++){t5_app_input_t input={0};assert(poll(&input,25));}
  assert(launches==1&&!strcmp(last_launch,"default.elf")&&!opened&&!begins&&!activations);
  app_module_fini();assert(!grant_count&&!sub_count&&!frame_count);return 0;
 }
 assert(portable_update_close());
 mock_update.count=6;mock_update.state=SOFTWARE_UPDATE_LIST;
 if(test==38){event(2,0,100,240);event(6,0,400,730);event(10,T5_APP_BUTTON_BACK,-1,0);}
 else if(test==39){event(2,T5_APP_BUTTON_DOWN,-1,0);event(4,T5_APP_BUTTON_CONFIRM,-1,0);event(6,RISC_NAV_HOME,-1,0);}
 else if(test==40){event(2,T5_APP_BUTTON_CONFIRM,-1,0);event(6,RISC_NAV_HOME,-1,0);}
 else if(test==41){event(2,0,100,240);event(6,0,280,730);event(10,0,100,250);event(14,0,40,730);event(18,0,40,730);}
 else if(test==45||test==46){event(2,0,200,20);event(3,0,200,100);if(test==45){event(7,0,340,450);event(12,T5_APP_BUTTON_BACK,-1,0);event(18,T5_APP_BUTTON_BACK,-1,0);}else event(7,RISC_NAV_HOME,-1,0);}
 else if(test==42){mock_update.state=SOFTWARE_UPDATE_ERROR;event(2,T5_APP_BUTTON_CONFIRM,-1,0);event(6,T5_APP_BUTTON_BACK,-1,0);}
 else assert(!"unknown paper scenario");
 app_main();if(test!=40&&test!=42)assert(!connects&&!checks);assert(!strcmp(last_launch,(test==39||test==40||test==46)?"default.elf":"springboard.elf"));assert(!opened&&launches==1&&!begins&&!activations&&!native_active);
 app_module_fini();assert(!grant_count&&!sub_count&&!frame_count&&!writes);
 puts("Paper update: navigation never installs; cleanup before handoff passed");
}
