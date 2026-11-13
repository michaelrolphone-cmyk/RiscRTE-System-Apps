/* Production adapter with deterministic provider media, never hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_SCENE_QUICK
#define TEST_SCENE_ALARM
#define PORTABLE_QUICK_ACTIONS
#ifndef PORTABLE_QUICK_SAFE_CONTENT
#define PORTABLE_QUICK_SAFE_CONTENT
#endif
#ifndef PORTABLE_QUICK_CAPABILITY_SAFE
#define PORTABLE_QUICK_CAPABILITY_SAFE
#endif
#define PORTABLE_QUICK_ATOMIC_PREFERENCES
#define PORTABLE_PREFERENCE_STORE
#ifndef PORTABLE_WATCH_NATIVE_HEIGHT
#define PORTABLE_WATCH_NATIVE_HEIGHT
#endif
#define PORTABLE_WATCH_COLOR_ONLY
#endif
#ifdef TEST_SCENE_ALARM
#define PORTABLE_ALARM_CLIENT
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_ALARM_TERMINAL_RETENTION
#endif
#ifdef TEST_SCENE_LIGHT
#define PORTABLE_FOREGROUND_IDLE
#define PORTABLE_APP_SLEEP_LOCAL
#endif
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_NATIVE_SCENE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_APP_OWNS_TOUCH_CHROME
#define PORTABLE_RF_NATIVE_CANVAS
#define PORTABLE_NOVA_UI
#include "../../lib/PortableApps/src/adapter.c"
#ifdef TEST_SCENE_QUICK
#include "PortableQuickLayout.h"
#endif
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
#ifdef TEST_SCENE_ROUND
#define TEST_SIZE 466u
#define PORTABLE_TEST_INSET 70u
#else
#define TEST_SIZE 240u
#define PORTABLE_TEST_INSET 0u
#endif
static uint16_t pixels[TEST_SIZE*TEST_SIZE];
static unsigned now,calls,grants,frames,submitted,completed,snapshots;
static bool retained,down,bad_poll,bad_snapshot,bad_next;
static bool auto_complete,bad_geometry,zero_token;
static uint64_t token_override;
static unsigned launches;
static uint16_t touch_x=100,touch_y=100;
#ifdef TEST_SCENE_QUICK
static bool quick_drive_close,quick_fail_submit;
static unsigned quick_yields;
static bool quick_wifi_route;
static unsigned quick_wifi_phase;
#endif
#ifdef TEST_SCENE_ALARM
static bool alert;
#endif
static void active(void);
#ifdef TEST_SCENE_LIGHT
static int light_result=1,checkpoint_result;
static unsigned light_calls;
uint32_t portable_foreground_idle_timeout_ms(void){return 15000;}
int portable_foreground_idle_configure(const risc_runtime_api_v1*r){(void)r;return 0;}
void portable_foreground_idle_timeout_changed(uint32_t ms){(void)ms;}
int portable_app_foreground_idle_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){
 (void)r;(void)d;(void)b;active();assert(!native_scene.visible&&!touch.grant.api&&!surface.frame&&!paper_token);++light_calls;return light_result;
}
static int32_t service_checkpoint(void){active();return checkpoint_result;}
#endif
static uint64_t sequence;static unsigned edge;static bool sampled_down;
static uint16_t sampled_x=100,sampled_y=100;
static void active(void){assert(!retained);++calls;}
static bool health(risc_runtime_health_v1*h){active();h->uptime_ms=now;return true;}
static void yield(uint32_t n){active();now+=n;
#ifdef TEST_SCENE_QUICK
if(quick_drive_close&&quick_modal){
 ++quick_yields;if(quick_yields>500){fprintf(stderr,"Quick stuck phase=%u gesture=%u pos=%d target=%d pending=%u x=%u y=%u\n",quick_wifi_phase,quick.ui.gesture,quick.ui.position_q8,quick.ui.target_q8,quick.ui.pending,touch_x,touch_y);abort();}down=false;
 if(quick_wifi_route){
  if(quick_yields>=2&&quick.ui.position_q8==pqa_open_q8(&quick.ui)&&quick.ui.target_q8==pqa_open_q8(&quick.ui)&&!quick_wifi_phase){
   pqa_layout layout=pqa_layout_for(&quick.ui);touch_x=layout.tiles[3].x+layout.tiles[3].w/2;touch_y=layout.tiles[3].y+layout.tiles[3].h/2;down=true;quick_wifi_phase=1;
  }else if(quick_wifi_phase==1)quick_wifi_phase=2;
 }else if(quick_yields>=2)navigation_pending|=T5_APP_BUTTON_BACK;
}
#endif
}
static bool diag(const char*s){active();(void)s;return true;}
static bool request(const char*s){active();
#ifdef TEST_SCENE_QUICK
if(quick_wifi_route){assert(!strcmp(s,"wifi_settings.elf"));}else {assert(!strcmp(s,"default.elf"));}
#else
assert(!strcmp(s,"default.elf"));
#endif
++launches;return true;}
static bool retain(void){active();retained=true;return true;}
static bool dinfo(void*c,risc_display_info_v1*i){active();(void)c;*i=(risc_display_info_v1){.width=TEST_SIZE,.height=TEST_SIZE,.safe_insets={PORTABLE_TEST_INSET,PORTABLE_TEST_INSET,PORTABLE_TEST_INSET,PORTABLE_TEST_INSET},.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool dacquire(void*c,uint32_t fmt,risc_display_surface_v1*s){active();(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=TEST_SIZE,.height=TEST_SIZE,.stride_bytes=TEST_SIZE*2,.size_bytes=sizeof(pixels),.pixel_format=fmt};return true;}
static void drelease(void*c,risc_display_frame_v1 f){active();(void)c;assert(f&&frames);frames=0;}
static bool submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){active();(void)c;(void)r;(void)n;(void)o;assert(f&&frames);
#ifdef TEST_SCENE_QUICK
if(quick_fail_submit&&quick_modal)return false;
#endif
frames=0;*t=zero_token?0:token_override?token_override:++submitted;return true;}
static bool status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){active();(void)c;s->state=(auto_complete||t<=completed)?RISC_DISPLAY_PRESENT_COMPLETE:RISC_DISPLAY_PRESENT_ACTIVE;return true;}
static uint64_t subscribe(void*c){active();(void)c;return 1;}
static bool unsubscribe(void*c,uint64_t t){active();(void)c;assert(t==1);return true;}
static bool tpoll(void*c,size_t n){active();(void)c;(void)n;if(bad_poll)return false;
#ifdef TEST_SCENE_ALARM
if(alert&&alarm_modal)navigation_pending|=T5_APP_BUTTON_BACK;
#endif
if(down!=sampled_down){sampled_down=down;edge=down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP;++sequence;}else if(down&&(sampled_x!=touch_x||sampled_y!=touch_y)){edge=RISC_TOUCH_EVENT_MOVE;++sequence;}sampled_x=touch_x;sampled_y=touch_y;return true;}
static int32_t touch_next(void*c,uint64_t t,risc_touch_event_v1*e){active();(void)c;(void)t;if(bad_next)return -2;if(bad_poll)return -1;if(!edge)return 0;*e=(risc_touch_event_v1){.sequence=sequence,.timestamp_ms=now,.kind=edge,.id=1,.x=touch_x,.y=touch_y};edge=0;return 1;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){active();(void)c;++snapshots;if(bad_snapshot)return false;assert(!bad_poll);*s=(risc_touch_snapshot_v1){.width=bad_geometry?0:TEST_SIZE,.height=TEST_SIZE,.sequence=sequence,.timestamp_ms=now,.contact_count=down?1:0};s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=touch_x,.y=touch_y};return true;}
static const risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=dinfo,.acquire=dacquire,.release=drelease,.submit=submit,.present_status=status};
static const risc_touch_api_v1 t={1,sizeof(t),0,subscribe,unsubscribe,tpoll,touch_next,snapshot};
#ifdef TEST_SCENE_ALARM
static unsigned acknowledged;
static int alarm_result;
static int32_t astatus(void*c,alarm_status_v1*s){active();(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alert?ALARM_STATE_ALERT:ALARM_STATE_READY,.occurrence=alert?(alarm_token_v1){1,1,1,1}:(alarm_token_v1){0},.mode=0};return alarm_result;}
static int32_t astep(void*c){active();(void)c;return alarm_result;}
static int32_t aack(void*c,const alarm_token_v1*t){active();(void)c;(void)t;alert=false;++acknowledged;return alarm_result;}
static int32_t aprepare(void*c,alarm_sleep_v1*t){active();(void)c;(void)t;return alarm_result;}
static const alarm_service_descriptor_v2 a={.base={2,sizeof(a),0,astatus,astep,astep,aack,aprepare,astep},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION};
#endif
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){active();
#ifdef TEST_SCENE_QUICK
assert(i==0||i==1);
#else
assert(!i);
#endif
assert(v==1
#ifdef TEST_SCENE_ALARM
||v==2
#endif
);g->struct_size=sizeof(*g);g->slot=++grants;g->generation=1;if(!strcmp(n,"display.output"))g->api=&d;else if(!strcmp(n,"input.touch.raw"))g->api=&t;
#ifdef TEST_SCENE_ALARM
else if(!strcmp(n,"alarm.service"))g->api=&a;
#endif
else {--grants;return false;}return true;}
static bool release(risc_runtime_capability_v1*g){active();assert(grants&&g->api);--grants;g->api=0;g->slot=g->generation=0;return true;}
static const risc_runtime_api_v1 api={.api_version=1,.struct_size=sizeof(api),.health=health,.yield_ms=yield,.diagnostic=diag,.request_launch=request,.acquire=acquire,.release=release,.retain_invocation=retain
#ifdef TEST_SCENE_LIGHT
,.service_checkpoint=service_checkpoint
#endif
};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&api:0;}
static void step(void){t5_app_input_t i={0};assert(poll(&i,1));}
static void paint(uint64_t map){assert(portable_paper_frame_ready());clear();portable_native_scene_layout(map);portable_native_scene_present(false);assert(paper_token);}
int main(int argc,char**argv){
 unsigned mode=argc>1?(unsigned)atoi(argv[1]):0;assert(app_module_init()==0);
 portable_native_scene_begin();paint(1);assert(!portable_native_scene_ready(0,0));
 down=true;step();completed=submitted;step();assert(!native_scene.gesture);down=false;step();step();assert(portable_native_scene_ready(0,0));
#ifdef TEST_SCENE_QUICK
 if(mode>=18) {
  assert(quick.ui.native_width==TEST_SIZE);
  touch_x=TEST_SIZE/2;touch_y=2;down=true;step();
  assert(quick.ui.gesture==PQA_TOP_PENDING);
#ifdef TEST_SCENE_ROUND
  assert(!native_scene_quick_saved); /* Outside app content cannot replay into it. */
