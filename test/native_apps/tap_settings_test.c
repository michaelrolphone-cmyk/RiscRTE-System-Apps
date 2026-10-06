/* Real Settings controller/view, modeled capability status and fresh samples.
 * No synthetic observation in this fixture enters target inputs. */
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_TAP_SETTINGS
#define PORTABLE_SLEEP_SETTINGS
#define main old_alert_main
#include "portable_alarm_settings_test.c"
#undef main
static unsigned sleep_calls;
static unsigned test_case,profile,expected_value,motion_grants,motion_releases,begins,ends,observations,tap_writes;
static bool sensor_live,fail_restore,record_exists,write_error,verify_error,io_error;
static uint8_t tap_record[8];
int portable_app_sleep(const risc_runtime_api_v1 *runtime,const risc_display_output_api_v1 *display_api,const risc_battery_gauge_api_v1 *gauge_api){
 (void)runtime;(void)display_api;(void)gauge_api;assert(!sensor_live);sleep_calls++;return 0;
}
static bool health_failed;
static unsigned restore_failures;
static unsigned gesture_page,gesture_phase,gesture_at,gesture_step;
static int32_t tap_get(void*c,const char*key,void*data,uint32_t cap,uint32_t*size){
 if(strcmp(key,PORTABLE_TAP_KEY))return kv_get(c,key,data,cap,size);
 assert(cap==8);*size=0;if(verify_error && tap_writes)return RISC_KEY_VALUE_IO;
 if(!record_exists)return RISC_KEY_VALUE_NOT_FOUND;
 *size=8;memcpy(data,tap_record,8);return RISC_KEY_VALUE_OK;
}
static int32_t tap_put(void*c,const char*key,const void*data,uint32_t size){
 (void)c;assert(!strcmp(key,PORTABLE_TAP_KEY)&&size==8);tap_writes++;
 if(write_error)return RISC_KEY_VALUE_IO;
 memcpy(tap_record,data,8);record_exists=true;return RISC_KEY_VALUE_OK;
}
static bool test_health(risc_runtime_health_v1*out){
 out->uptime_ms=ticks;
 if(test_case==19 && observations>20 && !health_failed){health_failed=true;restore_failures=3;}
 return polls<50000 && !(test_case==16 && observations>20) && !health_failed;
}
static bool tap_info_read(void*c,twatch_tap_info_v1*out){(void)c;assert(out->struct_size==sizeof(*out));*out=(twatch_tap_info_v1){sizeof(*out),profile,0,profile==1?7:15,profile==1?3:12};return true;}
static bool configure(void*c,uint16_t value){(void)c;(void)value;return !sensor_live;}
static bool observe_begin(void*c,uint16_t value){(void)c;assert(!sensor_live&&value==tap_calibration.value);sensor_live=true;begins++;return test_case!=9;}
static bool observe_end(void*c){(void)c;ends++;if(restore_failures){restore_failures--;return false;}if(fail_restore)return false;sensor_live=false;return true;}
static bool observe(void*c,twatch_tap_observation_v1*out){
 (void)c;assert(sensor_live&&out->struct_size==sizeof(*out));observations++;
 if(test_case==8 && observations>5)return false;
 unsigned phase=tap_calibration.phase,elapsed=ticks-tap_calibration.phase_at;
 *out=(twatch_tap_observation_v1){.struct_size=sizeof(*out),.sample_ready=test_case!=7,.z_mg=1000};
 if(phase==TAP_CAL_MOVE && test_case!=6)out->x_mg=(ticks%100<50)?0:400;
 if(phase==TAP_CAL_PAIR && elapsed>=250 && tap_calibration.value<=expected_value && test_case!=18)out->double_tap=true;
 if(test_case==4 && phase==TAP_CAL_QUIET && elapsed>=200)out->double_tap=true;
 if(test_case==5 && phase==TAP_CAL_MOVE && elapsed>=500)out->double_tap=true;
 if(test_case==14 && observations==20)ticks+=600;
 return !io_error;
}
static twatch_motion_api_v1 motion_api={.api_version=1,.struct_size=sizeof(motion_api),.resume_wake=observe_end,
 .tap_info=tap_info_read,.tap_configure=configure,.tap_observe_begin=observe_begin,.tap_observe=observe};
