#define PAPER_QUICK_MAIN legacy_quick_main
#define PAPER_QUICK_RUNTIME legacy_quick_runtime
#include "paper_quick_test.c"
#undef PAPER_QUICK_MAIN
#undef PAPER_QUICK_RUNTIME
static bool motion_health(risc_runtime_health_v1 *out){out->uptime_ms=ms;return ms<1800;}
static bool motion_touch(void*c,risc_touch_snapshot_v1*out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 bool down=false;unsigned y=20;
 if(ms>=80&&ms<260){down=true;y=ms<120?20:ms<180?200:400;}
 if(ms>=1000&&ms<1100){down=true;y=ms<1040?660:520;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=(uint16_t)y};}
 return true;
}
static bool motion_acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){
 if(!qa_acquire(name,version,id,out))return false;
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 input;input=*(const risc_touch_api_v1*)out->api;input.snapshot=motion_touch;out->api=&input;}
 return true;
}
static const risc_runtime_api_v1 motion_runtime={.api_version=1,.struct_size=sizeof(motion_runtime),.health=motion_health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=motion_acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&motion_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==2);capture_dir=argv[1];qa_case=0;
 assert(app_module_init()==0);app_main();app_module_fini();
 fprintf(stderr,"motion state frames=%u grants=%u subs=%u radios=%u launches=%u writes=%u presents=%u ms=%u\n",frames,grants,subs,radio_acquires,launches,writes,presents,ms);
 assert(!frames&&!grants&&!subs&&!radio_acquires&&!launches&&!writes);
 assert(presents>=5&&presents<100&&!memcmp(background,pixels,sizeof(pixels)));
 printf("Paper pull-down: %u actual frames, exact background restored, no actions/launches/leaks\n",presents);
 return 0;
}
