#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
extern unsigned scroll_wifi_page(void);
extern const char *scroll_wifi_ssid(void);
extern bool scroll_wifi_cleanup(void);
static const char *scroll_case,*scroll_frames;
/* A queue GAP cancels derived gestures; only an uncertain error retains. */
static unsigned event_errors,cancelled_samples,recovered_samples;
static const void *scroll_radio_grant;
static unsigned started,submitted,delay_ms=40,home_sent,back_sent,nav_step,confirm_sent;
static unsigned shown,drag_frames,momentum_frames,busy_samples,quick_frames,cancels;
static bool pending,scan_case,first_tap,sent_up,have_background,selected_seen;
static int maximum_offset,release_offset,minimum_offset=1000000,expected_row=-1;
static uint8_t pending_pixels[sizeof(pixels)],background[sizeof(pixels)];
static bool scase(const char *s){return !strcmp(scroll_case,s);}
static unsigned elapsed(void){return started&&ticks>=started?ticks-started:0;}
static bool list_page(void){return scroll_wifi_page()==(scan_case?3u:0u)&&wpv_scroll.count==(scan_case?17:10);}
static bool scroll_health(risc_runtime_health_v1 *out){io();assert(ticks<12000);out->uptime_ms=ticks;return true;}
static bool scroll_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool scroll_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!pending&&!raster_clip_active);
 if(list_page()&&!started)started=ticks+delay_ms+80;
 if(list_page()&&!quick_modal){
  ++shown;unsigned at=elapsed();
  if(started&&ticks>=started&&at>=80&&at<280)++drag_frames;
  if(started&&ticks>=started&&at>=280&&at<700)++momentum_frames;
  if(!have_background){memcpy(background,pixels,sizeof(pixels));have_background=true;}
  if(!scase("cleanup")&&!scase("refresh"))for(int y=0;y<height();++y)if(y<WPV_TOP||y>=WPV_TOP+wpv_scroll.view.height)
   for(int x=0;x<width();++x){int px=y,py=info.height-1-x;
#ifdef PORTABLE_PAPER_PREFERENCES
if(paper_flip_ui){px=info.width-1-px;py=info.height-1-py;}
#endif
size_t at=(size_t)py*(TEST_TOOLBAR_NATIVE_WIDTH/8)+(unsigned)px/8;
    assert(!((pixels[at]^background[at])&(0x80u>>(px&7))));}
 }
 if(quick_modal){++quick_frames;assert(!wpv_scroll.contact&&!wpv_scroll.velocity_q8);}
 if(scroll_frames){char path[1024];snprintf(path,sizeof(path),"%s/%03u-page-%u-offset-%d.pbm",scroll_frames,presents,scroll_wifi_page(),portable_scroll_offset(&wpv_scroll));FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n%d %d\n",TEST_TOOLBAR_NATIVE_WIDTH,TEST_TOOLBAR_NATIVE_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
 bool ok=fx_submit(c,f,r,n,options,token);pending=true;submitted=ticks;memcpy(pending_pixels,pixels,sizeof(pixels));return ok;
}
static bool scroll_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&token==presents&&!memcmp(pending_pixels,pixels,sizeof(pixels)));
 if(ticks-submitted<delay_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 pending=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool scroll_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};unsigned b=0;
 if(scan_case&&nav_step<4&&ticks>=150+nav_step*100){b=nav_step<3?RISC_NAV_DOWN:RISC_NAV_CONFIRM;++nav_step;}
 if(scan_case&&nav_step>=4&&!started&&scroll_wifi_page()==0&&ticks>=150+nav_step*100){b=RISC_NAV_CONFIRM;++nav_step;}
 unsigned at=elapsed();if(started&&ticks>=started){
  if(scase("confirm-hidden")&&!confirm_sent&&at>=900){b=RISC_NAV_CONFIRM;++confirm_sent;}
  if(scase("confirm-hidden")&&confirm_sent==1&&at>=1100){assert(scroll_wifi_page()==3);b=RISC_NAV_CONFIRM;++confirm_sent;expected_row=0;}
  if(scase("bounds")&&at>=80&&at<105)b=RISC_NAV_UP;
  if(scase("bounds")&&at>=620&&at<645)b=RISC_NAV_DOWN;
  if(scase("keyboard")&&!back_sent&&at>=1600){b=RISC_NAV_BACK;++back_sent;}
  if((scase("back")||scase("modal"))&&!back_sent&&at>=(scase("back")?180u:1200u)){b=RISC_NAV_BACK;++back_sent;}
  if(!home_sent&&at>=(scase("home")?180u:2300u)){b=RISC_NAV_HOME;++home_sent;}
 }
 if(scase("cancelled")&&event_errors){
  if(!input_sample.valid||input_sample.cancelled)++cancelled_samples;
  if(elapsed()>=230&&elapsed()<300){const portable_touch_scroll *s=&wpv_scroll;assert(!s->contact&&!s->velocity_q8);++recovered_samples;}
 }
 out->buttons=out->pressed=b;return true;
}
static int32_t scroll_next(void *c,uint64_t token,risc_touch_event_v1 *out){
 (void)c;(void)token;io();
 if(scase("queued-up")&&!sent_up&&started&&elapsed()>=280){sent_up=true;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=(uint16_t)(width()/2),.y=WPV_TOP+10};
#ifdef TEST_READER_FLIP
 out->x=width()-1-out->x;out->y=height()-1-out->y;
#endif
 return 1;}
 if((scase("cancelled")||scase("touch-retained"))&&started&&elapsed()>=180&&elapsed()<205){++event_errors;return scase("touch-retained")?-2:-1;}
 return 0;
}
static bool scroll_snapshot_unflipped(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};if(!started||ticks<started)return true;
 unsigned at=elapsed();int offset=portable_scroll_offset(&wpv_scroll);
 if(getenv("SCROLL_TRACE")&&at<1100)fprintf(stderr,"t%u offset%d v%d selected%u page%u id%u complete%u hit%d\n",at,offset,wpv_scroll.velocity_q8,wpv_selected,scroll_wifi_page(),wpv_identity,wpv_completed_identity,wpv_touch_row);
 if(list_page()){assert(offset>=0&&offset<=wpv_scroll.limit);if(offset>maximum_offset)maximum_offset=offset;if(offset<minimum_offset)minimum_offset=offset;if(at<280)release_offset=offset;}
 if(scan_case&&!list_page()&&expected_row>=0&&!selected_seen){char want[32];snprintf(want,sizeof(want),"Network %02d",expected_row);assert(!strcmp(scroll_wifi_ssid(),want));selected_seen=true;}
 bool down=false;int x=width()/2,y=WPV_TOP+220,id=1;
 if(at>=80&&at<280&&!scase("bounds")){
  down=true;if(scase("horizontal")){x=100+(int)(at-80);y=WPV_TOP+100;}
  else if(scase("footer-drag"))y=height()-90-(int)(at-80);
  else y-=(int)(at-80);
  if(scase("replaced")&&at>=180)id=2;
 }
 if(scase("bounds")){if(at>=220&&at<420){down=true;y=WPV_TOP+220-(int)(at-220);}if(at>=800&&at<1000){down=true;y=WPV_TOP+20+(int)(at-800);}}
 if(scase("reverse")&&at>=320&&at<520){down=true;y=WPV_TOP+30+(int)(at-320);}
 if((scase("stop-tap")||scase("busy-hit"))&&at>=300&&at<375){down=true;y=WPV_TOP+40;}
 if(scase("modal")&&at>=400&&at<700){down=true;y=20+(int)(at-400)*2;}
 bool select=scase("keyboard")||scase("select")||scase("busy-hit")||scase("cleanup")||scase("refresh");unsigned choose_at=scase("busy-hit")?425:900;
 if(select&&at>=choose_at&&at<choose_at+75){down=true;y=WPV_TOP+100;if(!first_tap){first_tap=true;expected_row=(y-WPV_TOP+wpv_completed_q8/256)/WPV_ROW;}}
 if(scase("keyboard")&&at>=1150&&at<1225){down=true;x=50;y=210;}
 if(scase("keyboard")&&at>=1400&&at<1475){down=true;x=width()/6;y=height()-180;}
 if(scase("cleanup")&&at>=1300&&at<1375){down=true;y=WPV_TOP+100;}
 if(down){if(pending)++busy_samples;out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}return true;
}
static bool scroll_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 bool ok=scroll_snapshot_unflipped(c,out);
#ifdef TEST_READER_FLIP
 assert(paper_flip_ui);
 for(unsigned i=0;i<out->contact_count;++i){out->contacts[i].x=out->width-1-out->contacts[i].x;out->contacts[i].y=out->height-1-out->contacts[i].y;}
