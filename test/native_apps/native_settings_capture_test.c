/* Regression: captured raw edges cannot become actions on a later visible page.
 * Only host providers are staged; Apps/settings.c and adapter/controller source
 * remain the production compilation inputs. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size,native_script_started;
static uint64_t raw_ticks;
static unsigned capture_case,capture_snapshots;
static void capture_clock_advance(uint32_t ms);
static uint64_t capture_raw_timestamp(void);
static uint8_t capture_contact_id(void);
static void capture_event(unsigned kind,uint64_t sequence,uint64_t timestamp);
static void deliver_event(unsigned kind,uint64_t sequence,uint64_t timestamp);
#include SETTINGS_CAPTURE_FIXTURE
static unsigned latency=600,submitted_at,status_checks,completed_at,first_completed_at;
static unsigned invocation,observed_fields,observed_other,cancelled_before_exit;
static bool asynchronous,pending,exit_seen,back_seen,ever_fields,previous_fields,observe;
static bool early_exit,physical_back,zero_timestamp;
static uint8_t *frozen,*guarded;
static risc_display_output_api_v1 capture_display;
static risc_input_navigation_api_v1 capture_navigation;
static unsigned captured_elapsed[64];
static unsigned captured_down,captured_up,delivered_down,delivered_up,previsible_edges;
static unsigned stale_after_complete,fresh_after_complete,last_capture_elapsed,last_delivery_elapsed;
static unsigned now_elapsed(void){return (uint32_t)(ticks-native_script_started);}
static uint8_t capture_contact_id(void){return capture_case==23&&now_elapsed()>=200&&now_elapsed()<400?2:1;}
static void capture_clock_advance(uint32_t ms){ticks+=ms;raw_ticks+=ms;}
static uint64_t capture_raw_timestamp(void){return zero_timestamp?0:raw_ticks;}
static void observe_page(void){
 if(!observe)return;
 if(sv_page==SV_FIELDS){ever_fields=true;previous_fields=true;observed_fields++;}
 else if(sv_page!=SV_ROOT)observed_other++;
 else if(previous_fields&&!exit_seen){cancelled_before_exit++;previous_fields=false;}
}
static void capture_event(unsigned kind,uint64_t sequence,uint64_t timestamp){
 unsigned elapsed=now_elapsed();assert(sequence<64);captured_elapsed[sequence]=elapsed;
 if(kind==RISC_TOUCH_EVENT_DOWN)captured_down++;
 if(kind==RISC_TOUCH_EVENT_UP)captured_up++;
 if(!first_completed_at)previsible_edges++;
 last_capture_elapsed=elapsed;
 fprintf(stderr,"CAPTURE elapsed=%u raw=%llu sequence=%llu kind=%u page=%u pending=%u\n",elapsed,(unsigned long long)timestamp,(unsigned long long)sequence,kind,sv_page,pending);
}
static void deliver_event(unsigned kind,uint64_t sequence,uint64_t timestamp){
 if(kind==RISC_TOUCH_EVENT_DOWN)delivered_down++;
 if(kind==RISC_TOUCH_EVENT_UP)delivered_up++;
 unsigned elapsed=now_elapsed();last_delivery_elapsed=elapsed;
 if(first_completed_at){if(captured_elapsed[sequence]<first_completed_at)stale_after_complete++;else fresh_after_complete++;}
 fprintf(stderr,"DELIVER elapsed=%u raw=%llu sequence=%llu kind=%u page=%u ready=%u\n",elapsed,(unsigned long long)timestamp,(unsigned long long)sequence,kind,sv_page,sn_touch_ready());
}
static bool capture_health(risc_runtime_health_v1*out){
 active_provider();observe_page();assert(now_elapsed()<10000);out->uptime_ms=ticks;return true;
}
static bool capture_info(void*c,risc_display_info_v1*out){bool ok=display_info(c,out);
 if(asynchronous)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 if(capture_case==20)out->flags|=RISC_DISPLAY_INFO_PARTIAL_DAMAGE;
 return ok;}
static bool capture_acquire(void*c,uint32_t f,risc_display_surface_v1*out){assert(!pending);return frame_acquire(c,f,out);}
static bool capture_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 assert(!pending);bool ok=frame_submit(c,f,r,n,o,t);
 if(ok){pending=true;submitted_at=ticks;memcpy(frozen,framebuffer,test_size);}return ok;
}
static bool capture_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 (void)c;assert(t&&pending&&!memcmp(frozen,framebuffer,test_size));status_checks++;
 out->state=(uint32_t)(ticks-submitted_at)<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){pending=false;completed_at=now_elapsed();
 if(!first_completed_at)first_completed_at=completed_at;
 fprintf(stderr,"COMPLETE elapsed=%u page=%u\n",completed_at,sv_page);}
 return true;
}
static bool capture_exit(void){
 observe_page();unsigned limit=early_exit?100:capture_case==9?2800:2200;
 if(now_elapsed()<limit)return false;
 if(!exit_seen&&early_exit)assert(pending);
 exit_seen=true;return true;
}
static bool capture_nav(void*c,risc_input_navigation_frame_v1*out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(physical_back&&!back_seen&&now_elapsed()>=100){assert(pending);back_seen=true;out->buttons=out->pressed=RISC_NAV_BACK;}
 return true;
}
static bool capture_grant(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 bool ok=acquire_checked(n,v,i,g);
 if(ok&&!strcmp(n,"display.output"))g->api=&capture_display;
 if(ok&&!strcmp(n,"input.navigation"))g->api=&capture_navigation;
 return ok;
}
static void guards(void){
 for(unsigned i=0;i<32;i++)assert(guarded[i]==0xa5&&guarded[32+test_size+i]==0xa5);
 const uint8_t*p=(const uint8_t*)framebuffer;for(unsigned y=0;y<test_height;y++)for(unsigned x=test_width*2;x<test_stride;x++)assert(p[y*test_stride+x]==0xa5);
}
static void setup(void){
 input_count=polls=0;pending=exit_seen=back_seen=ever_fields=previous_fields=false;first_completed_at=completed_at=0;
 native_script_started=ticks;scenario=100;mode=0;capture_display=display_api;capture_display.get_info=capture_info;capture_display.acquire=capture_acquire;capture_display.submit=capture_submit;capture_display.present_status=capture_status;
 capture_navigation=nav_api;capture_navigation.poll=capture_nav;checked_runtime=runtime_api;checked_runtime.health=capture_health;checked_runtime.exit_requested=capture_exit;checked_runtime.acquire=capture_grant;checked_runtime.release=release_checked;checked_runtime.retain_invocation=retain_checked;
 observe=false;assert(app_module_init()==0);assert(settings_atomic_api&&settings_store);observe=true;
}
static void finish(void){
 observe_page();assert(!writes&&!format_writes&&!kv_writes&&!submissions&&!saved_size&&!retained);
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!token&&!pending);guards();observe=false;
}
static void root_contact(unsigned first,unsigned last){
 unsigned top=test_height<200?28:56,row=test_height<200?32:44;
 for(unsigned at=first;at<last;at++)tap(at,test_width/2,top+row/2);
}
static void cancel_contact(unsigned at){
 unsigned margin=test_width<200?8:20,gap=test_width<200?6:12;
 unsigned fh=test_height<200?28:44,half=(test_width-2*margin-gap)/2;
 tap(at,margin+half/2,test_height-8-fh/2);
}
/* No second app_main redraw is allowed to erase evidence in this path. The
 * production API's poll and settings_touch consume the post-drain raw stream. */
