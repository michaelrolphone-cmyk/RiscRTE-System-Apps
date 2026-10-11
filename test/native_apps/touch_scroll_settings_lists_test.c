/* Production Settings entry, controller, raster and async lifecycle. Only the
 * native providers and their clock are scripted; no direct editor actions. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main
#ifndef PORTABLE_QUICK_ACTIONS
#define quick_modal false
#endif
static const char *scenario,*frames_dir;
static bool fields_case,pending,have_background,first_tap,chosen,returned,queued_up;
static unsigned started,delay_ms,submitted,home_sent,back_sent,shown,drag_frames,momentum_frames,busy_samples,quick_frames;
static int maximum_offset,minimum_offset=1000000,release_offset,expected_row=-1,chosen_row=-1,return_offset=-1;
static unsigned submitted_page,visible_page=~0u,press_visible_changes,alarm_frames;
static bool alarm_fired,static_seen;
static bool held_selection,checked_reentry;
static int submitted_offset,visible_offset;
static uint8_t submitted_pixels[sizeof(pixels)],background[sizeof(pixels)];
static bool is(const char *s){return !strcmp(scenario,s);}
static bool list_page(void){return sv_page==(fields_case?SV_FIELDS:SV_ROOT);}
static portable_touch_scroll *list_scroll(void){return &sp_scroll[fields_case];}
static unsigned elapsed(void){return started&&ticks>=started?ticks-started:0;}
static unsigned release_at(void){return fields_case?200:280;}
static bool scroll_health(risc_runtime_health_v1 *out){io();assert(ticks<15000);out->uptime_ms=ticks;return true;}
static bool scroll_info(void *c,risc_display_info_v1 *out){bool ok=get_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool scroll_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *damage,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
  assert(!pending&&!raster_clip_active);
  assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
  if(list_page()&&!started)started=ticks+delay_ms+80;
  if(list_page()&&!quick_modal&&!alarm_modal){
    ++shown;unsigned at=elapsed();portable_touch_scroll *s=list_scroll();
    if(started&&ticks>=started&&at>=80&&at<release_at())++drag_frames;
    if(started&&ticks>=started&&at>=release_at()&&at<650)++momentum_frames;
    if(!have_background){memcpy(background,pixels,sizeof(pixels));have_background=true;}
    if(!chosen&&!is("bounds"))for(int y=0;y<LOGICAL_HEIGHT;++y)if(y<s->view.y || y>=s->view.y+s->view.height)
      for(int x=0;x<LOGICAL_WIDTH;++x){int px=x,py=y;native_point(&px,&py);unsigned offset=py*(NATIVE_WIDTH/8)+(unsigned)px/8;
        if((pixels[offset]^background[offset])&(0x80u>>(px&7))){fprintf(stderr,"outside viewport x%d y%d page%u offset%d view%d/%d tick%u\n",x,y,sv_page,portable_scroll_offset(s),s->view.y,s->view.height,ticks);assert(false);}}
    if(chosen){returned=true;return_offset=portable_scroll_offset(s);}
  }
  if(started&&ticks>=started&&!list_page()&&!quick_modal&&!alarm_modal){
    if(fields_case&&sv_page==SV_VALUE){chosen=true;chosen_row=(int)editor_field;}
    if(!fields_case&&sv_page==SV_ABOUT){chosen=true;chosen_row=SETTINGS_ABOUT_ROW;}
    if(!fields_case&&sv_page==SV_TIME_FORMAT){chosen=true;chosen_row=2;}
  }
  if(sv_page==SV_DESK_FACE){static_seen=true;assert(!list_scroll()->contact&&!list_scroll()->velocity_q8);}
  if(alarm_modal)++alarm_frames;
  if(quick_modal){++quick_frames;assert(!list_scroll()->contact&&!list_scroll()->velocity_q8);}
  if(frames_dir){char path[1024];snprintf(path,sizeof(path),"%s/%03u-page-%u-offset-%d.pbm",frames_dir,presents,sv_page,portable_scroll_offset(list_scroll()));FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n%d %d\n",NATIVE_WIDTH,NATIVE_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
  bool ok=frame_show(c,f,damage,n,options,token);pending=true;submitted=ticks;
  submitted_page=sv_page;submitted_offset=portable_scroll_offset(list_scroll());
  memcpy(submitted_pixels,pixels,sizeof(pixels));return ok;
}
static bool scroll_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
  (void)c;io();assert(pending&&token==presents&&!memcmp(submitted_pixels,pixels,sizeof(pixels)));
  if(ticks-submitted<delay_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
  pending=false;if(held_selection&&visible_offset!=submitted_offset)++press_visible_changes;visible_page=submitted_page;visible_offset=submitted_offset;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool scroll_nav(void *c,risc_input_navigation_frame_v1 *out){
  (void)c;io();*out=(risc_input_navigation_frame_v1){0};unsigned b=0;
  if(fields_case&&ticks>=100&&ticks<120)b=RISC_NAV_DOWN;
  if(fields_case&&ticks>=180&&ticks<200)b=RISC_NAV_CONFIRM;
  unsigned at=elapsed();
  if(started&&ticks>=started){
    if((is("nested")||(is("partial")&&fields_case)||is("modal")||is("reentry"))&&!back_sent&&at>=1600){b=RISC_NAV_BACK;++back_sent;}
    if(is("static-grid")){
      if(at>=80&&at<80+(SETTINGS_FACE_ROW+1u)*40&&(at-80)%40<20)b=RISC_NAV_DOWN;
      if(at>=440&&at<460)b=RISC_NAV_CONFIRM;
      if(!back_sent&&at>=1400){b=RISC_NAV_BACK;++back_sent;}
    }
    if(is("back")&&!back_sent&&at>=180){b=RISC_NAV_BACK;++back_sent;}
    if(is("supersede")){
      if(fields_case){
        if((at>=80&&at<100)||(at>=120&&at<140)||(at>=160&&at<180))b=RISC_NAV_UP;
        if((at>=220&&at<240)||(at>=260&&at<280)||(at>=300&&at<320))b=RISC_NAV_DOWN;
      } else {
        if(at>=80&&at<100)b=RISC_NAV_UP;
        if(at>=140&&at<160)b=RISC_NAV_DOWN;
        if(at>=200&&at<220)b=RISC_NAV_DOWN;
      }
    }
    if(!home_sent&&at>=(is("home")?180u:2600u)){b=RISC_NAV_HOME;++home_sent;}
  }
  out->buttons=out->pressed=b;return true;
}
static int32_t scroll_next(void *c,uint64_t token,risc_touch_event_v1 *out){
  (void)c;(void)token;io();
  if(is("queued-up")&&!queued_up&&started&&elapsed()>=release_at()){
    int x=LOGICAL_WIDTH/2,y=SP_TOP+10;
    if(flipped){x=LOGICAL_WIDTH-1-x;y=LOGICAL_HEIGHT-1-y;}
    queued_up=true;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=(uint16_t)x,.y=(uint16_t)y};return 1;
  }
  return is("cancelled")&&started&&elapsed()>=180&&elapsed()<200?-1:0;
}
static bool scroll_snapshot(void *c,risc_touch_snapshot_v1 *out){
  (void)c;io();*out=(risc_touch_snapshot_v1){.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
  if(!started||ticks<started)return true;
  portable_touch_scroll *s=list_scroll();unsigned at=elapsed();int offset=portable_scroll_offset(s);
  if(getenv("SCROLL_TRACE")&&at<2000)fprintf(stderr,"t%u offset%d v%d contact%d selected%u field%u page%u completed%u/%d\n",at,offset,s->velocity_q8,s->contact,sv_selected,editor_field,sv_page,sp_completed_page,sp_completed_q8/256);
  if(list_page()){
    assert(offset>=0&&offset<=s->limit);
    if(offset>maximum_offset)maximum_offset=offset;
    if(offset<minimum_offset)minimum_offset=offset;
    if(at<release_at())release_offset=offset;
  }
  bool down=false;int x=LOGICAL_WIDTH/2,y=SP_TOP+190,id=1;
  if(at>=80&&at<release_at()&&!is("bounds")&&!is("supersede")&&!is("static-grid")){
    down=true;
    if(is("horizontal")){x=100+(int)(at-80);y=SP_TOP+100;}
    else if(is("footer-drag"))y=SP_TOP+100+(FOOTER_Y-SP_TOP-100)*(int)(at-80)/(int)(release_at()-100);
    else y-=(int)(at-80)/(fields_case?2:1);
    if(is("replaced")&&at>=160)id=2;
  }
  if(is("static-grid")&&at>=700&&at<900){down=true;y=SP_TOP+190-(int)(at-700);assert(sv_page==SV_DESK_FACE);}
  if(is("bounds")){
    if(at>=80&&at<360){down=true;y=SP_TOP+s->view.height-10-(int)(at-80)*3;}
    if(at>=700&&at<980){down=true;y=SP_TOP+10+(int)(at-700)*3;}
  }
  if(is("reverse")&&at>=320&&at<520){down=true;y=SP_TOP+30+(int)(at-320);}
  if((is("stop-tap")||is("busy-hit"))&&at>=300&&at<360){down=true;y=SP_TOP+40;}
  if(is("modal")&&at>=400&&at<700){down=true;y=20+(int)(at-400)*2;}
  bool select=is("select")||is("busy-hit")||is("nested")||is("partial")||is("reentry");unsigned choose_at=is("busy-hit")?420:900;
  if(select&&at>=choose_at&&at<choose_at+(is("busy-hit")?300u:80u)){
    down=true;y=SP_TOP+(is("partial")?4:fields_case?150:190);
    if(!first_tap){first_tap=true;assert(visible_page==(fields_case?SV_FIELDS:SV_ROOT));expected_row=(y-SP_TOP+visible_offset)/SP_ROW;}
  }
  if(is("reentry")&&at>=1640&&at<1720){down=true;y=SP_TOP+100;}
  if(is("reentry")&&at>=1800&&at<2000){assert(list_page());checked_reentry=true;}
  held_selection=is("busy-hit")&&at>=choose_at&&at<choose_at+300;
  if(is("save")&&at>=900&&at<980){down=true;x=SAVE_X;y=FOOTER_Y;}
  if(is("cancel")&&at>=900&&at<980){down=true;x=80;y=FOOTER_Y;}
  if(down){
    if(pending)++busy_samples;
    if(flipped){x=LOGICAL_WIDTH-1-x;y=LOGICAL_HEIGHT-1-y;}
    if(x>=0&&x<LOGICAL_WIDTH&&y>=0&&y<LOGICAL_HEIGHT){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}
  }
  return true;
}
static int32_t scroll_alarm_step(void *c){
  int32_t result=alarm_step(c);
  if(is("alarm")&&started&&elapsed()>=160&&!alarm_fired){
    alarm_fired=true;alarm_state.state=ALARM_STATE_ALERT;alarm_state.occurrence=(alarm_token_v1){1,2,3,4};
  }
  if(is("alarm")&&alarm_fired&&elapsed()>=500){alarm_state.state=ALARM_STATE_READY;memset(&alarm_state.occurrence,0,sizeof(alarm_state.occurrence));}
  return result;
}
static int32_t scroll_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){
  if(is("render-retained")&&started&&elapsed()>=80&&sv_page==SV_ROOT&&!strcmp(key,PORTABLE_RTC_BASIS_KEY)){io();hidden=true;return RISC_KEY_VALUE_CONTEXT;}
  if(!strcmp(key,PORTABLE_DESK_DIRECTION_KEY)){io();*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
  return kv_get(c,key,out,cap,size);
}
static bool scroll_launch(const char *path){io();assert(!strcmp(path,PORTABLE_HOME_APP)||!strcmp(path,PORTABLE_RETURN_APP));++launches;return true;}
static bool scroll_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
  if(!acquire(name,version,instance,out))return false;
  if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){static alarm_service_descriptor_v2 api;api=alarm_api;api.base.step=scroll_alarm_step;out->api=&api;}
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){static risc_key_value_v1 api;api=kv_api;api.get=scroll_get;out->api=&api;}
  if(!strcmp(name,"display.output")){static risc_display_output_api_v1 api;api=display_api;api.get_info=scroll_info;api.submit=scroll_submit;api.present_status=scroll_status;out->api=&api;}
  if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 api;api=touch_api;api.snapshot=scroll_snapshot;api.next=scroll_next;out->api=&api;}
  if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 api;api=navigation_api;api.poll=scroll_nav;out->api=&api;}
  return true;
}
int main(int argc,char **argv){
  assert(argc>=3);fields_case=!strcmp(argv[1],"fields");scenario=argv[2];test_name=scenario;frames_dir=argc>3?argv[3]:NULL;
  delay_ms=(is("busy-hit")||is("reentry")||is("supersede"))?500:40;
  zone_id="America/Denver";native_epoch=expected_epoch=1768505696;
#ifdef TEST_SETTINGS_LIST_FLIPPED
  flipped=true;
#endif
  configure_records();runtime.acquire=scroll_acquire;runtime.health=scroll_health;runtime.request_launch=scroll_launch;
  assert(app_module_init()==0);in_main=true;app_main();
  if(is("render-retained")){
    assert(retained&&barriers==1&&!launches&&!zone_puts&&!raster_clip_active);app_module_fini();
    printf("{\"case\":\"%s\",\"retained\":true}\n",scenario);return 0;
  }
  assert(!failed&&!retained&&!pending);
  assert(!sp_scroll[0].contact&&!sp_scroll[0].velocity_q8&&!sp_scroll[1].contact&&!sp_scroll[1].velocity_q8&&!raster_clip_active);
  if(fields_case&&is("save")){assert(rtc_writes==1&&seeds==1&&basis_puts==1);assert(!memcmp(&first_draft,&settings_draft,sizeof(first_draft)));}
  else assert(!rtc_reads&&!rtc_writes&&!seeds&&!basis_puts);
  assert(!zone_puts&&!other_puts);
  if(is("select")||is("busy-hit")||is("nested")){assert(chosen&&chosen_row==expected_row);}
  if(is("nested")){assert(returned&&return_offset>0);}
  if(is("partial")){
    if(fields_case)assert(chosen&&chosen_row==expected_row&&returned&&return_offset==maximum_offset);
    else assert(!chosen&&sv_selected==(unsigned)expected_row+1&&portable_scroll_offset(list_scroll())==maximum_offset);
  }
  if(is("drag")){assert(drag_frames>=2&&momentum_frames>=1&&maximum_offset>release_offset);}
  if(is("bounds")){assert(maximum_offset==list_scroll()->limit&&minimum_offset==0);}
  if(is("busy-hit")){assert(busy_samples>5&&shown<12&&press_visible_changes>0);}
  if(is("reentry")){assert(checked_reentry&&returned&&chosen_row==expected_row);}
  if(is("supersede")){assert(portable_scroll_offset(list_scroll())==0&&maximum_offset==list_scroll()->limit);}
  if(is("cancelled"))assert(!chosen); /* Queue loss cancels the gesture; negative custody errors retain. */
  if(is("stop-tap")){assert(!chosen);if(!fields_case)assert(!sv_selected);}
  if(is("horizontal")||is("footer-drag")){assert(!chosen&&!maximum_offset);}
  if(is("static-grid")){assert(static_seen&&back_sent&&!chosen);}
  if(is("modal")){assert(quick_frames>0&&!quick_modal);}
  if(is("alarm")){assert(alarm_fired&&alarm_frames>0&&!alarm_modal&&!chosen);}
  if(is("queued-up")){assert(queued_up&&maximum_offset>release_offset);}
  in_main=false;in_fini=true;app_module_fini();in_fini=false;assert(!live&&!subscriptions&&!frames&&!barriers);
  printf("{\"list\":\"%s\",\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"maximum\":%d,\"limit\":%d,\"release\":%d,\"busy_samples\":%u,\"expected\":%d,\"chosen\":%d,\"return_offset\":%d,\"rtc_writes\":%u}\n",argv[1],scenario,shown,drag_frames,momentum_frames,maximum_offset,list_scroll()->limit,release_offset,busy_samples,expected_row,chosen_row,return_offset,rtc_writes);
  return 0;
}