#endif
 return ok;
}

static bool scroll_scan(void *c,garden_radio_scan_result_v1 *out){
 (void)c;io();*out=(garden_radio_scan_result_v1){.struct_size=sizeof(*out),.state=GARDEN_RADIO_SCAN_DONE,.count=16};
 for(unsigned i=0;i<16;++i){snprintf(out->entries[i].ssid,sizeof(out->entries[i].ssid),"Network %02u",i);out->entries[i].rssi=-30-(int)i;out->entries[i].auth=scase("keyboard")?GARDEN_RADIO_AUTH_WPA2_PSK:GARDEN_RADIO_AUTH_OPEN;out->entries[i].channel=1;}
 if(scase("refresh")&&started&&elapsed()>=930)out->entries[0].rssi=-99;
 return true;
}
static bool scroll_cancel(void *c){(void)c;io();++cancels;return !scase("cleanup")||cancels>1;}
static bool scroll_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!app_acquire(name,version,instance,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=scroll_info;a.submit=scroll_submit;a.present_status=scroll_present;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.snapshot=scroll_snapshot;a.next=scroll_next;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=scroll_nav;out->api=&a;}
 if(!strcmp(name,"net.wifi")){static wifi_api_v1 a;a=app_wifi;a.scan_poll=scroll_scan;a.scan_cancel=scroll_cancel;out->api=&a;scroll_radio_grant=&a;}
 return true;
}
static bool scroll_release(risc_runtime_capability_v1 *g){
 if(g->api==scroll_radio_grant){assert(wifi_live);--wifi_live;return source_release(g);}
 return app_release(g);
}
int main(int argc,char **argv){
 assert(argc==2||argc==3);scroll_case=argv[1];scroll_frames=argc>2?argv[2]:NULL;scenario="valid";
 scan_case=!scase("root");delay_ms=scase("busy-hit")?500:40;
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=scroll_acquire;fx_runtime.release=scroll_release;fx_runtime.request_launch=app_launch;fx_runtime.health=scroll_health;
 assert(app_module_init()==0);const paper_presentation *retained_view=&pp_view;app_main();
 if(scase("touch-retained")||scase("cleanup")){
  assert(retained&&barriers==1&&!launches);
  /* Native uncertain cleanup keeps its owner pinned; retries are forbidden. */
  if(scase("cleanup"))assert(cancels==1&&wifi_live==1&&credentials_live==1);
  check_retained(retained_view);printf("{\"case\":\"%s\",\"retained\":true}\n",scroll_case);return 0;
 }
 assert(!failed&&!retained&&!pending&&launches==1&&!wpv_scroll.contact&&!wpv_scroll.velocity_q8&&!raster_clip_active);
 assert(!radio_connects&&!kv_writes&&!rtc_reads&&!rtc_writes);
 if(scase("confirm-hidden")||scase("keyboard")||scase("select")||scase("busy-hit"))assert(selected_seen&&expected_row>=0&&cancels>0);
 if(scase("drag")||scase("select")||scase("root")){assert(drag_frames>=2&&momentum_frames>=1&&maximum_offset>release_offset);}
 if(scase("bounds"))assert(maximum_offset>800&&minimum_offset==0);
 if(scase("busy-hit"))assert(busy_samples>10&&shown<15);
 if(scase("queued-up"))assert(sent_up&&maximum_offset>200);
 if(scase("stop-tap"))assert(!selected_seen);
 if(scase("modal"))assert(quick_frames>0&&!quick_modal);
 if(scase("footer-drag"))assert(maximum_offset==0);
 if(scase("refresh"))assert(!selected_seen&&!cancels);
 if(scase("cancelled"))assert(event_errors&&cancelled_samples&&recovered_samples);
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"max_offset\":%d,\"release_offset\":%d,\"busy_samples\":%u,\"selected_row\":%d}\n",scroll_case,shown,drag_frames,momentum_frames,maximum_offset,release_offset,busy_samples,expected_row);
 return 0;
}
