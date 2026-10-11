/* Real Settings entry/controller/rendering; only capability providers are
 * scripted. Selection, Save, rendering and lifecycle run through app_main. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main
static unsigned delay_ms,started,regions_started,home_sent,back_sent,open_sent;
static unsigned shown,drag_frames,busy_samples,post_release_frames,quick_frames;
static int release_offset,maximum_offset,minimum_offset=1000000,expected_row=-1;
static bool pending,region_case,first_tap,stop_seen,have_background,sent_up;
static unsigned reentry_confirm;
static unsigned submitted;
static uint8_t submitted_pixels[sizeof(pixels)],background[sizeof(pixels)];
static const char *frames_dir;
static bool is(const char *s){return !strcmp(test_name,s);}
static unsigned elapsed(void){return started&&ticks>=started?ticks-started:0;}
static bool scroll_health(risc_runtime_health_v1 *out){io();assert(ticks<15000);out->uptime_ms=ticks;return true;}
static bool scroll_info(void *c,risc_display_info_v1 *out){
  bool ok=get_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;
}
static bool list_page(void){return sv_page==(region_case?SV_TIMEZONE_REGIONS:SV_TIMEZONE_CITIES);}
static bool scroll_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *damage,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
  assert(!pending);assert(!raster_clip_active);
  if(sv_page==SV_TIMEZONE_REGIONS&&!regions_started)regions_started=ticks+delay_ms+80;
  if(list_page()&&!started)started=ticks+delay_ms+80;
  if(is("reentry"))assert(sv_page!=SV_TIMEZONE_CITIES);
  if(list_page()&&!quick_modal) {
    ++shown;unsigned at=elapsed();
    if(started&&ticks>=started&&at>=80&&at<280)++drag_frames;
    if(started&&ticks>=started&&at>=280&&at<650)++post_release_frames;
    if(!have_background){memcpy(background,pixels,sizeof(pixels));have_background=true;}
    /* Rows cannot paint over the fixed header/message/footer. The thumb is
     * in the same vertical viewport, to its right. */
    for(int y=0;y<LOGICAL_HEIGHT;++y)if(!is("reentry") && (y<stz_scroll.view.y || y>=stz_scroll.view.y+stz_scroll.view.height))
      for(int x=0;x<LOGICAL_WIDTH;++x){int px=x,py=y;native_point(&px,&py);unsigned offset=py*(NATIVE_WIDTH/8)+(unsigned)px/8;
        assert(!((pixels[offset]^background[offset])&(0x80u>>(px&7))));}
  }
  if(quick_modal){++quick_frames;assert(!stz_scroll.contact&&!stz_scroll.velocity_q8);}
  if(frames_dir){char path[1024];snprintf(path,sizeof(path),"%s/%03u-page-%u-offset-%d.pbm",frames_dir,presents,sv_page,portable_scroll_offset(&stz_scroll));FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n%d %d\n",NATIVE_WIDTH,NATIVE_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
  bool ok=frame_show(c,f,damage,n,options,token);pending=true;submitted=ticks;
  memcpy(submitted_pixels,pixels,sizeof(pixels));return ok;
}
static bool scroll_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
  (void)c;io();assert(pending&&token==presents&&!memcmp(submitted_pixels,pixels,sizeof(pixels)));
  if(ticks-submitted<delay_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
  pending=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool scroll_nav(void *c,risc_input_navigation_frame_v1 *out){
  (void)c;io();*out=(risc_input_navigation_frame_v1){0};
  unsigned b=0;
  if(ticks>=100&&ticks<120)b=RISC_NAV_DOWN;
  if(ticks>=180&&ticks<200)b=RISC_NAV_DOWN;
  if(ticks>=260&&ticks<280)b=RISC_NAV_CONFIRM;
  if(!region_case&&regions_started&&ticks>=regions_started&&sv_page==SV_TIMEZONE_REGIONS&&!open_sent){b=RISC_NAV_CONFIRM;++open_sent;}
  unsigned at=elapsed();
  if(started&&ticks>=started){
    if(is("reentry")&&!back_sent&&at>=1000){b=RISC_NAV_BACK;++back_sent;}
    if(is("reentry")&&!reentry_confirm&&at>=1100){b=RISC_NAV_CONFIRM;++reentry_confirm;}
    if(is("bounds")&&at>=80&&at<100)b=RISC_NAV_UP;
    if(is("bounds")&&at>=620&&at<640)b=RISC_NAV_DOWN;
    if((is("back")||is("modal"))&&!back_sent&&at>=(is("back")?180u:1200u)){b=RISC_NAV_BACK;++back_sent;}
    if(!home_sent&&at>=(is("home")?180u:2300u)){b=RISC_NAV_HOME;++home_sent;}
  }
  out->buttons=out->pressed=b;return true;
}
static int32_t scroll_next(void *c,uint64_t token,risc_touch_event_v1 *out){
  (void)c;(void)token;io();
  if(is("queued-up")&&!sent_up&&started&&elapsed()>=280) {
    sent_up=true;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=LOGICAL_WIDTH/2,.y=stz_top()+10};return 1;
  }
  return is("cancelled")&&started&&elapsed()>=180&&elapsed()<200?-1:0;
}
static bool scroll_snapshot(void *c,risc_touch_snapshot_v1 *out){
  (void)c;io();*out=(risc_touch_snapshot_v1){.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
  if(!started||ticks<started)return true;
  unsigned at=elapsed();int offset=portable_scroll_offset(&stz_scroll);
  if(getenv("SCROLL_TRACE")&&at<800)fprintf(stderr,"t%u offset%d v%d contact%d selected%u page%u\n",at,offset,stz_scroll.velocity_q8,stz_scroll.contact,stz_selected,sv_page);
  if(list_page()){
    assert(offset>=0&&offset<=stz_scroll.limit);
    if(offset>maximum_offset)maximum_offset=offset;
    if(offset<minimum_offset)minimum_offset=offset;
    if(at<280)release_offset=offset;
  }
  bool down=false;int x=LOGICAL_WIDTH/2,y=stz_top()+220,id=1;
  if(at>=80&&at<280&&!is("bounds")) {
    down=true;
    if(is("horizontal")){x=100+(int)(at-80);y=stz_top()+100;}
    else if(is("footer-drag"))y=stz_top()+100+(FOOTER_Y-stz_top()-100)*(int)(at-80)/180;
    else y-= (int)(at-80);
    if(is("replaced")&&at>=180)id=2;
  }
  if(is("bounds")) {
    if(at>=220&&at<420){down=true;y=stz_top()+220-(int)(at-220);}
    if(at>=800&&at<1000){down=true;y=stz_top()+20+(int)(at-800);}
  }
  if(is("reverse")&&at>=320&&at<520){down=true;y=stz_top()+30+(int)(at-320);}
  if((is("stop-tap")||is("busy-hit"))&&at>=300&&at<360){down=true;y=stz_top()+40;stop_seen=true;}
  if(is("modal")&&at>=400&&at<700){down=true;y=20+(int)(at-400)*2;}
  if(is("reentry")&&at>=1200&&at<1280){down=true;y=stz_top()+100;}
  bool select=is("select")||is("busy-hit")||is("flipped");
  unsigned choose_at=is("busy-hit")?420:900;
  if(select&&at>=choose_at&&at<choose_at+80) {
    down=true;y=stz_top()+100;
    if(!first_tap){
      first_tap=true;expected_row=(y-stz_top()+stz_completed_q8/256)/stz_step();
    }
  }
  if(select&&at>=1500&&at<1580){down=true;x=LOGICAL_WIDTH-80;y=FOOTER_Y;}
  if(down) {
    if(pending)++busy_samples;
    if(flipped){x=LOGICAL_WIDTH-1-x;y=LOGICAL_HEIGHT-1-y;}
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};
  }
  return true;
}
static bool scroll_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
  if(!acquire(name,version,instance,out))return false;
  if(!strcmp(name,"display.output")){static risc_display_output_api_v1 api;api=display_api;api.get_info=scroll_info;api.submit=scroll_submit;api.present_status=scroll_status;out->api=&api;}
  if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 api;api=touch_api;api.snapshot=scroll_snapshot;api.next=scroll_next;out->api=&api;}
  if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 api;api=navigation_api;api.poll=scroll_nav;out->api=&api;}
  return true;
}
int main(int argc,char **argv){
  assert(argc>=2);test_name=argv[1];frames_dir=argc>2?argv[2]:NULL;
  delay_ms=(is("busy-hit")||is("reentry"))?500:40;region_case=is("regions")||is("reentry");
  zone_id=region_case?"UTC":"America/Adak";native_epoch=1768505696;
  flipped=is("flipped");configure_records();
  runtime.acquire=scroll_acquire;runtime.health=scroll_health;
  assert(app_module_init()==0);in_main=true;app_main();
  if(is("cancelled")){
    assert(retained&&barriers==1&&!launches&&!zone_puts);app_module_fini();
    printf("{\"case\":\"%s\",\"retained\":true}\n",test_name);return 0;
  }
  assert(!failed&&!retained&&!pending&&launches==1&&home_sent==1);
  assert(!stz_scroll.contact&&!stz_scroll.velocity_q8&&!raster_clip_active);
  assert(!rtc_reads&&!rtc_writes&&!seeds&&!basis_puts);
  if(is("select")||is("busy-hit")||is("flipped")) {
    assert(zone_puts==1&&expected_row>=0);
    const char *wanted=portable_timezone_get((unsigned)portable_timezone_city_index(PORTABLE_TIMEZONE_AMERICA,(unsigned)expected_row))->id;
    assert(!strcmp((char*)zone_record+4,wanted));
  } else assert(!zone_puts);
  if(is("drag")||is("select")||is("flipped")){assert(drag_frames>=2&&post_release_frames>=1&&maximum_offset>release_offset);}
  if(is("bounds")){assert(maximum_offset>5000&&minimum_offset==0);}
  if(is("busy-hit")){assert(busy_samples>10&&shown<15);}
  if(is("regions")){assert(maximum_offset>0);}
  if(is("queued-up")){assert(sent_up&&maximum_offset>310);}
  if(is("reentry")){assert(back_sent&&reentry_confirm);}
  if(is("stop-tap")){assert(stop_seen&&stz_selected==0);}
  if(is("modal")){assert(quick_frames>0&&!quick_modal);}
  in_main=false;in_fini=true;app_module_fini();in_fini=false;
  assert(!live&&!subscriptions&&!frames&&!barriers);
  printf("{\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"max_offset\":%d,\"release_offset\":%d,\"busy_samples\":%u,\"selected_row\":%d,\"writes\":%u}\n",test_name,shown,drag_frames,post_release_frames,maximum_offset,release_offset,busy_samples,expected_row,zone_puts);
  return 0;
}
