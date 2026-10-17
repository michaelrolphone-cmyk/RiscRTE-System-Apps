/* Exact production adapter with scripted capability providers. No app-specific
 * Settings controller or idle-policy correction is included in this fixture. */
#include "PortableQuickSession.h"
#include "PortableQuickLayout.h"
#include "AlarmServiceV1.h"
static unsigned geometry_width,geometry_height,geometry_stride,geometry_size;
static bool nqa_acquire(const char *,uint32_t,uint64_t,risc_runtime_capability_v1 *);
static bool nqa_brightness(void *,uint16_t,uint16_t);
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main

static unsigned hardware_brightness=40,preference_writes;
static uint8_t preference[4];
static bool preference_present[4];
static int key_index(const char *key) {
 const char *const keys[]={PQA_BRIGHTNESS_KEY,PQA_VOLUME_KEY,PQA_RESTORE_VOLUME_KEY,PQA_DND_KEY};
 for(unsigned i=0;i<4;i++)if(!strcmp(key,keys[i]))return (int)i;
 return -1;
}
static int32_t nqa_get(void *ctx,const char *key,void *data,uint32_t cap,uint32_t *size) {
 (void)ctx;int i=key_index(key);*size=0;
 if(i<0||!preference_present[i])return RISC_KEY_VALUE_NOT_FOUND;
 *size=1;if(cap<1)return RISC_KEY_VALUE_BUFFER_SMALL;*(uint8_t *)data=preference[i];return RISC_KEY_VALUE_OK;
}
static int32_t nqa_put(void *ctx,const char *key,const void *data,uint32_t size) {
 (void)ctx;assert(!surface.frame);int i=key_index(key);assert(i>=0&&size==1);
 preference[i]=*(const uint8_t *)data;preference_present[i]=true;preference_writes++;return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 nqa_kv={1,sizeof(nqa_kv),NULL,nqa_get,nqa_put};
static int32_t nqa_status(void *c,alarm_status_v1 *out){(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t nqa_service(void *c){(void)c;return ALARM_OK;}
static int32_t nqa_ack(void *c,const alarm_token_v1 *t){(void)c;(void)t;return ALARM_OK;}
static int32_t nqa_sleep(void *c,alarm_sleep_v1 *s){(void)c;(void)s;return ALARM_INVALID;}
static const alarm_service_v1 nqa_alarm={1,sizeof(nqa_alarm),NULL,nqa_status,nqa_service,nqa_service,nqa_ack,nqa_sleep,nqa_service};
static bool nqa_brightness(void *c,uint16_t value,uint16_t maximum){(void)c;assert(!surface.frame&&maximum==100);hardware_brightness=value;return true;}
static bool nqa_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 if(version!=1){assert(version==2&&!strcmp(name,"rtc.clock")&&!instance);return false;}
 assert(version==1);g->struct_size=sizeof(*g);
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);g->api=&nqa_kv;}
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(!instance);g->api=&nqa_alarm;}
 else return mock_acquire(name,version,instance,g);
 mock_grants++;return true;
}
static void feed(bool down,unsigned x,unsigned y) {
 mock_ms+=20;mock_down=down;mock_x=(uint16_t)x;mock_y=(uint16_t)y;
 input_pending=false;input_service();input_dispatch();assert(!failed);
}
static void release(void){feed(false,0,0);}
static void open_sheet(void) {
 release();feed(true,geometry_width/2,0);feed(true,geometry_width/2,geometry_height-1);release();
 assert(quick.ui.target_q8==pqa_open_q8(&quick.ui));
 pqa_animate(&quick.ui,mock_ms);pqa_animate(&quick.ui,mock_ms+1000);mock_ms+=1000;
 assert(quick.ui.position_q8==pqa_open_q8(&quick.ui));
}
static void tap_rect(pqa_rect r) {feed(true,(unsigned)(r.x+r.w/2),(unsigned)(r.y+r.h/2));release();}
static void render_check(unsigned mode) {
 const uint16_t background=0x39e7;
 uint8_t *expected=malloc(geometry_size);assert(expected);
 memset(expected,0xa5,geometry_size);
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=0;x<geometry_width;x++){
  expected[y*geometry_stride+2*x]=(uint8_t)background;expected[y*geometry_stride+2*x+1]=(uint8_t)(background>>8);
 }
 pqa_state saved=quick.ui;
 risc_display_surface_v1 oracle={1,expected,geometry_width,geometry_height,geometry_stride,geometry_size,RISC_DISPLAY_FORMAT_RGB565};
 assert(pqa_render(&oracle,&saved,"12:34",true,74));
 memset(mock_pixels,0xa5,geometry_size);
 assert(portable_paper_frame_ready());
 clear_color(background);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(mode==2)mock_fail_allocation=mock_allocations+1;
#endif
 assert(quick_render_watch("12:34",true,74));mock_fail_allocation=0;
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(mode==2)assert(!raster_recording&&surface.frame);
 else assert(raster_recording&&raster_tail&&raster_tail->kind==RS_QA_RENDER);
 if(mode==1){quick.ui.torch=!quick.ui.torch;quick.ui.brightness=10;}
#else
 (void)mode;
