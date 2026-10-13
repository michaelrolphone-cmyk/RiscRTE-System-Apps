/* Actual production initialize/poll/fini with deterministic capability faults.
 * Providers are fixtures, not device-service replacements. */
#define PORTABLE_SETTINGS_RUNTIME_OVERRIDE
#define PORTABLE_QUICK_FIXTURE_MAIN unused_quick_fixture_main
#include "quick_adapter_test.c"
static unsigned context_steps,context_pauses;
static bool pause_refuse;
static bool ctx_pause(void*c){(void)c;context_pauses++;return !pause_refuse;}
static bool ctx_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;context_steps++;return true;}
static bool ctx_status(void*c,contexts_status_v1*out){(void)c;*out=(contexts_status_v1){.struct_size=sizeof(*out)};return true;}
static bool ctx_mask(void*c,uint32_t mask){(void)c;(void)mask;return true;}
static bool ctx_export(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;return true;}
static bool ctx_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static int32_t ctx_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*out){(void)c;(void)a;(void)b;(void)out;return 0;}
static bool ctx_claim(void*c,uint32_t a,uint32_t b,const char*n,uint32_t d){(void)c;(void)a;(void)b;(void)n;(void)d;return false;}
static bool ctx_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;return true;}
static bool ctx_capture(void*c){(void)c;return true;}
static const contexts_service_v1 ctx_api={.api_version=1,.struct_size=sizeof(ctx_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_mask,.begin_export=ctx_mask,.export_record=ctx_export,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_capture};

static unsigned missing_mask,target_service,fault;
static bool fenced,health_dead,hold_present;
static unsigned display_status_reads;
static unsigned acquires,releases,health_reads,fences,broadcast_steps,broadcast_pauses;
static unsigned unavailable_notices;
static contexts_service_v1 startup_context;
static telemetry_broadcast_v1 startup_broadcast;
static alarm_service_v1 startup_alarm;
static contexts_fingerprint_service_v1 startup_fp;
static unsigned file_calls,fp_calls;
static unsigned fp_dirty;
static bool custody_failed;
static risc_runtime_api_v1 startup_runtime;
static bool startup_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){
 (void)c;(void)allow;(void)p;assert(!fenced);broadcast_steps++;return true;
}
static bool startup_broadcast_pause(void*c){(void)c;assert(!fenced);broadcast_pauses++;return true;}
static bool startup_broadcast_status(void*c,telemetry_broadcast_status_v1*out){
 (void)c;assert(!fenced);*out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out)};return true;
}
static int32_t startup_broadcast_enum(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;(void)i;(void)out;assert(!fenced);return 0;}
static int32_t startup_broadcast_read(void*c,uint32_t i,int32_t*out){(void)c;(void)i;(void)out;assert(!fenced);return 0;}
static unsigned service_number(const char*n){
 return !strcmp(n,CONTEXTS_SERVICE_CAPABILITY)?1:!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)?2:!strcmp(n,ALARM_SERVICE_CAPABILITY)?3:0;
}
static int32_t startup_get(void*c,const char*key,void*out,uint32_t cap,uint32_t*size){
 assert(!custody_failed&&!fenced);
 if(c&&fault==25){*size=0;custody_failed=true;return RISC_KEY_VALUE_CONTEXT;}
 if(!strcmp(key,PORTABLE_CONTEXT_ENABLED_KEY)){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
 return quick_get(c,key,out,cap,size);
}
static int32_t startup_put(void*c,const char*key,const void*bytes,uint32_t size){
 assert(!custody_failed&&!fenced);
 if(c&&fault==26){custody_failed=true;return RISC_KEY_VALUE_CONTEXT;}
 return quick_put(NULL,key,bytes,size);
}
static const risc_key_value_v1 startup_kv={1,sizeof(startup_kv),NULL,startup_get,startup_put};
static const risc_key_value_v1 startup_fault_kv={1,sizeof(startup_fault_kv),(void*)1,startup_get,startup_put};
static bool startup_fingerprint(void*c,uint32_t op,void*request){
 (void)c;assert(!fenced&&!custody_failed);fp_calls++;
 if(op==CONTEXTS_FP_CONFIG)return true;
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=request;s->sources=(fault==14||fault==15||fault==16||fault==19||fault==21)?0:CONTEXTS_AUDIO;s->dirty_sources=fp_dirty;return true;}
 assert(op==CONTEXTS_FP_RULES);return true;
}
static int32_t startup_file_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){
 (void)c;(void)n;assert(!fenced&&!custody_failed);file_calls++;*size=0;*revision=0;
 if(fault==11||fault==14){custody_failed=true;return RISC_APP_DATA_RETAINED;}
 if(fault==12||fault==15){custody_failed=true;return RISC_APP_DATA_CONTEXT;}
 return RISC_APP_DATA_NOT_FOUND;
}
static int32_t startup_file_read(void*c,const char*n,uint64_t revision,void*b,uint32_t cap,uint32_t*size,uint64_t*actual){
 (void)c;(void)n;(void)revision;(void)b;(void)cap;(void)size;(void)actual;assert(!"unexpected file read");return RISC_APP_DATA_INVALID;
}
static int32_t startup_file_replace(void*c,const char*n,uint64_t revision,const void*b,uint32_t size){
 (void)c;(void)n;(void)revision;(void)b;(void)size;assert(!"unexpected file write");return RISC_APP_DATA_INVALID;
}
static const risc_app_data_v1 startup_files={1,sizeof(startup_files),NULL,startup_file_stat,startup_file_read,startup_file_replace};
static bool startup_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 assert(!fenced);acquires++;unsigned service=service_number(n);
 if(service){
  assert(version==1&&!instance&&g->struct_size==sizeof(*g)&&!portable_optional_grant_owned(g));
  if(service==target_service&&fault==5){health_dead=true;return false;}
  if(missing_mask&(1u<<(service-1)))return false;
  const void *api=service==1?((fault>=11&&fault<=21)?(const void*)&startup_fp:(const void*)&startup_context):service==2?(const void*)&startup_broadcast:(const void*)&startup_alarm;
  if(service==target_service&&(fault==4||fault==6)){g->slot=service+100;g->generation=23;grants++;return fault==6;}
  g->api=api;g->slot=service+100;g->generation=23;grants++;
  return !(service==target_service&&fault==3);
 }
 if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){
  assert(version==1&&(instance==2||instance==4));
  if(fault==20||fault==21){health_dead=true;return false;}
  g->api=&startup_files;g->slot=200;g->generation=1;grants++;
  if(fault==18||fault==19){custody_failed=true;return false;}
  return true;
 }
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){
  assert(version==1&&instance==1);
  bool selected=(target_service==1&&g==&contexts_client.storage)||(target_service==2&&g==&broadcast_client.storage);
  if(selected&&fault==23){health_dead=true;return false;}
  g->api=selected&&(fault==25||fault==26)?&startup_fault_kv:&startup_kv;grants++;
  if(selected&&fault==27)g->slot=201;
  if(selected&&(fault==22||fault==24)){g->slot=201;if(fault==22){custody_failed=true;return false;}}
  return true;
 }
 return quick_acquire(n,version,instance,g);
}
static bool startup_release(risc_runtime_capability_v1*g){
 assert(!fenced&&!custody_failed&&grants&&portable_optional_grant_owned(g));releases++;
 if(fault==27&&g->slot==201){custody_failed=true;memset(g,0,sizeof(*g));return false;}
 if(fault==24&&g->slot==201){custody_failed=true;return false;}
 if((fault==13||fault==16)&&g->slot==200){custody_failed=true;return false;}
 if(fault==7&&g->slot==target_service+100)return false;
 grants--;memset(g,0,sizeof(*g));return true;
}
static bool startup_health(risc_runtime_health_v1*h){assert(!fenced);health_reads++;h->uptime_ms=ticks;return !health_dead;}
static bool startup_diagnostic(const char*text){assert(!fenced);if(strstr(text,"unavailable"))unavailable_notices++;return test_diagnostic(text);}
static bool startup_retain(void){assert(!fenced);fenced=true;fences++;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&startup_runtime:NULL;}
static bool startup_present_status(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 assert(!fenced);display_status_reads++;
 if(hold_present){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 return frame_status(c,token,out);
}
static bool startup_display_info(void*c,risc_display_info_v1*out){
 bool ok=display_info(c,out);
#ifdef PORTABLE_ASYNC_RGB565
 out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
#endif
 return ok;
}
static void configure(void){
 scenario=100;alarm_fake.state=ALARM_STATE_READY;
 quick_display_api=display_api;quick_display_api.get_info=startup_display_info;quick_display_api.present_status=startup_present_status;quick_display_api.set_brightness=quick_brightness;
 quick_touch_api=touch_api;quick_touch_api.next=quick_next;
 quick_alarm_api=service_api;quick_alarm_api.step=quick_step;
 startup_alarm=quick_alarm_api;startup_context=ctx_api;
 startup_fp=(contexts_fingerprint_service_v1){.base=ctx_api,.fingerprint_abi=0x31504643u,.fingerprint=startup_fingerprint};startup_fp.base.struct_size=sizeof(startup_fp);
 startup_broadcast=(telemetry_broadcast_v1){1,sizeof(startup_broadcast),NULL,startup_broadcast_step,startup_broadcast_pause,startup_broadcast_status,startup_broadcast_enum,startup_broadcast_read};
 startup_runtime=runtime_api;startup_runtime.acquire=startup_acquire;startup_runtime.release=startup_release;
 startup_runtime.health=startup_health;startup_runtime.diagnostic=startup_diagnostic;startup_runtime.retain_invocation=startup_retain;
 if(fault==2){if(target_service==1)startup_context.api_version=9;else if(target_service==2)startup_broadcast.api_version=9;else startup_alarm.api_version=9;}
 if(fault==9){if(target_service==1)startup_context.struct_size=8;else if(target_service==2)startup_broadcast.struct_size=8;else startup_alarm.struct_size=8;}
 if(fault==10){if(target_service==1)startup_context.capture_audio=NULL;else if(target_service==2)startup_broadcast.read=NULL;else startup_alarm.stop_only=NULL;}
}
static void check_fenced(void){
 assert(fenced&&fences==1);
 unsigned a=acquires,r=releases,h=health_reads,d=diagnostics,c=context_pauses,s=service_steps,b=broadcast_steps,fp=fp_calls,files=file_calls,display_reads=display_status_reads;
 app_module_fini();assert(app_module_init()==-1);
 t5_app_input_t input;assert(!poll(&input,8));assert(!portable_contexts_service()&&!portable_contexts_models_save()&&!portable_contexts_enable(true));
 cr_store store;cr_init(&store);assert(!portable_contexts_rules_read(&store)&&!portable_contexts_rules_save(&store));
 telemetry_broadcast_status_v1 status={.struct_size=sizeof(status)};assert(!portable_broadcast_status(&status)&&!portable_broadcast_enable(true));
 clear();present(false);input_service();
 assert(acquires==a&&releases==r&&health_reads==h&&diagnostics==d&&context_pauses==c&&service_steps==s&&broadcast_steps==b&&fences==1&&fp_calls==fp&&file_calls==files&&display_status_reads==display_reads);
}
#ifndef OPTIONAL_SERVICE_FIXTURE_MAIN
#define OPTIONAL_SERVICE_FIXTURE_MAIN main
#endif
int OPTIONAL_SERVICE_FIXTURE_MAIN(int argc,char**argv){
 assert(argc==4);missing_mask=(unsigned)atoi(argv[1]);target_service=(unsigned)atoi(argv[2]);fault=(unsigned)atoi(argv[3]);configure();
 unsigned original_fault=fault;int result=app_module_init();
#ifdef PORTABLE_ASYNC_RGB565
 if(original_fault>=30){assert(result==0);hold_present=true;clear();present(false);assert(canvas_token);if(original_fault>=34&&original_fault<=36)hold_present=false;}
#endif
 if(original_fault>=50){
  assert(result==0&&!fenced);contexts_client.api=&startup_fp.base;fp_dirty=CONTEXTS_AUDIO;fault=original_fault%2?12:11;
  if(original_fault<52){alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};bool consumed=false;assert(!alarm_foreground(&consumed));}
  else assert(!alarm_failure());
  check_fenced();
 }else if(original_fault>=40){
  assert(result==0&&!fenced);fault=original_fault==40?25:26;
  if(original_fault==40){ticks+=1001;if(target_service==1){contexts_client.loaded=false;assert(!contexts_tick());}else assert(!broadcast_tick());}
  else if(target_service==1)assert(!portable_contexts_enable(true));else assert(!portable_broadcast_enable(true));
  check_fenced();
 }else if(original_fault>=30){
  assert(result==0&&!fenced);contexts_client.api=&startup_fp.base;
  if(original_fault==36){pause_refuse=true;app_module_fini();}
  else if(original_fault==37){fault=20;fp_dirty=CONTEXTS_AUDIO;ticks=60001;assert(!contexts_tick());}
  else if(original_fault==38){fault=21;cr_store store;cr_init(&store);store.generation=1;contexts_client.rules_available=true;assert(!portable_contexts_rules_save(&store));}
  else {
   fault=(original_fault==30||original_fault==32||original_fault==34)?11:12;
   fp_dirty=CONTEXTS_AUDIO;
   if(original_fault==30||original_fault==31){ticks=60001;assert(!contexts_tick());}
   else if(original_fault==32||original_fault==33){cr_store store;cr_init(&store);store.generation=1;contexts_client.rules_available=true;assert(!portable_contexts_rules_save(&store));}
   else app_module_fini();
  }
  assert(custody_failed||health_dead||pause_refuse);
  check_fenced();
 }else if(fault==3||fault==4||fault==5||fault>=11){assert(result==-1);check_fenced();}
 else if(fault==2||fault==6||fault==9||fault==10){
  assert(result==-1&&!fenced&&!grants&&!subscriptions&&!frame_count&&!service_steps&&!broadcast_steps);
  app_module_fini();assert(!grants);fault=0;configure();assert(app_module_init()==0);app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
 }else{
  assert(result==0&&!fenced);
  for(unsigned entry=0;entry<3;entry++){
   assert((contexts_client.unavailable?1u:0u)==(missing_mask&1u));
   assert((broadcast_client.unavailable?2u:0u)==(missing_mask&2u));
   assert((alarms.unavailable?4u:0u)==(missing_mask&4u));
   clear();fill(0,0,240,240,0x1234);present(false);assert(!failed&&displays);
   t5_app_input_t input;assert(poll(&input,8)&&!failed);
   opening(polls+3);tap(polls+80,120,222);drain_to(polls+160);assert(!pqa_visible(&quick.ui)&&!failed);
   if(missing_mask&1u){assert(!quick.ui.contexts_valid&&!quick.ui.contexts_enabled);quick.ui.action_contexts=true;assert(quick_apply(PQA_CONTEXTS));assert((quick.ui.error_flags&PQA_ERROR_CONTEXTS)&&!quick.ui.contexts_valid);assert(quick_load()&&!quick.ui.contexts_valid);assert(!portable_contexts_service()&&!portable_contexts_enable(true)&&!portable_contexts_training(true)&&!portable_contexts_models_save());cr_store store;assert(!portable_contexts_rules_read(&store)&&!portable_contexts_rules_save(&store));assert(!context_steps);}
   if(missing_mask&2u){telemetry_broadcast_status_v1 status={.struct_size=sizeof(status)};assert(!portable_broadcast_status(&status)&&!portable_broadcast_enable(true)&&!broadcast_steps);}
   if(missing_mask&4u)assert(!alarm_sleep_api()&&!service_steps);
   assert(portable_background_stop());
   app_module_fini();
   if(fault==7){check_fenced();break;}
   assert(!grants&&!subscriptions&&!frame_count);app_module_fini();
   if(entry<2)assert(app_module_init()==0);
  }
  if(!fault){unsigned missing=(!!(missing_mask&1u))+(!!(missing_mask&2u))+(!!(missing_mask&4u));assert(unavailable_notices==missing*3);}
 }
 printf("Actual optional-service adapter missing=%u target=%u fault=%u PASS\n",missing_mask,target_service,original_fault);return 0;
}