#endif
  touch_y=TEST_SIZE/2;++now;input_service();input_dispatch();assert(pqa_visible(&quick.ui));
  auto_complete=true;quick_drive_close=true;quick_fail_submit=mode==19;quick_wifi_route=mode==20;
  if(quick_wifi_route){quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);quick.ui.gesture=PQA_IDLE;down=false;}

  bool consumed=false;bool ok=quick_foreground(&consumed);
  if(mode==19){assert(!ok&&retained);unsigned c=calls;app_module_fini();assert(c==calls);puts("PASS native Quick display custody failure no later callbacks");return 0;}
  assert(ok&&consumed&&quick_yields>=2&&!quick_modal&&!pqa_visible(&quick.ui)&&!native_scene.visible);
  if(mode==20){assert(launches==1&&handoff_requested&&quick_launch_pending&&!quick.ui.radio_controls);app_module_fini();assert(!grants&&!frames);puts("PASS native Timecard Quick WI-FI SETUP actual tile launches settings, not fictitious radio toggle");return 0;}
  step();step();assert(portable_native_scene_repaint());paint(1);step();step();
  touch_x=touch_y=150;down=true;step();assert(native_scene.gesture);down=false;step();assert(native_scene_contact.released);
  app_module_fini();assert(!grants&&!frames);puts("PASS actual native round Quick raw top-edge/open/render/Back/restore/fresh app input");return 0;
 }
 if(mode>=15) {
  /* A reserved top-edge tap replays the actual eligible DOWN before UP. */
  touch_x=100;touch_y=2;down=true;step();
  assert(quick.ui.gesture==PQA_TOP_PENDING&&!native_scene.gesture&&native_scene_quick_saved);
  if(mode==16)portable_native_scene_layout(9);
  if(mode==17){bad_poll=true;step();assert(!native_scene_quick_saved);bad_poll=false;}
  down=false;step();
  if(mode==15){assert(native_scene_contact.began&&native_scene_contact.down&&quick_replay_delivery);step();assert(native_scene_contact.released&&!native_scene_contact.down&&!quick_replay_delivery);}
  else assert(!native_scene.gesture&&!native_scene_contact.released&&!quick_replay_delivery);
  /* Modal pixels and asynchronous underlay restore cannot authorize the map. */
  pqa_cancel(&quick.ui);step();step();
  if(!native_scene.visible){paint(9);completed=submitted;step();step();}
  quick_background=malloc(quick_frame_bytes());assert(quick_background);memcpy(quick_background,alarm_pixels,quick_frame_bytes());
  quick_modal=true;assert(!portable_native_scene_ready(0,0)&&!portable_native_scene_repaint());
  assert(portable_paper_frame_ready());clear();present(false);assert(!native_scene.visible);
  quick_modal=false;quick_restore_pending=true;assert(quick_restore_step());assert(quick_restore_pending&&!portable_native_scene_repaint());
  completed=submitted;assert(paper_present_progress());assert(quick_restore_step());assert(!quick_restore_pending&&paper_token&&!native_scene.visible);
  touch_y=100;down=true;step();completed=submitted;step();assert(!native_scene.gesture&&portable_native_scene_repaint());
  paint(9);completed=submitted;step();step();assert(!native_scene.gesture);
  down=false;step();step();touch_y=100;down=true;step();assert(native_scene.gesture);down=false;step();assert(native_scene_contact.released);
  app_module_fini();assert(!grants&&!frames);puts("PASS native scene actual Quick reservation/replay/map or fault rejection/asynchronous restore/fresh contact");return 0;
 }
