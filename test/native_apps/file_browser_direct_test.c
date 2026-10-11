#define FILE_SETUP_TEST
#define FILE_DIRECT_FIXTURE
#include "file_browser_sharing_test.c"
int main(int argc,char**argv){
 const char*mode=argc>1?argv[1]:"complete";
 empty=true;init();assert(!store_count);fsh_direct=true;fsh_action();
 for(unsigned i=0;i<80&&!setup_owned;i++)step();
 assert(setup_owned&&!listeners&&!begin_calls&&!store_count);
 setup_state=RISC_SETUP_PAIR_CONFIRM;step();
 fsh_setup_choice(true);assert(!setup_confirms); /* Number must be displayed. */
 setup_draw_and_wait();fsh_setup_display_sync();assert(fsh_setup_display_generation);
 if(!strcmp(mode,"cancel")){(void)fsh_back();}
 else if(!strcmp(mode,"expiry")){test_ticks+=300001;step();}
 else {
  fsh_setup_choice(strcmp(mode,"refuse")!=0);
  if(!strcmp(mode,"refuse")){for(unsigned i=0;i<30&&fsh_phase!=FSH_OFF;i++)step();assert(!begin_calls&&!listeners);}
  else {
   for(unsigned i=0;i<80&&!listeners;i++)step();
   assert(listeners==1&&begin_calls==1&&fsh_private_started&&!strcmp(fsh_url,"http://192.168.4.1:8080/"));
   assert(!store_count); /* No router profile and no persistent credential write. */
   if(!strcmp(mode,"active-expiry")){test_ticks+=300001;step();}
   else (void)fsh_back();
  }
 }
 for(unsigned i=0;i<100&&fsh_phase!=FSH_OFF;i++){if(cancel_calls)quiescent=true;step();}
 assert(fsh_phase==FSH_OFF&&!listeners&&!owned&&!setup_owned&&!store_count&&!retains);
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 puts("Actual Files direct flow: no saved network, displayed mutual confirmation, AP then WebDAV, cancellation/expiry and cleanup PASS");
 return 0;
}
