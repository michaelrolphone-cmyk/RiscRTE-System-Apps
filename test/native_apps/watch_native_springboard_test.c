/* Real honeycomb controller + production presenter. One executable accepts
 * every display geometry; the provider and touch fixtures remain physical. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SPRINGBOARD_SOURCE
#define SPRINGBOARD_SOURCE "../../Apps/springboard.c"
#endif
#include SPRINGBOARD_SOURCE
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"

int app_module_init(void);
void app_module_fini(void);
extern const t5_app_manifest_t portable_catalog[];
extern const unsigned portable_catalog_count;
static unsigned fw,fh,pitch,ticks,polls,grants,subscriptions,frames,presents,launches;
static uint16_t *guarded,*pixels;
static const char *directory;
static char launched[128];
static bool use_async,expect_transition,expect_presentation=true;
static unsigned refresh_millihz=60000,present_latency_us=16000,display_flags;
static const char *metadata_profile="eligible";
/* Keep the exact provider output under observation throughout real presenter,
 * controller, input, submission and teardown. UI selection must never invent
 * refresh/latency values or rewrite any physical display metadata. */
static const risc_display_info_v1 *reported_info;
static risc_display_info_v1 reported_snapshot;
static bool contact_down;
static unsigned contact_x,contact_y,contact_id,contact_count;
static unsigned full_run; /* 1 launch, 2 return, 3 held entry, 4 drag, 5 idle fallback */
static const springboard_presentation *production;
static int logical_height(void) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 return (int)((fh*240u+fw-1)/fw);
#else
 return (int)fh;
#endif
}
static unsigned physical(unsigned logical) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 return (logical*fw+239u)/240u;
#else
 return logical;
#endif
}
static void guards(void) {
 if(reported_info)assert(!memcmp(reported_info,&reported_snapshot,sizeof(reported_snapshot)));
 assert(guarded[0]==0xa55a && guarded[fh*pitch+1]==0xa55a);
 for(unsigned y=0;y<fh;y++)for(unsigned x=fw;x<pitch;x++)assert(pixels[y*pitch+x]==0xa55a);
}
static bool nt_info(void *c,risc_display_info_v1 *out) {
 (void)c;*out=(risc_display_info_v1){.width=fw,.height=fh,
 .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
 .nominal_refresh_millihz=refresh_millihz,.typical_present_latency_us=present_latency_us,
 .flags=display_flags|(use_async?RISC_DISPLAY_INFO_ASYNC_PRESENT:0)};
 reported_info=out;memcpy(&reported_snapshot,out,sizeof(reported_snapshot));return true;
}
static bool nt_frame(void *c,uint32_t format,risc_display_surface_v1 *out) {
 (void)c;assert(!frames);frames=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.size_bytes=fh*pitch*2,
 .width=fw,.height=fh,.stride_bytes=pitch*2,.pixel_format=format};return true;
}
static void nt_frame_release(void *c,risc_display_frame_v1 f) {(void)c;assert(f==1&&frames);frames=0;}
static bool nt_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
 (void)c;(void)r;(void)n;(void)options;assert(f==1&&frames);frames=0;*token=++presents;guards();
 if(!refresh_millihz || !expect_presentation)assert(!springboard_transition_active());
 return true;
}
static bool nt_brightness(void *c,uint16_t value,uint16_t maximum) {(void)c;assert(value<=maximum);return true;}
static bool nt_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
 (void)c;assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static const risc_display_output_api_v1 nt_display={.api_version=1,.struct_size=sizeof(nt_display),
 .get_info=nt_info,.acquire=nt_frame,.release=nt_frame_release,.submit=nt_submit,.present_status=nt_status,.set_brightness=nt_brightness};
