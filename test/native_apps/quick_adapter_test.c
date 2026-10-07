/* Real adapter/controller/session integration over deterministic host providers.
 * Hardware fakes prove dispatch, storage and ownership, not physical operation. */
#define PORTABLE_QUICK_ACTIONS
#define PORTABLE_ALARM_FIXTURE_MAIN base_alarm_fixture_main
#include "portable_alarm_test.c"

static uint8_t pref_value[4];
static uint16_t expected_background[240*240];
static bool pref_present[4], pref_write_fail, pref_read_fail;
static unsigned pref_writes[4], brightness_calls, hardware_brightness=40;
static unsigned inject_alarm_at, wifi_launches;
static bool alarm_injected;
static const char *capture_directory;
static risc_display_output_api_v1 quick_display_api;
static alarm_service_v1 quick_alarm_api;
static risc_touch_api_v1 quick_touch_api;
static unsigned emitted_up_poll;
static risc_runtime_api_v1 quick_runtime;
static int pref_index(const char *key) {
 if(!strcmp(key,PQA_BRIGHTNESS_KEY))return 0;
 if(!strcmp(key,PQA_VOLUME_KEY))return 1;
 if(!strcmp(key,PQA_RESTORE_VOLUME_KEY))return 2;
 if(!strcmp(key,PQA_DND_KEY))return 3;
 return -1;
}
#ifdef PORTABLE_QUICK_RADIOS
static uint8_t radio_record[4],ble_state;static bool radio_saved;
static bool radio_bluetooth_enable(void*c,bool on){(void)c;ble_state=on?1:0;return true;}
static bool radio_bluetooth_status(void*c,uint8_t*out){(void)c;*out=ble_state;return true;}
static wifi_link_t radio_wifi_status(void*c){(void)c;return WIFI_LINK_DOWN;}
static bool radio_wifi_off(void*c){(void)c;return true;}
static const portable_bluetooth_control_v1 radio_ble={.api_version=1,.struct_size=sizeof(radio_ble),.set_enabled=radio_bluetooth_enable,.status=radio_bluetooth_status};
static const wifi_api_v1 radio_wifi={.api_version=1,.struct_size=sizeof(radio_wifi),.status=radio_wifi_status,.disconnect_checked=radio_wifi_off};
#endif
#ifdef PORTABLE_LOW_BATTERY
static uint8_t low_records[3][5];static uint32_t low_sizes[3];static unsigned low_writes[3];
static int low_write_fail=-1;
static int low_key(const char *key) {
 return !strcmp(key,PORTABLE_LOW_BATTERY_KEY)?0:!strcmp(key,PORTABLE_SLEEP_IDLE_KEY)?1:!strcmp(key,PORTABLE_SLEEP_DEEP_KEY)?2:-1;
}
#endif
static int32_t quick_get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size) {
 /* Settings row reads retain their existing read-only model contract. Quick
  * control preferences still require a settled, unleased display. */
 if(pref_index(key)>=0 || !strcmp(key,"quick_radio"))assert(!surface.frame && display_settled);
#ifdef PORTABLE_LOW_BATTERY
 int low_index=low_key(key);
 if(low_index>=0){*size=low_sizes[low_index];if(!*size)return RISC_KEY_VALUE_NOT_FOUND;if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(data,low_records[low_index],*size);return RISC_KEY_VALUE_OK;}
#endif
#ifdef PORTABLE_QUICK_RADIOS
 if(!strcmp(key,PORTABLE_RADIO_KEY)){*size=0;if(!radio_saved)return RISC_KEY_VALUE_NOT_FOUND;assert(capacity>=4);memcpy(data,radio_record,4);*size=4;return RISC_KEY_VALUE_OK;}
#endif
 int index=pref_index(key);if(index<0)return kv_get(c,key,data,capacity,size);
 *size=0;if(pref_read_fail)return RISC_KEY_VALUE_IO;
 if(!pref_present[index])return RISC_KEY_VALUE_NOT_FOUND;
 *size=1;if(capacity<1)return RISC_KEY_VALUE_BUFFER_SMALL;
 *(uint8_t*)data=pref_value[index];return RISC_KEY_VALUE_OK;
}
static int32_t quick_put(void *c,const char *key,const void *data,uint32_t size) {
 assert(!surface.frame && display_settled);
#ifdef PORTABLE_LOW_BATTERY
 int low_index=low_key(key);
 if(low_index>=0){assert(size<=5);low_writes[low_index]++;if(low_write_fail==low_index)return RISC_KEY_VALUE_IO;low_sizes[low_index]=size;memcpy(low_records[low_index],data,size);return RISC_KEY_VALUE_OK;}
#endif
#ifdef PORTABLE_QUICK_RADIOS
 if(!strcmp(key,PORTABLE_RADIO_KEY)){assert(size==4);memcpy(radio_record,data,4);radio_saved=true;return RISC_KEY_VALUE_OK;}
#endif
 int index=pref_index(key);if(index<0)return kv_put(c,key,data,size);
 assert(size==1);pref_writes[index]++;
 if(pref_write_fail)return RISC_KEY_VALUE_IO;
 pref_value[index]=*(const uint8_t*)data;pref_present[index]=true;return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 quick_kv_api={1,sizeof(quick_kv_api),NULL,quick_get,quick_put};
static bool quick_brightness(void *c,uint16_t level,uint16_t maximum) {
 (void)c;assert(!surface.frame && display_settled);assert(maximum==100 && level<=100);
 hardware_brightness=level;brightness_calls++;return true;
}
static int32_t quick_step(void *c) {
 if(inject_alarm_at && polls>=inject_alarm_at && !alarm_injected) {
  alarm_injected=true;alarm_fake.state=ALARM_STATE_ALERT;
  alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};
 }
 return service_step(c);
}
static bool quick_health(risc_runtime_health_v1 *out) {
 out->uptime_ms=ticks;
 if(polls>=700){fprintf(stderr,"fixture exhausted polls=%u position=%d target=%d gesture=%d gate=%d torch=%d\n",polls,quick.ui.position_q8,quick.ui.target_q8,quick.ui.gesture,quick.ui.neutral_gate,quick.ui.torch);abort();}
 return true;
}
static bool quick_launch(const char *path) {
 if(!strcmp(path,"wifi_settings.elf")){wifi_launches++;return true;}
 return test_launch(path);
}
static int32_t quick_next(void *c,uint64_t subscription,risc_touch_event_v1 *event) {
 if(polls==gap_at)return touch_next(c,subscription,event);
 bool current=false;const contact *last=NULL;
 for(size_t i=0;i<input_count;i++) {
  if(input_script[i].poll==polls)current=true;
  if(input_script[i].poll+1==polls)last=input_script+i;
 }
 if(!current && last && emitted_up_poll!=polls) {
  emitted_up_poll=polls;
  *event=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=last->x,.y=last->y};
#if PORTABLE_TOUCH_ROTATION == 180
  event->x=239-event->x;event->y=239-event->y;
#endif
  return 1;
 }
 return 0;
}
static bool quick_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
#ifdef PORTABLE_QUICK_RADIOS
 if(!strcmp(name,"net.wifi")){assert(version==1&&instance==15);grant->api=&radio_wifi;grants++;return true;}
 if(!strcmp(name,"bluetooth.hci")){assert(version==1&&instance==16);grant->api=&radio_ble;grants++;return true;}