#endif
 if(mode==14){
  paint(2);bad_poll=true;step();assert(touch.poll_failed&&touch.resync);
  unsigned before=snapshots;uint64_t pending=paper_token;
  portable_native_scene_begin();portable_native_scene_begin();
  assert(snapshots==before&&paper_token==pending&&!native_scene.known&&touch.resync&&touch.cancel_pending);
  completed=submitted;step();assert(snapshots==before&&!native_scene.visible&&!retained);
  bad_poll=false;step();step();paint(4);completed=submitted;step();step();assert(portable_native_scene_ready(0,0));
  down=true;step();assert(native_scene.gesture);down=false;step();assert(native_scene_contact.released);
  app_module_fini();puts("PASS native scene begin/rebegin failed-poll preserves snapshot deferral, token and fresh recovery");return 0;
 }
 if(mode==8){home_pending=true;t5_app_input_t input={0};assert(poll(&input,1)&&input.exit_requested&&launches==1&&!portable_native_scene_ready(0,0));assert(native_scene_contact.cancelled);app_module_fini();puts("PASS native scene physical Home is terminal handoff without stale contact");return 0;}
 if(mode==9){paint(2);bad_geometry=true;completed=submitted;step();step();assert(!native_scene.visible&&!retained);bad_geometry=false;step();step();step();assert(native_scene.visible);app_module_fini();puts("PASS native scene invalid snapshot geometry defers safely then recovers");return 0;}
 if(mode==10){token_override=UINT64_MAX;auto_complete=true;paint(2);native_scene_completed(UINT64_MAX-1);assert(native_scene_token_owned&&!native_scene.visible);step();step();assert(native_scene.visible);zero_token=true;assert(portable_paper_frame_ready());clear();portable_native_scene_layout(3);portable_native_scene_present(false);assert(retained);unsigned c=calls;app_module_fini();assert(c==calls);puts("PASS native scene UINT64_MAX opaque token works; unrepresentable zero token retains");return 0;}

