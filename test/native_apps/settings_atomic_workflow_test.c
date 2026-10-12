/* Actual Settings controller + paper input/view + checked RTC/native helpers.
 * The atomic endpoint is a scheduled copied provider double, never hardware. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main

static const char *const preference_keys[]={"time_format","time_zone","desk_clock_face","desk_direction",
 "reader_flip_ui","reader_language","sleep_idle","rtc_basis"};
static uint8_t values[8][64],pending_value[64],copied_intent[PORTABLE_PREFERENCE_INTENT_BYTES];
static uint32_t sizes[8],pending_size;
static unsigned submitted,collected,cancelled,atomic_reads,atomic_acquires,legacy_puts,atomic_releases;
static unsigned collect_ticks,busy_refusals,checkpoint_owned,page_visits[32];
static uint64_t revision=7,pending_token,submitted_revision;
static int pending_index;static bool cancellation,intervened,closing;
static int fault_status;
static int preference_index(const char *key) {
 for(unsigned i=0;i<8;++i)if(!strcmp(preference_keys[i],key))return (int)i;
 return -1;
}
static int32_t record_get(const char *key,void *out,uint32_t cap,uint32_t *used) {
 int index=preference_index(key);*used=index<0?0:sizes[index];
 if(!*used)return RISC_ATOMIC_NOT_FOUND;
 if(cap<*used)return RISC_ATOMIC_BUFFER_SMALL;
 memcpy(out,values[index],*used);return RISC_ATOMIC_OK;
}
static int32_t selected_legacy_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *used) {
 (void)ctx;io();++kv_reads;int rc=record_get(key,out,cap,used);
 return rc==RISC_ATOMIC_OK?RISC_KEY_VALUE_OK:rc==RISC_ATOMIC_NOT_FOUND?RISC_KEY_VALUE_NOT_FOUND:RISC_KEY_VALUE_BUFFER_SMALL;
}
static int32_t selected_legacy_put(void *ctx,const char *key,const void *value,uint32_t size) {
 (void)ctx;(void)key;(void)value;(void)size;io();++legacy_puts;assert(!"Selected Settings called legacy put");return RISC_KEY_VALUE_CONTEXT;
}
static int32_t atomic_state(void *ctx,risc_key_value_atomic_state_v1 *state) {
 (void)ctx;io();assert(kind_live(13));
 if(pending_token)return RISC_ATOMIC_BUSY;
 *state=(risc_key_value_atomic_state_v1){sizeof(*state),3,which("wrong-namespace")?6:1,16,64,4096,1024,0,revision};
 if(which("small-intent"))state->max_intent_bytes=128;
 return RISC_ATOMIC_OK;
}
static int32_t atomic_get(void *ctx,uint64_t expected,const char *key,void *out,uint32_t cap,uint32_t *used) {
 (void)ctx;io();assert(kind_live(13));++atomic_reads;*used=0;
 if(which("read-retained")){hidden=true;return RISC_ATOMIC_RETAINED;}
 if(pending_token)return RISC_ATOMIC_BUSY;
 if(expected!=revision)return RISC_ATOMIC_STALE;
 if(which("read-io")&&settings_atomic_edit.revision)return RISC_ATOMIC_IO;
 return record_get(key,out,cap,used);
}
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int32_t atomic_submit(void *ctx,uint64_t expected,const risc_key_value_atomic_mutation_v1 *changes,uint32_t count,const void *intent,uint32_t intent_size,uint64_t *token) {
 (void)ctx;io();assert(kind_live(13)&&!pending_token);*token=0;
 if(which("submit-busy")&&busy_refusals++<3)return RISC_ATOMIC_BUSY;
 if(expected!=revision)return RISC_ATOMIC_STALE;
 assert(count==1&&changes&&changes[0].kind==RISC_ATOMIC_PUT&&changes[0].size<=64&&changes[0].size);
 pending_index=preference_index(changes[0].key);assert(pending_index>=0);
 assert(intent_size==sizeof(copied_intent));memcpy(copied_intent,intent,intent_size);
 assert(!memcmp(copied_intent,"PFI0001",8));assert(copied_intent[8]==PORTABLE_PREFERENCE_USER&&copied_intent[9]==RISC_ATOMIC_PUT);
 assert(copied_intent[10]==!!sizes[pending_index]&&!strcmp((char *)copied_intent+12,changes[0].key));
 assert(word(copied_intent+28)==sizes[pending_index]&&word(copied_intent+32)==changes[0].size);
 assert(word(copied_intent+36)==revision&&word(copied_intent+40)==0);
 assert(!memcmp(copied_intent+44,values[pending_index],sizes[pending_index]));
 assert(!memcmp(copied_intent+108,changes[0].value,changes[0].size));
 assert(word(copied_intent+172)==portable_preference_crc(copied_intent,172));
 memcpy(pending_value,changes[0].value,changes[0].size);pending_size=changes[0].size;
 submitted_revision=expected;*token=pending_token=++submitted;collect_ticks=0;
 if(which("submit-dirty-refusal"))return RISC_ATOMIC_IO;
 return RISC_ATOMIC_OK;
}
static int32_t atomic_collect(void *ctx,uint64_t token,risc_key_value_atomic_receipt_v1 *receipt) {
 (void)ctx;io();assert(token&&token==pending_token);++collected;
 if(which("collect-error"))return RISC_ATOMIC_IO;
 if(which("collect-retained")){hidden=true;return RISC_ATOMIC_RETAINED;}
 if(which("close-pending"))return RISC_ATOMIC_PENDING;
 if(++collect_ticks<(which("cancel-pending")||which("time-cancel-pending")||which("cancel-touch-pending")||which("cancel-race")||which("home-pending")||which("cancel-error")?12u:3u))return RISC_ATOMIC_PENDING;
 int status=fault_status?fault_status:cancellation&&!which("cancel-race")?RISC_ATOMIC_CANCELLED:RISC_ATOMIC_OK;
 if(status==RISC_ATOMIC_OK){memcpy(values[pending_index],pending_value,pending_size);sizes[pending_index]=pending_size;++revision;}
 *receipt=(risc_key_value_atomic_receipt_v1){sizeof(*receipt),status,revision,status==RISC_ATOMIC_OK?45:0};
 pending_token=0;
 if(which("bad-receipt"))receipt->operation_id=0;
 if(which("bad-revision"))receipt->revision=submitted_revision;
 return RISC_ATOMIC_OK;
}
static int32_t atomic_cancel(void *ctx,uint64_t token) {
 (void)ctx;io();assert(token==pending_token&&token);++cancelled;cancellation=true;
 if(which("cancel-error"))return RISC_ATOMIC_IO;
 return RISC_ATOMIC_PENDING;
}
static risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
static bool selected_show(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t count,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
 ++page_visits[sv_page];
 if(settings_atomic_edit.valid&&!intervened&&
   (which("stale-draft")||which("stale-unrelated")||which("stale-time-before"))) {
  /* An independently committed write after draft capture, before Save. */
  ++revision;intervened=true;
  if(which("stale-draft")){sizes[0]=4;memcpy(values[0],(uint8_t[]){0x54,1,1,0xa4},4);}
 }
 return frame_show(ctx,frame,damage,count,options,token);
}
static bool selected_rtc_write(void *ctx,const twatch_rtc_time_v1 *value) {
 bool ok=rtc_write(ctx,value);
 if(which("stale-time-during")){++revision;intervened=true;}
 return ok;
}
/* The current touch reducer consumes ordered raw edges; the older shared
 * fixture's snapshot-only schedule must be converted at its provider boundary. */