static void run_controller(void){
 const t5_app_api_v1*api=t5_app_get_api(T5_APP_ABI_VERSION);
 uint32_t category=0;int32_t selected=0;t5_app_input_t input;bool armed=false;
 while(api->poll(&input,20)){
  if(input.exit_requested)break;
  if(!input.buttons&&!input.tapped){armed=true;continue;}
  if(!armed)continue;
  armed=false;
  if(input.tapped)(void)api->settings_touch(input.touch_x,input.touch_y,&category,&selected);
 }
}
static void run_explicit_drain(void){
 settings_render(0,0);assert(portable_paper_frame_drain());
 assert(first_completed_at>=600&&captured_down&&captured_up);run_controller();
}
static void stable_page(unsigned page){
 if(!sv_active){settings_render(0,0);assert(portable_paper_frame_drain());}
 if(page==SV_ROOT)settings_render(0,0);else sv_switch(page);
 assert(portable_paper_frame_drain());t5_app_input_t input;assert(poll(&input,20));assert(!input.tapped&&!input.buttons);
 native_script_started=ticks;first_completed_at=0;input_count=0;
}
static void unowned_overlay(void){
 assert(portable_paper_frame_ready());clear_color(0x1234);present(false);
}
static void extended_case(void){
 observe=false;
 if(capture_case==16){
  stable_page(SV_ROOT);unsigned snapshots=capture_snapshots;
  root_contact(40,45);unowned_overlay();assert(portable_paper_frame_drain());
  fprintf(stderr,"OVERLAY snapshots_before=%u after=%u\n",snapshots,capture_snapshots);assert(capture_snapshots==snapshots);
  observe=true;run_controller();assert(ever_fields);return;
 }
 if(capture_case==17){
#ifdef PORTABLE_RASTER_SNAPSHOT
  stable_page(SV_ROOT);root_contact(5,10);unsigned snapshots=capture_snapshots;
  unowned_overlay();while(!paper_token){assert(raster_progress());checked_runtime.yield_ms(1);}
  risc_display_present_token_v1 overlay=paper_token;assert(pending);
  sv_switch(SV_FIELDS);assert(raster_sealed&&paper_token==overlay);
  while(paper_token==overlay){checked_runtime.yield_ms(1);input_service();assert(paper_token_progress());}
  fprintf(stderr,"OLD_OVERLAY_COMPLETE snapshots_before=%u after=%u sealed=%u\n",snapshots,capture_snapshots,raster_sealed);
  assert(capture_snapshots==snapshots&&raster_sealed);
  assert(portable_paper_frame_drain());assert(capture_snapshots==snapshots+1);
  fprintf(stderr,"NEW_SETTINGS_COMPLETE snapshots=%u watermark=%llu captured=%llu\n",capture_snapshots,(unsigned long long)touch.sequence,(unsigned long long)touch_state.sequence);
  assert(touch.sequence==touch_state.sequence);
  while(now_elapsed()<1500){t5_app_input_t input;assert(poll(&input,20));assert(!input.tapped&&!input.buttons&&sv_page==SV_FIELDS);}
  return;
#else
  assert(!"case 17 requires --snapshot");
#endif
 }
 latency=40;
 unsigned margin=test_width<200?8:20,top=test_height<200?28:56,side=test_width<200?32:44;
 unsigned fh=test_height<200?28:44,footer=test_height-8-fh,row=test_height<200?32:44;
 if(capture_case==23){
  latency=200;editor_field=4;settings_draft=(twatch_rtc_time_v1){2026,10,17,6,12,30,0};stable_page(SV_VALUE);
  unsigned y=test_height<160?footer-24:top+side+12;
  for(unsigned at=5;at<20;at++)tap(at,at<10?margin:test_width-margin-1,y+2);
  while(now_elapsed()<450){t5_app_input_t input;assert(poll(&input,20));}
  fprintf(stderr,"REPLACEMENT value=%u expected=0\n",editor_value(4));assert(editor_value(4)==0);
  input_count=0;assert(portable_paper_frame_drain());native_script_started=ticks;tap(5,test_width-margin-1,y+2);tap(6,test_width-margin-1,y+2);
  while(now_elapsed()<500){t5_app_input_t input;assert(poll(&input,20));}
  assert(editor_value(4)==59);return;
 }
 if(capture_case==24){
  latency=200;stable_page(SV_ROOT);root_contact(5,25);root_contact(40,45);
  while(!touch.down){t5_app_input_t input;assert(poll(&input,20));assert(now_elapsed()<180);}
  assert(sv_gesture);sv_switch(SV_FIELDS);
  unsigned fresh=0;while(now_elapsed()<1000){t5_app_input_t input;assert(poll(&input,20));assert(!input.buttons);
   if(input.tapped){assert(now_elapsed()>=800);fresh++;}
  }
  fprintf(stderr,"CROSS_PAGE_HELD fresh=%u expected=1\n",fresh);assert(fresh==1&&sv_page==SV_FIELDS);return;
 }
 if(capture_case==25){
  latency=600;stable_page(SV_ROOT);root_contact(5,10);root_contact(13,16);root_contact(50,55);
  while(!touch.down){t5_app_input_t input;assert(poll(&input,20));assert(now_elapsed()<180);}
  assert(sv_gesture);settings_view_redraw();assert(portable_paper_frame_drain());
  /* The existing owned release may complete. A second start captured before
   * that completion cannot borrow it, while a genuinely later tap can. */
  unsigned accepted=0;while(now_elapsed()<1300){t5_app_input_t input;assert(poll(&input,20));assert(!input.buttons);
   if(input.tapped){accepted++;if(accepted==1)assert(now_elapsed()<800);else assert(now_elapsed()>=1000);}
  }
  fprintf(stderr,"SECOND_QUEUED_CONTACT accepted=%u expected=2\n",accepted);assert(accepted==2);return;
 }
 if(capture_case==18||capture_case==21){
  editor_field=4;settings_draft=(twatch_rtc_time_v1){2026,10,17,6,12,30,0};stable_page(SV_VALUE);
  unsigned y=test_height<160?footer-24:top+side+12;
  for(unsigned at=5;at<20;at++)tap(at,at<10?margin:at<15?test_width/2:test_width-margin-1,y+2);
  while(now_elapsed()<500){t5_app_input_t input;assert(poll(&input,20));}
  fprintf(stderr,"SLIDER final=%u expected=59 gesture=%u\n",editor_value(4),sv_gesture);assert(editor_value(4)==59);assert(sv_page==SV_VALUE);return;
 }
 if(capture_case==19||capture_case==22){
  stable_page(SV_ROOT);unsigned visible=(footer-18-top)/row,limit=SETTINGS_ROW_COUNT>visible?(SETTINGS_ROW_COUNT-visible)*row:0;
  for(unsigned at=5;at<20;at++)tap(at,test_width/2,footer-2-(at<10?0:at<15?row:2*row));
  while(now_elapsed()<500){t5_app_input_t input;assert(poll(&input,20));}
  unsigned expected=limit<2*row?limit:2*row;
  fprintf(stderr,"SCROLL final=%d expected=%u gesture=%u\n",sv_scroll[0],expected,sv_gesture);assert(sv_scroll[0]==(int)expected);return;
 }
 assert(capture_case==20);stable_page(SV_ROOT);root_contact(5,10);
 while(!touch.down){t5_app_input_t input;assert(poll(&input,20));assert(now_elapsed()<180);}
 assert(sv_gesture);unsigned before=displays;settings_view_redraw();assert(portable_paper_frame_drain());
 fprintf(stderr,"UNCHANGED_REDRAW submissions_before=%u after=%u gesture=%u\n",before,displays,sv_gesture);
 if(test_width<=320&&test_height<=320)assert(displays==before);
 observe=true;run_controller();assert(ever_fields);
}
int main(int argc,char**argv){
 assert(argc==4);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);capture_case=(unsigned)atoi(argv[3]);assert(capture_case<=25);
 test_stride=test_width*2+6;test_size=test_stride*test_height;guarded=malloc(test_size+64);frozen=malloc(test_size);assert(guarded&&frozen);memset(guarded,0xa5,test_size+64);framebuffer=(uint16_t*)(guarded+32);
 asynchronous=capture_case==1||capture_case==2||capture_case==4||capture_case==6||capture_case==7||capture_case==8||capture_case==9||capture_case==14||capture_case==16||capture_case==17||capture_case==21||capture_case==22||capture_case==23||capture_case==24||capture_case==25;
 early_exit=capture_case==8;physical_back=capture_case==7;zero_timestamp=capture_case==12||capture_case==13||capture_case==14;
 if(capture_case==11||capture_case==15)ticks=UINT32_MAX-350u;
 raw_ticks=ticks;
 unsigned passes=capture_case==10?2:1;
 for(invocation=0;invocation<passes;invocation++){
  setup();if(capture_case>=16){extended_case();finish();continue;}
  bool positive=capture_case==5||capture_case==6||capture_case==9||capture_case==13||capture_case==15||(capture_case==10&&invocation==1);
  if(positive)root_contact(40,45);
  else if(capture_case==3||capture_case==4)root_contact(5,40);
  else if(!early_exit&&!physical_back)root_contact(5,10);
  if(capture_case==9)cancel_contact(85);
  if(capture_case==2||capture_case==14)run_explicit_drain();else app_main();
  observe_page();fprintf(stderr,"RESULT invocation=%u case=%u ever_fields=%u other=%u cancelled=%u exit=%u back=%u elapsed=%u\n",invocation,capture_case,ever_fields,observed_other,cancelled_before_exit,exit_seen,back_seen,now_elapsed());
  assert(ever_fields==positive);assert(!observed_other);
  if(capture_case==9)assert(cancelled_before_exit);
  if(physical_back)assert(back_seen&&!exit_seen);
  if(!physical_back)assert(exit_seen);
  if(!early_exit&&!physical_back)assert(captured_down&&captured_up);
  finish();
 }
 assert(status_checks);free(frozen);free(guarded);
 printf("{\"size\":[%u,%u],\"case\":%u,\"passes\":%u,\"captured_down\":%u,\"captured_up\":%u,\"delivered_down\":%u,\"delivered_up\":%u,\"previsible_edges\":%u,\"stale_delivered_after_complete\":%u,\"fields_observations\":%u,\"cancelled_before_exit\":%u,\"status_checks\":%u,\"result\":\"passed\"}\n",test_width,test_height,capture_case,passes,captured_down,captured_up,delivered_down,delivered_up,previsible_edges,stale_after_complete,observed_fields,cancelled_before_exit,status_checks);return 0;
}