#ifdef TEST_SCENE_ALARM
 if(mode>=6){
  auto_complete=true;
  if(mode==7)alarm_result=ALARM_RETAINED;
  alert=true;bool consumed=false;bool ok=alarm_foreground(&consumed);
  if(mode==7){assert(!ok&&retained);unsigned c=calls;app_module_fini();assert(c==calls);puts("PASS native scene retained alarm checkpoint no later callback");return 0;}
  assert(ok&&consumed&&acknowledged==1&&!native_scene.visible&&!native_scene.gesture&&portable_native_scene_repaint());
  paint(1);completed=submitted;step();step();assert(portable_native_scene_ready(0,0));
  app_module_fini();assert(!grants&&!frames);puts("PASS native scene actual alarm modal/ack/restore requires fresh app frame");return 0;
 }
#endif
#ifdef TEST_SCENE_LIGHT
 if(mode>=3){
  if(mode==4)light_result=-2;
  if(mode==5)checkpoint_result=RISC_RUNTIME_CHECKPOINT_BUSY;
  if(mode==11)checkpoint_result=RISC_RUNTIME_CHECKPOINT_CONTEXT;
  if(mode==12)checkpoint_result=RISC_RUNTIME_CHECKPOINT_RETAINED;
  if(mode==13)light_result=0;
  bool ok=foreground_idle_enter();
  if(mode==4||mode==11||mode==12){assert(!ok&&retained);if(mode==4)assert(!native_scene.visible);else assert(!light_calls);unsigned c=calls;app_module_fini();assert(c==calls);puts("PASS native scene retained Light has no later callbacks");return 0;}
  assert(ok&&!retained);
  if(mode==5)assert(!light_calls&&native_scene.visible);
  else {assert(light_calls==1&&!native_scene.visible&&portable_native_scene_repaint());paint(1);completed=submitted;step();step();assert(portable_native_scene_ready(0,0));}
  app_module_fini();assert(!grants&&!frames);puts("PASS native scene actual foreground Light/BUSY preserves scene invalidation and fresh repaint");return 0;
 }
