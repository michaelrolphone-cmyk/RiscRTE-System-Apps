/* Real app + adapter + shared USB provider + TinyUSB. SD/PHY/display are doubles. */
#define main baseline_main
#define risc_runtime_get_api baseline_runtime
#define TEST_ASYNC_PRESENT
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
#include "RiscInputNavigationV1.h"
#include "PortableUsbTransfer.h"
#include <RiscUsbDeviceMscV1.h>
extern const risc_usb_device_msc_api_v1 *usb_fixture_provider(unsigned);
extern unsigned usb_fixture_prepare_calls(void);
extern bool usb_fixture_phy_owned(void),usb_fixture_quiesce(void);
extern void usb_fixture_recover(void);
static const risc_usb_device_msc_api_v1 *real_usb;
static risc_usb_device_msc_api_v1_diagnostics checked_usb;
static unsigned mode,queued_until,last_prepare_input,prepare_frames,prepare_polls,prepare_before_frame;
static uint64_t owner_token;
static uint32_t latest_state;
static unsigned states,usb_releases;
static int32_t begin_usb(void*c,uint64_t*t){int32_t r=real_usb->begin(c,t);owner_token=*t;return r;}
static int32_t poll_usb(void*c,uint64_t t,risc_usb_device_msc_status_v1*out){
 unsigned before=usb_fixture_prepare_calls();int32_t r=real_usb->poll(c,t,out);assert(before==usb_fixture_prepare_calls());
 latest_state=out->state;states|=1u<<latest_state;
 if(latest_state==RISC_USB_MSC_PREPARING){prepare_polls++;assert(!usb_fixture_phy_owned());}
 return r;
}
static int32_t end_usb(void*c,uint64_t t,uint32_t why){int32_t r=real_usb->end(c,t,why);if(!r)owner_token=0;return r;}
static int32_t prepare_usb(void*c,uint64_t t){
 assert(prepare_frames && !frames && ms>=queued_until && polls!=last_prepare_input);
 last_prepare_input=polls;return risc_usb_device_msc_prepare(real_usb)->prepare_step(c,t);
}
void usb_fixture_sd_transaction(void){
 assert(owner_token && latest_state==RISC_USB_MSC_PREPARING && prepare_frames && !frames && ms>=queued_until);
 assert(last_prepare_input==polls);
}
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(polls==10||polls==14||polls==100)out->pressed=out->released=RISC_NAV_HOME;
 if(mode==2 && polls!=100)out->pressed=out->released=0;
 return true;
}
static bool nav_foreground(void*c,const risc_input_foreground_v1*p,size_t n){(void)c;(void)p;(void)n;return true;}
static bool nav_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,nav_foreground,nav_reset};
static bool input(void*c,risc_touch_snapshot_v1*out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(mode==3 && polls==50)usb_fixture_recover();
 bool down=polls==5 || ((mode==0||mode==1||mode==3)&&polls==30) || (mode==3&&polls==60);
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=644};}return true;
}
static bool display_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*tkn){
 (void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);frames=0;*tkn=++presents;queued_until=ms+24;
 if(latest_state==RISC_USB_MSC_PREPARING){prepare_frames++;if(prepare_frames==1)prepare_before_frame=usb_fixture_prepare_calls();}
 return true;
}
static bool display_status(void*c,risc_display_present_token_v1 tkn,risc_display_present_status_v1*out){(void)c;(void)tkn;out->state=ms<queued_until?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool app_health(risc_runtime_health_v1*out){out->uptime_ms=ms;return polls<120;}
static bool app_launch(const char*name){assert(!owner_token&&!portable_usb_transfer_owned()&&usb_releases);return launch_app(name);}
static bool app_release(risc_runtime_capability_v1*g){if(g->api==&checked_usb){assert(!owner_token&&usb_fixture_quiesce());usb_releases++;}return release(g);}
static bool app_acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*out){
 assert(v==1&&!id);
 if(!strcmp(name,"usb.device.msc")){out->api=&checked_usb;grants++;return true;}
 if(!strcmp(name,"input.navigation")){out->api=&nav;grants++;return true;}
 if(!strcmp(name,"board.battery"))return false;
 if(!acquire(name,v,id,out))return false;
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 p;p=t;p.snapshot=input;out->api=&p;}
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 p;p=d;p.submit=display_submit;p.present_status=display_status;out->api=&p;}
 return true;
}
static const risc_runtime_api_v1 runtime={1,sizeof(runtime),app_health,yield_ms,diagnostic,app_launch,app_acquire,app_release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==2);mode=(unsigned)atoi(argv[1]);real_usb=usb_fixture_provider(mode);checked_usb=*risc_usb_device_msc_diagnostics(real_usb);
 checked_usb.base.base.begin=begin_usb;checked_usb.base.base.poll=poll_usb;checked_usb.base.base.end=end_usb;checked_usb.base.prepare_step=prepare_usb;
 assert(!app_module_init());app_main();app_module_fini();
 assert(!grants&&!subs&&!frames&&!owner_token&&usb_releases==1&&launches==1&&!strcmp(launched,"default.elf"));
 assert(prepare_frames&&prepare_polls&&!prepare_before_frame&&usb_fixture_prepare_calls());
 if(mode==0)assert(usb_fixture_prepare_calls()==3&&(states&(1u<<RISC_USB_MSC_WAITING)));
 if(mode==1)assert(!(states&(1u<<RISC_USB_MSC_WAITING)));
 if(mode==2)assert(states&(1u<<RISC_USB_MSC_MEDIA_UNAVAILABLE));
 if(mode==3)assert(states&(1u<<RISC_USB_MSC_FAULT_RETAINED));
 printf("Production USB/app preparation mode%u: %u SD steps, %u RAM polls, settled frame first, clean return\n",mode,usb_fixture_prepare_calls(),prepare_polls);return 0;
}
