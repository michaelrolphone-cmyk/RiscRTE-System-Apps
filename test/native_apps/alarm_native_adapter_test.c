/* The real generic adapter and alarm_foreground loop, with capability-provider
 * doubles only. Input is an ordered native-pixel raw event stream. */
#include "PortableAlarmLayout.h"
#include "AlarmServiceV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#ifdef PORTABLE_QUICK_ACTIONS
#include "PortableQuickSession.h"
#include "PortableQuickLayout.h"
#endif
static unsigned geometry_width,geometry_height,geometry_stride,geometry_size;
static bool ana_acquire(const char *,uint32_t,uint64_t,risc_runtime_capability_v1 *);
static bool ana_brightness(void *,uint16_t,uint16_t);
static bool ana_poll(void *,size_t);
static int32_t ana_next(void *,uint64_t,risc_touch_event_v1 *);
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main

static alarm_status_v1 provider;
static unsigned steps,acks,retries,stops,phase,min_ack_step,first_ack_step;
static unsigned hardware_brightness,brightness_calls,preference_writes,case_count;
static unsigned scenario,script_count,script_cursor;
static bool script_active,overflow_pending,held_second;
static const char *case_name;
static uint8_t *storage;
static uint16_t underlay;
static portable_alarm_layout layout;
static alarm_token_v1 acknowledged[3];
typedef struct {unsigned at;bool down;int x,y;uint8_t id;bool second,overflow;} action;
static action script[24];
enum { NORMAL, STALE, SIMULTANEOUS, LOADING_GAP, BLOCKED_RETRY, BLOCKED_BACK,
       UNCERTAIN, QUICK_INTERRUPT, TORCH_INTERRUPT };
static void service_safe(void) {assert(!failed&&!raster_lease_mutable());}
static void append(unsigned at,bool down,int x,int y,uint8_t id,bool second,bool overflow) {
 assert(script_count<sizeof(script)/sizeof(script[0]));
 assert(!script_count||at>=script[script_count-1].at);
 script[script_count++]=(action){at,down,x,y,id,second,overflow};
}
static void tap_at(unsigned at,int x,int y) {
 append(at,true,x,y,7,false,false);append(at+2,false,x,y,7,false,false);
}
static void tap_rect(unsigned at,portable_alarm_rect r) {tap_at(at,r.x+r.w/2,r.y+r.h/2);}
static void next_occurrence(void) {
 provider.state=ALARM_STATE_ALERT;
 provider.occurrence=(alarm_token_v1){ALARM_KIND_COUNTDOWN,19,0x4567,provider.occurrence.generation+1};
 if(!provider.occurrence.generation)provider.occurrence.generation=91;
 tap_rect(steps+5,layout.dismiss);
}
static int32_t ana_status(void *c,alarm_status_v1 *out) {(void)c;service_safe();*out=provider;return ALARM_OK;}
static int32_t ana_step(void *c) {
 (void)c;service_safe();++steps;
 if(steps>160){fprintf(stderr,"stalled case=%s size=%ux%u state=%u acks=%u retries=%u script=%u/%u\n",case_name,geometry_width,geometry_height,provider.state,acks,retries,script_cursor,script_count);abort();}
#ifdef PORTABLE_QUICK_ACTIONS
 if((scenario==QUICK_INTERRUPT&&quick_modal&&steps>=4)||
    (scenario==TORCH_INTERRUPT&&quick_modal&&quick.ui.torch&&hardware_brightness==100)) {
  if(provider.state==ALARM_STATE_READY&&!acks) {
   provider.state=ALARM_STATE_ALERT;provider.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,11,0x2345,73};
   tap_rect(steps+5,layout.dismiss);
  }
 }
