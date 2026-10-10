/* Production Apps/settings.c plus production adapter/controller/rasterizer.
 * All interaction uses the granted raw-touch/navigation interfaces. */
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_RTC_WALL_TIME
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_INPUT_NAVIGATION
#ifndef TEST_TIMEZONE_COMPACT
#define TEST_PAPER_SETTINGS
#define PORTABLE_DISPLAY_ROTATION 90
#endif
#define main original_fixture_main
#include "portable_settings_test.c"
#undef main

static uint8_t zone_bytes[64];static uint32_t zone_size;
static unsigned zone_reads,zone_writes,mode;
static bool read_error,write_error,after_commit,verify_error,verify_missing,verify_mismatch;
static uint32_t nav_script[2048];
static contact tz_contacts[1024];static unsigned tz_contact_count;
static unsigned next_at,script_started_at;
/* Script time is independent of raw captures performed during raster work. */
static unsigned timezone_step(void){return (ticks-script_started_at)/20u;}
static uint32_t timezone_buttons;
static int32_t zone_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
  if(strcmp(key,PORTABLE_TIMEZONE_KEY))return kv_get(c,key,out,cap,size);
  ++zone_reads;*size=zone_size;
  if(read_error || (verify_error&&zone_writes))return RISC_KEY_VALUE_IO;
  if(!zone_size || (verify_missing&&zone_writes))return RISC_KEY_VALUE_NOT_FOUND;
  if(cap<zone_size)return RISC_KEY_VALUE_BUFFER_SMALL;
  memcpy(out,zone_bytes,zone_size);
  if(verify_mismatch&&zone_writes)((uint8_t*)out)[3]^=1;
  return RISC_KEY_VALUE_OK;
}
static int32_t zone_put(void *c,const char *key,const void *in,uint32_t size) {
  (void)c;assert(!strcmp(key,PORTABLE_TIMEZONE_KEY)&&size==44);++zone_writes;
  if(write_error)return RISC_KEY_VALUE_IO;
  memcpy(zone_bytes,in,size);zone_size=size;
  return after_commit?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static risc_key_value_v1 timezone_kv={1,sizeof(timezone_kv),NULL,zone_get,zone_put};
static bool timezone_health(risc_runtime_health_v1 *out) {out->uptime_ms=ticks;assert(timezone_step()<2000);return true;}
static bool timezone_nav(void *c,risc_input_navigation_frame_v1 *out) {
  (void)c;*out=(risc_input_navigation_frame_v1){0};unsigned at=timezone_step();assert(at<2048);
  out->buttons=nav_script[at];out->pressed=out->buttons&~timezone_buttons;
  out->released=timezone_buttons&~out->buttons;timezone_buttons=out->buttons;return true;
}
static bool timezone_touch(void *c,risc_touch_snapshot_v1 *out) {
  (void)c;memset(out,0,sizeof(*out));out->width=width();out->height=height();
  for(unsigned i=0;i<tz_contact_count;++i)if(tz_contacts[i].poll==timezone_step()) {
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=timezone_step()==replace_at?2:1,.x=tz_contacts[i].x,.y=tz_contacts[i].y};
#if PORTABLE_TOUCH_ROTATION == 180
    out->contacts[0].x=(uint16_t)(width()-1-out->contacts[0].x);
    out->contacts[0].y=(uint16_t)(height()-1-out->contacts[0].y);
#endif
    break;
  }
  if(mode==16&&timezone_step()==next_at)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
  return true;
}
/* Raw events and the snapshot must describe the same captured sequence. */
static bool timezone_poll(void *c,size_t n) {
  assert(n==1);++polls;if(timezone_step()==fail_poll_at&&fail_poll_at)return false;
  risc_touch_snapshot_v1 next;assert(timezone_touch(c,&next));
  bool old_down=touch_state.contact_count!=0,new_down=next.contact_count!=0;
  bool same=old_down&&new_down&&touch_state.contacts[0].id==next.contacts[0].id;
  if(old_down&&!same)touch_emit(RISC_TOUCH_EVENT_UP,touch_state.contacts[0]);
  if(new_down) {
    if(!same)touch_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[0]);
    else if(memcmp(&touch_state.contacts[0],&next.contacts[0],sizeof(next.contacts[0])))touch_emit(RISC_TOUCH_EVENT_MOVE,next.contacts[0]);
  }
  if(next.buttons!=touch_state.buttons)touch_emit(next.buttons?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,(risc_touch_contact_v1){0});
  next.sequence=touch_state.sequence;next.timestamp_ms=ticks;touch_state=next;return true;
}
static int32_t timezone_next(void *c,uint64_t s,risc_touch_event_v1 *e) {
  if(gap_at&&timezone_step()==gap_at){touch_head=touch_count=0;return -1;}
  (void)c;assert(s==1);if(!touch_count)return 0;
  *e=touch_events[touch_head];touch_head=(touch_head+1)%RISC_TOUCH_QUEUE_LENGTH;--touch_count;return 1;
}
#ifdef TEST_TIMEZONE_SHORT
static bool short_acquire(void *c,uint32_t f,risc_display_surface_v1 *out) {
  (void)c;assert(!frame_count&&f==RISC_DISPLAY_FORMAT_MONO1);frame_count=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=600,.height=400,
    .stride_bytes=75,.size_bytes=30000,.pixel_format=f};return true;
}
#endif
static risc_runtime_api_v1 timezone_runtime;
static risc_touch_api_v1 timezone_input;
static risc_input_navigation_api_v1 timezone_navigation;
static risc_display_output_api_v1 timezone_display;
static unsigned capture_no;
static void capture(const char *name) {
  const char *dir=getenv("TIMEZONE_SETTINGS_FRAMES");if(!dir)return;
  char path[512];snprintf(path,sizeof(path),"%s/%u-%s-%u.%s",dir,mode,name,capture_no++,pp_enabled()?"pbm":"ppm");
  FILE *f=fopen(path,"wb");assert(f);
#ifdef TEST_PAPER_SETTINGS
  fprintf(f,"P4\n%u %u\n",info.width,info.height);
  assert(fwrite(framebuffer,1,(size_t)info.width*info.height/8,f)==(size_t)info.width*info.height/8);
#else
  fprintf(f,"P6\n240 240\n255\n");
  for(unsigned i=0;i<240*240;++i){unsigned v=framebuffer[i];uint8_t rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}
#endif
  assert(!fclose(f));
}
static bool timezone_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t) {
  if(mode==30&&sv_page==SV_TIMEZONE_REGIONS)capture("regions");
  if(mode==31&&sv_page==SV_TIMEZONE_CITIES)capture("america-first");
  if(mode==32&&sv_page==SV_TIMEZONE_CITIES)capture("america-last");
  if(mode>=33&&mode<=36&&sv_page==SV_TIMEZONE_CITIES)capture("city-status");
  return frame_submit(c,f,r,n,o,t);
}
static void reset_script(void) {
  memset(nav_script,0,sizeof(nav_script));tz_contact_count=0;next_at=3;
  polls=0;gap_at=fail_poll_at=replace_at=0;script_started_at=ticks;timezone_buttons=0;
}
static void setup(void) {
  scenario=300;reset_script();assert(app_module_init()==0);
  timezone_runtime=runtime_api;timezone_runtime.health=timezone_health;rt=&timezone_runtime;
  timezone_input=touch_api;timezone_input.poll=timezone_poll;timezone_input.next=timezone_next;touch.api=&timezone_input;
  timezone_navigation=nav_api;timezone_navigation.poll=timezone_nav;navigation=&timezone_navigation;
  timezone_display=display_api;timezone_display.submit=timezone_submit;display=&timezone_display;
#ifdef TEST_TIMEZONE_SHORT
  info.width=600;info.height=400;timezone_display.acquire=short_acquire;
#endif
  settings_store=&timezone_kv;settings_render(0,0);
}
static void finish(void) {
  assert(!writes&&!format_writes);app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!failed);
}
static void nav(unsigned button) {assert(next_at<2048);nav_script[next_at]=button;next_at+=3;}
static void contact_at(unsigned at,int x,int y) {assert(tz_contact_count<1024);tz_contacts[tz_contact_count++]=(contact){at,(uint16_t)x,(uint16_t)y};}
static void touch_at(int x,int y) {contact_at(next_at,x,y);next_at+=4;}
static int row_y(unsigned row) {return stz_top()+(int)(row%stz_rows())*stz_step()+stz_step()/2;}
static void touch_page(bool forward) {touch_at(forward?stz_width()-stz_margin()-20:stz_margin()+20,pp_enabled()?stz_height()-164:190);}
static void touch_footer(unsigned slot,bool cities) {
  int margin=stz_margin(),columns=cities?3:2,gap=pp_enabled()?12:6;
  int fw=(stz_width()-2*margin-(columns-1)*gap)/columns;
  touch_at(margin+(int)slot*(fw+gap)+fw/2,pp_enabled()?stz_height()-68:224);
}
static unsigned city_number(unsigned index) {
  unsigned region=portable_timezone_get(index)->region;
  for(unsigned i=0;i<portable_timezone_region_count(region);++i)if(portable_timezone_city_index(region,i)==(int)index)return i;
  assert(!"missing catalog index");return 0;
}
/* Navigate from fresh UTC fallback, through the actual region and city pages. */
static void select_zone(unsigned index,bool touch_mode,bool save) {
  unsigned region=portable_timezone_get(index)->region,city=city_number(index),rows=stz_rows();
  if(touch_mode) {
    for(unsigned i=0;i<region/rows;++i)touch_page(true);
    touch_at(stz_width()/2,row_y(region));
    for(unsigned i=0;i<city/rows;++i)touch_page(true);
    touch_at(stz_width()/2,row_y(city));
    if(save)touch_footer(2,true);
  }else {
    for(unsigned i=0;i<region/rows;++i)nav(RISC_NAV_RIGHT);
    for(unsigned i=0;i<region%rows;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);
    for(unsigned i=0;i<city/rows;++i)nav(RISC_NAV_RIGHT);
    for(unsigned i=0;i<city%rows;++i)nav(RISC_NAV_DOWN);
    if(save)nav(RISC_NAV_CONFIRM);
  }
}
static void assert_row(const char *value) {t5_app_setting_t row;assert(settings_get(0,1,&row)&&row.type==T5_APP_SETTING_ACTION&&!strcmp(row.value,value));}
static void text_checks(void) {
  unsigned old=sv_page;sv_page=SV_TIMEZONE_CITIES;
  for(unsigned i=0;i<419;++i) {
    char text[40],lines[3][40],rebuilt[120]={0};
    assert(portable_timezone_display_name(i,text,sizeof(text))==0);
    unsigned count=stz_lines(text,stz_width()-2*stz_margin()-24,lines);assert(count>0&&count<=2);
    for(unsigned j=0;j<count;++j){assert(stz_measure(lines[j])<=stz_width()-2*stz_margin()-24);strcat(rebuilt,lines[j]);}
    assert(!strcmp(text,rebuilt));
  }
  sv_page=old;
}
int main(int argc,char **argv) {
  assert(argc>=2);mode=(unsigned)atoi(argv[1]);setup();text_checks();
  if(mode==0||mode==1) {
    assert(argc==3);unsigned index=(unsigned)atoi(argv[2]);assert(index<419);
    select_zone(index,mode==1,true);assert(settings_activate(0,1)==T5_APP_SETTING_UPDATED);
    assert(zone_writes==(index?1u:0u));char id[40];
    assert(portable_timezone_preference_load(settings_store,id)==(index?PORTABLE_TIMEZONE_LOADED:PORTABLE_TIMEZONE_MISSING));
    assert(!strcmp(id,portable_timezone_get(index)->id));
    if(!index)assert(!strcmp(settings_message,"UTC default (not saved)"));
    else assert_row(id);
  } else if(mode==2) { /* Real root touch routing through unchanged app_main. */
    touch_at(pp_enabled()?120:50,pp_enabled()?SP_TOP+SP_ROW+30:sv_row_y(1)+10);
    nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);
    app_main();assert(zone_writes==1);
  } else if(mode>=3&&mode<=7) {
    select_zone(100,true,false);
    if(mode==3)touch_footer(1,true); /* Cancel discards the whole editor. */
    if(mode==4){nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);}
    if(mode==5)nav(RISC_NAV_HOME);
    if(mode==6){touch_footer(0,true);touch_footer(0,false);}
    if(mode==7){ /* continuous held confirm at entry and across both levels */
      reset_script();for(unsigned i=0;i<9;++i)nav_script[i]=RISC_NAV_CONFIRM;
      nav_script[12]=RISC_NAV_DOWN;for(unsigned i=15;i<24;++i)nav_script[i]=RISC_NAV_CONFIRM;
      next_at=27;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);
    }
    assert(settings_activate(0,1)==T5_APP_SETTING_NO_CHANGE);assert(!zone_writes);
    if(mode==5)assert(return_launches==1);
  } else if(mode>=8&&mode<=14) {
    write_error=mode==8;after_commit=mode==9;verify_error=mode==10;
    verify_missing=mode==11;verify_mismatch=mode==12;read_error=mode==13;
    if(mode==14)timezone_kv.put=NULL;
    select_zone(100,true,true);touch_footer(1,true);
    assert(settings_activate(0,1)==T5_APP_SETTING_NO_CHANGE);
    if(mode<=12){assert(zone_writes==1&&stz_unconfirmed);assert_row("Unconfirmed");}
    else {assert(!zone_writes&&!stz_unconfirmed);assert(!strcmp(stz_message,"Storage unavailable"));}
    if(mode==9)assert(!strcmp((char*)zone_bytes+4,portable_timezone_get(100)->id));
    if(mode<=12) { /* Retain draft across reopen; retry only on explicit confirm. */
      unsigned before=zone_writes;reset_script();nav(RISC_NAV_BACK);assert(settings_activate(0,1)==0);
      assert(stz_choice==100&&stz_unconfirmed&&zone_writes==before);
      write_error=after_commit=verify_error=verify_missing=verify_mismatch=false;
      reset_script();nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);
      assert(settings_activate(0,1)==T5_APP_SETTING_UPDATED&&!stz_unconfirmed);
      assert_row(portable_timezone_get(100)->id);
    }
  } else if(mode==15) {
    zone_size=44;memset(zone_bytes,255,zone_size);assert_row("Invalid (UTC fallback)");assert(!zone_writes);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);assert(settings_activate(0,1)==T5_APP_SETTING_UPDATED);
    assert(zone_writes==1);assert_row("UTC");
  } else if(mode==16) {
    select_zone(100,true,false);assert(settings_activate(0,1)==0);assert(return_launches==1&&!zone_writes);
  } else if(mode>=17&&mode<=21) {
    /* A vertical drag starts over a city, crosses Save, and is interrupted. */
    select_zone(100,true,false);unsigned start=next_at;
    contact_at(start,stz_width()/2,stz_top()+10);
    contact_at(start+1,stz_width()/2,stz_height()-68);
    if(mode==17)gap_at=start+1;
    if(mode==18)fail_poll_at=start+1;
    if(mode==19)replace_at=start+1;
    if(mode==20){contact_at(start+2,stz_width()/2,stz_top()+10);gap_at=start+2;}
    next_at=start+6;touch_footer(1,true);
    assert(settings_activate(0,1)==0&&!zone_writes);
  } else if(mode==22) {
    /* Physical backwards wrap and first/last page clamping. */
    nav(RISC_NAV_LEFT);nav(RISC_NAV_UP);nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_UP);nav(RISC_NAV_RIGHT);nav(RISC_NAV_CONFIRM);
    assert(settings_activate(0,1)==T5_APP_SETTING_UPDATED);
    unsigned last=portable_timezone_region_count(PORTABLE_TIMEZONE_AMERICA)-1;
    assert(stz_choice==(unsigned)portable_timezone_city_index(PORTABLE_TIMEZONE_AMERICA,last));
  } else if(mode==23) {
    /* No record or API becomes an available fallback by reading. */
    settings_store=NULL;assert_row("Unavailable");nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);touch_footer(1,true);
    assert(settings_activate(0,1)==0&&!zone_writes&&!stz_unconfirmed);
  } else if(mode==24) {
    /* Entering on a city-page press cannot use its held repeats to Save. */
    nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);unsigned at=next_at;
    for(unsigned i=0;i<8;++i)nav_script[at+i]=RISC_NAV_CONFIRM;
    next_at=at+10;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);
    assert(settings_activate(0,1)==0&&!zone_writes);
  } else if(mode>=25&&mode<=29) {
    if(mode==25||mode==26) {
      /* Region paging by full touch swipes, including the return gesture. */
      int x=stz_width()/2,y=stz_top()+2*stz_step();
      contact_at(3,x,y);contact_at(4,x,y-70);next_at=8;
      if(mode==26){contact_at(8,x,y-70);contact_at(9,x,y);next_at=13;}
      nav(RISC_NAV_BACK);assert(settings_activate(0,1)==0&&!zone_writes);
      assert(stz_first==(mode==25?stz_rows():0));
    } else if(mode==27) {
      /* A deliberate right swipe is Back to regions, never Save. */
      select_zone(100,true,false);unsigned at=next_at;contact_at(at,60,stz_top()+10);
      contact_at(at+1,140,stz_top()+10);next_at+=5;nav(RISC_NAV_BACK);
      assert(settings_activate(0,1)==0&&!zone_writes);
    } else if(mode==28) {
      /* An invalid record is not repaired merely by loading or cancelling. */
      zone_size=44;memset(zone_bytes,255,44);select_zone(0,true,false);touch_footer(1,true);
      assert(settings_activate(0,1)==0&&!zone_writes);assert_row("Invalid (UTC fallback)");
    } else {
      /* Unconfirmed commit followed by unavailable storage must retain both facts. */
      after_commit=true;select_zone(100,true,true);touch_footer(1,true);
      assert(settings_activate(0,1)==0&&zone_writes==1&&stz_unconfirmed);
      read_error=true;reset_script();nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);touch_footer(1,true);
      assert(settings_activate(0,1)==0&&zone_writes==1&&stz_unconfirmed);
      assert(!strcmp(stz_message,"Unconfirmed; storage unavailable"));assert_row("Unconfirmed");
    }
  } else if(mode>=30&&mode<=36) {
    if(mode==30){nav(RISC_NAV_RIGHT);nav(RISC_NAV_RIGHT);nav(RISC_NAV_BACK);}
    else if(mode==31){select_zone((unsigned)portable_timezone_city_index(PORTABLE_TIMEZONE_AMERICA,0),true,false);touch_footer(1,true);}
    else if(mode==32){select_zone((unsigned)portable_timezone_city_index(PORTABLE_TIMEZONE_AMERICA,portable_timezone_region_count(PORTABLE_TIMEZONE_AMERICA)-1),true,false);touch_footer(1,true);}
    else {
      if(mode==33){zone_size=44;memset(zone_bytes,255,44);}
      if(mode==34)read_error=true;
      if(mode==35)after_commit=true;
      select_zone(mode==36?(unsigned)portable_timezone_find("America/Argentina/Buenos_Aires",40):100,true,mode==35);touch_footer(1,true);
    }
    assert(settings_activate(0,1)==0);
  } else assert(!"unknown scenario");
  finish();printf("Timezone Settings mode %u passed\n",mode);
}
