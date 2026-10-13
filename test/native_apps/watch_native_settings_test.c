/* Production Settings controller/render/input at native rectangular dimensions. */
#define main legacy_alarm_main
#include "portable_alarm_settings_test.c"
#undef main
static uint16_t at(unsigned x,unsigned y) {
  unsigned px=(x*SETTINGS_FIXTURE_WIDTH+239u)/240u;
  unsigned py=(y*SETTINGS_FIXTURE_WIDTH+239u)/240u;
  assert(px<SETTINGS_FIXTURE_WIDTH && py<SETTINGS_FIXTURE_HEIGHT);
  return framebuffer[py*SETTINGS_FIXTURE_PITCH+px];
}
static void padding(void) {
  for(unsigned y=0;y<SETTINGS_FIXTURE_HEIGHT;y++)
    for(unsigned x=SETTINGS_FIXTURE_WIDTH;x<SETTINGS_FIXTURE_PITCH;x++)
      assert(framebuffer[y*SETTINGS_FIXTURE_PITCH+x]==0xa55a);
}
static void capture(const char *dir,unsigned page) {
  if(!dir)return;
  char path[512];snprintf(path,sizeof(path),"%s/settings-%02u.ppm",dir,page);
  FILE *f=fopen(path,"wb");assert(f);
  fprintf(f,"P6\n%u %u\n255\n",SETTINGS_FIXTURE_WIDTH,SETTINGS_FIXTURE_HEIGHT);
  for(unsigned y=0;y<SETTINGS_FIXTURE_HEIGHT;y++)for(unsigned x=0;x<SETTINGS_FIXTURE_WIDTH;x++) {
    unsigned p=framebuffer[y*SETTINGS_FIXTURE_PITCH+x];
    unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
    assert(fwrite(rgb,1,3,f)==3);
  }
  assert(!fclose(f));
}
int main(int argc,char **argv) {
  assert(argc<=2);scenario=100;
  for(unsigned i=0;i<sizeof(framebuffer)/sizeof(*framebuffer);i++)framebuffer[i]=0xa55a;
  assert(app_module_init()==0);
  assert(sv_height()==(SETTINGS_FIXTURE_HEIGHT*240+SETTINGS_FIXTURE_WIDTH-1)/SETTINGS_FIXTURE_WIDTH);
  settings_render(0,0);padding();capture(argc==2?argv[1]:NULL,0);
  int extra=sv_height()-240;
  assert(sv_inside(83,189+extra,82,192,76,26));
  assert(!sv_inside(83,188+extra-1,82,192,76,26));
  assert(!sv_inside(83,232+extra,82,192,76,26));
  if(extra) {
    assert(at(20,222)!=0); /* Additional third row, previously covered by footer. */
    assert(at(82,200+extra)!=0); /* Original-width bottom-anchored Back outline. */
    assert(!sv_inside(83,200,82,192,76,26));
  }
  sv_page=SV_FIELDS;assert(sv_max_scroll()==168-extra);
  sv_scroll[1]=10000;sv_clamp_scroll();assert(sv_scroll[1]==168-extra);sv_scroll[1]=0;
  settings_draft=(twatch_rtc_time_v1){2026,10,4,0,0,5,0};editor_field=3;
  const unsigned pages[]={SV_FIELDS,SV_VALUE,SV_TIME_FORMAT,SV_SLEEP,SV_ALERT,SV_ABOUT,SV_FOLD};
  for(unsigned i=0;i<sizeof(pages)/sizeof(*pages);i++){sv_switch(pages[i]);padding();capture(argc==2?argv[1]:NULL,i+1);}
  /* Native provider contacts exercise the real inverse map and controller. */
  sv_switch(SV_ROOT);time_format_mode=0;
  tap(3,120,160);tap(4,120,60);tap(8,120,96);tap(12,200,96);
  tap(16,120,160);tap(20,120,210);tap(24,60,210);
  assert(settings_activate(0,0)==T5_APP_SETTING_NO_CHANGE);
  assert(settings_draft.hour==1);assert(!rtc_writes && !kv_writes);padding();
  /* Footer contact remains reachable in the native lower region; outside
   * contacts cannot be clamped into a control. */
  uint16_t x,y;
  assert(watch_layout_touch(&native_watch_layout,SETTINGS_FIXTURE_WIDTH-1,SETTINGS_FIXTURE_HEIGHT-1,&x,&y));
  assert(x==239 && y==(SETTINGS_FIXTURE_HEIGHT-1)*240/SETTINGS_FIXTURE_WIDTH);
  assert(!watch_layout_touch(&native_watch_layout,SETTINGS_FIXTURE_WIDTH,0,&x,&y));
  assert(!watch_layout_touch(&native_watch_layout,0,SETTINGS_FIXTURE_HEIGHT,&x,&y));
  app_module_fini();assert(!grants && !subscriptions && !frames);
  puts("Native Settings: full-height rows, footer hits, actual edit/cancel, padding and bounds passed");
  return 0;
}