#endif
 present(false);
 for(unsigned i=0;i<10000&&!portable_paper_frame_idle();i++) {mock_ms++;assert(paper_present_progress());}
 assert(portable_paper_frame_idle());
 assert(!failed&&!surface.frame&&!mock_frames);
 assert(!memcmp(mock_pixels,expected,geometry_size));quick.ui=saved;free(expected);
}
static void run_size(unsigned w,unsigned h) {
 geometry_width=w;geometry_height=h;geometry_stride=w*2+7;geometry_size=geometry_stride*h;
 uint8_t *storage=malloc(geometry_size+38);assert(storage);memset(storage,0xa5,geometry_size+38);mock_pixels=(uint16_t *)(void *)(storage+19);
 mock_ms=mock_presents=mock_polls=mock_latency=mock_last_touch=mock_allocations=mock_launches=0;
 mock_down=mock_reject_submit=mock_fail_status=false;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;
 memset(preference_present,0,sizeof(preference_present));preference_writes=0;
 assert(!app_module_init());
#ifdef PORTABLE_QUICK_NATIVE
 assert(quick.ui.native_width==w&&quick.ui.logical_height==h);
#else
 assert(!quick.ui.native_width&&w==240&&h==240);
#endif
 assert(!preference_writes&&quick.ui.brightness_valid&&quick.ui.volume_valid);
 /* A native edge tap is replayed to the foreground at the same pixel. */
 release();feed(true,w-1,0);release();
 assert(quick.ui.route==PQA_REPLAY&&quick.ui.start_x==(int)w-1&&input_sample.released&&input_sample.x==w-1);
 open_sheet();pqa_layout l=pqa_layout_for(&quick.ui);
 render_check(0);render_check(1);mock_latency=20;render_check(2);mock_latency=0;
 quick.ui.dnd_valid=quick.ui.radios_valid=quick.ui.radio_controls=true;
 const uint32_t expected[]={PQA_SILENT,PQA_DND,PQA_AIRPLANE,PQA_WIFI,PQA_BLUETOOTH,PQA_TORCH};
 for(unsigned i=0;i<6;i++) {
  tap_rect(l.tiles[i]);uint32_t action=pqa_take_action(&quick.ui);assert(action==expected[i]);
  if(i==0||i==1||i==5)assert(quick_apply(action));
  if(i==5){
   feed(true,w/2,h/2);release();assert(quick_apply(pqa_take_action(&quick.ui)));assert(!quick.ui.torch);
   pqa_animate(&quick.ui,mock_ms);pqa_animate(&quick.ui,mock_ms+1000);mock_ms+=1000;
   open_sheet();
  }
 }
 feed(true,(unsigned)l.slider_left,(unsigned)l.slider_y[0]);assert(quick_apply(pqa_take_action(&quick.ui)));
 feed(true,(unsigned)l.slider_right,(unsigned)l.slider_y[0]);release();assert(quick_apply(pqa_take_action(&quick.ui)));
 assert(hardware_brightness==100&&preference_present[0]&&preference[0]==100);
 feed(true,(unsigned)l.slider_left,(unsigned)l.slider_y[1]);release();assert(quick_apply(pqa_take_action(&quick.ui)));
 assert(preference_present[1]&&preference[1]==0);
 tap_rect(l.handle);assert(!quick.ui.target_q8);pqa_cancel(&quick.ui);release();
 /* Exercise the native modal's real close/underlay restoration lifecycle. */
 open_sheet();bool consumed=false;navigation_pending|=T5_APP_BUTTON_BACK;
 assert(quick_foreground(&consumed)&&consumed&&!quick_modal&&!failed);
 for(unsigned i=0;i<10000&&(quick_restore_pending||!portable_paper_frame_idle());i++) {
  mock_ms++;assert(paper_present_progress());assert(quick_restore_step());
 }
 assert(!quick_restore_pending&&!quick_background&&portable_paper_frame_idle());
 app_module_fini();assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live&&!surface.frame);
 for(unsigned i=0;i<19;i++)assert(storage[i]==0xa5&&storage[geometry_size+19+i]==0xa5);
 free(storage);printf("native Quick adapter %ux%u: init, raw input, actions, replay, snapshot pixels and cleanup PASS\n",w,h);
}
#ifdef PORTABLE_QUICK_NATIVE
static void reject_size(unsigned w,unsigned h) {
 geometry_width=w;geometry_height=h;geometry_stride=w*2+7;geometry_size=geometry_stride*h;
 mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;
 assert(app_module_init()!=0);
 assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live&&!surface.frame);
 printf("native Quick adapter %ux%u: rejected unsupported Quick extent and cleaned up PASS\n",w,h);
}
#endif
int main(void) {
#ifdef PORTABLE_QUICK_NATIVE
 const unsigned sizes[][2]={{120,150},{160,160},{240,240},{410,502},{502,410},{640,240},{2048,240}};
#else
 const unsigned sizes[][2]={{240,240}};
#endif
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)run_size(sizes[i][0],sizes[i][1]);
#ifdef PORTABLE_QUICK_NATIVE
 reject_size(2049,240);reject_size(240,2049);
#endif
 return 0;
}
