/* Real paper Settings renderer/controller, bounded app-owned preferences. */
#define PORTABLE_SETTINGS_VERSION "1.3.6"
#define PORTABLE_RTC_WALL_TIME
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_SETTINGS_X4_DESK_CLOCK
#ifndef TEST_DESK_RGB
#define TEST_PAPER_SETTINGS
#endif
#ifndef PORTABLE_DISPLAY_ROTATION
#define PORTABLE_DISPLAY_ROTATION 90
#endif
#define PORTABLE_NOVA_UI
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_ALARM_SETTINGS
#define main original_fixture_main
#include "portable_settings_test.c"
#undef main
static uint8_t reader_bytes[2][8];static uint32_t reader_size[2];static unsigned reader_writes[2];
static uint8_t face_bytes[8];static uint32_t face_size;
static unsigned face_writes,sleep_writes,late_io,release_failures,nav_step;
static unsigned nav_buttons[128];
static bool read_error,put_error,commit_error,verify_error,mismatch,retained,jumped;
static unsigned retained_page;
static int32_t preference_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
  if(retained)++late_io;
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)||!strcmp(key,PORTABLE_ALERT_KEY))return kv_get(c,key,out,cap,size);
  if(!strcmp(key,PORTABLE_READER_LANGUAGE_KEY)||!strcmp(key,PORTABLE_READER_FLIP_KEY)) {
    unsigned i=!strcmp(key,PORTABLE_READER_LANGUAGE_KEY);
    if(read_error||(verify_error&&reader_writes[i]))return RISC_KEY_VALUE_IO;
    *size=reader_size[i];if(!*size)return RISC_KEY_VALUE_NOT_FOUND;
    if(*size>cap)return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(out,reader_bytes[i],*size);return RISC_KEY_VALUE_OK;
  }
  bool face=!strcmp(key,PORTABLE_DESK_FACE_KEY);
  assert(face||!strcmp(key,PORTABLE_SLEEP_KEY));
  if(read_error||(verify_error&&(face_writes||sleep_writes)))return RISC_KEY_VALUE_IO;
  uint32_t length=face?face_size:kv_size;*size=length;
  if(!length)return RISC_KEY_VALUE_NOT_FOUND;
  if(length>cap)return RISC_KEY_VALUE_BUFFER_SMALL;
  memcpy(out,face?face_bytes:kv_bytes,length);return RISC_KEY_VALUE_OK;
}
static int32_t preference_put(void *c,const char *key,const void *in,uint32_t size) {
  (void)c;if(retained)++late_io;assert(size==4);
  if(!strcmp(key,PORTABLE_READER_LANGUAGE_KEY)||!strcmp(key,PORTABLE_READER_FLIP_KEY)) {
    unsigned i=!strcmp(key,PORTABLE_READER_LANGUAGE_KEY);++reader_writes[i];
    if(put_error)return RISC_KEY_VALUE_IO;
    memcpy(reader_bytes[i],in,size);reader_size[i]=size;
    if(mismatch){reader_bytes[i][2]^=1;reader_bytes[i][3]=reader_bytes[i][2]^0xa5;}
    return commit_error?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
  }
  bool face=!strcmp(key,PORTABLE_DESK_FACE_KEY);assert(face||!strcmp(key,PORTABLE_SLEEP_KEY));
  if(face)++face_writes;else ++sleep_writes;
  if(put_error)return RISC_KEY_VALUE_IO;
  memcpy(face?face_bytes:kv_bytes,in,size);if(face)face_size=size;else kv_size=size;
  if(mismatch){uint8_t *b=face?face_bytes:kv_bytes;b[2]=(b[2]+1)%(face?6:3);b[3]=b[2]^0xa5u;}
  return commit_error?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static bool preference_release(risc_runtime_capability_v1 *grant) {
  if(retained)++late_io;
  if(grant==&settings_grant&&release_failures){--release_failures;return false;}
  return test_release(grant);
}
static bool scripted_nav(void *c,risc_input_navigation_frame_v1 *out) {
  (void)c;if(retained)++late_io;
  *out=(risc_input_navigation_frame_v1){0};assert(nav_step<128);
  out->buttons=out->pressed=nav_buttons[nav_step++];return true;
}
static bool guarded_nav_reset(void *c){if(retained)++late_io;return nav_reset(c);}
static bool guarded_read(void *c,twatch_rtc_time_v1 *out){if(retained)++late_io;return rtc_read(c,out);}
static void idle_yield(uint32_t ms){test_yield(ms);if(retained_page&&!jumped&&sv_page==retained_page){ticks+=60000;jumped=true;}}
int portable_app_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b) {
  (void)r;(void)d;(void)b;assert(!subscriptions&&!surface.frame);retained=true;return -1;
}
#ifdef TEST_DESK_SHORT
static bool short_frame_acquire(void *c,uint32_t f,risc_display_surface_v1 *out) {
  (void)c;assert(!frame_count&&f==RISC_DISPLAY_FORMAT_MONO1);frame_count=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=600,.height=400,
    .stride_bytes=75,.size_bytes=30000,.pixel_format=f};return true;
}
#endif
static bool desk_snapshot(void *c,risc_touch_snapshot_v1 *out) {
  bool ok=touch_snapshot(c,out);out->width=(uint16_t)width();out->height=(uint16_t)height();return ok;
}
static void record_face(unsigned face){face_size=4;face_bytes[0]=0x46;face_bytes[1]=1;face_bytes[2]=face;face_bytes[3]=face^0xa5;}
static void record_sleep(unsigned mode){kv_size=4;kv_bytes[0]=0x53;kv_bytes[1]=1;kv_bytes[2]=mode;kv_bytes[3]=mode^0xa5;}
static void capture(const char *name) {
  const char *dir=getenv("DESK_SETTINGS_FRAMES");if(!dir)return;
  char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",dir,name);
  FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(framebuffer,1,sizeof(framebuffer),f)==sizeof(framebuffer));assert(!fclose(f));
}
static void helpers(const risc_key_value_v1 *kv) {
  unsigned n=99;
  assert(portable_desk_face_load(kv,&n)==PORTABLE_DESK_FACE_MISSING&&n==0);
  assert(portable_desk_face_load(NULL,&n)==PORTABLE_DESK_FACE_UNAVAILABLE&&n==0);
  assert(portable_desk_face_load(kv,NULL)==PORTABLE_DESK_FACE_INVALID);
  assert(!portable_desk_face_save(kv,6)&&!portable_desk_face_save(kv,UINT32_MAX));
  for(unsigned i=0;i<6;++i){assert(portable_desk_face_save(kv,i));assert(portable_desk_face_load(kv,&n)==0&&n==i);unsigned before=face_writes;assert(portable_desk_face_save(kv,i)&&face_writes==before);}
  for(unsigned i=0;i<4;++i){record_face(2);face_bytes[i]^=0x80;assert(portable_desk_face_load(kv,&n)==PORTABLE_DESK_FACE_INVALID&&n==0);}
  for(unsigned i=1;i<=8;++i)if(i!=4){face_size=i;assert(portable_desk_face_load(kv,&n)==PORTABLE_DESK_FACE_INVALID&&n==0);}
  record_face(6);assert(portable_desk_face_load(kv,&n)==PORTABLE_DESK_FACE_INVALID&&n==0);
  record_face(0);read_error=true;assert(portable_desk_face_load(kv,&n)==PORTABLE_DESK_FACE_UNAVAILABLE);read_error=false;
  risc_key_value_v1 bad=*kv;bad.api_version=2;assert(portable_desk_face_load(&bad,&n)==3);bad=*kv;bad.struct_size=0;assert(portable_desk_face_load(&bad,&n)==3);bad=*kv;bad.put=NULL;assert(!portable_desk_face_save(&bad,0));
  for(unsigned mask=1;mask<8;++mask)for(unsigned fallback=0;fallback<3;++fallback)if(mask&(1u<<fallback))for(unsigned saved=0;saved<3;++saved){
    record_sleep(saved);unsigned before=sleep_writes;int status=portable_sleep_load_profile(kv,mask,fallback,&n);
    assert(status==((mask&(1u<<saved))?PORTABLE_SLEEP_LOADED:PORTABLE_SLEEP_UNSUPPORTED));assert(n==((mask&(1u<<saved))?saved:fallback)&&sleep_writes==before);
    if(!(mask&(1u<<saved)))assert(!portable_sleep_save_profile(kv,mask,saved)&&sleep_writes==before);
  }
  assert(portable_sleep_load_profile(kv,0,0,&n)==PORTABLE_SLEEP_INVALID);
  assert(portable_sleep_load_profile(kv,8,0,&n)==PORTABLE_SLEEP_INVALID);
  assert(!portable_sleep_save_profile(kv,8,0));
  record_sleep(2);assert(portable_sleep_load(kv,&n)==0&&n==2);kv_size=0;assert(portable_sleep_load(kv,&n)==PORTABLE_SLEEP_MISSING&&n==2);
}
int main(int argc,char **argv) {
  assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);scenario=test==106?301:300;