#endif
 if(!strcmp(name,"input.touch.raw")){assert(version==1 && !instance);grant->api=&quick_touch_api;++grants;return true;}
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {assert(version==1 && instance==1);grant->api=&quick_kv_api;++grants;return true;}
 if(!strcmp(name,"display.output")){assert(version==1 && !instance);grant->api=&quick_display_api;++grants;return true;}
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==1 && !instance);grant->api=&quick_alarm_api;++grants;return true;}
 return test_acquire(name,version,instance,grant);
}
static void capture_frame(const char *name) {
 if(!capture_directory)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",capture_directory,name);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){
  unsigned p=framebuffer[i];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
  assert(fwrite(rgb,1,3,f)==3);
 }
 assert(!fclose(f));
}
static bool capture_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,size_t count,
                           const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
 bool result=frame_submit(c,frame,rect,count,options,token);
 if(quick.ui.position_q8==PQA_OPEN_Q8 && !quick.ui.torch)capture_frame("adapter-panel");
 if(quick.ui.torch)capture_frame("adapter-torch");
 if(alarm_modal)capture_frame("adapter-alarm");
 return result;
}
static void setup(void) {
 scenario=100;alarm_scenario=0;
 quick_display_api=display_api;quick_display_api.set_brightness=quick_brightness;quick_display_api.submit=capture_submit;
 quick_alarm_api=service_api;quick_alarm_api.step=quick_step;
 quick_touch_api=touch_api;quick_touch_api.next=quick_next;
 quick_runtime=runtime_api;quick_runtime.acquire=quick_acquire;quick_runtime.health=quick_health;quick_runtime.request_launch=quick_launch;
 rt=&quick_runtime;display_settled=true;failed=false;
 dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
 assert(rt->acquire("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));
 assert(portable_touch_open(&touch,rt));
 alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,rt));
 alarm_fake.state=ALARM_STATE_READY;
 assert(settings_open());
 pqa_session_init(&quick);assert(pqa_session_load(&quick,rt));
#ifdef PORTABLE_QUICK_RADIOS
 assert(pqa_radios_load(&quick_radios,&quick.ui,rt));assert(quick.ui.radios_valid&&quick.ui.wifi_enabled&&!quick.ui.bluetooth_enabled);
#endif
 assert(quick.volume==50 && quick.restore_volume==50 && quick.brightness==40);
 assert(!pref_writes[0] && !pref_writes[1] && !pref_writes[2]);
 settings_draft=(twatch_rtc_time_v1){2028,4,20,4,17,23,31};
 sv_page=SV_ROOT;settings_editing=false;
 clear();fill(0,0,240,240,0x24e2);present(false);assert(alarm_pixels_valid);
 memcpy(expected_background,framebuffer,sizeof(expected_background));
 capture_frame("adapter-background");
}
static void opening(unsigned at) {tap(at,100,10);tap(at+1,100,80);tap(at+2,100,230);}
static void background_unchanged(void) {
 assert(settings_draft.year==2028 && settings_draft.hour==17 && settings_draft.minute==23);
 assert(!return_launches && !wifi_launches && !writes && !format_writes);
 assert(!memcmp(framebuffer,expected_background,sizeof(expected_background)));
 assert(!pqa_visible(&quick.ui));
}
static void drain_to(unsigned target) {
 while(polls<target) {
  t5_app_input_t input={0};assert(poll(&input,8));
  assert(!input.tapped && !input.buttons && !input.exit_requested);
 }
}
static void cleanup(void) {
 assert(!failed);app_module_fini();assert(!grants && !subscriptions && !frame_count);
}
#ifndef PORTABLE_QUICK_FIXTURE_MAIN
#define PORTABLE_QUICK_FIXTURE_MAIN main
#endif
int PORTABLE_QUICK_FIXTURE_MAIN(int argc,char **argv) {
 assert(argc>=2);unsigned test=(unsigned)atoi(argv[1]);if(argc>2)capture_directory=argv[2];setup();
 if(test==0) {
  opening(3);tap(80,120,222);drain_to(160);background_unchanged();
  assert(!pref_writes[0] && !pref_writes[1]);
 } else if(test==1 || test==2) {
  if(test==1)tap(3,20,20);
  else {tap(3,40,20);tap(4,110,20);}
  t5_app_input_t input={0};
  for(unsigned n=0;n<10 && !(input.buttons&T5_APP_BUTTON_BACK);n++){assert(poll(&input,8));

  }
  assert(input.buttons&T5_APP_BUTTON_BACK);
  if(test==1)assert(input.exit_requested && return_launches==1);
  else assert(!input.exit_requested && !return_launches);
  assert(!pqa_visible(&quick.ui) && !pref_writes[0] && !pref_writes[1]);
 } else if(test==3) {
  opening(3);
  tap(80,44,90);tap(81,110,90);tap(82,182,90);
  tap(88,182,62);tap(89,44,62);tap(90,110,62);
  tap(96,44,62);tap(100,182,62);
  tap(106,51,139);tap(112,120,222);
  drain_to(180);background_unchanged();
  assert(pref_present[0] && pref_value[0]==100 && pref_writes[0]==3);
  assert(pref_present[1] && pref_value[1]==0 && pref_writes[1]==2);
  assert(pref_present[2] && pref_value[2]==100 && pref_writes[2]==1);
  assert(quick.volume==0 && hardware_brightness==100);
  pqa_session reloaded;pqa_session_init(&reloaded);assert(pqa_session_load(&reloaded,rt));
  assert(reloaded.volume==0 && reloaded.restore_volume==100);
  assert(reloaded.ui.last_nonzero_volume==100);
  opening(190);tap(268,51,139);tap(272,120,222);drain_to(350);background_unchanged();
  assert(pref_value[1]==100 && quick.volume==100);
 } else if(test==4 || test==5) {
  opening(3);tap(80,193,185);
  if(test==4)tap(100,100,100);
  else {inject_alarm_at=95;tap(110,100,165);}
  drain_to(180);background_unchanged();
  if(quick.ui.torch || hardware_brightness!=40 || brightness_calls<2)fprintf(stderr,"torch=%u hardware=%u calls=%u pending=%u position=%d gate=%u\n",quick.ui.torch,hardware_brightness,brightness_calls,quick.ui.pending,quick.ui.position_q8,quick.ui.neutral_gate);
  assert(!quick.ui.torch && hardware_brightness==40 && brightness_calls>=2);
  assert(!pref_writes[0] && !pref_writes[1]);
  if(test==5)assert(alarm_injected && acks==1 && alarm_fake.state==ALARM_STATE_READY);
 } else if(test==6) {
  pref_write_fail=true;opening(3);tap(80,182,62);tap(86,44,90);tap(92,120,222);
  drain_to(170);background_unchanged();
  assert(quick.brightness==40 && quick.volume==50 && hardware_brightness==40);
  assert(quick.ui.error_flags&PQA_ERROR_SAVE);assert(!pref_present[0] && !pref_present[1]);
 } else if(test==7) {
#ifdef PORTABLE_QUICK_RADIOS
  opening(3);tap(80,51,185);tap(86,122,185);tap(92,193,139);tap(98,120,222);drain_to(160);background_unchanged();
  assert(quick.ui.airplane&&!quick.ui.wifi_enabled&&!quick.ui.bluetooth_enabled&&!ble_state&&radio_saved&&!wifi_launches&&!return_launches);
  opening(180);tap(260,193,139);tap(266,51,185);tap(272,120,222);drain_to(330);background_unchanged();
  assert(!quick.ui.airplane&&quick.ui.wifi_enabled&&quick.ui.bluetooth_enabled&&ble_state==1);
  assert(pqa_radios_suspend(rt)&&ble_state==0);assert(pqa_radios_resume(&quick_radios,&quick.ui,rt)&&ble_state==1);
#else
  opening(3);tap(80,51,185);t5_app_input_t input={0};
  while(!input.exit_requested)assert(poll(&input,8));
  assert(wifi_launches==1 && !return_launches && quick_launch_pending);
  assert(!pref_writes[0] && !pref_writes[1]);
#endif
 } else if(test==8) {
  /* Actual provider cancellation restores preview without saving. */
  opening(3);tap(80,182,62);tap(81,182,62);fail_poll_at=82;tap(90,120,222);
  drain_to(170);background_unchanged();
  assert(quick.brightness==40 && hardware_brightness==40 && !pref_writes[0]);
 } else if(test==9 || test==10) {
  settings_render(0,0);editor_field=3;sv_switch(SV_VALUE);
  memcpy(expected_background,framebuffer,sizeof(expected_background));capture_frame("adapter-settings-value");
  assert(sv_active && settings_editing && sv_page==SV_VALUE);
  if(test==9) {
   opening(3);tap(80,120,222);drain_to(160);background_unchanged();
   assert(sv_active && settings_editing && sv_page==SV_VALUE && editor_field==3);
  } else {
   tap(3,40,20);tap(4,110,20);t5_app_input_t input={0};
   for(unsigned n=0;n<10 && !(input.buttons&T5_APP_BUTTON_BACK);n++)assert(poll(&input,8));
   assert(input.buttons&T5_APP_BUTTON_BACK);assert(!input.exit_requested && !return_launches);
   assert(sv_page==SV_VALUE && editor_field==3); /* Caller owns nested pop. */
  }
 } else if(test==11) {
  tap(3,100,160);t5_app_input_t input={0};
  for(unsigned n=0;n<10 && !input.tapped;n++)assert(poll(&input,8));
  assert(input.tapped && input.touch_x==100 && input.touch_y==160);
  assert(!input.buttons && !input.exit_requested && !pqa_visible(&quick.ui));
 } else if(test==12) {
  opening(3);tap(80,182,62);tap(81,182,62);replace_at=81;tap(90,120,222);
  drain_to(170);background_unchanged();
  assert(quick.brightness==40 && hardware_brightness==40 && !pref_writes[0]);
 } else assert(!"unknown scenario");
 cleanup();printf("quick adapter scenario %u passed\n",test);return 0;
}