static uint64_t nt_subscribe(void *c) {(void)c;++subscriptions;return 1;}
static bool nt_unsubscribe(void *c,uint64_t id) {(void)c;assert(id==1&&subscriptions);--subscriptions;return true;}
static bool nt_poll_touch(void *c,size_t n) {(void)c;assert(n==1);++polls;return true;}
static int32_t nt_next(void *c,uint64_t id,risc_touch_event_v1 *e) {(void)c;(void)id;(void)e;return 0;}
static bool nt_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;*out=(risc_touch_snapshot_v1){.width=fw,.height=fh};
 bool down=contact_down;unsigned x=contact_x,y=contact_y;
 if(full_run) {
  down=full_run==5?false:full_run==3?polls<=3:polls>=2&&polls<=4;
  x=physical(full_run==2?20u:120u);y=physical(full_run==2?20u:(unsigned)logical_height()/2);
  if(full_run==4 && polls>=3){x=physical(120u+(polls-2)*15u);y=physical((unsigned)logical_height()/2+(polls-2)*12u);}
 }
 if(down){out->contact_count=contact_count;out->contacts[0]=(risc_touch_contact_v1){.id=contact_id,.x=x,.y=y};}
 return true;
}
static const risc_touch_api_v1 nt_touch={1,sizeof(nt_touch),NULL,nt_subscribe,nt_unsubscribe,nt_poll_touch,nt_next,nt_snapshot};
static bool nt_health(risc_runtime_health_v1 *out) {out->uptime_ms=ticks;return !full_run||polls<200;}
static bool nt_monotonic(uint32_t *out) {*out=ticks;return !full_run||polls<200;}
static void nt_yield(uint32_t n) {ticks+=n?n:1;}
static bool nt_diagnostic(const char *s) {(void)s;return true;}
static bool nt_launch(const char *name) {assert(strlen(name)<sizeof(launched));strcpy(launched,name);++launches;return true;}
static bool nt_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out) {
 assert(out->struct_size==sizeof(*out));
 if(!strcmp(name,"display.output")){assert(version==1&&!id);out->api=&nt_display;}
 else if(!strcmp(name,"input.touch.raw")){assert(version==1&&!id);out->api=&nt_touch;}
 else return false;
 ++grants;return true;
}
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
#include "PortableNavigation.h"
static bool nt_nav_poll(void *c,risc_input_navigation_frame_v1 *out) {(void)c;*out=(risc_input_navigation_frame_v1){0};return true;}
static bool nt_nav_foreground(void *c,const risc_input_foreground_v1 *claims,size_t n) {(void)c;(void)claims;(void)n;return true;}
static bool nt_nav_reset(void *c) {(void)c;return true;}
static const risc_input_navigation_api_v1 nt_navigation={1,sizeof(nt_navigation),NULL,nt_nav_poll,nt_nav_foreground,nt_nav_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1 *r) {(void)r;return &nt_navigation;}
void portable_input_navigation_close(const risc_runtime_api_v1 *r) {(void)r;}
bool portable_input_navigation_retained(void) {return false;}
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
#include "PortableAppSleep.h"
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
 const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
 (void)r;(void)d;(void)b;(void)a;assert(!"short Springboard fixture must not enter hardware sleep");return -1;
}
#endif
static bool nt_retain(void) {assert(!"healthy fixture cannot retain");return false;}
static bool nt_release(risc_runtime_capability_v1 *g) {assert(grants&&g->api);--grants;g->api=NULL;return true;}
static const risc_runtime_api_v1 nt_runtime={.api_version=1,.struct_size=sizeof(nt_runtime),.health=nt_health,
 .yield_ms=nt_yield,.diagnostic=nt_diagnostic,.request_launch=nt_launch,.acquire=nt_acquire,.release=nt_release,.monotonic_ms=nt_monotonic,.retain_invocation=nt_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {return version==1?&nt_runtime:NULL;}
static void capture(const char *name) {
 if(!directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *f=fopen(path,"wb");assert(f);
 fprintf(f,"P6\n%u %u\n255\n",fw,fh);
 for(unsigned y=0;y<fh;y++)for(unsigned x=0;x<fw;x++) {
  unsigned p=pixels[y*pitch+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
  assert(fwrite(rgb,1,3,f)==3);
 }
 assert(!fclose(f));
}
static void open_controller(void) {
 ticks=polls=launches=0;launched[0]=0;contact_down=false;contact_count=1;contact_id=0;
 assert(app_module_init()==0);api=t5_app_get_api(T5_APP_ABI_VERSION);assert(has_required_app_api());
 assert(api->screen_width()==(int)fw && api->screen_height()==(int)fh);
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 assert(springboard_watch_open());
#endif
 assert(api->screen_width()==240 && api->screen_height()==logical_height());
 count=api->installed_apps_count();assert(count==23 && portable_catalog_count==23);selected=0;layout();assert(compact);
 production=springboard_presentation_get();assert(!!production==expect_presentation);guards();
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 expect_transition=expect_presentation && refresh_millihz>=20000;
#endif
 /* The provider-facing API still reports physical geometry after app setup. */
 const t5_app_api_v1 *provider=t5_app_get_api(T5_APP_ABI_VERSION);
 assert(provider->screen_width()==(int)fw && provider->screen_height()==(int)fh);
}
static void close_controller(void) {
 app_module_fini();assert(!frames&&!grants&&!subscriptions);guards();
}
static springboard_contact read_contact(bool down,unsigned x,unsigned y) {
 contact_down=down;contact_x=x;contact_y=y;t5_app_input_t in={0};assert(api->poll(&in,20));
 springboard_contact c={0};production->contact(&c);return c;
}
static springboard_contact input(nova_state *s,bool down,unsigned x,unsigned y) {
 springboard_contact c=read_contact(down,physical(x),physical(y));
 nova_contact_input(s,&c,20);return c;
}
static void render(nova_state *s) {
 nova_render(s);guards();
 if(expect_transition) {assert(springboard_transition_active());expect_transition=false;}
 if(!refresh_millihz)assert(!springboard_transition_active());
 /* Observe the complete destination after the real retained-frame transition. */
 unsigned remaining=20;
 while(springboard_transition_active()&&remaining--){ticks+=40;nova_render(s);guards();}
 assert(!springboard_transition_active());
}
static void controller_tests(void) {
 open_controller();nova_state s={.view=production,.pending=-1,.initial=true};nova_layout(&s);
 assert(s.n==19);render(&s);capture("initial");
 int cy=logical_height()/2;
 assert(nova_hit(&s,120,cy)==0);assert(nova_hit(&s,145,cy)==0);assert(nova_hit(&s,146,cy)!=0);
 /* Every settled catalog entry is centered by the original axial honeycomb,
  * with the original strict hit target, then rendered by production pixels. */
 for(unsigned first=0;first<count;first+=19) {
  s.first=first;nova_layout(&s);
  for(unsigned i=0;i<s.n;i++) {
   s.motion.x=-(float)s.points[i].x/16;s.motion.y=-(float)s.points[i].y/16;s.focus=i;
   render(&s);assert(nova_hit(&s,120,cy)==(int)i);
   if(i==0){char name[32];snprintf(name,sizeof(name),"page-%u",first/19);capture(name);}
  }
 }
 s.first=0;nova_layout(&s);render(&s);
 input(&s,false,120,(unsigned)cy);springboard_contact c=input(&s,true,120,(unsigned)cy);
 assert(c.valid&&c.down&&c.began&&c.tap_eligible&&c.x==120&&c.y==cy);
 input(&s,true,150,(unsigned)cy+24);assert(s.moved>=54 && s.motion.x>0 && s.motion.y>0);
 input(&s,false,150,(unsigned)cy+24);assert(s.pending<0);
 for(unsigned i=0;i<100;i++)nova_tick(&s,20);
 render(&s);capture("after-drag");assert(s.pending<0&&!launches);
 /* Page indicator is bottom anchored and touchable in the taller lower area. */
 s.first=0;nova_layout(&s);render(&s);
 input(&s,false,120,(unsigned)logical_height()-10);
 input(&s,true,120,(unsigned)logical_height()-10);
 input(&s,false,120,(unsigned)logical_height()-10);
 assert(s.first==19&&s.n==4);render(&s);capture("page-touch");
 input(&s,true,120,(unsigned)logical_height()-10);input(&s,false,120,(unsigned)logical_height()-10);
 assert(s.first==0&&s.n==19);render(&s);
 /* A pending launch is cancelled by an invalid/out-of-surface contact. */
 input(&s,true,120,(unsigned)cy);input(&s,false,120,(unsigned)cy);assert(s.pending==0);
 c=read_contact(true,fw,fh/2);assert(!c.valid);nova_contact_input(&s,&c,20);assert(s.pending<0);
 read_contact(false,0,0);c=read_contact(true,fw/2,fh);assert(!c.valid);
 read_contact(false,0,0);contact_count=2;c=read_contact(true,physical(120),(unsigned)physical((unsigned)cy));
 assert(!c.valid);contact_count=1;read_contact(false,0,0);
 /* Last native row maps to the same visible logical cell; never a gutter. */
 c=read_contact(true,fw-1,fh-1);assert(c.valid&&c.x==239&&c.y==(int)((fh-1)*240/fw));
 read_contact(false,0,0);
 if(logical_height()>240) {
  s.first=0;nova_layout(&s);s.motion.y=15;render(&s);capture("lower-honeycomb");
  int lower=-1,lowx=0,lowy=0;
  for(unsigned i=0;i<s.n;i++) {
   int x=120+s.points[i].x/16,y=cy+s.points[i].y/16+15;
   if(y>240 && y<logical_height()-24 && nova_hit(&s,x,y)==(int)i){lower=(int)i;lowx=x;lowy=y;break;}
  }
  assert(lower>=0);
  unsigned colored=0;for(unsigned y=fw;y<fh;y++)for(unsigned x=0;x<fw;x++)colored+=pixels[y*pitch+x]!=0;
  assert(colored>1000);
  c=input(&s,true,(unsigned)lowx,(unsigned)lowy);assert(c.x==lowx&&c.y==lowy);
  input(&s,false,(unsigned)lowx,(unsigned)lowy);assert(s.motion.snapping&&s.motion.target==(unsigned)lower);
  assert(s.pending<0); /* Second ring moves to focus before opening. */
 }
 close_controller();
}
static void application_tests(void) {
 for(unsigned mode=1;mode<=4;mode++) {
  full_run=mode;open_controller();
  /* app_main must independently select the same full-height honeycomb. */
  app_main();
  if(mode==1)assert(launches==1&&!strcmp(launched,"clock.elf"));
#ifdef PORTABLE_RETURN_APP
  else if(mode==2)assert(launches==1&&!strcmp(launched,PORTABLE_RETURN_APP));
#endif
  else assert(!launches);
  if(mode==1)capture("launch-complete");
  close_controller();
 }
 full_run=0;
 /* Re-entry after launch/return starts clean and recreates the initial frame. */
 open_controller();nova_state s={.view=production,.pending=-1,.initial=true};nova_layout(&s);render(&s);capture("reentry");close_controller();
}
static void fallback_tests(void) {
 full_run=5;open_controller();assert(!production && !nova_run());
 unsigned before=presents;app_main();assert(presents>before && !launches);
 assert(!springboard_transition_active());capture("generic-grid");close_controller();full_run=0;
}
static void select_metadata(const char *profile) {
 metadata_profile=profile;
 if(!strcmp(profile,"eligible"))return;
 if(!strcmp(profile,"eligible-threshold")){refresh_millihz=20000;present_latency_us=50000;return;}
 if(!strcmp(profile,"unknown-refresh")){refresh_millihz=0;return;}
 if(!strcmp(profile,"unknown-timing")){refresh_millihz=present_latency_us=0;return;}
 expect_presentation=false;
 if(!strcmp(profile,"slow-refresh")){refresh_millihz=19999;return;}
 if(!strcmp(profile,"slow-latency")){present_latency_us=50001;return;}
 if(!strcmp(profile,"retains-image")){display_flags=RISC_DISPLAY_INFO_RETAINS_IMAGE;return;}
 if(!strcmp(profile,"unknown-slow-latency")){refresh_millihz=0;present_latency_us=50001;return;}
 if(!strcmp(profile,"unknown-retains-image")){refresh_millihz=0;display_flags=RISC_DISPLAY_INFO_RETAINS_IMAGE;return;}
 assert(!"unrecognized metadata profile");
}
int main(int argc,char **argv) {
 assert(argc>=3&&argc<=6);fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);directory=argc>=4&&strcmp(argv[3],"-")?argv[3]:NULL;use_async=argc>=5&&!strcmp(argv[4],"async");
 if(argc==6)select_metadata(argv[5]);
 assert(fw>=160&&fw<=1024&&fh>=fw&&fh<=1024);pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 if(expect_presentation){controller_tests();application_tests();}else fallback_tests();
 free(guarded);
 printf("Springboard %ux%u %s: %s; exact provider metadata, transition policy and guards passed\n",fw,fh,metadata_profile,
  expect_presentation?"original honeycomb, 23 apps, two pages, actual mapped touch, pan, lower hits, launch/return/re-entry":"known-ineligible metadata keeps generic grid");
 return 0;
}
