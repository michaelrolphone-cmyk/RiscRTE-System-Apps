/* Real clock/launcher + shared adapter with native MONO1 and logical touch.
 * Reuse the strict frame/grant/alarm fixture; all stimuli remain providers. */
#define main paper_base_main
#define risc_runtime_get_api paper_base_runtime_get_api
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
#include "PortableQuickPreferences.h"
#include "RiscInputNavigationV1.h"
static unsigned qa_case,writes,refreshes,brightness_calls,radio_acquires,extra_frames;
static bool bad_storage;
static uint8_t preferences[4];static bool has_preference[4];
static uint8_t background[48000];
static const char *capture_dir;
static int key_index(const char *key){return !strcmp(key,PQA_BRIGHTNESS_KEY)?0:!strcmp(key,PQA_VOLUME_KEY)?1:!strcmp(key,PQA_RESTORE_VOLUME_KEY)?2:!strcmp(key,PQA_DND_KEY)?3:-1;}
static int32_t qa_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*len){
 (void)c;assert(!frames);int n=key_index(k);*len=0;
#ifdef TEST_READER_FLIP
 if(!strcmp(k,"reader_flip_ui")){assert(cap>=4);const uint8_t value[]={0x52,1,1,0xa4};memcpy(out,value,4);*len=4;return RISC_KEY_VALUE_OK;}
#endif
 if(n<0){if(!strcmp(k,"time_format"))return get(c,k,out,cap,len);return RISC_KEY_VALUE_NOT_FOUND;}
 if(bad_storage)return RISC_KEY_VALUE_IO;
 if(!has_preference[n])return RISC_KEY_VALUE_NOT_FOUND;
 assert(cap>=1);*(uint8_t*)out=preferences[n];*len=1;return RISC_KEY_VALUE_OK;
}
static int32_t qa_put(void*c,const char*k,const void*data,uint32_t size){(void)c;assert(!frames&&size==1);int n=key_index(k);assert(n>=0);writes++;if(bad_storage)return RISC_KEY_VALUE_IO;preferences[n]=*(const uint8_t*)data;has_preference[n]=true;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 qa_kv={1,sizeof(qa_kv),NULL,qa_get,qa_put};
static bool qa_brightness(void*c,uint16_t level,uint16_t max){(void)c;assert(!frames&&level<=100&&max==100);brightness_calls++;return true;}
static bool qa_info(void*c,risc_display_info_v1*out){get_info(c,out);if(qa_case==9||qa_case==16||qa_case>=30)out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return true;}
static bool qa_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*tkn){
 bool ok=submit(c,f,r,n,o,tkn);if(!ok)return false;
 if(presents==1)memcpy(background,pixels,sizeof(background));else extra_frames++;
 if(capture_dir){char path[512];snprintf(path,sizeof(path),"%s/frame-%u.pbm",capture_dir,presents);FILE*file=fopen(path,"wb");assert(file);fprintf(file,"P4\n%u %u\n",PANEL_WIDTH,PANEL_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));fclose(file);}
 return true;
}
static bool qa_snapshot_raw(void*c,risc_touch_snapshot_v1*s){
 (void)c;memset(s,0,sizeof(*s));s->width=480;s->height=800;unsigned step=polls;int x=200,y=20;bool down=false;
 if(qa_case>=20&&qa_case<28){
  if(qa_case==26||qa_case==27){
   if(step==2||step==3){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=200,.y=step==3?100:20};}
   if(qa_case==26&&step==6)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;
   return true;
  }
  if(qa_case>=24)return true;
  if(qa_case==21)s->buttons=step<5?RISC_TOUCH_BUTTON_PRIMARY:0;
  else if(qa_case==22)s->buttons=(step>=3&&step<=8)?RISC_TOUCH_BUTTON_PRIMARY:0;
  else if(qa_case==23){down=step==2;x=110;y=136;}
  else s->buttons=step==3?RISC_TOUCH_BUTTON_PRIMARY:0;
 }else if(qa_case==1){down=step==2||step==3;x=step==3?280:200;}
 else if(qa_case==2){down=step==2;}
 else if(qa_case==3){down=step==2||step==3;y=step==3?0:70;}
 else if(qa_case==4){down=step==2||step==3;y=step==3?440:360;}
 else if(qa_case==5){down=step==2||step==3;y=step==3?100:20;if(step==3){s->contact_count=2;return true;}}
 else {
  down=step==2||step==3;y=step==3?100:20;
  if(qa_case==6||qa_case==7){if(step==6){down=true;x=130;y=345;}if(step==9){down=true;x=130;y=345;}}
  if(qa_case==8){if(step==6){down=true;x=340;y=345;}}
  if(qa_case==9){if(step==6||step==7){down=true;x=step==6?100:364;y=150;}}
  if(qa_case==10){if(step==6){down=true;x=140;y=450;}if(step==9){down=true;x=340;y=450;}}
  if(qa_case==11){if(step==6){down=true;x=340;y=553;}}
  if(qa_case==12){if(step==7){down=true;x=240;y=650;}}
  if(qa_case==13 && step==6)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;
  if(qa_case==15 && (step==9||step==10)){down=true;x=240;y=step==9?620:560;}
  if(qa_case==16 && (step==6||step==10)){down=true;x=340;y=553;}
  if(qa_case==30||qa_case==31||qa_case==33){
   if(step==6){down=true;x=130;y=345;}
   if(step==9||step==10){down=true;x=step==9?100:364;y=240;}
  }
  if(qa_case==32&&step==6){down=true;x=340;y=345;}
  if(step==14){down=true;x=240;y=675;}
 }
 if(down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}return true;
}
static bool qa_snapshot(void*c,risc_touch_snapshot_v1*s) {
 bool ok=qa_snapshot_raw(c,s);
#ifdef TEST_READER_FLIP
 for(unsigned i=0;i<s->contact_count && i<sizeof(s->contacts)/sizeof(s->contacts[0]);++i) {
  s->contacts[i].x=479-s->contacts[i].x;s->contacts[i].y=799-s->contacts[i].y;
 }
#endif
 return ok;
}
static int32_t qa_alarm_step(void*c){
 if(qa_case==12&&!alarm_fired&&polls>=5){alarm_fired=true;alarm_state.state=ALARM_STATE_ALERT;alarm_state.occurrence=(alarm_token_v1){1,1,1,1};}
 return alarm_step(c);
}
static int32_t qa_refresh(void*c){(void)c;assert(!frames);refreshes++;return ALARM_OK;}
static bool qa_nav_poll(void*c,risc_input_navigation_frame_v1*out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if((qa_case==14&&polls==6)||(qa_case==24&&polls==3))out->pressed=out->released=RISC_NAV_BACK;
 if((qa_case==25&&polls==3)||(qa_case==17&&polls==6)||(qa_case==27&&polls==6)||(qa_case==18&&polls==40))out->pressed=out->released=RISC_NAV_HOME;
 return true;
}
static bool qa_nav_foreground(void*c,const risc_input_foreground_v1*claims,size_t count){(void)c;(void)claims;(void)count;return true;}
static bool qa_nav_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 qa_nav={1,sizeof(qa_nav),NULL,qa_nav_poll,qa_nav_foreground,qa_nav_reset};
static bool qa_health(risc_runtime_health_v1*h){h->uptime_ms=ms;return polls<80;}
static bool qa_acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(name,"input.navigation")){g->api=&qa_nav;grants++;return true;}
 static risc_display_output_api_v1 display;static risc_touch_api_v1 touch;static alarm_service_v1 service;
 if(!strcmp(name,"storage.key-value")){assert(id==1);g->api=&qa_kv;grants++;return true;}
 if(!strcmp(name,"input.touch.raw")){touch=t;touch.snapshot=qa_snapshot;g->api=&touch;grants++;return true;}
 if(!strcmp(name,"display.output")){display=d;display.get_info=qa_info;display.submit=qa_submit;display.set_brightness=qa_brightness;g->api=&display;grants++;return true;}
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){service=alarm_api;service.step=qa_alarm_step;service.refresh=qa_refresh;
  static alarm_service_outputs_v1 outputs;
  if(qa_case>=30){outputs=(alarm_service_outputs_v1){.service=service,.output_modes=qa_case==31?ALARM_MODE_VIBRATE:qa_case==33?ALARM_MODE_SOUND:ALARM_MODE_VISUAL};outputs.service.struct_size=sizeof(outputs);g->api=&outputs;}else g->api=&service;
  grants++;return true;}
 if(strstr(name,"wifi")||strstr(name,"bluetooth"))radio_acquires++;
 return acquire(name,v,id,g);
}
static const risc_runtime_api_v1 qa_runtime={1,sizeof(qa_runtime),qa_health,yield_ms,diagnostic,launch_app,qa_acquire,release};
#ifndef PAPER_QUICK_RUNTIME
#define PAPER_QUICK_RUNTIME risc_runtime_get_api
#endif
#ifndef PAPER_QUICK_MAIN
#define PAPER_QUICK_MAIN main
#endif
const risc_runtime_api_v1*PAPER_QUICK_RUNTIME(uint32_t v){return v==1?&qa_runtime:NULL;}
int PAPER_QUICK_MAIN(int argc,char**argv){
 assert(argc>=2);qa_case=(unsigned)atoi(argv[1]);capture_dir=argc>2?argv[2]:NULL;bad_storage=qa_case==7;
 assert(app_module_init()==0);app_main();
#ifdef TEST_SPRINGBOARD
 if(qa_case==20||qa_case==22||qa_case==25||qa_case==26){
  unsigned accepted=launches;assert(t5_app_get_api(1)->request_app_launch(0));assert(launches==accepted);
  t5_app_input_t terminal={0};assert(t5_app_get_api(1)->poll(&terminal,0)&&terminal.exit_requested&&launches==accepted);
 }
#endif
 app_module_fini();assert(!frames&&!grants&&!subs&&!radio_acquires);
#ifdef TEST_SPRINGBOARD
 if(qa_case==21||qa_case==27)assert(!launches);else if(qa_case==24)assert(launches==1&&!strcmp(launched,"parent.elf"));else if(qa_case==23)assert(launches==1&&!strcmp(launched,"file_browser.elf"));else assert(launches==1&&!strcmp(launched,"default.elf"));
#else
 if(qa_case>=1&&qa_case<=4&&qa_case!=2)assert(launches==1&&!strcmp(launched,"springboard.elf"));else assert(!launches);
 if(qa_case==0||qa_case==7||qa_case==10||qa_case==11||qa_case==13||qa_case==14||qa_case==15||qa_case==17)assert(presents==3&&!memcmp(background,pixels,sizeof(pixels)));
 if(qa_case==1||qa_case==2||qa_case==3||qa_case==4||qa_case==5)assert(presents==1);
 if(qa_case==6)assert(writes==3&&preferences[1]==50&&refreshes==2);
 else if(qa_case==8||qa_case==32)assert(writes==1&&preferences[3]==1&&refreshes==1);
 else if(qa_case==9)assert(writes==1&&preferences[0]==100&&brightness_calls>=1);
 else if(qa_case==33)assert(writes==3&&preferences[1]==100);
 else assert(!writes);
 if(qa_case==16)assert(brightness_calls==2&&presents==4&&!memcmp(background,pixels,sizeof(pixels)));
 else if(qa_case!=9)assert(!brightness_calls);
 if(qa_case==30||qa_case==31)assert(presents==3&&!writes&&!refreshes&&!memcmp(background,pixels,sizeof(pixels)));
 if(qa_case==18)assert(diagnostics>=1&&strstr(last_diagnostic,"sleep=unavailable")&&presents==4);
 if(qa_case==12)assert(alarm_acks==1&&!memcmp(background,pixels,sizeof(pixels)));
#endif
 printf("Paper controls %u: %u frames, %u launches, %u writes, clean lifecycle\n",qa_case,presents,launches,writes);return 0;
}
