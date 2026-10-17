/* Provider-only witness: unchanged production Settings app and shared adapter. */
#define main clock_fixture_main
#define risc_runtime_get_api clock_fixture_runtime
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
static uint8_t before_frame[48000], alert_frame[48000];
static unsigned alarm_acquired, display_admitted, touch_emitted;
static bool settings_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 bool ok=submit(c,f,r,n,o,t);if(!ok)return false;
 if(presents==1)memcpy(before_frame,pixels,sizeof pixels);
 if(presents==2)memcpy(alert_frame,pixels,sizeof pixels);
 const char*dir=getenv("WITNESS_FRAMES");
 if(dir&&presents<=3){char p[512];snprintf(p,sizeof p,"%s/frame-%u.pbm",dir,presents);FILE*f=fopen(p,"wb");assert(f);fprintf(f,"P4\n%u %u\n",PANEL_WIDTH,PANEL_HEIGHT);assert(fwrite(pixels,1,sizeof pixels,f)==sizeof pixels);fclose(f);}
 return true;
}
static int32_t settings_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*len){
 if(!strcmp(k,"time_format"))return get(c,k,out,cap,len);
 (void)c;(void)out;(void)cap;*len=0;return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_key_value_v1 settings_kv={1,sizeof(settings_kv),NULL,settings_get,NULL};
static int32_t settings_next(void*c,uint64_t id,risc_touch_event_v1*e){
 int32_t result=next_touch(c,id,e);if(result>0){touch_emitted++;if(e->kind==RISC_TOUCH_EVENT_UP)printf("RAW_UP %u,%u\n",e->x,e->y);}return result;
}
static bool settings_rtc_write(void*c,const twatch_rtc_time_v1*t){(void)c;(void)t;assert(!"Unrequested RTC write");return false;}
static bool settings_acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 static risc_display_output_api_v1 display;static risc_touch_api_v1 touch;static twatch_rtc_api_v1 settings_rtc;
 if(!strcmp(name,"rtc.clock")){settings_rtc=rtc_api;settings_rtc.write=settings_rtc_write;g->api=&settings_rtc;grants++;return true;}
 if(!strcmp(name,"storage.key-value")){assert(version==1&&id==1);g->api=&settings_kv;grants++;return true;}
 if(!strcmp(name,"display.output")){display=d;display.submit=settings_submit;g->api=&display;grants++;display_admitted++;return true;}
 if(!strcmp(name,"input.touch.raw")){touch=t;touch.next=settings_next;g->api=&touch;grants++;return true;}
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY))alarm_acquired++;
 return acquire(name,version,id,g);
}
static const risc_runtime_api_v1 settings_runtime={.api_version=1,.struct_size=sizeof(settings_runtime),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=settings_acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&settings_runtime:NULL;}
static bool black(unsigned x,unsigned y){return (alert_frame[y*(PANEL_WIDTH/8)+x/8]&(0x80u>>(x&7)))!=0;}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);scenario=13;int admitted=app_module_init();printf("ADMISSION init=%d display=%u alarm=%u\n",admitted,display_admitted,alarm_acquired);assert(admitted==0);
 app_main();
 bool restored=!memcmp(before_frame,pixels,sizeof pixels);
 /* Pixel witness uses the independently expected native paper button border. */
 unsigned black_top=0;for(unsigned x=32;x<448;x++)black_top+=black(x,612);
 printf("MEASURE frames=%u ack=%u restored=%u input_events=%u button_top_black=%u expected=416 rectangle=32,612,416,88 state=%u\n",presents,alarm_acks,restored,touch_emitted,black_top,alarm_state.state);
 app_module_fini();printf("LIFECYCLE frames=%u grants=%u subscriptions=%u launches=%u\n",frames,grants,subs,launches);assert(!frames&&!grants&&!subs&&!launches);
 return !(alarm_acks==1&&presents==3&&restored&&black_top==416);
}