#if PORTABLE_DISPLAY_ROTATION == 0 || defined(TEST_DESK_RGB)
  assert(app_module_init()!=0);app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
  puts("Desk profile rejected unsupported display without I/O leaks");return 0;
#endif
  if(test==106) {
    assert(app_module_init()!=0&&paper_preferences_retained&&failed&&paper_preferences_grant.api);
    unsigned held=grants;assert(held==2&&!subscriptions&&!frame_count);
    app_module_fini();assert(grants==held&&paper_preferences_grant.api);
    assert(app_module_init()!=0&&grants==held);puts("Initial preference release failure retains exact grant and performs no later I/O");return 0;
  }
  assert(app_module_init()==0);assert(width()==(PORTABLE_DISPLAY_ROTATION==90?480:800)&&height()==(PORTABLE_DISPLAY_ROTATION==90?800:480)&&pp_enabled());
#ifdef TEST_DESK_SHORT
  info.width=600;info.height=400;
  risc_display_output_api_v1 short_display=display_api;short_display.acquire=short_frame_acquire;display=&short_display;
  assert(width()==400&&height()==600&&pp_enabled()&&sp_face_columns()==2);
#endif
  risc_touch_api_v1 input=touch_api;input.snapshot=desk_snapshot;touch.api=&input;
  risc_key_value_v1 kv={1,sizeof(kv),NULL,preference_get,preference_put};settings_store=alert_store=&kv;
  risc_runtime_api_v1 runtime=runtime_api;runtime.release=preference_release;runtime.yield_ms=idle_yield;rt=&runtime;
  risc_input_navigation_api_v1 nav=nav_api;nav.poll=scripted_nav;nav.reset=guarded_nav_reset;navigation=&nav;rtc_api.read=guarded_read;
  settings_render(0,0);assert(SETTINGS_FACE_ROW==7&&SETTINGS_ALERT_ROW==6&&SETTINGS_ABOUT_ROW==5);capture("root");
  uint8_t result=T5_APP_SETTING_NO_CHANGE;unsigned expected=0;
  if(test==0){helpers(&kv);face_writes=sleep_writes=0;}
  else if(test<=6){unsigned face=test-1;tap(3,200,150+face*84);tap(7,350,730);result=settings_activate(0,SETTINGS_FACE_ROW);expected=1;unsigned f;assert(portable_desk_face_load(&kv,&f)==0&&f==face);}
  else if(test==7||test==8){tap(3,200,570);if(test==7)tap(7,100,730);else nav_buttons[7]=RISC_NAV_BACK;result=settings_activate(0,SETTINGS_FACE_ROW);assert(!face_writes);}
  else if(test==9){nav_buttons[2]=RISC_NAV_LEFT;nav_buttons[4]=RISC_NAV_CONFIRM;result=settings_activate(0,SETTINGS_FACE_ROW);expected=1;assert(face_bytes[2]==5);}
  else if(test==10){for(unsigned i=0;i<7;++i)nav_buttons[2+i*2]=RISC_NAV_DOWN;nav_buttons[16]=RISC_NAV_CONFIRM;result=settings_activate(0,SETTINGS_FACE_ROW);expected=1;assert(face_bytes[2]==1);}
  else if(test==11){for(unsigned i=0;i<5;++i)nav_buttons[i]=RISC_NAV_CONFIRM;nav_buttons[8]=RISC_NAV_BACK;result=settings_activate(0,SETTINGS_FACE_ROW);assert(!face_writes);}
  else if(test==12||test==13){tap(3,200,570);if(test==12)gap_at=4;else fail_poll_at=4;tap(7,100,730);result=settings_activate(0,SETTINGS_FACE_ROW);assert(!face_writes);}
  else if(test>=14&&test<=18){put_error=test==14;commit_error=test==15;verify_error=test==16;mismatch=test==17;read_error=test==18;tap(3,200,570);tap(7,350,730);tap(11,100,730);result=settings_activate(0,SETTINGS_FACE_ROW);assert(face_writes==1&&face_unconfirmed);t5_app_setting_t item;assert(settings_get(0,SETTINGS_FACE_ROW,&item)&&!strcmp(item.value,"Unconfirmed"));}
  else if(test>=19&&test<=22){if(test==19)record_sleep(2);if(test==20){kv_size=4;memset(kv_bytes,255,4);}if(test==22)read_error=true;unsigned mode;int status=settings_sleep_load(&mode);assert(mode==0&&status==(test==19?4:test==20?2:test==21?1:3));tap(3,100,730);result=settings_activate(0,4);assert(!sleep_writes);assert(strstr(sleep_message,"Light default"));sv_switch(SV_SLEEP);capture(test==19?"sleep-stale":test==20?"sleep-invalid":test==21?"sleep-missing":"sleep-unavailable");}
  else if(test==23||test==24){tap(3,200,test==23?180:280);tap(7,350,730);result=settings_activate(0,4);expected=1;assert(sleep_writes==1&&kv_bytes[2]==(test==23?0:1));}
  else if(test==25){record_sleep(1);tap(3,200,400);tap(7,350,730);result=settings_activate(0,4);expected=1;assert(!sleep_writes&&sleep_choice==1);}
  else if(test==26){for(unsigned i=0;i<7;++i)nav_buttons[2+i*2]=RISC_NAV_RIGHT;nav_buttons[16]=RISC_NAV_CONFIRM;result=settings_activate(0,4);expected=1;assert(kv_bytes[2]==1);}
  else if(test==27||test==28){nav_buttons[4]=RISC_NAV_HOME;result=settings_activate(0,test==27?SETTINGS_FACE_ROW:4);assert(return_launches==1&&!face_writes&&!sleep_writes);unsigned before=displays;settings_view_redraw();assert(displays==before);}
  else if(test==29||test==30){retained_page=test==29?SV_DESK_FACE:SV_SLEEP;result=settings_activate(0,test==29?SETTINGS_FACE_ROW:4);assert(retained&&jumped&&failed&&!face_writes&&!sleep_writes);unsigned before=displays;settings_view_redraw();sv_switch(SV_ROOT);assert(!late_io&&displays==before);retained=false;app_module_fini();assert(!grants&&!subscriptions&&!frame_count);return 0;}
  else if(test==31){release_failures=1;unsigned before=diagnostics;app_module_fini();assert(diagnostics>before&&grants==1&&settings_grant.api);assert(test_release(&settings_grant));return 0;}
  else if(test>=32&&test<=37){face_choice=test-32;face_message="DRAFT ONLY - USE SAVE";sv_switch(SV_DESK_FACE);char name[32];snprintf(name,sizeof(name),"face-%u",face_choice);capture(name);}
  else if(test==38||test==39){sleep_choice=test-38;sleep_message="DRAFT ONLY - USE SAVE";sv_switch(SV_SLEEP);capture(test==38?"sleep-light":"sleep-deep");}
  else if(test==40){face_size=8;tap(3,100,730);result=settings_activate(0,SETTINGS_FACE_ROW);assert(strstr(face_message,"Invalid")&&!face_writes);}
  else if(test==41){nav_buttons[3]=RISC_NAV_BACK;result=settings_activate(0,SETTINGS_FACE_ROW);assert(strstr(face_message,"Missing")&&!face_writes);}
  else if(test==42){read_error=true;nav_buttons[3]=RISC_NAV_BACK;result=settings_activate(0,SETTINGS_FACE_ROW);assert(strstr(face_message,"Unavailable")&&!face_writes);}
  else if(test>=43&&test<=46){put_error=test==43;commit_error=test==44;verify_error=test==45;mismatch=test==46;tap(3,200,280);tap(7,350,730);tap(11,100,730);result=settings_activate(0,4);assert(sleep_unconfirmed&&sleep_writes==1);t5_app_setting_t item;assert(settings_get(0,4,&item)&&!strcmp(item.value,"Unconfirmed"));}
  else if(test>=47&&test<=54){
    unsigned choice=test-47;
    if(choice<6){unsigned columns=sp_face_columns();tap(3,32+(choice%columns)*(sp_face_width()+16)+sp_face_width()/2,120+(choice/columns)*sp_face_step()+(sp_face_step()-12)/2);}
    else tap(3,width()/2,sp_sleep_top()+(choice-6)*sp_sleep_step()+(sp_sleep_step()-24)/2);
    tap(7,width()*3/4,height()-68);result=settings_activate(0,choice<6?SETTINGS_FACE_ROW:4);expected=1;
    if(choice<6)assert(face_writes==1&&face_bytes[2]==choice);else assert(sleep_writes==1&&kv_bytes[2]==choice-6);
  }
  else if(test==55){sp_first=6;settings_render(0,8);capture("root-more");}
  else if(test==56){tap(3,350,730);tap(7,200,240);tap(11,200,570);tap(15,350,730);tap(19,100,730);app_main();assert(face_writes==1&&face_bytes[2]==5);}
  else if(test==57){nav_buttons[2]=nav_buttons[4]=nav_buttons[6]=RISC_NAV_UP;nav_buttons[8]=RISC_NAV_CONFIRM;nav_buttons[11]=RISC_NAV_RIGHT;nav_buttons[13]=RISC_NAV_CONFIRM;nav_buttons[17]=nav_buttons[19]=RISC_NAV_BACK;app_main();assert(face_writes==1&&face_bytes[2]==1);}
  else if(test==58){scenario=218;result=settings_activate(0,SETTINGS_FACE_ROW);assert(return_launches==1&&!face_writes&&!sleep_writes);}
  else if(test>=59&&test<=80) {
    unsigned language=test-59;reader_choice=language;
    for(unsigned i=0;i<language;++i)nav_buttons[2+i*2]=RISC_NAV_RIGHT;
    nav_buttons[2+language*2]=RISC_NAV_CONFIRM;
    result=settings_activate(0,SETTINGS_LANGUAGE_ROW);expected=1;
    assert(reader_bytes[1][2]==language&&reader_writes[1]==1);
    sv_switch(SV_LANGUAGE);char name[32];snprintf(name,sizeof(name),"language-%02u",language);capture(name);
  }
  else if(test==81 || test==82) {
    nav_buttons[2]=RISC_NAV_RIGHT;nav_buttons[4]=test==81?RISC_NAV_CONFIRM:RISC_NAV_BACK;
    result=settings_activate(0,SETTINGS_FLIP_ROW);expected=test==81;
    assert(paper_flip_ui==(test==81)&&reader_writes[0]==(test==81));capture("flip-saved");
  }
  else if(test>=83&&test<=86) {
    bool language=test>=85;nav_buttons[3]=RISC_NAV_HOME;
    result=settings_activate(0,language?SETTINGS_LANGUAGE_ROW:SETTINGS_FLIP_ROW);
    assert(return_launches==1&&!reader_writes[0]&&!reader_writes[1]);
  }
  else if(test>=87&&test<=96) {
    bool language=test>=92;unsigned error=(test-87)%5;
    put_error=error==0;commit_error=error==1;verify_error=error==2;mismatch=error==3;read_error=error==4;
    nav_buttons[2]=RISC_NAV_RIGHT;nav_buttons[4]=RISC_NAV_CONFIRM;nav_buttons[8]=RISC_NAV_BACK;
    result=settings_activate(0,language?SETTINGS_LANGUAGE_ROW:SETTINGS_FLIP_ROW);
    assert(reader_writes[language]==1&&reader_unconfirmed[language]&&!paper_flip_ui);
  }
  else if(test==97) {
    for(unsigned language=0;language<2;++language) {
      unsigned count=language?22:2,n=99;
      assert(portable_reader_preference_load(&kv,language,&n)==1&&n==0);
      assert(!portable_reader_preference_save(&kv,language,count));
      assert(portable_reader_preference_load(NULL,language,&n)==3&&n==0);
      assert(portable_reader_preference_load(&kv,language,NULL)==2);
      for(unsigned j=0;j<count;++j) {
        assert(portable_reader_preference_save(&kv,language,j));
        assert(portable_reader_preference_load(&kv,language,&n)==0&&n==j);
        unsigned before=reader_writes[language];assert(portable_reader_preference_save(&kv,language,j)&&reader_writes[language]==before);
      }
      for(unsigned j=0;j<4;++j) {reader_bytes[language][j]^=0x80;assert(portable_reader_preference_load(&kv,language,&n)==2&&n==0);reader_bytes[language][j]^=0x80;}
      for(unsigned j=1;j<=8;++j)if(j!=4){reader_size[language]=j;assert(portable_reader_preference_load(&kv,language,&n)==2&&n==0);}
      reader_size[language]=4;reader_bytes[language][2]=count;reader_bytes[language][3]=count^0xa5;assert(portable_reader_preference_load(&kv,language,&n)==2&&n==0);
    }
  }
  else if(test==98) {
    /* Physical pixels rotate exactly; logical touch follows, and old gesture
     * state cannot survive a toggle or a held contact after one. */
    uint8_t original[sizeof(framebuffer)];memcpy(original,framebuffer,sizeof(original));
    for(unsigned cycle=0;cycle<4;++cycle) {
      input_pending=true;input_sample=(portable_touch_sample){.released=true,.tap_eligible=true};touch.neutral=touch.down=true;
      assert(paper_apply_flip(!(cycle&1))&&!input_pending&&!touch.neutral&&!touch.down);
      settings_render(0,0);
      for(unsigned y=0;y<480;++y)for(unsigned x=0;x<800;++x) {
        unsigned px=cycle&1?x:799-x,py=cycle&1?y:479-y;
        assert(!!(framebuffer[y*100+x/8]&(0x80u>>(x&7)))==!!(original[py*100+px/8]&(0x80u>>(px&7))));
      }
      portable_touch_sample sample={.valid=true,.down=true,.x=11,.y=25};paper_orient_input(&sample);
      assert(sample.x==(cycle&1?11:468)&&sample.y==(cycle&1?25:774));
    }
    assert(paper_apply_flip(1));
    tap(polls+1,40,40);portable_touch_sample sample;input_service();input_take(&sample);
    assert(!sample.tap_eligible);
    assert(!paper_apply_flip(2));failed=true;assert(!paper_apply_flip(0)&&paper_flip_ui);failed=false;
  }
  else if(test==99||test==100) {
    reader_choice=test==99?0:21;sv_switch(SV_LANGUAGE);capture(test==99?"language-first":"language-last");
  }
  else if(test==101) {
    nav_buttons[2]=RISC_NAV_RIGHT;nav_buttons[4]=RISC_NAV_CONFIRM;
    assert(settings_activate(0,SETTINGS_FLIP_ROW)==T5_APP_SETTING_UPDATED&&paper_flip_ui);
    memset(nav_buttons,0,sizeof(nav_buttons));input_count=0;
    tap(polls+3,129,499);tap(polls+7,129,69);
    result=settings_activate(0,SETTINGS_FLIP_ROW);expected=1;
    assert(!paper_flip_ui&&reader_writes[0]==2&&reader_bytes[0][2]==0);
    assert(!input_pending&&!touch.down&&!touch.neutral);
  }
  else if(test>=102&&test<=105) {
    bool language=test>=104;tap(3,350,300);
    if(test%2)fail_poll_at=4;else gap_at=4;
    tap(7,100,730);result=settings_activate(0,language?SETTINGS_LANGUAGE_ROW:SETTINGS_FLIP_ROW);
    assert(!reader_writes[0]&&!reader_writes[1]&&!paper_flip_ui);
  }
  else assert(!"Unknown scenario");
  assert((result==T5_APP_SETTING_UPDATED)==(expected!=0));assert(!writes&&!format_writes);
  app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
  printf("Desk Settings scenario %u passed\n",test);
}
