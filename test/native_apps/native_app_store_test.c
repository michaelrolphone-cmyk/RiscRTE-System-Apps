/* Native app boundary: real updater/renderer/adapter, synthetic hardware. */
#define PORTABLE_UPDATE_NATIVE_UI
#define PORTABLE_UPDATE_VERSION "1.4.0"
#define PORTABLE_NATIVE_CANVAS
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_NOVA_UI
#define PORTABLE_ASYNC_RGB565
#define PORTABLE_FORCE_FULL_FRAMES
#include <stdbool.h>
static bool native_retain(void),native_exit(void);
static void native_tick(void);
static unsigned native_retains,native_exit_at,native_change_at,native_present_delay,native_present_deadline;
static const char *native_release_version="1.1.0";
#include NATIVE_APP_STORE_FIXTURE
static bool native_retain(void){++native_retains;return true;}
static bool native_exit(void){return native_exit_at&&ticks>=native_exit_at;}
static void native_tick(void){if(native_change_at&&ticks>=native_change_at){native_change_at=0;native_release_version="1.1.1";}}
static void native_frame(void){render_update();assert(!failed);assert(paper_frame_drain());assert(portable_wifi_native_ready());assert(!dirty);}
static void bounds(wn_rect r){assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=UW&&r.y+r.h<=UH);}
static void tap(unsigned after,wn_rect r){bounds(r);event(input_step()+after,0,r.x+r.w/2,r.y+r.h/2);}
static void native_list(unsigned n){mock_update.count=n;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;dirty=true;}
static void choose(void){native_list(3);selected=1;native_frame();assert(un_get(&selected_row));confirming=true;confirm_choice=1;un_prepare();native_frame();}
static void finish(void){fail_disconnect=fail_release=fail_cancel=false;http_retained=false;native_present_delay=0;assert(portable_update_close());app_module_fini();assert(!opened&&!grant_count&&!sub_count&&!frame_count&&!native_active&&!writes&&!native_retains);}
int main(int argc,char **argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);start_fixture();limit_polls=3000;
 if(which==0){
  assert(wn_fits());bounds(wn_back());for(unsigned i=0;i<3;i++)bounds(wn_footer(i));native_list(31);
  for(unsigned i=0;i<32;i++){selected=i;un_prepare();native_frame();unsigned local=i%16,visible=local%wn_rows();wn_rect r=wn_row(visible);bounds(r);assert(portable_wifi_hit(&uv,r.x+r.w/2,r.y+r.h/2)==(int)local);}
  choose();assert(!strcmp(uv.rows[1],"Install and restart"));confirming=false;connecting=true;native_frame();assert(!strcmp(uv.rows[0],"Cancel operation"));connecting=false;
  cleanup_pending=true;native_frame();assert(!strcmp(uv.rows[0],"Retry cleanup"));cleanup_pending=false;restart_pending=true;native_frame();assert(!strcmp(uv.rows[0],"Restart device"));restart_pending=false;
 }else if(which==1){
  native_frame();assert(portable_update_close());native_list(3);tap(12,wn_footer(2));tap(28,wn_footer(1));tap(44,wn_footer(2));tap(60,wn_footer(0));tap(76,wn_footer(1));tap(92,wn_back());app_main();assert(launches==1&&!begins&&!connects&&!opened);
  script_count=0;tap(12,wn_back());app_main();assert(launches==2&&!opened);
 }else if(which==2){
  choose();assert(un_model_current());native_release_version="1.1.1";assert(!un_selected_current());assert(!confirming&&!selected&&!begins&&!connects);
 }else if(which==3){
  native_list(3);selected=1;native_frame();native_release_version="1.1.1";un_prepare();assert(!un_model_valid&&!selected&&!begins&&!connects);native_frame();assert(!strcmp(uv.values[1],"1.1.1  Update"));
 }else if(which==4){
  choose();assert(connect_saved(true));native_release_version="1.1.1";fake_link=WIFI_LINK_UP;update_tick();assert(!begins&&!native_active&&!connecting&&!confirming);
 }else if(which==5){
  choose();assert(connect_saved(true));fake_link=WIFI_LINK_UP;update_tick();assert(begins==1&&!connecting);mock_update.state=SOFTWARE_UPDATE_READY;update_tick();assert(activations==1&&restart_pending&&!native_active);restart_pending=false;
 }else if(which==6){
  native_frame();assert(portable_update_close());native_present_delay=500;tap(4,wn_footer(1));event(input_step()+36,T5_APP_BUTTON_BACK,-1,0);app_main();assert(!checks&&!connects&&!begins&&launches==1);
 }else if(which==7){
  native_list(3);native_frame();tap(2,wn_footer(2));t5_app_input_t in;unsigned start=ticks;while(ticks<start+75)assert(ua->poll(&in,25));assert(wn_gesture);confirming=true;un_prepare();native_frame();while(ticks<start+250){assert(ua->poll(&in,25));assert(!in.tapped);}confirming=false;
 }else if(which==8){
  native_frame();assert(portable_update_close());native_present_delay=600;event(input_step()+3,T5_APP_BUTTON_BACK,-1,0);app_main();assert(!opened&&!begins&&launches==1);
 }else if(which==9){
  native_list(3);native_frame();tap(2,wn_footer(2));t5_app_input_t in;unsigned start=ticks;while(ticks<start+75)assert(ua->poll(&in,25));assert(wn_gesture);message="Status refreshed";dirty=true;native_frame();unsigned taps=0;while(ticks<start+250){assert(ua->poll(&in,25));taps+=in.tapped;}assert(taps==1);
 }else if(which==10){
  native_frame();assert(portable_update_close());wn_rect r=wn_footer(0);event(input_step()+12,0,r.x+r.w+1,r.y+r.h/2);tap(32,wn_back());app_main();assert(!checks&&!connects&&!begins&&launches==1);
 }else if(which==11){
  choose();assert(portable_update_suspend());assert(!confirming&&!un_model_valid);portable_update_resume();native_frame();assert(!begins&&!connects);
 }else if(which==12){
  connect_fixture(false);assert(checks==1);assert(portable_update_suspend());assert(!native_active&&!connecting&&!cleanup_pending);native_frame();assert(!strcmp(uv.rows[0],"Check / retry"));
 }else if(which==13){
  choose();assert(connect_saved(true));native_frame();assert(portable_update_close());assert(!begins&&!activations&&!native_active);assert(open_update());native_frame();assert(selected==0&&!confirming);
 }else if(which==14){
  native_list(3);native_frame();assert(portable_update_close());native_present_delay=500;event(input_step()+2,T5_APP_BUTTON_DOWN,-1,0);event(input_step()+4,T5_APP_BUTTON_CONFIRM,-1,0);event(input_step()+36,T5_APP_BUTTON_BACK,-1,0);app_main();assert(!begins&&!connects&&launches==1);
 }else if(which==15){
  choose();assert(connect_saved(true));native_exit_at=ticks+75;app_main();assert(!begins&&!activations&&!opened&&!native_active);
 }else if(which==16){
  native_list(3);selected=1;native_frame();native_present_delay=500;native_change_at=ticks+100;dirty=true;native_frame();assert(!un_model_current());assert(!begins&&!connects&&!confirming);
 }else if(which==17){
  choose();mock_update.count=0;ust=mock_update;assert(!un_selected_current());assert(!confirming&&!begins&&!connects);
 }else if(which==18){
  connect_fixture(false);fail_cancel=true;unsigned calls=service_calls;assert(!portable_update_suspend());assert(native_retains==1&&portable_adapter_retained());app_main();app_module_fini();assert(service_calls==calls&&native_active&&grant_count);return 0;
 }else if(which==19){
  connect_fixture(false);fail_disconnect=1;unsigned calls=service_calls;assert(!portable_update_suspend());assert(native_retains==1&&portable_adapter_retained());app_main();app_module_fini();assert(service_calls==calls&&native_active&&grant_count);return 0;
 }else if(which==20){
  choose();fail_service_status=true;un_prepare();assert(cleanup_pending&&dirty);native_frame();assert(!strcmp(uv.rows[0],"Retry cleanup"));fail_service_status=false;assert(portable_update_suspend());native_frame();assert(!cleanup_pending&&!begins);
 }else if(which==21){
  choose();assert(connect_saved(true));fake_link=WIFI_LINK_UP;update_tick();assert(begins==1);mock_update.state=SOFTWARE_UPDATE_ERROR;update_tick();assert(!native_active&&!activations&&!restart_pending);native_frame();
 }else if(which==22){
  choose();assert(connect_saved(true));fake_link=WIFI_LINK_UP;update_tick();unknown_activation=true;mock_update.state=SOFTWARE_UPDATE_READY;update_tick();assert(!activations&&restart_pending&&!native_active);native_frame();assert(!portable_update_suspend());restart_pending=false;
 }else if(which==23){
  native_frame();unsigned before=grant_count;assert(open_update());assert(grant_count==before);assert(portable_update_close());assert(open_update());native_frame();assert(grant_count==before&&!confirming&&!begins);
 }else assert(false);
 finish();printf("Native App Store %ux%u case%u passed\n",UW,UH,which);return 0;
}