static bool test_acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 if(strcmp(name,TWATCH_MOTION_CAPABILITY))return acquire(name,version,id,g);
 assert(version==1&&!id);g->api=&motion_api;motion_grants++;return true;
}
static bool test_release(risc_runtime_capability_v1*g){
 if(g->api!=&motion_api)return release(g);
 assert(!sensor_live);motion_releases++;g->api=NULL;return true;
}
static bool test_snapshot(void*c,risc_touch_snapshot_v1*out){
 (void)c;memset(out,0,sizeof(*out));out->width=out->height=240;
 if(gesture_page!=sv_page || gesture_phase!=tap_calibration.phase){gesture_page=sv_page;gesture_phase=tap_calibration.phase;gesture_at=polls;gesture_step=0;}
 unsigned elapsed=polls-gesture_at,x=0,y=0;bool down=false;
 if(sv_page==SV_TAP){
  if(test_case==0 || test_case==1 || test_case==11 || test_case==12 || test_case==17){
   if(elapsed==3){x=50;y=80;down=true;}
   if(elapsed>=7 && elapsed<=(test_case==17?10:7)){x=test_case==1?60:170;y=210;down=true;}
   if(elapsed==14){x=60;y=210;down=true;}
  }else if(gesture_step==0 && elapsed==3 && !motion_grants){x=120;y=135;down=true;gesture_step++;}
  else if(elapsed==5 && tap_cleanup_pending){x=120;y=135;down=true;fail_restore=false;}
  else if(elapsed==12 && motion_grants){x=60;y=210;down=true;}
 }else if(sv_page==SV_TAP_INTRO && elapsed==3){x=170;y=210;down=true;}
 else if(sv_page==SV_TAP_CAL){
  if(test_case==3 && tap_calibration.phase==TAP_CAL_PAIR && elapsed==3){x=120;y=210;down=true;}
  if(tap_calibration.phase==TAP_CAL_READY && elapsed==3){x=170;y=210;down=true;}
 }
 if(down){
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};
#if PORTABLE_TOUCH_ROTATION == 180
  out->contacts[0].x=239-x;out->contacts[0].y=239-y;
#endif
 }
 return true;
}
static bool evidence_submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*rect,size_t count,
 const risc_display_present_options_v1*options,risc_display_present_token_v1*token){
 bool ok=frame_submit(c,frame,rect,count,options,token);const char*dir=getenv("TAP_FRAME_DIR");
 if(dir && (sv_page==SV_TAP || sv_page==SV_TAP_INTRO || sv_page==SV_TAP_CAL)){
  char path[512];snprintf(path,sizeof(path),"%s/case-%u-profile-%u-page-%u-phase-%u.ppm",dir,test_case,profile,sv_page,tap_calibration.phase);
  FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
  for(unsigned i=0;i<240*240;i++){unsigned p=framebuffer[i];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));
 }
 return ok;
}
int main(int argc,char**argv){
 assert(argc==3);test_case=(unsigned)atoi(argv[1]);profile=(unsigned)atoi(argv[2]);assert(test_case<=19 && (profile==1 || profile==2));
 expected_value=profile==1?5:13;scenario=0;kv_api.get=tap_get;kv_api.put=tap_put;
 assert(app_module_init()==0);
 risc_runtime_api_v1 test_runtime=runtime_api;test_runtime.health=test_health;test_runtime.acquire=test_acquire;test_runtime.release=test_release;rt=&test_runtime;
 risc_touch_api_v1 test_touch=touch_api;test_touch.snapshot=test_snapshot;touch.api=&test_touch;
 risc_display_output_api_v1 test_display=display_api;test_display.submit=evidence_submit;display=&test_display;
 if(test_case==13)motion_api.struct_size=TWATCH_MOTION_DIAGNOSTIC_SIZE;
 if(test_case==10)fail_restore=true;
 if(test_case==11)write_error=true;
 if(test_case==12)verify_error=true;
 if(test_case==15){portable_tap_settings p=portable_tap_defaults();p.enabled=0;assert(portable_tap_save(&kv_api,&p));tap_writes=0;}
 gesture_page=gesture_phase=999;settings_render(0,SETTINGS_TAP_ROW+1);
 uint8_t result=settings_activate(0,SETTINGS_TAP_ROW);
 assert((sv_page==SV_ROOT || ((test_case==16 || test_case==19) && failed)) && !settings_motion_active && !sensor_live && !tap_cleanup_pending);
 bool saved=test_case==0 || test_case==2 || test_case==15 || test_case==17;
 assert(result==(saved?T5_APP_SETTING_UPDATED:T5_APP_SETTING_NO_CHANGE));
 assert(tap_writes==((saved || test_case==11 || test_case==12)?1u:0u));
 assert(motion_grants==motion_releases);
 if(saved){portable_tap_settings p;assert(portable_tap_load(&kv_api,&p)==PORTABLE_TAP_LOADED);
  if(test_case==0 || test_case==17)assert(!p.enabled && !motion_grants && p.bma423==3 && p.bma456h==12);
  else {assert((profile==1?p.bma423:p.bma456h)==expected_value && p.measured==(1u<<(profile-1)));assert(p.enabled==(test_case!=15));assert(begins==3);}
 }
 if(test_case==1)assert(!record_exists && !motion_grants);
 assert(!rtc_writes && !kv_writes && !sleep_calls);app_module_fini();assert(!grants && !subscriptions);
 printf("Tap Settings production UI case %u, profile %u PASS\n",test_case,profile);
}
