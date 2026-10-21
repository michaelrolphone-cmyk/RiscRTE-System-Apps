/* Real native adapter and monitor controller over deterministic RGB565, touch,
 * and invocation-scoped stream doubles. No physical transport is exercised.
 * Compile with the exact pinned Runtime SDK staged ahead of PortableApps. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PortableSerialClient.h"
#include "PortableSerialMonitor.h"
#include "../../lib/PortableApps/src/adapter.c"

extern void app_main(void);
const t5_app_manifest_t portable_catalog[1]={{.display_name="Fixture",.file_name="fixture.elf",.compatible=true}};
const unsigned portable_catalog_count=1;
const portable_serial_monitor_instance portable_serial_monitor_instances[]={
 {41,"USB A"},{42,"USB B"}
};
const unsigned portable_serial_monitor_instance_count=2;

#define FX_WIDTH_MAX 480u
#define FX_HEIGHT_MAX 800u
#define FX_PIXEL_CAPACITY (FX_WIDTH_MAX*FX_HEIGHT_MAX*2u)
static struct { uint8_t before[16], pixels[FX_PIXEL_CAPACITY], after[16]; } fx_buffer;
static uint8_t fx_frozen[FX_PIXEL_CAPACITY],fx_queued_pixels[FX_PIXEL_CAPACITY];
static unsigned fx_width=240,fx_height=240,fx_ticks,fx_calls,fx_presents;
static unsigned fx_acquires,fx_releases,fx_live,fx_subscription,fx_frame_live;
static unsigned fx_serial_acquires,fx_serial_releases,fx_stream_opens,fx_stream_closes;
static unsigned fx_fences,fx_terminal_calls,fx_terminal_presents,fx_last_frame_black;
static unsigned fx_reads,fx_writes,fx_launches,fx_reads_pending,fx_queued_at;
static risc_display_present_token_v1 fx_pending;
static uint64_t fx_device,fx_generation,fx_instance,fx_sequence;
static bool fx_retained,fx_terminal_result,fx_session_live,fx_touch_down;
static uint16_t fx_touch_x,fx_touch_y;
static const char *fx_case,*fx_capture;
static risc_runtime_api_v1 fx_runtime;
static unsigned fx_event_index;
static jmp_buf fx_owner_escape;
static uint8_t fx_received[]="Ab9 !?[]{}=+-_/\\\r\nRX COMPLETE\r\n";
static size_t fx_rx_offset;
static bool fx_is(const char *name){return !strcmp(fx_case,name);}
static void fx_io(void){assert(!fx_retained&&!fx_terminal_result);++fx_calls;}
static void fx_guards(void){
 for(unsigned i=0;i<sizeof(fx_buffer.before);++i)assert(fx_buffer.before[i]==0xa5&&fx_buffer.after[i]==0x5a);
 for(unsigned i=fx_width*fx_height*2;i<FX_PIXEL_CAPACITY;++i)assert(fx_buffer.pixels[i]==0xc3);
}
static bool fx_health(risc_runtime_health_v1 *out){fx_io();out->uptime_ms=fx_ticks;return true;}
static bool fx_monotonic(uint32_t *out){fx_io();*out=fx_ticks;return true;}
static void fx_yield(uint32_t ms){fx_io();assert(fx_ticks<4000);fx_ticks+=ms;}
static bool fx_diagnostic(const char *line){fx_io();assert(line);return true;}
static bool fx_launch(const char *path){fx_io();assert(path&&!strcmp(path,"fixture.elf")&&!fx_session_live&&fx_serial_releases==1);++fx_launches;return true;}
static bool fx_exit_requested(void){
 fx_io();
 /* Fixture-only interrupted app return: leave the live client untouched so
  * the real module finalizer must prove app-first cleanup independently. */
 if(fx_ticks>=1000&&(fx_is("fini-clean")||fx_is("fini-retained")||fx_is("launch-clean")||fx_is("launch-retained")))longjmp(fx_owner_escape,1);
 return fx_ticks>=1000;
}
static bool fx_retain(void){
 assert(!fx_retained);fx_terminal_calls=fx_calls;fx_terminal_presents=fx_presents;
 memcpy(fx_frozen,fx_buffer.pixels,FX_PIXEL_CAPACITY);fx_retained=true;++fx_fences;return true;
}
static bool fx_display_info(void *context,risc_display_info_v1 *out){
 (void)context;fx_io();*out=(risc_display_info_v1){.width=fx_width,.height=fx_height,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT};return true;
}
static bool fx_frame(void *context,uint32_t format,risc_display_surface_v1 *out){
 (void)context;fx_io();assert(!fx_frame_live&&!fx_pending&&format==RISC_DISPLAY_FORMAT_RGB565);fx_frame_live=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=fx_buffer.pixels,.width=fx_width,.height=fx_height,
  .stride_bytes=fx_width*2,.size_bytes=fx_width*fx_height*2,.pixel_format=format};return true;
}
static void fx_frame_release(void *context,risc_display_frame_v1 frame){
 (void)context;fx_io();assert(frame==1&&fx_frame_live);fx_frame_live=0;
}
static void fx_save_frame(void){
 if(!fx_capture)return;
 char path[768];int length=snprintf(path,sizeof(path),"%s/%s-%ux%u-%03u.ppm",fx_capture,fx_case,fx_width,fx_height,fx_presents);
 assert(length>0&&(size_t)length<sizeof(path));FILE *file=fopen(path,"wb");assert(file);
 fprintf(file,"P6\n%u %u\n255\n",fx_width,fx_height);
 for(unsigned i=0;i<fx_width*fx_height;++i){
  uint16_t value;memcpy(&value,fx_buffer.pixels+i*2,2);
  uint8_t rgb[3]={(uint8_t)(((value>>11)&31u)*255u/31u),(uint8_t)(((value>>5)&63u)*255u/63u),(uint8_t)((value&31u)*255u/31u)};
  assert(fwrite(rgb,1,sizeof(rgb),file)==sizeof(rgb));
 }
 assert(!fclose(file));
}
static bool fx_submit(void *context,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 (void)context;(void)damage;(void)count;fx_io();assert(options&&frame==1&&fx_frame_live);fx_guards();
 fx_last_frame_black=0;for(unsigned i=0;i<fx_width*fx_height;++i){uint16_t value;memcpy(&value,fx_buffer.pixels+i*2,2);fx_last_frame_black+=value==0;}
 assert(fx_last_frame_black>50&&fx_last_frame_black<fx_width*fx_height);
 fx_frame_live=0;*token=++fx_presents;fx_pending=*token;fx_queued_at=fx_ticks;
 memcpy(fx_queued_pixels,fx_buffer.pixels,FX_PIXEL_CAPACITY);fx_save_frame();return true;
}
static bool fx_present_status(void *context,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)context;fx_io();assert(token&&token==fx_pending);
 assert(!memcmp(fx_queued_pixels,fx_buffer.pixels,FX_PIXEL_CAPACITY));
 if(fx_is("slow-frame")&&fx_ticks-fx_queued_at<600u)out->state=RISC_DISPLAY_PRESENT_ACTIVE;
 else {out->state=RISC_DISPLAY_PRESENT_COMPLETE;fx_pending=0;}
 return true;
}
static const risc_display_output_api_v1 fx_display={.api_version=1,.struct_size=sizeof(fx_display),
 .get_info=fx_display_info,.acquire=fx_frame,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_present_status};
