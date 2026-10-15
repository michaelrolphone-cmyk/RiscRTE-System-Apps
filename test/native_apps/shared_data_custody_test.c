/* Actual production adapter, strict providers, no native/hardware qualification.
 * The runner includes unchanged source through a renamed existing test fixture. */
#include "context-custody-fixture.inc"
static unsigned storage_fault,storage_attempts,health_calls,storage_reads,storage_namespaces;
static bool deny_health,allow_storage,deny_release,change_identity;
static unsigned callback_injection;
static risc_runtime_api_v1 replacement_runtime;
static contexts_fingerprint_service_v1 audit_context;
#ifdef AUDIT_RGB_WIDTH
static uint8_t rgb_pixels[AUDIT_RGB_WIDTH*AUDIT_RGB_HEIGHT*2];
static uint8_t frozen_rgb[sizeof(rgb_pixels)];
static bool audit_info(void*c,risc_display_info_v1*out){
 (void)c;io();*out=(risc_display_info_v1){.width=AUDIT_RGB_WIDTH,.height=AUDIT_RGB_HEIGHT,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT};return true;
}
static bool audit_frame(void*c,uint32_t format,risc_display_surface_v1*out){
 (void)c;io();assert(!frames&&format==RISC_DISPLAY_FORMAT_RGB565);frames=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=rgb_pixels,.width=AUDIT_RGB_WIDTH,.height=AUDIT_RGB_HEIGHT,
  .stride_bytes=AUDIT_RGB_WIDTH*2,.size_bytes=sizeof(rgb_pixels),.pixel_format=format};return true;
}
#endif
static bool audit_retain(void){
#ifdef AUDIT_RGB_WIDTH
 memcpy(frozen_rgb,rgb_pixels,sizeof(rgb_pixels));
#endif
 return fx_retain();
}
static void audit_verify_retained(void){
 verify_retained();assert(calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents);
 assert(!memcmp(retained_pixels,pixels,sizeof(pixels)));
#ifdef AUDIT_RGB_WIDTH
 assert(!memcmp(frozen_rgb,rgb_pixels,sizeof(rgb_pixels)));
#endif
}
static bool audit_capture(void*c){
 if(callback_injection==1){io();portable_adapter_retain();return true;}
 return ctx_capture(c);
}
static bool audit_step(void*c,const contexts_policy_v1*p){
 if(callback_injection==2){io();portable_adapter_retain();return true;}
 return ctx_step(c,p);
}
static bool audit_fingerprint(void*c,uint32_t op,void*request){
 (void)c;io();if(op==CONTEXTS_FP_CONFIG){if(callback_injection==3)portable_adapter_retain();return true;}
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=request;s->sources=CONTEXTS_ALL;s->dirty_sources=0;}
 return true;
}
static bool audit_rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;(void)t;io();return false;}
static const twatch_rtc_api_v1 audit_rtc={.api_version=2,.struct_size=sizeof(audit_rtc),.read=audit_rtc_read};
static int32_t audit_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){
 (void)c;(void)n;io();++storage_reads;*size=0;*revision=0;return RISC_APP_DATA_NOT_FOUND;
}
static int32_t audit_read(void*c,const char*n,uint64_t rev,void*p,uint32_t cap,uint32_t*size,uint64_t*actual){
 (void)c;(void)n;(void)rev;(void)p;(void)cap;(void)size;(void)actual;io();assert(false);return 0;
}
static int32_t audit_replace(void*c,const char*n,uint64_t rev,const void*p,uint32_t size){
 (void)c;(void)n;(void)rev;(void)p;(void)size;io();assert(false);return 0;
}
static const risc_app_data_v1 audit_data={1,sizeof(audit_data),NULL,audit_stat,audit_read,audit_replace};
static bool audit_health(risc_runtime_health_v1*out){++health_calls;if(deny_health){io();return false;}return fx_health(out);}
static bool audit_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){
  io();++storage_attempts;if(instance<32)storage_namespaces|=1u<<instance;
  if(change_identity){replacement_runtime=fx_runtime;native_custody_runtime=&replacement_runtime;}
  if(storage_fault==1)g->api=&audit_data;
  if(storage_fault==2)g->slot=77;
  if(storage_fault==3)g->generation=42;
  if(storage_fault==4)g->struct_size=4;
  if(storage_fault==5)g->struct_size=sizeof(*g)+4;
  if(!allow_storage)return false;
  ++acquires;++live;*g=(risc_runtime_capability_v1){sizeof(*g),acquires,1,&audit_data};return true;
 }
 if(!strcmp(n,RISC_REALTIME_CONTROL_CAPABILITY)){io();return false;}
 if(!strcmp(n,"rtc.clock")){io();++acquires;++live;*g=(risc_runtime_capability_v1){sizeof(*g),acquires,1,&audit_rtc};return true;}
 bool ok=ctx_acquire(n,v,instance,g);if(ok&&g->api==&ctx_api)g->api=&audit_context;return ok;
}
static bool audit_release(risc_runtime_capability_v1*g){
 if(deny_release&&g->api==&audit_data){io();return false;}
 return ctx_release(g);
}
int main(int argc,char**argv){
 assert(argc==2);const char*which=argv[1];
 audit_context=(contexts_fingerprint_service_v1){.base=ctx_api,.fingerprint_abi=0x31504643u,.fingerprint=audit_fingerprint};
 audit_context.base.struct_size=sizeof(audit_context);audit_context.base.capture_audio=audit_capture;audit_context.base.step=audit_step;
 ctx_display=fx_display;ctx_display.get_info=ctx_info;ctx_display.present_status=ctx_present;
#ifdef AUDIT_RGB_WIDTH
 ctx_display.get_info=audit_info;ctx_display.acquire=audit_frame;
#endif
#ifdef AUDIT_SYNCHRONOUS_PAPER
 async_display=false;
#endif
 fx_runtime.acquire=audit_acquire;fx_runtime.release=audit_release;fx_runtime.health=audit_health;fx_runtime.retain_invocation=audit_retain;
 assert(!app_module_init()&&!retained);
#ifdef AUDIT_RGB_WIDTH
 assert(width()==AUDIT_RGB_WIDTH&&height()==AUDIT_RGB_HEIGHT&&surface_format==RISC_DISPLAY_FORMAT_RGB565);
#else
 assert(width()==480&&height()==800&&surface_format==RISC_DISPLAY_FORMAT_MONO1);
#endif
 if(!strcmp(which,"baseline-first-poll")){
  t5_app_input_t in;assert(!poll(&in,20)&&storage_attempts==1);audit_verify_retained();
  puts("unchanged baseline: first optional shared-data denial retains invocation PASS");return 0;
 }
 if(!strcmp(which,"first-poll")){
  t5_app_input_t in;assert(poll(&in,20));assert(!failed&&!retained&&storage_attempts==3&&storage_namespaces==28&&contexts_client.api);
  assert(!contexts_client.rules_available&&(contexts_client.fingerprint_unread&CONTEXTS_ALL)==CONTEXTS_ALL);
  clear_color(0xffff);fill(1,1,16,16,0);present(false);assert(!failed&&!retained&&presents);
  assert(poll(&in,20)&&!retained);assert(contexts_tick()&&!retained);
  quick.ui.action_dnd=true;assert(quick_apply(PQA_DND)&&dnd_saved);
  assert(custody_launch("default.elf")&&!retained);app_module_fini();assert(!retained&&!live&&!subscriptions&&!frames);
  puts("optional stores denied: first poll, raster, presentation, settings, Contexts, launch and fini PASS");return 0;
 }
 if(!strcmp(which,"failed-release")){
  allow_storage=deny_release=true;t5_app_input_t in;assert(!poll(&in,20));
  assert(storage_attempts==1&&storage_reads==1&&contexts_client.fingerprint_store.api==&audit_data);
  audit_verify_retained();puts("failed shared-data release preserves grant and terminal fences PASS");return 0;
 }
 if(!strcmp(which,"fingerprint-open-retained")||!strcmp(which,"fingerprint-generic-open-retained")){
  callback_injection=3;
  if(!strcmp(which,"fingerprint-open-retained"))assert(!contexts_tick()&&retained);
  else {portable_contexts_client client;assert(!portable_contexts_open(&client,rt)&&retained);}
  audit_verify_retained();printf("initial CONFIG caller %s PASS\n",which);return 0;
 }
 if(!strcmp(which,"capture-raster-retained")||!strcmp(which,"capture-input-retained")||
    !strcmp(which,"capture-present-retained")||!strcmp(which,"step-caller-retained")||
    !strcmp(which,"fingerprint-caller-retained")||!strcmp(which,"fingerprint-checkpoint-retained")){
  assert(contexts_tick()&&!retained);
  if(!strncmp(which,"capture-",8)){
   if(strcmp(which,"capture-input-retained")){clear_color(0xffff);assert(surface.frame&&surface.pixels);}
   callback_injection=1;
   if(!strcmp(which,"capture-raster-retained"))fill(0,0,width(),height(),0);
   else if(!strcmp(which,"capture-present-retained"))present(false);
   else input_service();
   assert(retained);
  }else{
   callback_injection=!strcmp(which,"step-caller-retained")?2:3;
   if(!strcmp(which,"fingerprint-checkpoint-retained"))assert(!portable_fp_checkpoint(&contexts_client,false)&&retained);
   else assert(!contexts_tick()&&retained);
  }
  audit_verify_retained();printf("production enclosing caller %s PASS\n",which);return 0;
 }
 uint32_t version=1;uint64_t instance=2;
 if(!strncmp(which,"clean-",6))instance=(unsigned)(which[6]-'0');
 else if(!strncmp(which,"dirty-",6))storage_fault=(unsigned)(which[6]-'0');
 else if(!strcmp(which,"bad-version"))version=2;
 else if(!strcmp(which,"bad-namespace-1"))instance=1;
 else if(!strcmp(which,"bad-namespace-5"))instance=5;
 else if(!strcmp(which,"revoked-identity"))change_identity=true;
 else if(!strcmp(which,"revoked-health"))deny_health=true;
 else if(!strcmp(which,"missing-health"))fx_runtime.health=NULL;
 else assert(!"unknown case");
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};unsigned before=health_calls;
 assert(!custody_acquire(RISC_SHARED_DATA_CAPABILITY,version,instance,&grant)&&storage_attempts==1);
 if(!strncmp(which,"clean-",6)){
  assert(!retained&&!failed&&health_calls==before+1&&!grant.api&&!grant.slot&&!grant.generation);
  assert(custody_launch("default.elf")&&!retained);app_module_fini();assert(!live&&!subscriptions&&!frames&&!retained);
 }else audit_verify_retained();
 printf("native shared-data custody %s PASS\n",which);return 0;
}
