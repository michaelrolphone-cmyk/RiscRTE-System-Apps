/* Actual Settings adapter/controller with independently dimensioned providers. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size;
#define main retained_alert_fixture_main
#include SETTINGS_FRAME_FIXTURE
#undef main
static void guards(void) {
  const uint8_t*p=(const uint8_t*)framebuffer;
  for(unsigned y=0;y<test_height;y++)for(unsigned x=test_width*2;x<test_stride;x++)assert(p[y*test_stride+x]==0xa5);
}
static void capture(const char *directory,const char *name) {
  char path[1024];snprintf(path,sizeof(path),"%s/%ux%u-%s.ppm",directory,test_width,test_height,name);
  FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",test_width,test_height);
  for(unsigned y=0;y<test_height;y++)for(unsigned x=0;x<test_width;x++){
    const uint8_t*p=(const uint8_t*)framebuffer+y*test_stride+2*x;unsigned v=p[0]+256u*p[1];
    unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);
  }
  assert(!fclose(f));guards();
}
static void script_reset(void){contact_count=0;script_started_at=ticks;}
static void script_choice(unsigned row,unsigned *next) {
  /* Independent geometry expectations from the UI contract, not production
   * rect helpers. One full row per swipe keeps small displays navigable. */
  unsigned top=test_height<200?28:56,rh=test_height<200?32:44;
  unsigned fh=test_height<200?28:44,footer=test_height-8-fh;
  unsigned rows=(footer-18-top)/rh;assert(rows);
  unsigned first=row>=rows?row-rows+1:0,t=3;
  for(unsigned i=0;i<first;i++){
    tap(t,test_width/2,top+rh/2);tap(t+1,test_width/2,top+rh/2-rh);t+=4;
  }
  tap(t,test_width/2,top+(row-first)*rh+rh/2);*next=t+4;
}
int main(int argc,char **argv) {
  assert(argc==4);test_width=(unsigned)atoi(argv[2]);test_height=(unsigned)atoi(argv[3]);
  assert(test_width>=128&&test_height>=128);test_stride=test_width*2+6;test_size=test_stride*test_height;
  framebuffer=malloc(test_size);assert(framebuffer);memset(framebuffer,0xa5,test_size);scenario=100;
  assert(app_module_init()==0);assert(sn_enabled());settings_render(0,0);capture(argv[1],"root");
  unsigned margin=test_width<200?8:20,fh=test_height<200?28:44,footer=test_height-8-fh;
  sn_rect back=sn_control(82,192,76,26);
  assert(back.x==(int)margin&&back.y==(int)footer&&back.w==(int)(test_width-2*margin)&&back.h==(int)fh);
  assert(sv_inside(back.x,back.y,82,192,76,26));assert(sv_inside(back.x+back.w-1,back.y+back.h-1,82,192,76,26));
  assert(!sv_inside(back.x+back.w,back.y,82,192,76,26));assert(!sv_inside(back.x,back.y+back.h,82,192,76,26));
  for(unsigned i=0;i<SETTINGS_ROW_COUNT;i++){
    settings_render(0,(int)i+1);sn_rect r=sn_row(i,sv_scroll[0]);assert(r.w>0);
    assert(r.y>=sn_geometry().top&&r.y+r.h<=sn_geometry().bottom);guards();
  }
  const unsigned pages[]={SV_FIELDS,SV_VALUE,SV_FOLD,SV_ABOUT,SV_SLEEP,SV_ALERT,SV_TIME_FORMAT};
  settings_draft=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};editor_field=3;
  for(unsigned i=0;i<sizeof(pages)/sizeof(*pages);i++){
    sv_switch(pages[i]);char label[32];snprintf(label,sizeof(label),"page-%u",pages[i]);capture(argv[1],label);
  }
  /* Physical/keyboard selection must reveal complete field rows at every size. */
  sv_switch(SV_FIELDS);
  for(unsigned i=0;i<6;i++){editor_select_field(i);sn_rect r=sn_row(i,sv_scroll[1]);assert(r.w>0);assert(sv_scroll[1]%sn_geometry().row==0);}
  unsigned next;
  script_reset();script_choice(5,&next);
  unsigned top=test_height<200?28:56,side=test_width<200?32:44;
  tap(next,test_width-margin-side/2,top+side/2);next+=4;
  tap(next,test_width/2,footer+fh/2);next+=4;
  unsigned gap=test_width<200?6:12,half=(test_width-2*margin-gap)/2;
  tap(next,margin+half/2,footer+fh/2);
  assert(settings_activate(0,0)==T5_APP_SETTING_NO_CHANGE);
  assert(settings_draft.second==1&&editor_field==5&&!rtc_writes);
  /* The native ten-segment indicator is also its actual slider target. */
  script_reset();script_choice(0,&next);
  unsigned slider_y=test_height<160?footer-24:top+side+12;
  tap(next,margin,slider_y+2);tap(next+1,test_width-margin-1,slider_y+2);next+=5;
  tap(next,test_width/2,footer+fh/2);next+=4;tap(next,margin+half/2,footer+fh/2);
  assert(settings_activate(0,0)==T5_APP_SETTING_NO_CHANGE);
  unsigned lo,hi;editor_bounds(0,&lo,&hi);assert(editor_value(0)==hi&&!rtc_writes);
  script_reset();script_choice(2,&next);
  tap(next,margin+half+gap+half/2,footer+fh/2);
  assert(settings_activate(0,SETTINGS_ALERT_ROW)==T5_APP_SETTING_UPDATED);
  assert(kv_writes==1&&alert_choice==PORTABLE_ALERT_BOTH&&!rtc_writes);guards();
  /* A later entry cancels a changed RAM choice without a second write. */
  script_reset();script_choice(0,&next);tap(next,margin+half/2,footer+fh/2);
  assert(settings_activate(0,SETTINGS_ALERT_ROW)==T5_APP_SETTING_NO_CHANGE);
  assert(kv_writes==1&&!rtc_writes);guards();
  app_module_fini();assert(!grants&&!subscriptions&&!frames);
  assert(kv_grants==kv_releases);kv_grants=kv_releases=0;
  script_reset();assert(app_module_init()==0);settings_render(0,0);assert(!sv_scroll[0]);
  app_module_fini();assert(!grants&&!subscriptions&&!frames);guards();free(framebuffer);
  printf("Native Settings %ux%u: all pages, reachable rows, exact footer bounds, real swipe/save/cancel/reentry and guarded native pixels passed\n",test_width,test_height);
  return 0;
}
