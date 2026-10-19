/* Production Firmware Update controller, native renderer and adapter.
 * Providers are bounded fakes: no hardware, radio, network or bank writes.
 * Cases 0-23 preserve the shared native App Store coverage; firmware-specific
 * cases strengthen provider authority, raw input, raster and checked custody. */
#define PORTABLE_UPDATE_FIRMWARE 1
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
static const char *native_release_version="1.1.0",*native_release_id="firmware",*native_installed="1.0.0",*native_reason="Update available";
static unsigned native_availability,native_size=1024,native_malformed,native_ready_at;
static unsigned native_firmware_acquires,native_apps_acquires,native_callbacks,native_update_calls;
static unsigned native_pending,native_frame_acquires,native_frame_releases,native_status_calls,native_present_fault;
static bool native_get_failure,native_deny_firmware;
#include NATIVE_FIRMWARE_UPDATE_FIXTURE
static bool native_retain(void){++native_retains;return true;}
static bool native_exit(void){return native_exit_at&&ticks>=native_exit_at;}
static void native_tick(void){if(native_ready_at&&ticks>=native_ready_at){native_ready_at=0;mock_update.state=SOFTWARE_UPDATE_READY;}if(native_change_at&&ticks>=native_change_at){native_change_at=0;native_release_version="1.1.1";}}
static void native_frame(void){render_update();assert(!failed);assert(paper_frame_drain());assert(portable_wifi_native_ready());assert(!dirty);assert(!native_pending&&!paper_token&&!frame_count);assert(!strcmp(uv.title,"FIRMWARE UPDATE"));}
static void bounds(wn_rect r){assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=UW&&r.y+r.h<=UH);}
static void tap(unsigned after,wn_rect r){bounds(r);event(input_step()+after,0,r.x+r.w/2,r.y+r.h/2);}
static void native_list(unsigned n){mock_update.count=n;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;dirty=true;}
static void choose(void){native_list(3);selected=1;native_frame();assert(un_get(&selected_row));confirming=true;confirm_choice=1;un_prepare();native_frame();}
static void retained(void){
 assert(native_retains==1&&portable_adapter_retained());
 unsigned grants=grant_count,frames=frame_count,pending=native_pending,calls=native_callbacks,updates=native_update_calls;
 app_main();app_module_fini();assert(!portable_update_suspend()&&!portable_update_close());portable_update_resume();
 assert(grant_count==grants&&frame_count==frames&&native_pending==pending);
 assert(native_callbacks==calls&&native_update_calls==updates&&!activations&&!restarts&&!launches);
}
static unsigned ink(wn_rect r){unsigned n=0;for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)n+=pixels[y*UW+x]!=0;return n;}
static int expected_hit(int x,int y){
 if(wn_inside(wn_back(),x,y))return WIFI_HIT_BACK;
 for(unsigned i=0;i<3;i++)if(wn_inside(wn_footer(i),x,y))return i==0?WIFI_HIT_PREVIOUS:i==1?WIFI_HIT_SELECT:WIFI_HIT_NEXT;
 unsigned rows=wn_rows(),first=uv.selected/rows*rows;
 for(unsigned i=0;i<rows&&first+i<uv.count;i++)if(wn_inside(wn_row(i),x,y))return (int)(first+i);
 return -1;
}
static void finish(void){fail_disconnect=fail_release=fail_cancel=false;http_retained=false;native_present_delay=0;assert(portable_update_close());app_module_fini();assert(!opened&&!grant_count&&!sub_count&&!frame_count&&!native_active&&!writes&&!native_retains&&!native_pending);assert(native_firmware_acquires&&!native_apps_acquires);}
int main(int argc,char **argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);start_fixture();limit_polls=3000;assert(native_firmware_acquires==1&&!native_apps_acquires);
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
  connect_fixture(false);fail_cancel=true;unsigned calls=service_calls;assert(!portable_update_suspend());assert(native_retains==1&&portable_adapter_retained());retained();assert(service_calls==calls&&native_active&&grant_count);return 0;
 }else if(which==19){
  connect_fixture(false);fail_disconnect=1;unsigned calls=service_calls;assert(!portable_update_suspend());assert(native_retains==1&&portable_adapter_retained());retained();assert(service_calls==calls&&native_active&&grant_count);return 0;
 }else if(which==20){
  choose();fail_service_status=true;un_prepare();assert(cleanup_pending&&dirty);native_frame();assert(!strcmp(uv.rows[0],"Retry cleanup"));fail_service_status=false;assert(portable_update_suspend());native_frame();assert(!cleanup_pending&&!begins);
 }else if(which==21){
  choose();assert(connect_saved(true));fake_link=WIFI_LINK_UP;update_tick();assert(begins==1);mock_update.state=SOFTWARE_UPDATE_ERROR;update_tick();assert(!native_active&&!activations&&!restart_pending);native_frame();
 }else if(which==22){
  choose();assert(connect_saved(true));fake_link=WIFI_LINK_UP;update_tick();unknown_activation=true;mock_update.state=SOFTWARE_UPDATE_READY;update_tick();assert(!activations&&restart_pending&&!native_active);native_frame();assert(!portable_update_suspend());restart_pending=false;
 }else if(which==23){
  native_frame();unsigned before=grant_count;assert(open_update());assert(grant_count==before);assert(portable_update_close());assert(open_update());native_frame();assert(grant_count==before&&!confirming&&!begins);
 }else if(which==24){
  /* Native raster includes actual header glyphs, the back icon, row glyphs and
   * footer glyphs. Every screen/gutter coordinate uses the same hit rectangles. */
  native_list(31);selected=17;native_frame();wn_layout g=wn_geometry();wn_rect b=wn_back();
  assert(ink((wn_rect){b.x+4,b.y+(b.h-16)/2,16,16})>8);
  assert(ink((wn_rect){g.margin+30,0,g.w-2*g.margin-30,g.header-1})>20);
  wn_rect row=wn_row(uv.selected%wn_rows());assert(ink((wn_rect){row.x+8,row.y+1,row.w-12,12})>10);
  for(unsigned i=0;i<3;i++){wn_rect r=wn_footer(i);assert(ink((wn_rect){r.x+3,r.y+3,r.w-6,r.h-6})>8);}
  for(int y=-1;y<=UH;y++)for(int x=-1;x<=UW;x++)assert(portable_wifi_hit(&uv,x,y)==expected_hit(x,y));
 }else if(which>=25&&which<=30){
  choose();
  switch(which){case 25:native_release_id="other-firmware";break;case 26:native_installed="1.0.1";break;case 27:native_reason="Blocked cohort";break;case 28:native_availability=SOFTWARE_UPDATE_USB_ONLY;break;case 29:++native_size;break;case 30:native_get_failure=true;break;}
  assert(!un_selected_current()&&!confirming&&!selected&&!begins&&!connects);native_get_failure=false;
 }else if(which>=31&&which<=34){
  choose();native_malformed=which-30;assert(!un_selected_current()&&!confirming&&!selected&&!begins&&!connects);native_malformed=0;
 }else if(which==35){
  /* Back arrives in the same poll in which verification becomes READY. */
  choose();connect_fixture(true);mock_update.state=SOFTWARE_UPDATE_VERIFYING;ust=mock_update;native_frame();
  native_ready_at=(input_step()+4)*25;event(input_step()+4,T5_APP_BUTTON_BACK,-1,0);app_main();
  assert(begins==1&&!activations&&!opened&&!native_active&&launches==1&&!native_ready_at);
 }else if(which==36){
  /* The raw Cancel tap release owns the VERIFYING -> READY boundary. */
  choose();connect_fixture(true);mock_update.state=SOFTWARE_UPDATE_VERIFYING;ust=mock_update;native_frame();
  native_ready_at=(input_step()+16)*25;tap(12,wn_footer(1));tap(30,wn_back());app_main();
  assert(begins==1&&!activations&&!opened&&!native_active&&launches==1&&!native_ready_at);
 }else if(which==37){
  native_frame();native_present_delay=500;dirty=true;render_update();
  assert(!portable_wifi_native_ready());
  unsigned acquired=native_frame_acquires;t5_app_input_t in;
  for(unsigned i=0;i<4;i++){assert(ua->poll(&in,25));assert(!in.tapped&&!portable_wifi_native_ready());}
  assert(native_frame_acquires<=acquired+1);assert(paper_frame_drain());assert(native_status_calls&&!native_pending&&portable_wifi_native_ready());
 }else if(which>=38&&which<=41){
  native_frame();native_present_delay=500;native_present_fault=which-37;dirty=true;render_update();
  assert(!paper_frame_drain());assert(native_pending&&paper_token&&!frame_count);retained();return 0;
 }else if(which==42){
  native_frame();fail_display=1;dirty=true;render_update();(void)paper_frame_drain();
  assert(frame_count&&!native_pending);retained();return 0;
 }else if(which==43){
  choose();assert(connect_saved(true));fail_release=1;assert(!portable_update_suspend());
  assert(!native_active&&uwg.api&&cleanup_pending&&!begins&&!activations);
  fail_release=0;assert(portable_update_suspend());assert(!uwg.api&&!cleanup_pending);native_frame();
 }else if(which==44){
  connect_fixture(false);http_retained=true;assert(!portable_update_suspend());assert(native_active&&usg.api&&uwg.api);retained();return 0;
 }else if(which==45){
  native_frame();assert(portable_update_close());native_deny_firmware=true;assert(!open_update());assert(!us&&!native_apps_acquires);retained();return 0;
 }else if(which>=46&&which<=48){
  native_frame();assert(portable_update_close());
  if(which==46)update_api.api_version=2;else if(which==47)update_api.struct_size=sizeof(update_api)-1;else update_api.activate=NULL;
  assert(!open_update());assert(!us&&!native_apps_acquires);retained();return 0;
 }else if(which==49){
  /* A status change invalidates catalog input but a message-only redraw in
   * case 9 preserves an already-owned gesture. */
  native_list(3);native_frame();tap(2,wn_footer(2));t5_app_input_t in;unsigned at=ticks;
  while(ticks<at+75)assert(ua->poll(&in,25));assert(wn_gesture);
  mock_update.state=SOFTWARE_UPDATE_DOWNLOAD;un_prepare();assert(un_mode==UN_PROGRESS&&!un_model_valid&&!wn_gesture);native_frame();
  while(ticks<at+250){assert(ua->poll(&in,25));assert(!in.tapped);}assert(!begins&&!activations);
 }else if(which==50){
  /* Native service keeps Firmware Update separate even when a release is
   * marked USB-only: no network or installation authority is exercised. */
  native_availability=SOFTWARE_UPDATE_USB_ONLY;native_list(1);native_frame();assert(portable_update_close());
  tap(12,wn_footer(2));tap(28,wn_footer(1));tap(44,wn_back());app_main();
  assert(!confirming&&!connects&&!begins&&!activations&&launches==1&&!opened);
 }else if(which==51){
  /* Captured scenes are explicit, bounded and named by the runner. */
  native_frame();native_list(3);selected=1;native_frame();choose();
  confirming=false;mock_update.state=SOFTWARE_UPDATE_DOWNLOAD;mock_update.done=14336;mock_update.total=32768;ust=mock_update;native_frame();
  mock_update.state=SOFTWARE_UPDATE_ERROR;ust=mock_update;message="Update failed; current install kept";native_frame();
  cleanup_pending=true;native_frame();cleanup_pending=false;restart_pending=true;native_frame();
  ust.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;native_frame();restart_pending=false;
 }else if(which>=52&&which<=55){
  native_list(1);selected=0;native_malformed=which-51;native_frame();
  assert(!strcmp(uv.rows[1],"Unavailable")&&!un_candidate_valid);
  selected=1;native_frame();
  assert(!strcmp(uv.rows[1],"Unavailable")&&!un_candidate_valid&&!begins&&!connects&&!activations);
  assert(!un_model_current());native_malformed=0;
 }else assert(false);
 finish();printf("Native Firmware Update %ux%u case%u passed\n",UW,UH,which);return 0;
}
