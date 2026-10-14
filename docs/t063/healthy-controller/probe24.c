#include "test/native_apps/fixtures/installed_catalog_provider.h"
/* Real honeycomb controller + production presenter. One executable accepts
 * every display geometry; the provider metadata and touch mappings are preserved. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SPRINGBOARD_SOURCE
#define SPRINGBOARD_SOURCE "Apps/springboard.c"
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
static char launched[256];
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
static risc_touch_event_v1 touch_events[32];
static unsigned event_head,event_count;
static uint64_t touch_sequence;
static risc_touch_snapshot_v1 reported_touch;
static bool nt_snapshot(void *,risc_touch_snapshot_v1 *);
static void nt_edge(unsigned kind,const risc_touch_contact_v1 *contact) {
 assert(event_count<32);
 touch_events[(event_head+event_count++)%32]=(risc_touch_event_v1){++touch_sequence,ticks,(uint8_t)kind,contact->id,contact->x,contact->y};
}
static bool nt_poll_touch(void *c,size_t n) {
 (void)c;assert(n==1);++polls;risc_touch_snapshot_v1 next;nt_snapshot(NULL,&next);
 for(unsigned i=0;i<reported_touch.contact_count;i++) {
  const risc_touch_contact_v1 *old=&reported_touch.contacts[i];bool found=false;
  for(unsigned j=0;j<next.contact_count;j++)if(next.contacts[j].id==old->id)found=true;
  if(!found)nt_edge(RISC_TOUCH_EVENT_UP,old);
 }
 for(unsigned i=0;i<next.contact_count;i++) {
  const risc_touch_contact_v1 *v=&next.contacts[i],*old=NULL;
  for(unsigned j=0;j<reported_touch.contact_count;j++)if(reported_touch.contacts[j].id==v->id)old=&reported_touch.contacts[j];
  if(!old)nt_edge(RISC_TOUCH_EVENT_DOWN,v);
  else if(old->x!=v->x||old->y!=v->y)nt_edge(RISC_TOUCH_EVENT_MOVE,v);
 }
 reported_touch=next;return true;
}
static int32_t nt_next(void *c,uint64_t id,risc_touch_event_v1 *e) {
 (void)c;(void)id;if(!event_count)return 0;*e=touch_events[event_head];event_head=(event_head+1)%32;--event_count;return 1;
}
static bool nt_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;*out=(risc_touch_snapshot_v1){.width=fw,.height=fh,.sequence=touch_sequence,.timestamp_ms=ticks};
 bool down=contact_down;unsigned x=contact_x,y=contact_y;
 if(full_run) {
  down=full_run==5?false:full_run==3?polls<=3:polls>=2&&polls<=4;
  x=physical(full_run==2?20u:120u);y=physical(full_run==2?20u:(unsigned)logical_height()/2);
  if(full_run==4 && polls>=3){x=physical(120u+(polls-2)*15u);y=physical((unsigned)logical_height()/2+(polls-2)*12u);}
 }
 if(down){out->contact_count=contact_count;out->contacts[0]=(risc_touch_contact_v1){.id=contact_id,.x=x,.y=y};if(contact_count>1)out->contacts[1]=(risc_touch_contact_v1){.id=(uint8_t)(contact_id+1),.x=x,.y=y};}
 return true;
}
static const risc_touch_api_v1 nt_touch={1,sizeof(nt_touch),NULL,nt_subscribe,nt_unsubscribe,nt_poll_touch,nt_next,nt_snapshot};
static bool nt_health(risc_runtime_health_v1 *out) {out->uptime_ms=ticks;return true;}
static bool nt_cancel(void){return full_run&&polls>=200;}
static bool nt_monotonic(uint32_t *out) {*out=ticks;return true;}
static void nt_yield(uint32_t n) {ticks+=n?n:1;}
static bool nt_diagnostic(const char *s) {(void)s;return true;}
static bool nt_launch(const char *name) {assert(strlen(name)<sizeof(launched));strcpy(launched,name);++launches;return true;}

#include "ContextsServiceV1.h"
#include "TelemetryBroadcastV1.h"
#include "AlarmServiceV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscPowerKeyV1.h"
#include "PortableQuickRadios.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
static unsigned context_steps,context_pauses;
static bool pause_refuse;
static bool ctx_pause(void*c){(void)c;context_pauses++;return !pause_refuse;}
static bool ctx_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;context_steps++;return true;}
static bool ctx_status(void*c,contexts_status_v1*out){(void)c;*out=(contexts_status_v1){.struct_size=sizeof(*out)};return true;}
static bool ctx_mask(void*c,uint32_t mask){(void)c;(void)mask;return true;}
static bool ctx_export(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;return true;}
static bool ctx_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static int32_t ctx_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*out){(void)c;(void)a;(void)b;(void)out;return 0;}
static bool ctx_claim(void*c,uint32_t a,uint32_t b,const char*n,uint32_t d){(void)c;(void)a;(void)b;(void)n;(void)d;return false;}
static bool ctx_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;return true;}
static bool ctx_capture(void*c){(void)c;return true;}
static const contexts_service_v1 ctx_api={.api_version=1,.struct_size=sizeof(ctx_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_mask,.begin_export=ctx_mask,.export_record=ctx_export,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_capture};

#include "ContextFingerprintService.h"
static bool healthy_fingerprint(void*c,uint32_t op,void*request){(void)c;if(op==CONTEXTS_FP_CONFIG)return true;if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=request;s->sources=CONTEXTS_ALL;s->dirty_sources=0;return true;}assert(op==CONTEXTS_FP_RULES||op==CONTEXTS_FP_EVALUATE);return true;}
static contexts_fingerprint_service_v1 extended_contexts;
static bool yes(void*c){(void)c;return true;}
static bool bstep(void*c,bool a,const telemetry_broadcast_policy_v1*p){(void)c;(void)a;(void)p;return true;}
static bool bstatus(void*c,telemetry_broadcast_status_v1*p){(void)c;*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p)};return true;}
static int32_t benum(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;return 0;}
static int32_t bread(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,bstep,yes,bstatus,benum,bread};
static int32_t astatus(void*c,alarm_status_v1*p){(void)c;*p=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*p),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t astep(void*c){(void)c;return ALARM_OK;}
static int32_t aack(void*c,const alarm_token_v1*p){(void)c;(void)p;return ALARM_OK;}
static int32_t asleep(void*c,alarm_sleep_v1*p){(void)c;(void)p;return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,astatus,astep,astep,aack,asleep,astep};
static bool battery_read(void*c,risc_battery_sample_v1*p){(void)c;*p=(risc_battery_sample_v1){4000,75,0};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,battery_read};
static int32_t kvget(void*c,const char*k,void*p,uint32_t z,uint32_t*n){(void)c;(void)k;(void)p;(void)z;*n=0;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t kvput(void*c,const char*k,const void*p,uint32_t z){(void)c;(void)k;(void)p;(void)z;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,kvget,kvput};
static bool keyevents(void*c,uint32_t*p){(void)c;*p=0;return true;}
static const risc_power_key_api_v1 key_api={1,sizeof(key_api),NULL,keyevents};
static wifi_link_t wstatus(void*c){(void)c;return WIFI_LINK_DOWN;}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=wstatus,.disconnect_checked=yes};
static bool bset(void*c,bool e){(void)c;(void)e;return true;}
static bool btstatus(void*c,uint8_t*p){(void)c;*p=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 bluetooth_api={.api_version=1,.struct_size=sizeof(bluetooth_api),.set_enabled=bset,.status=btstatus};
static bool rread(void*c,twatch_rtc_time_v1*p){(void)c;*p=(twatch_rtc_time_v1){2026,10,14,3,12,0,0};return true;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rread};
static unsigned peak_grants,generation;static bool slots[16];
static const void *apis[16];static unsigned generations[16];
#include "RiscKeyValueAtomicV1.h"
static int32_t atomicstate(void*c,risc_key_value_atomic_state_v1*p){(void)c;*p=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*p),.flags=3,.namespace_id=1,.max_mutations=16,.max_value_bytes=2048,.max_group_bytes=4096,.max_intent_bytes=176,.revision=1};return 0;}
static int32_t atomicget(void*c,uint64_t r,const char*k,void*p,uint32_t z,uint32_t*n){(void)c;(void)r;(void)k;(void)p;(void)z;*n=0;return RISC_ATOMIC_NOT_FOUND;}
static int32_t atomicsubmit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*p,uint32_t n,const void*i,uint32_t z,uint64_t*t){(void)c;(void)r;(void)p;(void)n;(void)i;(void)z;*t=0;assert(!"no writes expected");return RISC_ATOMIC_INVALID;}
static int32_t atomiccollect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*p){(void)c;(void)t;(void)p;assert(!"no pending writes");return RISC_ATOMIC_INVALID;}
static int32_t atomiccancel(void*c,uint64_t t){(void)c;(void)t;assert(!"no pending writes");return RISC_ATOMIC_INVALID;}
static const risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomicstate,atomicget,atomicsubmit,atomiccollect,atomiccancel};
static unsigned allocation_count,peak_allocations;static size_t allocation_bytes,peak_bytes;
static struct {void *p;size_t bytes;} allocations[128];
void *__real_malloc(size_t);void __real_free(void*);
void *__wrap_malloc(size_t bytes){void*p=__real_malloc(bytes);if(p){unsigned i=0;while(i<128&&allocations[i].p)i++;assert(i<128);allocations[i].p=p;allocations[i].bytes=bytes;allocation_count++;allocation_bytes+=bytes;if(allocation_count>peak_allocations)peak_allocations=allocation_count;if(allocation_bytes>peak_bytes)peak_bytes=allocation_bytes;}printf("MALLOC bytes=%zu live=%u total=%zu\n",bytes,allocation_count,allocation_bytes);return p;}
void __wrap_free(void*p){if(p)for(unsigned i=0;i<128;i++)if(allocations[i].p==p){allocation_count--;allocation_bytes-=allocations[i].bytes;allocations[i].p=NULL;break;}__real_free(p);}

static bool nt_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out) {
 assert(out->struct_size==sizeof(*out));
 if(!strcmp(name,RISC_APP_CATALOG_CAPABILITY)){assert(version==1&&!id);out->api=&fixture_catalog_api;}
 else if(!strcmp(name,"display.output")){assert(version==1&&!id);out->api=&nt_display;}
 else if(!strcmp(name,"input.touch.raw")){assert(version==1&&!id);out->api=&nt_touch;}
 else if(!strcmp(name,"storage.key-value.atomic"))out->api=&atomic_api;
 else if(!strcmp(name,"contexts.service"))out->api=&extended_contexts;
 else if(!strcmp(name,"telemetry.broadcast"))out->api=&broadcast_api;
 else if(!strcmp(name,"alarm.service"))out->api=&alarm_api;
 else if(!strcmp(name,"board.battery"))out->api=&battery_api;
 else if(!strcmp(name,"storage.key-value"))out->api=&kv_api;
 else if(!strcmp(name,"input.power-key"))out->api=&key_api;
 else if(!strcmp(name,"net.wifi"))out->api=&wifi_api;
 else if(!strcmp(name,"bluetooth.hci"))out->api=&bluetooth_api;
 else if(!strcmp(name,"rtc.clock"))out->api=&rtc_api;
 else {printf("MISSING %s\n",name);return false;}
 assert(grants<16);unsigned slot=0;while(slot<16&&slots[slot])slot++;assert(slot<16);slots[slot]=true;apis[slot]=out->api;generations[slot]=++generation;++grants;if(grants>peak_grants)peak_grants=grants;out->slot=slot+1;out->generation=generation;printf("ACQUIRE %-24s live=%u\n",name,grants);return true;
}
#ifdef PORTABLE_APP_SLEEP_LOCAL
#include "PortableAppSleep.h"
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
 const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
 (void)r;(void)d;(void)b;(void)a;assert(!"short Springboard fixture must not enter hardware sleep");return -1;
}
#endif
static bool nt_retain(void) {assert(!"healthy fixture cannot retain");return false;}
static bool nt_release(risc_runtime_capability_v1 *g) {assert(grants&&g->api&&g->slot&&g->slot<=16);unsigned slot=g->slot-1;assert(slots[slot]&&apis[slot]==g->api&&generations[slot]==g->generation);slots[slot]=false;apis[slot]=NULL;--grants;g->api=NULL;g->slot=g->generation=0;return true;}
static const risc_runtime_api_v1 nt_runtime={.api_version=1,.struct_size=sizeof(nt_runtime),.health=nt_health,
 .yield_ms=nt_yield,.diagnostic=nt_diagnostic,.request_launch=nt_launch,.acquire=nt_acquire,.release=nt_release,.monotonic_ms=nt_monotonic,.retain_invocation=nt_retain,.exit_requested=nt_cancel};
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
 ticks=polls=launches=0;event_head=event_count=0;touch_sequence=0;reported_touch=(risc_touch_snapshot_v1){0};launched[0]=0;contact_down=false;contact_count=1;contact_id=0;
 assert(app_module_init()==0);api=t5_app_get_api(T5_APP_ABI_VERSION);assert(has_required_app_api());
 assert(api->screen_width()==(int)fw && api->screen_height()==(int)fh);
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 assert(springboard_watch_open());
#endif
 assert(api->screen_width()==240 && api->screen_height()==logical_height());
 count=api->installed_apps_count();assert(count==24 && portable_catalog_count==24);selected=0;layout();assert(compact);
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
 /* Multi-contact cancellation drains one ordered edge per app poll. Retain
  * the observed cancellation while reaching the neutral stream boundary. */
 springboard_contact c={0};production->contact(&c);bool cancelled=!c.valid||c.cancelled;
 bool drained=false;unsigned limit=32;
 while(event_count && limit--){assert(api->poll(&in,20));production->contact(&c);cancelled|=!c.valid||c.cancelled;drained=true;}
 assert(!event_count);if(drained){assert(api->poll(&in,20));production->contact(&c);}
 if(cancelled){c.valid=false;c.cancelled=true;}return c;
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
 assert(s.first==19&&s.n==5);render(&s);capture("page-touch");
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
  if(mode==1)assert(launches==1&&!strcmp(launched,"alarms.elf"));
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
 extended_contexts=(contexts_fingerprint_service_v1){.base=ctx_api,.fingerprint_abi=0x31504643u,.fingerprint=healthy_fingerprint};extended_contexts.base.struct_size=sizeof(extended_contexts);
 assert(argc>=3&&argc<=6);fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);directory=argc>=4&&strcmp(argv[3],"-")?argv[3]:NULL;use_async=argc>=5&&!strcmp(argv[4],"async");
 if(argc==6)select_metadata(argv[5]);
 assert(fw>=160&&fw<=1024&&fh>=fw&&fh<=1024);pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 if(getenv("RUN_ALL")){if(expect_presentation){controller_tests();application_tests();}else fallback_tests();}
 else {open_controller(); printf("CATALOG count=%u live=%u peak=%u\n",count,grants,peak_grants);close_controller();}
 free(guarded);printf("HEALTHY FINAL live_grants=%u peak_grants=%u live_allocations=%u peak_allocations=%u peak_bytes=%zu\n",grants,peak_grants,allocation_count,peak_allocations,peak_bytes);assert(!grants&&!allocation_count);
 printf("Springboard %ux%u %s: %s; exact provider metadata, transition policy and guards passed\n",fw,fh,metadata_profile,
  expect_presentation?"original honeycomb, 24 apps, two pages, actual mapped touch, pan, lower hits, launch/return/re-entry":"known-ineligible metadata keeps generic grid");
 return 0;
}