#endif
 if(provider.state==ALARM_STATE_DISMISSING&&++phase==3) {
  phase=0;
  if(scenario==SIMULTANEOUS&&acks==1)next_occurrence();
  else if(scenario==LOADING_GAP&&acks==1) {
   provider.state=ALARM_STATE_LOADING;provider.occurrence=(alarm_token_v1){0};
   /* A dismiss gesture during the zero-token loading gap must not restore. */
   tap_rect(steps+2,layout.dismiss);
  } else {provider.state=ALARM_STATE_READY;provider.occurrence=(alarm_token_v1){0};}
 } else if(provider.state==ALARM_STATE_LOADING&&++phase==9) {phase=0;next_occurrence();}
 return ALARM_OK;
}
static int32_t ana_refresh(void *c) {
 (void)c;service_safe();++retries;assert(provider.state==ALARM_STATE_BLOCKED&&steps>=min_ack_step);
 assert(!acks);provider.output_uncertain=0;provider.state=ALARM_STATE_READY;return ALARM_PENDING;
}
static int32_t ana_ack(void *c,const alarm_token_v1 *token) {
 (void)c;service_safe();assert(provider.occurrence.generation);
 assert(!memcmp(token,&provider.occurrence,sizeof(*token)));
 assert(steps>=min_ack_step&&acks<3);acknowledged[acks++]=*token;
 if(acks==1)first_ack_step=steps;
 if(scenario==STALE&&acks==1) {
  ++provider.occurrence.generation;tap_rect(steps+5,layout.dismiss);return ALARM_STALE;
 }
 provider.state=ALARM_STATE_DISMISSING;phase=0;return ALARM_PENDING;
}
static int32_t ana_prepare(void *c,alarm_sleep_v1 *s) {(void)c;(void)s;assert(!"unexpected sleep");return ALARM_INVALID;}
static int32_t ana_stop(void *c) {(void)c;assert(!raster_lease_mutable());++stops;return ALARM_OK;}
static const alarm_service_v1 ana_alarm={1,sizeof(ana_alarm),NULL,ana_status,ana_step,ana_refresh,ana_ack,ana_prepare,ana_stop};
#ifdef PORTABLE_QUICK_ACTIONS
static int32_t ana_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size) {
 (void)c;*size=0;
 if(strcmp(key,PQA_BRIGHTNESS_KEY))return RISC_KEY_VALUE_NOT_FOUND;
 *size=1;if(!cap)return RISC_KEY_VALUE_BUFFER_SMALL;*(uint8_t *)data=40;return RISC_KEY_VALUE_OK;
}
static int32_t ana_put(void *c,const char *key,const void *data,uint32_t size) {
 (void)c;(void)key;(void)data;(void)size;assert(!surface.frame);++preference_writes;return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 ana_kv={1,sizeof(ana_kv),NULL,ana_get,ana_put};
#endif
static bool ana_brightness(void *c,uint16_t value,uint16_t maximum) {
 (void)c;assert(!surface.frame&&maximum==100);hardware_brightness=value;++brightness_calls;return true;
}
static bool ana_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 if(version!=1){assert(version==2&&!strcmp(name,"rtc.clock")&&!instance);return false;}
 g->struct_size=sizeof(*g);
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(!instance);g->api=&ana_alarm;}
#ifdef PORTABLE_QUICK_ACTIONS
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);g->api=&ana_kv;}
#endif
 else return mock_acquire(name,version,instance,g);
 ++mock_grants;return true;
}
static uint8_t active_id=7;
static void raw_position(int x,int y) {
 assert(x>=0&&y>=0&&(unsigned)x<geometry_width&&(unsigned)y<geometry_height);
#if PORTABLE_TOUCH_ROTATION == 180
 x=(int)geometry_width-1-x;y=(int)geometry_height-1-y;
#endif
 mock_x=(uint16_t)x;mock_y=(uint16_t)y;
}
static bool ana_poll(void *c,size_t n) {
 (void)c;assert(n==1);++mock_polls;
 if(script_active)while(script_cursor<script_count&&script[script_cursor].at<=steps) {
  action a=script[script_cursor++];mock_down=a.down;active_id=a.id;held_second=a.second;
  raw_position(a.x,a.y);overflow_pending|=a.overflow;
 }
 mock_timestamp_ms+=(uint32_t)(mock_ms-mock_timestamp_clock);mock_timestamp_clock=mock_ms;
 risc_touch_snapshot_v1 next={.width=geometry_width,.height=geometry_height,.contact_count=mock_down?(held_second?2:1):0};
 next.contacts[0]=(risc_touch_contact_v1){.id=active_id,.x=mock_x,.y=mock_y};
 next.contacts[1]=(risc_touch_contact_v1){.id=(uint8_t)(active_id+1),.x=mock_x,.y=mock_y};
 /* Additions before removals exercise same-report contact replacement. */
 for(unsigned j=0;j<next.contact_count;j++) {
  int old=-1;for(unsigned i=0;i<mock_touch_state.contact_count;i++)if(mock_touch_state.contacts[i].id==next.contacts[j].id)old=(int)i;
  if(old<0)mock_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[j]);
  else if(memcmp(&mock_touch_state.contacts[old],&next.contacts[j],sizeof(next.contacts[j])))mock_emit(RISC_TOUCH_EVENT_MOVE,next.contacts[j]);
 }
 for(unsigned i=0;i<mock_touch_state.contact_count;i++) {
  bool retained=false;for(unsigned j=0;j<next.contact_count;j++)retained|=mock_touch_state.contacts[i].id==next.contacts[j].id;
  if(!retained)mock_emit(RISC_TOUCH_EVENT_UP,mock_touch_state.contacts[i]);
 }
 next.sequence=mock_touch_state.sequence;next.timestamp_ms=mock_timestamp_ms;mock_touch_state=next;return true;
}
static int32_t ana_next(void *c,uint64_t s,risc_touch_event_v1 *out) {
 if(overflow_pending){overflow_pending=false;mock_event_head=mock_event_count=0;return -1;}
 return mock_next(c,s,out);
}
static void feed(bool down,int x,int y) {
 script_active=false;mock_down=down;active_id=7;held_second=false;raw_position(x,y);mock_ms+=20;
 input_pending=false;input_service();input_dispatch();assert(!failed);
}
static void drain(void) {
 for(unsigned n=0;n<20000&&!portable_paper_frame_idle();n++){++mock_ms;assert(paper_present_progress());}
 assert(portable_paper_frame_idle()&&!surface.frame&&!mock_frames);
}
static void assert_padding(void) {
 uint8_t *pixels=(uint8_t *)(void *)mock_pixels;
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=geometry_width*2;x<geometry_stride;x++)assert(pixels[y*geometry_stride+x]==0xa5);
 for(unsigned i=0;i<19;i++)assert(storage[i]==0xa5&&storage[geometry_size+19+i]==0xa5);
}
static void assert_underlay(void) {
 const uint8_t *pixels=(const uint8_t *)(const void *)mock_pixels;
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=0;x<geometry_width;x++) {
  const uint8_t *p=pixels+y*geometry_stride+x*2;assert((unsigned)(p[0]|p[1]<<8)==underlay);
 }
 assert_padding();
}
static void paint_underlay(uint16_t color,bool wait) {
 assert(portable_paper_frame_ready());clear_color(color);present(false);if(wait)drain();underlay=color;
}
static void setup(unsigned w,unsigned h,const char *name,unsigned kind) {
 assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);
 geometry_width=w;geometry_height=h;geometry_stride=w*2+7;geometry_size=geometry_stride*h;
 storage=malloc(geometry_size+38);assert(storage);memset(storage,0xa5,geometry_size+38);mock_pixels=(uint16_t *)(void *)(storage+19);
 mock_ms=mock_presents=mock_polls=mock_latency=mock_last_touch=mock_allocations=mock_fail_allocation=mock_launches=0;
 mock_down=mock_reject_submit=mock_fail_status=false;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;
 steps=acks=retries=stops=phase=min_ack_step=first_ack_step=script_count=script_cursor=0;
 brightness_calls=preference_writes=0;hardware_brightness=40;scenario=kind;case_name=name;
 script_active=overflow_pending=held_second=false;active_id=7;memset(acknowledged,0,sizeof(acknowledged));
 provider=(alarm_status_v1){.api_version=1,.struct_size=sizeof(provider),.state=ALARM_STATE_READY};
 assert(!app_module_init());assert(!failed);paint_underlay(0x39e7,true);
