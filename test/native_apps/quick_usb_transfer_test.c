#define PAPER_QUICK_MAIN legacy_quick_main
#define PAPER_QUICK_RUNTIME legacy_quick_runtime
#include "paper_quick_test.c"
#undef PAPER_QUICK_MAIN
#undef PAPER_QUICK_RUNTIME
static unsigned usb_case,usb_acquires;
static bool usb_quick_health(risc_runtime_health_v1*out){out->uptime_ms=ms;return ms<2200;}
static bool usb_quick_touch(void*c,risc_touch_snapshot_v1*out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 bool down=false;int x=240,y=20;
 if(ms>=80&&ms<260){down=true;y=ms<120?20:ms<180?200:400;}
 if(ms>=800&&ms<860){down=true;x=360;y=100;}
 if(ms>=1000&&ms<1060){down=true;x=300;y=150;}
 if(ms>=1300&&ms<1360){down=true;y=usb_case==1?644-(int)(ms-1300)*2:644;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
static bool usb_quick_submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*rect,size_t n,const risc_display_present_options_v1*options,risc_display_present_token_v1*token){
 assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 return qa_submit(c,frame,rect,n,options,token);
}
static uint8_t saved_brightness=40;
static int32_t usb_quick_get(void*c,const char*k,void*b,uint32_t n,uint32_t*used){
 if(!strcmp(k,PQA_RESTORE_BRIGHTNESS_KEY)){assert(n>=1);*(uint8_t*)b=saved_brightness;*used=1;return RISC_KEY_VALUE_OK;}return qa_get(c,k,b,n,used);
}
static int32_t usb_quick_put(void*c,const char*k,const void*b,uint32_t n){
 if(!strcmp(k,PQA_RESTORE_BRIGHTNESS_KEY)){assert(n==1);saved_brightness=*(const uint8_t*)b;writes++;return RISC_KEY_VALUE_OK;}return qa_put(c,k,b,n);
}
static const risc_key_value_v1 usb_quick_kv={1,sizeof(usb_quick_kv),NULL,usb_quick_get,usb_quick_put};
static bool usb_quick_acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*out){
 if(!strcmp(name,"usb.device.msc")){usb_acquires++;assert(!"Quick Actions must not acquire MSC");}
 if(!qa_acquire(name,v,id,out))return false;
 if(!strcmp(name,"storage.key-value"))out->api=&usb_quick_kv;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 output;output=*(const risc_display_output_api_v1*)out->api;output.submit=usb_quick_submit;out->api=&output;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 input;input=*(const risc_touch_api_v1*)out->api;qa_input_script=usb_quick_touch;assert(qa_input_script(NULL,&qa_input_state));out->api=&input;}
 return true;
}
static const risc_runtime_api_v1 usb_quick_runtime={.api_version=1,.struct_size=sizeof(usb_quick_runtime),.health=usb_quick_health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=usb_quick_acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&usb_quick_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==3);usb_case=(unsigned)atoi(argv[1]);capture_dir=argv[2];qa_case=9;
 assert(!app_module_init());app_main();app_module_fini();
 assert(!frames&&!grants&&!subs&&!radio_acquires&&!usb_acquires);
 assert(brightness_calls>=2&&writes>=2);
#ifdef PORTABLE_QUICK_USB_TRANSFER
 if(usb_case==0)assert(launches==1&&!strcmp(launched,"usb_sd_transfer.elf"));else assert(!launches);
#else
 assert(!launches);
#endif
 printf("Quick USB case %u: %u launches, %u brightness writes, zero USB grants\n",usb_case,launches,writes);return 0;
}
