#define PORTABLE_QUICK_FIXTURE_MAIN unused_quick_main
#include "quick_adapter_test.c"
static bool monitoring,pause_refused;
static unsigned context_steps,context_pauses;
static contexts_status_v1 context_status={.struct_size=sizeof(context_status)};
static bool ctx_pause(void*c){(void)c;assert(!surface.frame&&display_settled);context_pauses++;if(pause_refused)return false;monitoring=false;return true;}
static bool ctx_step(void*c,const contexts_policy_v1*p){(void)c;assert(!surface.frame&&display_settled);context_steps++;monitoring=p->enabled&&p->audio_allowed;return true;}
static bool ctx_status(void*c,contexts_status_v1*out){(void)c;*out=context_status;return true;}
static bool ctx_mask(void*c,uint32_t mask){(void)c;(void)mask;return true;}
static bool ctx_export(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;return true;}
static bool ctx_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static int32_t ctx_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*out){(void)c;(void)a;(void)b;(void)out;return 0;}
static bool ctx_claim(void*c,uint32_t a,uint32_t b,const char*n,uint32_t d){(void)c;(void)a;(void)b;(void)n;(void)d;return false;}
static bool ctx_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;return true;}
static const contexts_service_v1 ctx_api={1,sizeof(ctx_api),NULL,ctx_step,ctx_pause,ctx_status,ctx_mask,ctx_mask,ctx_export,ctx_finish,ctx_label,ctx_claim,ctx_result};
static int32_t ctx_get(void*c,const char*key,void*out,uint32_t capacity,uint32_t*size) {
 if(!strcmp(key,PORTABLE_CONTEXT_ENABLED_KEY)){const uint8_t bytes[]={'C',1,1,0xa4};assert(capacity>=4);memcpy(out,bytes,4);*size=4;return 0;}
 if(!strncmp(key,"ctx_p",5)){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
 return quick_get(c,key,out,capacity,size);
}
static const risc_key_value_v1 ctx_kv={1,sizeof(ctx_kv),NULL,ctx_get,quick_put};
static bool ctx_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g) {
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(version==1&&instance==1);g->api=&ctx_kv;grants++;return true;}
 return quick_acquire(n,version,instance,g);
}
static int32_t ctx_alarm_step(void*c){
 if(alarm_fake.state==ALARM_STATE_ALERT||alarm_fake.state==ALARM_STATE_CUE)assert(!monitoring);
 return quick_step(c);
}
static risc_battery_sample_v1 ctx_sample={3900,50,0};
static bool ctx_battery(void*c,risc_battery_sample_v1*out){(void)c;*out=ctx_sample;return true;}
static const risc_battery_gauge_api_v1 ctx_gauge={1,sizeof(ctx_gauge),NULL,ctx_battery};
int main(int argc,char**argv) {
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);
 contexts_client.api=&ctx_api;contexts_client.runtime=&quick_runtime;setup();
 quick_runtime.acquire=ctx_acquire;quick_alarm_api.step=ctx_alarm_step;
 contexts_client.grant=(risc_runtime_capability_v1){.struct_size=sizeof(contexts_client.grant),.api=&ctx_api};grants++;
 assert(contexts_tick()&&monitoring);context_steps=context_pauses=0;
 if(test==0){t5_app_input_t input;assert(poll(&input,8)&&monitoring&&context_steps&&!context_pauses);}
 else if(test==1){opening(3);tap(80,120,222);drain_to(160);assert(context_pauses&&context_steps&&monitoring);background_unchanged();}
 else if(test==2){gauge=&ctx_gauge;ctx_sample.percent=9;assert(low_battery_poll()&&!monitoring);assert(contexts_tick()&&!monitoring);gauge=NULL;}
 else if(test==3){assert(!idle_sleep()&&!monitoring&&native_sleep_calls==1&&native_sleep_retained);puts("Contexts paused before retained native sleep PASS");return 0;}
 else if(test==4){pause_refused=true;unsigned before=ble_state;assert(!quick_apply(PQA_BLUETOOTH)&&monitoring&&ble_state==before);pause_refused=false;assert(quick_apply(PQA_BLUETOOTH)&&!monitoring);}
 else if(test==5){alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};tap(10,100,165);bool consumed=false;assert(alarm_foreground(&consumed)&&consumed&&!monitoring&&context_pauses);}
 else if(test==6){cleanup();assert(!monitoring&&context_pauses&&!grants&&!subscriptions&&!frame_count);puts("Contexts ordinary app exit stops its capture and releases grant PASS");return 0;}
 else if(test==7){alarm_fake.state=ALARM_STATE_CUE;alarm_fake.output_uncertain=1;bool consumed=false;assert(alarm_foreground(&consumed)&&consumed&&!monitoring&&context_pauses);}
 else if(test==8){last_activity=0;ticks=60001;t5_app_input_t input;assert(!poll(&input,8)&&native_sleep_calls==1&&native_sleep_retained&&!monitoring);puts("Background Contexts does not inhibit ordinary idle sleep PASS");return 0;}
 else assert(!"unknown scenario");
 cleanup();printf("Contexts actual adapter lifecycle case %u PASS\n",test);return 0;
}