static uint64_t fx_subscribe(void *context){(void)context;fx_io();assert(!fx_subscription);return fx_subscription=1;}
static bool fx_unsubscribe(void *context,uint64_t token){
 (void)context;fx_io();assert(token==1&&fx_subscription&&!fx_session_live);fx_subscription=0;return true;
}
static bool fx_touch_poll(void *context,size_t maximum){(void)context;fx_io();assert(maximum==1);return true;}
/* A complete down/up sequence goes through the actual raw-touch reducer. */
static int32_t fx_touch_next(void *context,uint64_t subscription,risc_touch_event_v1 *out){
 (void)context;fx_io();assert(subscription==1&&fx_subscription);
 static const unsigned times[]={100,120,300,320};
 if(fx_event_index>=sizeof(times)/sizeof(times[0])||fx_ticks<times[fx_event_index])return 0;
 unsigned item=fx_event_index++;bool down=!(item&1u);
 fx_touch_x=20;fx_touch_y=item<2?64:88;
 fx_touch_down=down;++fx_sequence;
 *out=(risc_touch_event_v1){.sequence=fx_sequence,.timestamp_ms=fx_ticks,
  .kind=down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,.id=1,.x=fx_touch_x,.y=fx_touch_y};return 1;
}
static bool fx_touch_snapshot(void *context,risc_touch_snapshot_v1 *out){
 (void)context;fx_io();*out=(risc_touch_snapshot_v1){.sequence=fx_sequence,.timestamp_ms=fx_ticks,
  .width=(uint16_t)fx_width,.height=(uint16_t)fx_height,.contact_count=fx_touch_down?1:0};
 if(fx_touch_down)out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=fx_touch_x,.y=fx_touch_y};
 return true;
}
static const risc_touch_api_v1 fx_touch={1,sizeof(fx_touch),NULL,fx_subscribe,fx_unsubscribe,fx_touch_poll,fx_touch_next,fx_touch_snapshot};
static bool fx_battery_read(void *context,risc_battery_sample_v1 *out){(void)context;fx_io();*out=(risc_battery_sample_v1){3900,70,0};return true;}
static const risc_battery_gauge_api_v1 fx_battery={1,sizeof(fx_battery),NULL,fx_battery_read};
static bool fx_serial_snapshot(risc_serial_device_v1 *out,size_t *count){
 fx_io();assert(*count>=2);out[0]=(risc_serial_device_v1){17,9,RISC_SERIAL_TRANSPORT_USB,{0}};
 out[1]=(risc_serial_device_v1){23,11,RISC_SERIAL_TRANSPORT_USB,{0}};*count=2;return true;
}
static const risc_serial_port_inventory_v1 fx_serial={.discovery={.serial={.api_version=1,.struct_size=sizeof(fx_serial)}},.snapshot=fx_serial_snapshot};
static bool fx_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 fx_io();assert(version==1&&out->struct_size==sizeof(*out));const void *api=NULL;
 if(!strcmp(name,"display.output"))api=&fx_display;
 else if(!strcmp(name,"input.touch.raw"))api=&fx_touch;
 else if(!strcmp(name,"board.battery"))api=&fx_battery;
 else if(!strcmp(name,T5_SERIAL_PORT_CAPABILITY)){assert(instance==41);fx_instance=instance;api=&fx_serial;++fx_serial_acquires;}
 else assert(!"unexpected capability acquisition");
 ++fx_live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=++fx_acquires,.generation=1,.api=api};return true;
}
static bool fx_release(risc_runtime_capability_v1 *grant){
 fx_io();assert(grant&&grant->api&&grant->slot&&fx_live);
 if(grant->api==&fx_serial){assert(!fx_session_live);++fx_serial_releases;}
 else assert(!fx_session_live);
 --fx_live;++fx_releases;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static int32_t fx_open_stream(uint64_t context,const risc_runtime_capability_v1 *grant,const void *request,uint32_t size,
 uint32_t budget_ms,risc_stream_opened_v1 *out){
 fx_io();assert(context==42&&grant->api==&fx_serial&&request&&size==sizeof(risc_serial_stream_open_v1));
 assert(budget_ms>0&&budget_ms<=500&&!fx_session_live);++fx_stream_opens;
 const risc_serial_stream_open_v1 *open=request;assert(open->api_version==1&&open->struct_size==size);
 fx_device=open->provider_device;fx_generation=open->device_generation;
 assert(fx_device==17&&fx_generation==9&&fx_instance==41);
 if(fx_is("retained-open")){fx_terminal_result=true;return PSC_RETAINED;}
 if(fx_is("open-refused"))return PSC_BUSY;
 fx_session_live=true;*out=(risc_stream_opened_v1){sizeof(*out),0,100,101,102};return PSC_OK;
}
static int32_t fx_call_stream(uint64_t context,risc_stream_session_t session,const void *request,uint32_t size,
 uint32_t budget_ms,void *reply,uint32_t capacity,uint32_t *actual){
 fx_io();assert(context==42&&session==100&&fx_session_live&&request&&size==sizeof(risc_serial_stream_call_v1));
 assert(budget_ms>0&&budget_ms<=500&&!reply&&!capacity&&actual);*actual=0;
 const risc_serial_stream_call_v1 *call=request;
 assert(call->operation==RISC_SERIAL_STREAM_CHECK_DEVICE||call->operation==RISC_SERIAL_STREAM_CONFIGURE||call->operation==RISC_SERIAL_STREAM_CONTROL_LINES);
 if(fx_is("retained-tick")){fx_terminal_result=true;return PSC_RETAINED;}
 return PSC_OK;
}
static int32_t fx_close_stream(uint64_t context,risc_stream_session_t session,uint32_t budget_ms){
 fx_io();assert(context==42&&session==100&&fx_session_live&&budget_ms>0&&budget_ms<=1000);++fx_stream_closes;
 if(fx_is("retained-close")||fx_is("fini-retained")||fx_is("launch-retained")){fx_terminal_result=true;return PSC_IO;}
 fx_session_live=false;return PSC_OK;
}
static int32_t fx_read_stream(uint64_t context,risc_stream_client_handle_t stream,void *data,uint32_t capacity,uint32_t *count){
 fx_io();assert(context==42&&stream==101&&fx_session_live&&capacity&&capacity<=PSC_CHUNK);++fx_reads;if(fx_pending)++fx_reads_pending;
 *count=0;if(fx_rx_offset>=sizeof(fx_received)-1)return PSC_AGAIN;
 size_t n=sizeof(fx_received)-1-fx_rx_offset;if(n>capacity)n=capacity;if(n>7)n=7;
 memcpy(data,fx_received+fx_rx_offset,n);fx_rx_offset+=n;*count=(uint32_t)n;return PSC_OK;
}
static int32_t fx_write_stream(uint64_t context,risc_stream_client_handle_t stream,const void *data,uint32_t length,uint32_t *count){
 fx_io();assert(context==42&&stream==102&&fx_session_live&&data&&length&&length<=PSC_CHUNK);++fx_writes;
 *count=length>3?3:length;return PSC_OK;
}
static int32_t fx_stream_info(uint64_t context,risc_stream_client_handle_t stream,risc_stream_client_info_v1 *out){
 (void)context;(void)stream;(void)out;assert(!"unused stream info");return PSC_UNSUPPORTED;
}
static bool fx_stream_getter(risc_stream_client_v1 *out){
 fx_io();assert(out&&out->struct_size==sizeof(*out));
 *out=(risc_stream_client_v1){1,sizeof(*out),42,fx_open_stream,fx_call_stream,fx_close_stream,fx_read_stream,fx_write_stream,fx_stream_info};return true;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){fx_io();return version==1?&fx_runtime:NULL;}

int main(int argc,char **argv){
 assert(argc>=2&&argc<=5);fx_case=argv[1];
 if(argc>=4){fx_width=(unsigned)strtoul(argv[2],NULL,10);fx_height=(unsigned)strtoul(argv[3],NULL,10);}
 assert(fx_width>=160&&fx_width<=FX_WIDTH_MAX&&fx_height>=160&&fx_height<=FX_HEIGHT_MAX);
 if(argc==5)fx_capture=argv[4];
 memset(fx_buffer.pixels,0xc3,sizeof(fx_buffer.pixels));
 memset(fx_buffer.before,0xa5,sizeof(fx_buffer.before));memset(fx_buffer.after,0x5a,sizeof(fx_buffer.after));
 fx_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,.monotonic_ms=fx_monotonic,
  .yield_ms=fx_yield,.diagnostic=fx_diagnostic,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,
  .retain_invocation=fx_retain,.exit_requested=fx_exit_requested,.stream_client=fx_stream_getter};
 assert(!app_module_init());
 if(!setjmp(fx_owner_escape))app_main();
 else {
  assert(fx_session_live&&fx_stream_opens==1&&!fx_stream_closes&&!fx_serial_releases);
  if(fx_is("launch-clean")||fx_is("launch-retained")) {
   assert(app.request_app_launch(0)==fx_is("launch-clean"));
   assert(fx_launches==(fx_is("launch-clean")?1u:0u));
  }else app_module_fini();
 }
 assert(fx_serial_acquires==1&&fx_stream_opens==1&&fx_presents>=2);
 if(!strncmp(fx_case,"retained-",9)||fx_is("fini-retained")||fx_is("launch-retained")){
  assert(fx_retained&&fx_fences==1&&portable_adapter_retained());
  unsigned checkpoint=fx_calls;
  assert(!portable_serial_monitor_close());app_module_fini();app_module_fini();
  const t5_app_api_v1 *view=&app;t5_app_input_t input={0};
  view->clear();view->draw_text(5,5,"MUST NOT DRAW");view->fill_rect(0,0,20,20,true);view->present(false);assert(!view->poll(&input,1));
  assert(fx_calls==checkpoint&&fx_calls==fx_terminal_calls&&fx_presents==fx_terminal_presents);
  assert(!memcmp(fx_frozen,fx_buffer.pixels,FX_PIXEL_CAPACITY));
  assert(!fx_serial_releases&&fx_live==4&&fx_subscription==1);
 }else{
  assert(!fx_retained&&!fx_session_live&&fx_stream_closes==(fx_is("open-refused")?0u:1u)&&fx_serial_releases==1);
  if(fx_is("open-refused"))assert(!fx_reads&&!fx_writes&&!fx_rx_offset);
  else assert(fx_reads&&fx_rx_offset==sizeof(fx_received)-1);
  if(fx_is("slow-frame"))assert(fx_reads_pending>0);
  app_module_fini();assert(!fx_live&&!fx_subscription&&!fx_frame_live&&!fx_pending&&fx_acquires==fx_releases);
 }
 fx_guards();printf("{\"case\":\"%s\",\"width\":%u,\"height\":%u,\"frames\":%u,\"opens\":%u,\"closes\":%u,\"fences\":%u,\"result\":\"PASS\"}\n",
 fx_case,fx_width,fx_height,fx_presents,fx_stream_opens,fx_stream_closes,fx_fences);return 0;
}