static risc_touch_snapshot_v1 raw_state={.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
static risc_touch_event_v1 raw_queue[256];static unsigned raw_first,raw_last;
static void selected_touch_emit(unsigned kind,risc_touch_contact_v1 contact) {
 assert(raw_last-raw_first<256);
 raw_queue[raw_last++%256]=(risc_touch_event_v1){++raw_state.sequence,ticks,(uint8_t)kind,contact.id,contact.x,contact.y};
}
static bool selected_touch_poll(void *ctx,size_t count) {
 bool ok=touch_poll(ctx,count);risc_touch_snapshot_v1 report;assert(touch_snapshot(ctx,&report));
 if(raw_state.contact_count&&!report.contact_count)selected_touch_emit(RISC_TOUCH_EVENT_UP,raw_state.contacts[0]);
 if(report.contact_count) {
  if(!raw_state.contact_count)selected_touch_emit(RISC_TOUCH_EVENT_DOWN,report.contacts[0]);
  else if(memcmp(&raw_state.contacts[0],&report.contacts[0],sizeof(report.contacts[0])))selected_touch_emit(RISC_TOUCH_EVENT_MOVE,report.contacts[0]);
 }
 report.sequence=raw_state.sequence;report.timestamp_ms=ticks;raw_state=report;return ok;
}
static int32_t selected_touch_next(void *ctx,uint64_t token,risc_touch_event_v1 *out) {
 (void)ctx;io();assert(token==1);if(raw_first==raw_last)return 0;
 *out=raw_queue[raw_first++%256];return 1;
}
static bool selected_touch_snapshot(void *ctx,risc_touch_snapshot_v1 *out) {
 (void)ctx;io();*out=raw_state;return true;
}
static bool selected_acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *grant) {
 if(!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY)) {
  io();++atomic_acquires;assert(api==1&&instance==1&&grant->struct_size==sizeof(*grant));
  if(which("missing-capability"))return false;
  unsigned slot=1;while(leases[slot].kind)++slot;assert(slot<32);
  leases[slot]=(lease){13,++generation};++live;
  if(which("bad-api"))atomic_api.api_version=2;
  *grant=(risc_runtime_capability_v1){sizeof(*grant),slot,generation,&atomic_api};return true;
 }
 bool ok=acquire(name,api,instance,grant);if(!ok)return false;
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
  static risc_key_value_v1 kv;kv=kv_api;kv.get=selected_legacy_get;kv.put=selected_legacy_put;grant->api=&kv;
 } else if(!strcmp(name,"display.output")) {
  static risc_display_output_api_v1 display;display=display_api;display.submit=selected_show;grant->api=&display;
 } else if(!strcmp(name,"input.touch.raw")) {
  static risc_touch_api_v1 touch;touch=touch_api;touch.snapshot=selected_touch_snapshot;touch.poll=selected_touch_poll;touch.next=selected_touch_next;grant->api=&touch;
 } else if(!strcmp(name,TWATCH_RTC_CAPABILITY)) {
  static twatch_rtc_api_v1 rtc;rtc=rtc_api;rtc.write=selected_rtc_write;grant->api=&rtc;
 }
 return true;
}
static bool selected_release(risc_runtime_capability_v1 *grant) {
 if(leases[grant->slot].kind==13){
  ++atomic_releases;assert(!pending_token);
  if(which("release-error")){io();hidden=true;return false;}
 }
 return release(grant);
}
static int32_t selected_checkpoint(uint64_t invocation,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply) {
 if(pending_token)++checkpoint_owned;
 assert(!pending_token&&!settings_atomic_running);
 return fixture_resident_checkpoint(invocation,request,reply);
}
static bool selected_resident(risc_resident_client_v1 *out) {
 bool result=fixture_resident_shell(out);out->checkpoint=selected_checkpoint;return result;
}
static void inputs(void){memset(nav_buttons,0,sizeof(nav_buttons));contact_count=0;next_at=polls+5;}
static void cleanup(void) {
 in_main=false;in_fini=true;closing=true;app_module_fini();in_fini=false;
 if(retained) {
  unsigned calls=provider_calls,leases_held=live;
  app_module_fini();settings_view_redraw();assert(provider_calls==calls&&live==leases_held&&barriers==1);
 } else assert(!live&&!subscriptions&&!frames&&!barriers&&!pending_token);
}
#ifndef SETTINGS_AVAILABILITY_FIXTURE
int main(int argc,char **argv) {
 assert(argc==2);test_name=argv[1];zone_id="UTC";native_epoch=expected_epoch=1768505696;
 configure_records();memcpy(values[1],zone_record,zone_size);sizes[1]=zone_size;
 memcpy(values[4],flip_record,flip_size);sizes[4]=flip_size;
 memcpy(values[7],basis_record,basis_size);sizes[7]=basis_size;
 runtime.acquire=selected_acquire;runtime.release=selected_release;runtime.resident_shell=selected_resident;
 assert(app_module_init()==0);in_main=true;
 const t5_app_api_v1 *api=t5_app_get_api(1);
 if(which("bad-api")||which("read-retained")) {
  assert(!api);assert(retained);cleanup();assert(!legacy_puts);puts(test_name);return 0;
 }
 assert(api&&!retained&&atomic_acquires==1);
 if(which("missing-capability")||which("wrong-namespace")||which("small-intent")||which("read-io")) {
  assert(api->settings_activate(0,2)==T5_APP_SETTING_ERROR&&!submitted);cleanup();puts(test_name);return 0;
 }
 if(which("origin-fence")) {
  portable_preference_atomic stage={0};
  assert(portable_preference_atomic_begin(&stage,&atomic_api,"time_format",PORTABLE_PREFERENCE_AUTOMATIC,settings_atomic_guard,NULL)==RISC_ATOMIC_INVALID);
  assert(!submitted);cleanup();puts(test_name);return 0;
 }
 if(which("put-fence")) {
  assert(settings_store->put(settings_store->context,"time_format","x",1)==RISC_KEY_VALUE_CONTEXT&&!legacy_puts);
  cleanup();puts(test_name);return 0;
 }
 if(which("close-pending")) {
  assert(settings_atomic_begin("time_format"));uint8_t bytes[]={0x54,1,1,0xa4};
  assert(portable_preference_atomic_prepare(&settings_atomic_edit,bytes,4)==PORTABLE_PREFERENCE_READY);
  assert(portable_preference_atomic_submit(&settings_atomic_edit)==RISC_ATOMIC_PENDING);
  cleanup();assert(retained&&pending_token&&settings_atomic_edit.token&&!atomic_releases);puts(test_name);return 0;
 }
 unsigned row=2,index=0;
 if(!strcmp(test_name,"zone-save")){row=1;index=1;}
 if(!strcmp(test_name,"face-save")){row=SETTINGS_FACE_ROW;index=2;}
 if(!strcmp(test_name,"direction-save")){row=SETTINGS_DESK_DIRECTION_ROW;index=3;}
 if(!strcmp(test_name,"flip-save")){row=SETTINGS_FLIP_ROW;index=4;}
 if(!strcmp(test_name,"language-save")){row=SETTINGS_LANGUAGE_ROW;index=5;}
 if(!strcmp(test_name,"timer-save")){row=SETTINGS_TIMER_ROW;index=6;}
 bool clocks=strstr(test_name,"time-")||which("rtc-write-false")||which("native-seed-io");
 if(clocks){row=0;index=7;}
 if(which("uncertain")||which("time-uncertain"))fault_status=RISC_ATOMIC_UNCERTAIN;
 if(which("io"))fault_status=RISC_ATOMIC_IO;
 if(which("capacity"))fault_status=RISC_ATOMIC_CAPACITY;
 api->settings_render(0,(int32_t)row+1);inputs();
 if(row==0){save_time(which("time-touch-save"));}
 else if(row==1){next_at+=20;nav(RISC_NAV_DOWN);next_at+=20;nav(RISC_NAV_CONFIRM);next_at+=20;nav(RISC_NAV_CONFIRM);}
 else if(which("format-touch-save")||which("cancel-touch-before")||which("cancel-touch-pending")||which("held-touch-save")) {
  next_at+=16;tap(LOGICAL_WIDTH/2,300);next_at+=16;
  if(which("cancel-touch-before"))tap(80,FOOTER_Y);
  else if(which("held-touch-save")){for(unsigned i=0;i<12;++i)point(next_at+i,SAVE_X,FOOTER_Y);next_at+=20;}
  else tap(SAVE_X,FOOTER_Y);
 } else {
  if(!which("same-value"))nav(RISC_NAV_RIGHT);
  nav(which("cancel-before")?RISC_NAV_BACK:RISC_NAV_CONFIRM);
 }
 if(which("cancel-touch-pending"))tap(80,FOOTER_Y);
 else if(which("cancel-pending")||which("time-cancel-pending")||which("cancel-race")||which("cancel-error"))nav(RISC_NAV_BACK);
 else if(which("home-pending"))nav(RISC_NAV_HOME);
 else if(which("repeated-confirm"))nav(RISC_NAV_CONFIRM);
 next_at+=40;end_app();
 uint8_t result=api->settings_activate(0,row);
 if(getenv("NATIVE_SETTINGS_TRACE"))fprintf(stderr,"case=%s result=%u submits=%u revision=%llu status=%d message=%s tf=%s retained=%u pending=%llu\n",test_name,result,submitted,(unsigned long long)revision,settings_atomic_status,settings_message,time_format_message,retained,(unsigned long long)pending_token);
 assert(!legacy_puts&&!checkpoint_owned);
 bool no_submit=which("stale-draft")||which("stale-unrelated")||which("stale-time-before")||which("stale-time-during")||which("cancel-before")||which("cancel-touch-before")||which("rtc-write-false")||which("native-seed-io");
 bool retained_case=which("collect-error")||which("collect-retained")||which("bad-receipt")||which("bad-revision")||which("submit-dirty-refusal")||which("cancel-error")||which("rtc-write-false");
 bool rejected=no_submit||fault_status||retained_case||which("cancel-pending")||which("time-cancel-pending")||which("cancel-touch-pending")||which("home-pending");
 if(!rejected)assert(result==T5_APP_SETTING_UPDATED&&submitted==1&&sizes[index]&&revision==8);
 else assert(result!=T5_APP_SETTING_UPDATED);
 assert(retained==retained_case);
 if(no_submit)assert(!submitted);
 if(which("stale-time-before"))assert(!rtc_writes&&!seeds);
 if(which("stale-time-during"))assert(rtc_writes==1&&seeds==1&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED&&strstr(snt_status_text,"stale"));
 if(which("time-cancel-pending")||which("time-uncertain"))assert(rtc_writes==1&&seeds==1&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED&&snt_last_result.metadata_write_accepted);
 if(which("stale-time-during"))assert(!snt_last_result.metadata_write_accepted);
 if(which("time-save"))assert(rtc_writes==1&&seeds==1&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
 if(which("cancel-pending")||which("time-cancel-pending")||which("cancel-touch-pending")||which("cancel-race")||which("home-pending"))assert(cancelled==1);
 if(which("cancel-pending")||which("time-cancel-pending")||which("cancel-touch-pending")||fault_status)assert(!sizes[0]);
 if(which("submit-busy"))assert(busy_refusals==4);
 if(which("cancel-before"))assert(!cancelled&&!sizes[0]);
 if(which("same-value"))assert(sizes[0]==4&&!values[0][2]);
 if(which("flip-save"))assert(paper_flip_ui);
 if(which("timer-save"))assert(sizes[6]==5&&values[6][2]==65);
 if(which("time-save"))assert(word(values[7]+4)==expected_epoch);
 if(!retained&&result==T5_APP_SETTING_UPDATED) {
  uint8_t out[64];uint32_t size=0;assert(selected_legacy_get(NULL,preference_keys[index],out,64,&size)==0);
  assert(size==sizes[index]&&!memcmp(out,values[index],size));
 }
 cleanup();if(which("release-error"))assert(retained&&atomic_releases==1);
 printf("%s: submits=%u collect=%u cancel=%u retained=%u clocks=%u/%u\n",test_name,submitted,collected,cancelled,retained,rtc_writes,seeds);
}

#endif