#ifdef PORTABLE_ALARM_NATIVE
 assert(portable_alarm_layout_for(w,h,kind==BLOCKED_RETRY||kind==BLOCKED_BACK||kind==UNCERTAIN,&layout));
#else
 assert(w==240&&h==240);layout=(portable_alarm_layout){.dismiss={24,144,192,44},.retry={24,192,192,44}};
#endif
 provider.state=ALARM_STATE_ALERT;provider.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,11,0x2345,73};strcpy(provider.label,"NATIVE ALARM");
 if(kind==BLOCKED_RETRY||kind==BLOCKED_BACK||kind==UNCERTAIN) {
  provider.state=ALARM_STATE_BLOCKED;provider.error=ALARM_RTC;provider.snapshot=12;provider.occurrence=(alarm_token_v1){0};
  provider.output_uncertain=kind==UNCERTAIN;
 }
}
static void finish(unsigned expected_acks,unsigned expected_retries) {
 script_active=false;drain();assert(!failed&&!alarm_modal&&!input_pending&&!navigation_pending);
 assert(acks==expected_acks&&retries==expected_retries&&!stops&&!mock_launches&&!preference_writes);
 assert_underlay();app_module_fini();
 assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live&&!surface.frame);
 assert_padding();free(storage);storage=NULL;++case_count;
}
static void run_foreground(void) {
 bool consumed=false;script_active=true;assert(alarm_foreground(&consumed)&&consumed);script_active=false;
}
static void single_tap(unsigned w,unsigned h,const char *name,int position) {
 setup(w,h,name,NORMAL);portable_alarm_rect r=layout.dismiss;int x=r.x+r.w/2,y=r.y+r.h/2;
 if(position==1){x=r.x;y=r.y;}if(position==2){x=r.x+r.w-1;y=r.y+r.h-1;}
 tap_at(4,x,y);min_ack_step=6;run_foreground();finish(1,0);
}
static void rejected_tap(unsigned w,unsigned h,unsigned test) {
 const char *names[]={"outside-left","outside-right","outside-top","outside-bottom","held-entry","moved","cancelled","replaced-contact","multi-contact"};
 setup(w,h,names[test],NORMAL);portable_alarm_rect r=layout.dismiss;int x=r.x+r.w/2,y=r.y+r.h/2;
 if(test<4) {
  if(test==0)x=r.x-1;
  if(test==1)x=r.x+r.w;
  if(test==2)y=r.y-1;
  if(test==3)y=r.y+r.h;
  tap_at(4,x,y);
 } else if(test==4) {
  feed(true,x,y);append(6,false,x,y,7,false,false);
 } else {
  append(4,true,x,y,7,false,false);
  append(5,true,x+(test==5?8:0),y,(uint8_t)(test==7?9:7),test==8,test==6);
  append(7,false,x+(test==5?8:0),y,7,false,false);
 }
 tap_rect(12,r);min_ack_step=14;run_foreground();finish(1,0);
}
static void token_cases(unsigned w,unsigned h,unsigned kind,const char *name) {
 setup(w,h,name,kind);tap_rect(4,layout.dismiss);min_ack_step=6;run_foreground();
 assert(acks==2&&acknowledged[0].generation!=acknowledged[1].generation);
 if(kind==LOADING_GAP)assert(steps>first_ack_step+12);
 finish(2,0);
}
static void blocked_cases(unsigned w,unsigned h,unsigned kind,const char *name) {
 setup(w,h,name,kind);
 if(kind==BLOCKED_BACK)tap_rect(4,layout.dismiss);
 else if(kind==UNCERTAIN){tap_rect(4,layout.dismiss);tap_rect(12,layout.retry);}
 else tap_rect(4,layout.retry);
 run_foreground();
 if(kind==BLOCKED_BACK){bool consumed=true;assert(alarm_error_seen&&alarm_foreground(&consumed)&&!consumed);}
 else assert(provider.state==ALARM_STATE_READY&&steps>=(kind==UNCERTAIN?14u:6u));
 finish(0,kind==BLOCKED_BACK?0:1);
}
static void retry_boundaries(unsigned w,unsigned h,unsigned test) {
 setup(w,h,"retry-boundary",BLOCKED_RETRY);portable_alarm_rect r=layout.retry;
 int x=r.x+r.w/2,y=r.y+r.h/2;
 if(test==0){x=r.x;y=r.y;}
 if(test==1){x=r.x+r.w-1;y=r.y+r.h-1;}
 if(test==2)x=r.x-1;
 if(test==3)x=r.x+r.w;
 if(test==4)y=r.y-1;
 if(test==5)y=r.y+r.h;
 tap_at(4,x,y);min_ack_step=6;
 if(test>=2){tap_rect(12,r);min_ack_step=14;}
 run_foreground();finish(0,1);
}
static void delayed_or_oom(unsigned w,unsigned h,unsigned oom) {
 setup(w,h,oom?"allocation-fallback":"delayed-underlay",NORMAL);mock_latency=20;
 if(!oom)paint_underlay(0x52aa,false);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(oom){raster_discard();mock_fail_allocation=mock_allocations+oom;}
#else
 (void)oom;
#endif
 tap_rect(8,layout.dismiss);min_ack_step=10;run_foreground();
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(oom&&mock_allocations<mock_fail_allocation){fprintf(stderr,"OOM injection not reached offset=%u allocations=%u target=%u\n",oom,mock_allocations,mock_fail_allocation);abort();}
#endif
 mock_fail_allocation=0;finish(1,0);
}
static uint64_t pixel_hash(void) {
 uint64_t hash=UINT64_C(1469598103934665603);const uint8_t *p=(const uint8_t *)(const void *)mock_pixels;
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=0;x<geometry_width*2;x++){hash^=p[y*geometry_stride+x];hash*=UINT64_C(1099511628211);}
 return hash;
}
static void capture(unsigned w,unsigned h,unsigned blocked) {
 const char *directory=getenv("ANA_CAPTURE_DIR");
 if(!directory||!blocked||!((w==120&&h==150)||(w==160&&h==160)||(w==410&&h==502)||(w==502&&h==410)))return;
 char name[1024];int length=snprintf(name,sizeof(name),"%s/alarm-%ux%u-blocked.ppm",directory,w,h);assert(length>0&&(unsigned)length<sizeof(name));
 FILE *file=fopen(name,"wb");assert(file);fprintf(file,"P6\n%u %u\n255\n",w,h);
 const uint8_t *pixels=(const uint8_t *)(const void *)mock_pixels;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
  const uint8_t *p=pixels+y*geometry_stride+x*2;unsigned value=p[0]|p[1]<<8;
  uint8_t rgb[3]={(uint8_t)(((value>>11)&31)*255/31),(uint8_t)(((value>>5)&63)*255/63),(uint8_t)((value&31)*255/31)};
  assert(fwrite(rgb,1,3,file)==3);
 }
 assert(!fclose(file));
}
static void render_cases(unsigned w,unsigned h) {
 for(unsigned blocked=0;blocked<2;blocked++) {
  setup(w,h,"render",blocked?UNCERTAIN:NORMAL);
  if(blocked)strcpy(provider.label,"WIDE LABEL 123456789012");
  assert(portable_alarm_status(&alarms));alarm_modal=true;assert(portable_paper_frame_ready());alarm_draw();drain();
  uint64_t hash=pixel_hash();assert_padding();capture(w,h,blocked);
  printf("RENDER %ux%u blocked%u %016llx\n",w,h,blocked,(unsigned long long)hash);
#ifdef PORTABLE_ALARM_NATIVE
  const uint8_t *p=(const uint8_t *)(const void *)mock_pixels;
  portable_alarm_rect r=layout.dismiss;unsigned ink=0;
  for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)ink+=p[(unsigned)y*geometry_stride+(unsigned)x*2]||p[(unsigned)y*geometry_stride+(unsigned)x*2+1];
  assert(ink>20);
#endif
  assert(alarm_restore(alarm_pixels));alarm_modal=false;alarm_cancel_input();provider.state=ALARM_STATE_READY;provider.occurrence=(alarm_token_v1){0};provider.output_uncertain=0;
  finish(0,0);
 }
}
#ifdef PORTABLE_QUICK_ACTIONS
static void quick_case(unsigned w,unsigned h,bool torch) {
 setup(w,h,torch?"torch-interruption":"quick-interruption",torch?TORCH_INTERRUPT:QUICK_INTERRUPT);
 provider.state=ALARM_STATE_READY;provider.occurrence=(alarm_token_v1){0};
 feed(false,0,0);feed(true,w/2,0);feed(true,w/2,h-1);feed(false,0,0);
 assert(quick.ui.target_q8==pqa_open_q8(&quick.ui));
 pqa_animate(&quick.ui,mock_ms);pqa_animate(&quick.ui,mock_ms+1000);mock_ms+=1000;
 assert(quick.ui.position_q8==pqa_open_q8(&quick.ui));
 if(torch){pqa_layout q=pqa_layout_for(&quick.ui);tap_at(4,q.tiles[5].x+q.tiles[5].w/2,q.tiles[5].y+q.tiles[5].h/2);}
 bool consumed=false;script_active=true;assert(quick_foreground(&consumed)&&consumed);script_active=false;
 for(unsigned n=0;n<20000&&(quick_restore_pending||!portable_paper_frame_idle());n++){++mock_ms;assert(paper_present_progress()&&quick_restore_step());}
 assert(!quick_modal&&!quick_restore_pending&&!quick_background&&!quick.ui.torch&&hardware_brightness==40);
 if(torch)assert(brightness_calls>=2);
 alarm_cancel_input();finish(1,0);
}
#endif
static void run_size(unsigned w,unsigned h) {
 unsigned before=case_count;
 render_cases(w,h);single_tap(w,h,"center",0);single_tap(w,h,"leading-edge",1);single_tap(w,h,"trailing-edge",2);
 for(unsigned n=0;n<9;n++)rejected_tap(w,h,n);
 token_cases(w,h,STALE,"stale-token");token_cases(w,h,SIMULTANEOUS,"simultaneous");token_cases(w,h,LOADING_GAP,"loading-gap");
 for(unsigned n=0;n<6;n++)retry_boundaries(w,h,n);
 blocked_cases(w,h,BLOCKED_RETRY,"blocked-retry");blocked_cases(w,h,BLOCKED_BACK,"blocked-back");blocked_cases(w,h,UNCERTAIN,"output-uncertain");
 delayed_or_oom(w,h,0);
#ifdef PORTABLE_RASTER_SNAPSHOT
 for(unsigned n=1;n<=2;n++)delayed_or_oom(w,h,n);
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 quick_case(w,h,false);quick_case(w,h,true);
#endif
 printf("PASS alarm adapter %ux%u: %u render, input, token, service and custody cases\n",w,h,case_count-before);
}
int main(void) {
 setvbuf(stdout,NULL,_IONBF,0);
#ifdef PORTABLE_ALARM_NATIVE
 const unsigned sizes[][2]={{120,150},{160,160},{240,240},{410,502},{502,410},{640,240},{2048,240}};
#else
 const unsigned sizes[][2]={{240,240}};
#endif
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)run_size(sizes[i][0],sizes[i][1]);
 printf("PASS %u total cases; native raw touch rotation %u\n",case_count,PORTABLE_TOUCH_ROTATION);return 0;
}
