static void (*files_child)(void);
static int32_t files_home_run(uint64_t token,const char*path,risc_resident_result_v1*out){
 assert(token==9&&!path&&!owned&&!pending&&!touch.subscription);++attention_runs;effect_started=true;
 submitted=last_complete=1;unsigned keys=effect_keys,steps=attention_steps;
 files_child();
 assert(effect_keys==keys&&attention_steps==steps);
 if(!strcmp(effect_case,"brightness")||!strcmp(effect_case,"radios"))assert(!effect_refreshes&&!effect_steps);
 else assert(effect_refreshes==1&&effect_steps==(!strcmp(effect_case,"pending")?1u:0u));
 if(!strcmp(effect_case,"radios")){assert(effect_wifi_off==1&&effect_ble_set==1&&ble_state==1&&radio_saved&&radio_bytes[2]==2);}
 else assert(hardware_brightness==70&&quick_values[0]==70&&quick_saved[0]);
 ended=true;out->status=0;return 0;
}
int files_home_checkpoint(uint32_t reason){
 risc_resident_request_v1 q={sizeof(q),reason,0,0};risc_resident_reply_v1 r={sizeof(r),0};
 int rc=attention_host.dispatch(NULL,&q,&r);assert(!r.flags);return rc;
}
int32_t files_home_get(const char*k,void*b,uint32_t cap,uint32_t*n){return face_get(NULL,k,b,cap,n);}
int32_t files_home_put(const char*k,const void*b,uint32_t n){if(!strcmp(k,PORTABLE_RADIO_KEY)){assert(n==4);memcpy(radio_bytes,b,n);radio_saved=true;return 0;}int i=quick_key(k);assert(i>=0&&n==1);quick_saved[i]=true;quick_values[i]=*(const uint8_t*)b;return 0;}
unsigned files_home_brightness(void){return hardware_brightness;}
const void*files_home_alarm(void){return &att_alarm.base;}
int files_home_start(void(*child)(void),const char*name){
 files_child=child;effect_case=name;reset_case();
 effect_refresh_rc=!strcmp(name,"pending")?ALARM_PENDING:!strcmp(name,"clean-failure")?ALARM_STORAGE:ALARM_OK;effect_step_rc=ALARM_OK;
 assert(alarm_calendar_seconds(2026,10,4,0,0,40,&attention_base_seconds));alarm_view.snapshot=7;
 att_alarm=alarm_api;att_alarm.base.status=att_status;att_alarm.base.step=effect_step;att_alarm.base.refresh=effect_refresh;att_alarm.base.prepare_sleep=att_deadline;
 pa.key_events=effect_key;ra.read=att_rtc;fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.resident_shell=effect_get;fast_runtime.health=att_health;fast_runtime.diagnostic=att_diagnostic;fast_runtime.acquire=effect_acquire;
 app_main();assert(attention_runs==1&&!clock_effects.result&&!clock_effects.id&&!retains&&!owned&&!pending&&!held&&!touch.subscription&&grants==ungrants);
 return 0;
}
