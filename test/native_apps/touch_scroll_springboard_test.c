/* Real Springboard, raw input, raster and nonblocking adapter. Provider time
 * and catalog changes are simulated; no physical display performance claim. */
#define TEST_SCROLL_CATALOG
#define PAPER_TRANSITION_MAIN transition_fixture_main
#include "paper_transition_test.c"
#include "PortableTimeFormat.h"
extern void fixture_grid_geometry(void);
extern int fixture_offset(void),fixture_limit(void),fixture_velocity(void);
extern int fixture_render_offset(void);
extern bool fixture_render_highlight(void);
extern bool fixture_pressed(void),fixture_pending(void),fixture_highlight_completed(const char *name);
#ifndef TEST_DEPLOYMENT_CATALOG
#define ENTRY(n) {.display_name=n,.file_name=n ".elf",.icon="solid:f017",.compatible=true}
const t5_app_manifest_t portable_catalog[22]={
 {.display_name="Files",.file_name="file_browser.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="Serial",.file_name="serial_monitor.elf",.icon="solid:f120",.compatible=true},
 {.display_name="Bluetooth",.file_name="ble_scanner.elf",.icon="solid:f7c0",.compatible=true},
 {.display_name="Points",.file_name="points_in_time.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 ENTRY("Six"),ENTRY("Seven"),ENTRY("Eight"),ENTRY("Nine"),ENTRY("Ten"),ENTRY("Eleven"),ENTRY("Twelve"),ENTRY("Thirteen"),ENTRY("Fourteen"),ENTRY("Fifteen"),
 {.display_name="App Store",.file_name="app_store.elf",.icon="solid:f019",.compatible=true},ENTRY("Seventeen"),{.display_name="USB SD Transfer",.file_name="usb_sd_transfer.elf",.icon="solid:f0ec",.compatible=true},{.display_name="GameBoy",.file_name="gameboy.elf",.icon="solid:f11b",.compatible=true},ENTRY("Twenty"),ENTRY("TwentyOne"),ENTRY("TwentyTwo")};
const unsigned portable_catalog_count=22;
#endif
static unsigned start_at=700,drag_frames,momentum_frames,pressed_frames,busy_samples,quick_frames;
static int rendered_offset[64];static bool rendered_highlight[64];
static int max_offset,release_offset;static bool changed,saved_before_swipe;
static char launch_path[128];static unsigned home_launches;
static bool swipe_case(void){return !strncmp(test_case,"swipe-",6)||is_case("gameboy");}
unsigned fixture_count(void){
#ifdef TEST_DEPLOYMENT_CATALOG
 return portable_catalog_count;
#else
 return is_case("twelve")?12:is_case("thirteen")?13:is_case("eighteen")||is_case("swipe-usb")?18:is_case("empty")||is_case("swipe-empty")?0:is_case("one")||is_case("swipe-one")?1:is_case("seventeen")||is_case("swipe-seventeen")?17:is_case("twenty")?20:is_case("gameboy")?19:is_case("swipe-last")?22:is_case("remove")&&changed?20:is_case("insert")&&!changed?20:21;
#endif
}
unsigned fixture_index(unsigned i){return is_case("insert")&&changed?(i?i-1:20):is_case("reorder")&&changed&&i<2?1-i:is_case("remove")&&changed?i+1:i;}
static unsigned elapsed(void){return ticks>=start_at?ticks-start_at:0;}
static bool scroll_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();input_reads++;immutable_frame();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 unsigned at=elapsed();if(ticks<start_at)return true;
 if(swipe_case()&&at>=260&&!saved_before_swipe){save_image("before-swipe",completed_image);saved_before_swipe=true;}
 if(pending)busy_samples++;
 int offset=fixture_offset();assert(offset>=0&&offset<=fixture_limit());if(offset>max_offset)max_offset=offset;
 if(at<260)release_offset=offset;
 if((is_case("insert")||is_case("remove")||is_case("reorder"))&&at>=130)changed=true;
 int x=80,y=160;bool down=false;
 if(is_case("tap")||is_case("busy-tap")||is_case("flipped")||is_case("reorder")||is_case("remove")||is_case("insert")||is_case("launch-fail")||is_case("superseded-feedback")||is_case("quick-pending"))down=at>=80&&at<160;
 bool drag=!(is_case("dots")||is_case("empty")||is_case("one")||is_case("transition")||is_case("tap")||is_case("busy-tap")||is_case("flipped")||is_case("reorder")||is_case("remove")||is_case("insert")||is_case("launch-fail")||is_case("superseded")||is_case("superseded-feedback")||is_case("quick-pending")||is_case("new-contact"));
 if(!strncmp(test_case,"clock-",6)||is_case("gameboy"))drag=false;

 if(drag&&at>=80&&at<260){down=true;x=240;y=510-(int)(at-80)/2;}
 if((is_case("stop-tap")||is_case("busy-hit"))&&down){x=370-(int)(at-80);y=136;}
 if(is_case("horizontal")&&down){x=100+(int)(at-80);y=350;}
 if(is_case("bounds")&&at>=600&&at<900){down=true;x=240;y=120+(int)(at-600)*2;}
 if((is_case("stop-tap")||is_case("busy-hit"))&&at>=280&&at<340){down=true;x=175;y=fixture_limit()?240-fixture_offset():240;}
 if(is_case("bottom-tap")&&at>=1000&&at<1080){down=true;x=110;y=760-fixture_offset();}
 if(is_case("new-contact")){
  if(at>=80&&at<120){down=true;}
  if(at>=140&&at<260){down=true;x=240;y=136+(int)(at-140);}
 }
 unsigned top_start=is_case("quick-pending")?180:280;
 if((is_case("quick")||is_case("quick-pending"))&&at>=top_start&&at<top_start+300){down=true;x=240;y=20+(int)(at-top_start)*2;}
 if((is_case("quick")||is_case("quick-pending"))&&at>=1600&&at<1740){down=true;x=240;y=675-(int)(at-1600)*2;}
 if(swipe_case()) {
  down=at>=80&&at<260;x=370;y=136;
  if(down) {
   int distance=(int)(at-80);
   x-=distance;
   if(is_case("swipe-fast"))x=370-(distance<40?distance*4:160);
   if(is_case("swipe-reversed"))x=distance<100?370-distance*2:170+(distance-100)*2;
   if(is_case("swipe-short"))x=370-(distance<40?distance:40);
   if(is_case("swipe-threshold"))x=370-(distance<44?distance:44);
   if(is_case("swipe-diagonal"))y+=distance*2/3;
   if(is_case("swipe-horizontal-lock")) {x=370-distance;y+=distance>40?(distance-40)*3:0;}
   if(is_case("swipe-vertical-lock")) {x=distance<40?370:370-distance;y=136-(distance<20?distance:20);}
   if(is_case("swipe-queued-up"))x=370-(distance<20?distance:20);
   if(is_case("swipe-top"))y=20;
   if(is_case("swipe-quick")) {x=240;y=20+distance*2;}
  }
  if(is_case("swipe-fast")&&at>=140)down=false;
  if((is_case("swipe-right")||is_case("swipe-bounds"))&&at>=600&&at<780){down=true;x=40+(int)(at-600);y=350;}
  if(is_case("swipe-bounds")&&at>=350&&at<530){down=true;x=440-(int)(at-350);y=350;}
  if(is_case("swipe-bounds")&&at>=1000&&at<1180){down=true;x=40+(int)(at-1000);y=350;}
  if(is_case("swipe-busy-reverse")&&at>=300&&at<460){down=true;x=40+(int)(at-300);y=350;}
  if(is_case("swipe-new-contact")&&at>=300&&at<360){down=true;x=80;y=160;}
  if(is_case("gameboy")&&at>=1000&&at<1080){down=true;x=80;y=480;}
  if(is_case("swipe-usb")&&at>=1000&&at<1080){down=true;x=400;y=320;}
  if(is_case("swipe-last")&&at>=1000&&at<1080){down=true;x=80;y=640;}
  if(is_case("swipe-tap")&&at>=1000&&at<1080){down=true;x=80;y=160;}
  if(is_case("swipe-quick")&&at>=1600&&at<1740){down=true;x=240;y=675-(int)(at-1600)*2;}
 }
 if(is_case("flipped")||is_case("swipe-flipped")){x=479-x;y=799-y;}
 if(is_case("dots")&&at>=80&&at<160){down=true;x=266;y=768;}
 if(is_case("home-touch")&&at>=180)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
 if(is_case("swipe-home-touch")&&at>=180)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 if(is_case("cancelled")&&at>=160&&at<220)out->contact_count=2;
 if(is_case("swipe-cancelled")&&at>=160&&at<220)out->contact_count=2;
 if(is_case("swipe-new-id")&&at>=160)out->contacts[0].id=2;
 return true;
}
/* A provider reports ordered edges and a matching snapshot watermark. A
 * changing snapshot with next(empty) is corrupt input, not a drag report. */
static risc_touch_snapshot_v1 scroll_state={.width=480,.height=800};
static risc_touch_event_v1 scroll_events[256];
static unsigned scroll_head,scroll_count;
static void scroll_emit(unsigned kind,risc_touch_contact_v1 point) {
 assert(scroll_count<256);
 if(kind==RISC_TOUCH_EVENT_UP && (is_case("swipe-queued-up")||is_case("swipe-queued-short")))point.x=is_case("swipe-queued-up")?250:350;
 scroll_events[(scroll_head+scroll_count++)%256]=(risc_touch_event_v1){.sequence=++scroll_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static bool scroll_poll(void*c,size_t budget){
 assert(fx_touch_poll(c,budget));risc_touch_snapshot_v1 now;assert(scroll_touch(c,&now));
 for(unsigned i=0;i<scroll_state.contact_count;i++){
  unsigned j=0;for(;j<now.contact_count;j++)if(now.contacts[j].id==scroll_state.contacts[i].id)break;
  if(j==now.contact_count)scroll_emit(RISC_TOUCH_EVENT_UP,scroll_state.contacts[i]);
 }
 for(unsigned i=0;i<now.contact_count;i++){
  unsigned j=0;for(;j<scroll_state.contact_count;j++)if(now.contacts[i].id==scroll_state.contacts[j].id)break;
  if(j==scroll_state.contact_count)scroll_emit(RISC_TOUCH_EVENT_DOWN,now.contacts[i]);
  else if(memcmp(&now.contacts[i],&scroll_state.contacts[j],sizeof(now.contacts[i])))scroll_emit(RISC_TOUCH_EVENT_MOVE,now.contacts[i]);
 }
 for(unsigned bit=0;bit<32;bit++)if((now.buttons^scroll_state.buttons)&(1u<<bit))scroll_emit(now.buttons&(1u<<bit)?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,(risc_touch_contact_v1){.id=(uint8_t)bit});
 now.sequence=scroll_state.sequence;now.timestamp_ms=ticks;scroll_state=now;return true;
}
static bool scroll_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=scroll_state;return true;}
static int32_t scroll_next(void*c,uint64_t token,risc_touch_event_v1*out){
 (void)c;assert(token==1);if(!scroll_count)return 0;
 *out=scroll_events[scroll_head];scroll_head=(scroll_head+1)%256;--scroll_count;return 1;
}
static bool scroll_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};unsigned at=elapsed();
 unsigned stop=is_case("home")||is_case("swipe-home")?180:(is_case("quick")||is_case("quick-pending")||is_case("swipe-quick"))?2600:1800;
 if(ticks>=start_at&&at>=stop){out->buttons=out->pressed=is_case("home")||is_case("swipe-home")?RISC_NAV_HOME:RISC_NAV_BACK;stop_seen=true;}
 return true;
}
static bool scroll_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!raster_clip_active);
 /* The logical controller can advance during replay of a sealed frame. */
 rendered_offset[presents]=fixture_render_offset();rendered_highlight[presents]=fixture_render_highlight();
 if(ticks>=start_at){unsigned at=elapsed();if(at>=80&&at<260&&!quick_modal)drag_frames++;if(at>=260&&at<700&&!quick_modal)momentum_frames++;if(fixture_pressed())pressed_frames++;}
 if(quick_modal){quick_frames++;assert(!fixture_velocity()&&!fixture_pending());}
 return test_submit(c,frame,damage,n,options,token);
}
static bool scroll_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 if(is_case("superseded-feedback")&&ticks>=start_at+120){io();assert(pending==token);immutable_frame();out->state=RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 if(is_case("swipe-superseded")&&ticks>=start_at+260){io();assert(pending==token);immutable_frame();out->state=RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 return test_status(c,token,out);
}
static bool scroll_launch(const char *path){
 if(!strcmp(path,"default.elf")){assert(!pending&&!frames&&!fixture_velocity()&&!fixture_pending());home_launches++;return true;}
 assert(fixture_highlight_completed(path));assert(!pending&&!frames);assert(!paper_handoff_old);
 strcpy(launch_path,path);launch_count++;launch_tick=ticks;return !is_case("launch-fail");
}
static unsigned format_reads;
static int32_t scroll_get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size) {
 if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)) {
  format_reads++;
  if(is_case("clock-unavailable"))return RISC_KEY_VALUE_IO;
  if(is_case("clock-invalid")){*size=1;return RISC_KEY_VALUE_OK;}
  if(strstr(test_case,"24")||is_case("clock-12")) {
   unsigned mode=strstr(test_case,"24")?PORTABLE_TIME_FORMAT_24:PORTABLE_TIME_FORMAT_12;
   const uint8_t record[]={0x54,1,(uint8_t)mode,(uint8_t)(mode^0xa5u)};
   assert(capacity>=sizeof(record));memcpy(data,record,sizeof(record));*size=sizeof(record);return RISC_KEY_VALUE_OK;
  }
 }
 return test_get(c,key,data,capacity,size);
}
extern void fixture_time_text(char out[12],bool known,uint8_t hour,uint8_t minute);
static risc_input_navigation_api_v1 scroll_navigation;
static bool scroll_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 bool ok=test_acquire(name,version,instance,out);if(!strcmp(name,"input.navigation"))out->api=&scroll_navigation;return ok;
}
int main(int argc,char **argv){
 assert(argc==4);test_case=argv[1];capture_directory=strcmp(argv[3],"-")?argv[3]:NULL;
 FILE *source=fopen(argv[2],"rb");assert(source);assert(fread(completed_image,1,sizeof(completed_image),source)==sizeof(completed_image));assert(!fclose(source));save_image("outgoing",completed_image);
 test_display.metrics.power.history.base=fx_display;test_display.metrics.power.history.base.struct_size=sizeof(test_display);
 test_display.metrics.power.history.base.get_info=test_info;test_display.metrics.power.history.base.acquire=test_frame;test_display.metrics.power.history.base.release=test_frame_release;
 test_display.metrics.power.history.base.submit=scroll_submit;test_display.metrics.power.history.base.present_status=scroll_status;
 test_display.metrics.power.history.base.set_brightness=test_brightness;
 test_display.metrics.power.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;test_display.metrics.power.history.extension_version=1;test_display.metrics.power.history.seed_previous=test_seed;
 test_display.metrics.power.power_tag=RISC_DISPLAY_POWER_TAG;test_display.metrics.power.power_version=1;test_display.metrics.power.prepare=test_power;test_display.metrics.power.resume=test_power;
 test_display.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;test_display.metrics.metrics_version=RISC_DISPLAY_METRICS_VERSION;test_display.metrics.snapshot=test_metrics;
 test_display.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;test_display.snapshot_version=RISC_DISPLAY_SNAPSHOT_VERSION;test_display.copy_completed=test_snapshot;
 if(!is_case("transition"))test_display.metrics.power.history.base.struct_size=sizeof(risc_display_output_api_v1);
 frame_latency=(is_case("busy-tap")||is_case("busy-hit")||is_case("new-contact")||is_case("quick-pending")||is_case("swipe-busy")||is_case("swipe-busy-reverse")||is_case("swipe-new-contact"))?240:20;
 if(is_case("busy-tap")||is_case("busy-hit"))start_at=1000;
 if(is_case("superseded")||is_case("superseded-feedback"))frame_latency=100;
 test_touch_api=fx_touch;test_touch_api.snapshot=scroll_snapshot;test_touch_api.poll=scroll_poll;test_touch_api.next=scroll_next;
 scroll_navigation=test_navigation;scroll_navigation.poll=scroll_nav;
 test_kv=fx_kv;test_kv.put=test_put;test_kv.get=scroll_get;
 test_alarm=fx_alarm;test_alarm.step=test_alarm_step;test_alarm.status=test_alarm_status;test_alarm.acknowledge=test_alarm_ack;
 fx_runtime.acquire=scroll_acquire;fx_runtime.release=test_release;fx_runtime.request_launch=scroll_launch;fx_runtime.yield_ms=test_yield;
 assert(!app_module_init());const paper_presentation *view=paper_presentation_get();assert(view);
 if(is_case("flipped")||is_case("swipe-flipped")){paper_flip_ui=true;}
 if(!strncmp(test_case,"clock-",6)) {
  local_time.hour=strstr(test_case,"midnight")?0:strstr(test_case,"noon")?12:strstr(test_case,"single")?9:13;
  local_time.minute=strstr(test_case,"single")?5:0;
  char text[12];fixture_time_text(text,true,local_time.hour,local_time.minute);
  const char *expected=strstr(test_case,"midnight")?(strstr(test_case,"24")?"0:00":"12:00"):strstr(test_case,"noon")?"12:00":strstr(test_case,"single")?"9:05":strstr(test_case,"24")?"13:00":"1:00";
  assert(!strcmp(text,expected));assert(format_reads&&!kv_writes);
  fixture_time_text(text,false,0,0);assert(!strcmp(text,"--:--"));
 }
 app_main();fixture_grid_geometry();
 if(is_case("superseded")||is_case("superseded-feedback")||is_case("swipe-superseded")){assert(retained&&!launch_count);check_retained(view);}
 else {
  assert(!pending);save_image("endpoint",completed_image);app_module_fini();assert(!live&&!frames&&!subscriptions&&!paper_handoff_old);
  bool expected=is_case("gameboy")||is_case("tap")||is_case("busy-tap")||is_case("flipped")||is_case("reorder")||is_case("insert")||is_case("launch-fail")||is_case("swipe-tap")||is_case("swipe-last")||is_case("swipe-usb");
  assert(launch_count==(expected?1u:0u));
  if(expected){assert(!strcmp(launch_path,is_case("gameboy")?"gameboy.elf":is_case("swipe-usb")?"usb_sd_transfer.elf":is_case("swipe-last")?portable_catalog[21].file_name:is_case("swipe-tap")?portable_catalog[12].file_name:"file_browser.elf"));assert(pressed_frames);}
  if(is_case("drag")||is_case("queued-up")||is_case("bottom-tap"))assert(!max_offset);
  if(is_case("bounds"))assert(!max_offset);
  if(is_case("horizontal"))assert(!max_offset);
  if(swipe_case()) {
   if(is_case("gameboy")||is_case("swipe-left")||is_case("swipe-queued-up")||is_case("swipe-flipped")||is_case("swipe-busy")||is_case("swipe-tap")||is_case("swipe-seventeen")||is_case("swipe-last")||is_case("swipe-usb")||is_case("swipe-fast")||is_case("swipe-horizontal-lock"))assert(max_offset==fixture_limit()&&fixture_offset()==fixture_limit());
   else if(is_case("swipe-right")||is_case("swipe-bounds"))assert(max_offset==fixture_limit()&&!fixture_offset());
   else if(is_case("swipe-busy-reverse")||is_case("swipe-reversed"))assert(max_offset>0&&!fixture_offset());
   else if(is_case("swipe-new-contact"))assert(max_offset==fixture_limit()&&!launch_count);
   else if(is_case("swipe-short")||is_case("swipe-threshold")||is_case("swipe-cancelled")||is_case("swipe-new-id")||is_case("swipe-queued-short"))assert(max_offset>0&&!fixture_offset());
   else if(is_case("swipe-home")||is_case("swipe-home-touch")||is_case("swipe-superseded"))assert(max_offset>0);
   else assert(!max_offset);
   assert(!fixture_velocity());if(!expected)assert(!fixture_pending());
  }
  if(is_case("dots"))assert(fixture_offset()==fixture_limit()&&!launch_count);
  if(is_case("swipe-left")){assert(drag_frames>=2&&momentum_frames&&release_offset>0&&release_offset<fixture_limit());}
  if(is_case("swipe-busy")||is_case("swipe-busy-reverse"))assert(busy_samples>5);
  if(is_case("swipe-busy-reverse")) {
   assert(rendered_offset[presents-1]==0);
   for(unsigned i=0;i<presents;i++)assert(rendered_offset[i]<fixture_limit());
  }
  if(is_case("home")||is_case("home-touch"))assert(home_launches==1&&!fixture_velocity()&&!fixture_pending());
  if(is_case("quick")||is_case("quick-pending"))assert(quick_frames&&!fixture_velocity()&&!fixture_pending());
  if(is_case("swipe-home")||is_case("swipe-home-touch"))assert(home_launches==1);
  if(is_case("swipe-quick"))assert(quick_frames);
  if(is_case("transition"))assert(copies==1&&presents>=4&&frame_coverage[0]==1&&frame_coverage[presents-1]==16);
 }
 printf("{\"rendered\":[");for(unsigned i=0;i<presents;i++)printf("%s{\"frame\":%u,\"ms\":%u,\"offset\":%d,\"highlight\":%s}",i?",":"",i+1,frame_tick[i],rendered_offset[i],rendered_highlight[i]?"true":"false");
 printf("],\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"pressed_frames\":%u,\"max_offset\":%d,\"launches\":%u,\"busy_samples\":%u}\n",test_case,presents,drag_frames,momentum_frames,pressed_frames,max_offset,launch_count,busy_samples);return 0;
}