#endif
 portable_touch_sample invalid={.valid=true,.began=true,.down=true,.tap_eligible=true,.x=240,.y=100,.contact_id=1};native_scene_input(&invalid);assert(native_scene_contact.cancelled&&!native_scene.gesture);
 invalid=(portable_touch_sample){.valid=true,.began=true,.down=true,.x=100,.y=100,.contact_id=1};native_scene_input(&invalid);assert(native_scene_contact.cancelled&&!native_scene.gesture);
 down=true;step();assert(native_scene.gesture);down=false;step();assert(native_scene_contact.released&&!native_scene.gesture);
 paint(2);down=true;step();portable_native_scene_layout(3);completed=submitted;step();assert(!native_scene.visible&&!native_scene.gesture);down=false;step();
 paint(3);bad_poll=true;unsigned before=snapshots;completed=submitted;step();assert(snapshots==before&&!native_scene.visible&&!retained);
 if(mode==1){bad_next=true;t5_app_input_t i={0};assert(!poll(&i,1));assert(retained);unsigned c=calls;portable_native_scene_begin();app_module_fini();assert(calls==c);puts("PASS native scene true next fault retains without later callbacks");return 0;}
 bad_poll=false;step();step();step();assert(native_scene.visible&&!retained);
 /* Same map completion must preserve an owned gesture. */
 down=true;step();assert(native_scene.gesture);paint(3);completed=submitted;step();assert(native_scene.gesture);down=false;step();
 /* Foreign/restore pixels invalidate; later app paint establishes a fresh map. */
 assert(portable_paper_frame_ready());clear();present(false);assert(!native_scene.visible);completed=submitted;step();assert(portable_native_scene_repaint());paint(3);
 if(mode==2){bad_snapshot=true;completed=submitted;t5_app_input_t i={0};assert(!poll(&i,1));assert(retained);unsigned c=calls;portable_native_scene_begin();app_module_fini();assert(calls==c);puts("PASS native scene true snapshot fault retains without later callbacks");return 0;}
 completed=submitted;step();step();assert(portable_native_scene_ready(0,0));
 portable_native_scene_back();assert(navigation_pending&T5_APP_BUTTON_BACK);
 app_module_fini();assert(!grants&&!frames);puts("PASS native scene actual adapter async/map/neutral/malformed/foreign restore and Back");return 0;
}
