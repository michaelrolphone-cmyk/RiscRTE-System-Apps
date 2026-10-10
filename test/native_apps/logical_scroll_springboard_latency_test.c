/* Production Springboard and frame ownership; completed highlights are not
 * required to request a current logical target. */
#define TEST_CORE_PAPER_MOTION
#define TEST_CUSTOM_CATALOG
#include "native_system_apps_test.c"
#include "logical_touch_queue_fixture.h"
#define ITEM(n) {.display_name="App " #n,.file_name="app" #n ".elf",.icon="solid:f017",.compatible=true}
const t5_app_manifest_t portable_catalog[13]={ITEM(0),ITEM(1),ITEM(2),ITEM(3),ITEM(4),ITEM(5),ITEM(6),ITEM(7),ITEM(8),ITEM(9),ITEM(10),ITEM(11),ITEM(12)};
const unsigned portable_catalog_count=13;
static unsigned latency,submitted_at,logical_at,busy_inputs,edge_buttons,logical_requests;
static bool pending,by_touch,drag,verified;
extern int logical_board_offset(void);
static uint8_t immutable_pixels[sizeof(pixels)];
static bool logic_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool logic_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 assert(!pending&&!raster_clip_active);bool ok=fx_submit(c,f,r,n,o,t);pending=true;submitted_at=ticks;memcpy(immutable_pixels,pixels,sizeof(pixels));return ok;
}
static bool logic_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&t==presents&&!memcmp(immutable_pixels,pixels,sizeof(pixels)));
 out->state=ticks-submitted_at<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)pending=false;
 return true;
}
static bool logic_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();assert(ticks<10000);*out=(risc_input_navigation_frame_v1){0};unsigned b=0;
 if(!drag&&ticks>=100&&ticks<1060&&(ticks-100)%80<20)b=RISC_NAV_DOWN;
 if(!by_touch&&!drag&&ticks>=1100&&ticks<1120)b=RISC_NAV_CONFIRM;
 if(drag&&ticks>=900&&!verified){assert(logical_board_offset()==480);verified=true;logical_at=ticks;}
 if(drag&&ticks>=1100&&ticks<1120)b=RISC_NAV_HOME;
 out->buttons=b;out->pressed=b&~edge_buttons;out->released=edge_buttons&~b;edge_buttons=b;return true;
}
static bool logic_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};
 if(drag&&ticks>=100&&ticks<300){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)(400-(ticks-100)),.y=320};if(pending)++busy_inputs;}
 if(by_touch&&ticks>=1100&&ticks<1180){
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
  unsigned x=80,y=160;
#else
  unsigned x=370,y=552; /* Legacy page cell 12: third pair, third upper icon. */
#endif
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};if(pending)++busy_inputs;
 }
 return true;
}
void logical_board_request(unsigned index){assert(index==12&&ticks<1300);logical_at=ticks;++logical_requests;}
static bool logic_launch(const char *name){io();assert(!strcmp(name,drag?"default.elf":"app12.elf")&&!pending&&!frames);++launches;return true;}
static bool logic_acquire(const char *name,uint32_t v,uint64_t i,risc_runtime_capability_v1 *out){
 if(!app_acquire(name,v,i,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=logic_info;a.submit=logic_submit;a.present_status=logic_status;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.poll=logical_raw_poll;a.next=logical_raw_next;a.snapshot=logical_raw_snapshot;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=logic_nav;out->api=&a;}
 return true;
}
int main(int argc,char **argv){
 assert(argc==3);by_touch=!strcmp(argv[1],"touch");drag=!strcmp(argv[1],"drag");latency=(unsigned)atoi(argv[2]);scenario="valid";
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=logic_acquire;fx_runtime.release=app_release;fx_runtime.request_launch=logic_launch;
 logical_raw_init(TEST_TOOLBAR_NATIVE_HEIGHT,TEST_TOOLBAR_NATIVE_WIDTH,logic_touch);
 assert(app_module_init()==0);app_main();assert(logical_requests==(drag?0u:1u)&&logical_at<1300&&!failed&&!retained&&!pending&&launches==1);
 if(latency==2300){if(by_touch||drag)assert(busy_inputs>0);assert(presents==1);}
 assert(logical_raw_head==logical_raw_tail);
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"controller\":\"springboard\",\"input\":\"%s\",\"latency\":%u,\"logical_at\":%u,\"frames\":%u,\"busy_inputs\":%u}\n",argv[1],latency,logical_at,presents,busy_inputs);return 0;
}
