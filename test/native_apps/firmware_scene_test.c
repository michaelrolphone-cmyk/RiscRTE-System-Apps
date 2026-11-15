/* Deterministic host contract tests: actual intent controller, fake providers.
 * No network, hardware, persistence, update bank, or reboot is touched. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/ota_update_scene.c"

static risc_runtime_api_v1 rt;
static risc_scene_components_api_v1 scene;
static software_update_metadata_api_v1 service;
static software_update_v1 old_service; /* genuinely prefix-sized allocation */
static risc_key_value_v1 kv;
static wifi_api_v1 wifi;
static risc_realtime_api_v1 realtime;
static risc_rtc_clock_prefix_v2 raw_rtc;
static risc_rtc_utc_api_v1 utc_rtc;
static risc_civil_clock_policy_v1 civil_policy;
static risc_app_catalog_v1 apps;
static bool apps_present,apps_available,apps_changed;
static unsigned app_snapshots,launches;

static unsigned rtc_mode,rtc_acquires,rtc_reads,rtc_utc_reads,civil_acquires,civil_reads;
static bool civil_present,rtc_granted;
static int civil_result;

static software_update_status_v1 status_value;
static software_update_row_v1 catalog[128];
static risc_scene_event_v1 events[64];
static unsigned event_head,event_tail,calls,trip,retains,acquires,releases;
static unsigned connects,disconnects,begins,refreshes,cancels,activates,restarts,reads,steps,opens,updates,closes,checkpoints;
static uint32_t scene_flags,now_ms,last_poll_flags,pending_attention;
static bool live,ui_present,update_present,wifi_present,kv_present,time_present,time_valid,metadata,configured;
static bool cancel_ok,activate_ok,status_ok,verify_to_ready,inject_cancel,inject_disconnect_cancel,resident_on,host_exit;
static int close_result,checkpoint_result;
static unsigned snapshot_cancel;
static bool update_cancel_event;
static void queue(uint32_t,uint32_t,uint32_t);
static bool background_paused;
static unsigned background_steps,background_pauses,effect_completions;
static uint32_t effect_bits,checkpoint_reasons[128];
static wifi_link_t link_state;
static uint64_t sequence,last_utc;
static unsigned last_index;
static uint8_t blobs[5][64];static uint32_t blob_sizes[5];

static void io(void){assert(live);calls++;if(trip&&calls==trip)live=false;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&live?&rt:NULL;}
static bool retained(void){retains++;live=false;return true;}
/* Test-only CLEAN background seam. Unsafe paths must already be paused and
 * cannot invoke storage/provider I/O. Production background is tested separately. */
bool ota_update_scene_background_init(const risc_runtime_api_v1 *r,bool(*a)(void*),void(*t)(void*),void *owner){(void)r;(void)a;(void)t;(void)owner;io();background_paused=true;return live;}
bool ota_update_scene_background_capture(void){if(!ota_update_scene_background_safe()){assert(background_paused);return live;}io();return live;}
bool ota_update_scene_background_step(bool settled){(void)settled;assert(ota_update_scene_background_safe());io();background_steps++;background_paused=false;return live;}
bool ota_update_scene_background_pause(void){background_pauses++;if(!ota_update_scene_background_safe()){assert(background_paused);return live;}io();background_paused=true;return live;}
bool ota_update_scene_background_close(void){assert(ota_update_scene_background_safe()&&background_paused);io();return live;}
bool ota_update_scene_background_idle_timeout(uint32_t *out){assert(ota_update_scene_background_safe());io();*out=0;return live;}
bool ota_update_scene_background_capture_active(void){return false;}
uint32_t ota_update_scene_background_effects(void){return effect_bits;}
bool ota_update_scene_background_effect_done(unsigned e,unsigned r){assert(e>=1&&e<=4&&r==RISC_SETTINGS_APPLIED);assert(ota_update_scene_background_safe());io();effect_completions++;effect_bits&=~(1u<<e);return live;}
static bool release_grant(risc_runtime_capability_v1 *g){io();releases++;if(g->api==&raw_rtc||g->api==&utc_rtc)rtc_granted=false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 io();acquires++;assert(version==1||(!strcmp(name,"rtc.clock")&&version==2));const void *api=NULL;
 if(!strcmp(name,RISC_SCENE_CAPABILITY)&&ui_present)api=&scene;
 else if(!strcmp(name,SOFTWARE_UPDATE_FIRMWARE_CAPABILITY)&&update_present)api=metadata?(void*)&service:(void*)&old_service;
 else if(!strcmp(name,"net.wifi")&&wifi_present)api=&wifi;
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE&&kv_present)api=&kv;
 else if(!strcmp(name,RISC_REALTIME_CAPABILITY)&&time_present)api=&realtime;
 else if(!strcmp(name,"rtc.clock")){rtc_acquires++;if(rtc_mode){api=rtc_mode==1?(void*)&raw_rtc:(void*)&utc_rtc;rtc_granted=true;}}
 else if(!strcmp(name,RISC_APP_CATALOG_CAPABILITY)&&apps_present)api=&apps;
 else if(!strcmp(name,RISC_CIVIL_CLOCK_CAPABILITY)){assert(!rtc_granted);civil_acquires++;if(civil_present)api=&civil_policy;}
 if(api)*g=(risc_runtime_capability_v1){sizeof(*g),acquires,1,api};
 return api!=NULL;
}
static int32_t apps_snapshot(void *c,risc_app_catalog_snapshot_v1 *out){(void)c;io();out->count=1;out->generation=apps_changed&&app_snapshots?2:1;app_snapshots++;return RISC_APP_CATALOG_OK;}
static int32_t apps_get(void *c,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out){(void)c;io();assert(generation==1&&index==0);memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->flags=apps_available?RISC_APP_CATALOG_AVAILABLE:0;strcpy(out->file_name,"apps/target.elf");return RISC_APP_CATALOG_OK;}
static bool request_launch(const char *route){io();launches++;assert(fw.clean&&!fw.ui.token&&!fw.service&&!fw.credentials&&!fw.wifi&&!fw.background_started);assert(!strcmp(route,"apps/target.elf"));return true;}
static bool clock_ms(uint32_t *out){io();*out=now_ms;return true;}
static bool exit_requested(void){io();return host_exit;}
static void yield_ms(uint32_t ms){io();now_ms+=ms;}
static bool rtc_read(void *c,risc_rtc_calendar_v2 *out){(void)c;io();rtc_reads++;assert(rtc_granted);*out=(risc_rtc_calendar_v2){.year=2026,.month=11,.day=4,.hour=10,.minute=5,.second=7,.weekday=3};return true;}
static bool rtc_utc_read(void *c,risc_rtc_utc_snapshot_v1 *out){(void)c;io();rtc_utc_reads++;assert(rtc_granted);out->epoch_seconds=1800000123;return true;}
static int32_t civil_project(void *c,int64_t e,uint32_t n,risc_civil_calendar_v1 *out){(void)c;(void)e;(void)n;(void)out;assert(false);return -1;}
static int32_t civil_interpret(void *c,const risc_civil_calendar_v1 *in,int64_t *out){(void)c;io();civil_reads++;assert(!rtc_granted);assert(in->struct_size==sizeof(*in)&&in->year==2026&&in->month==11&&in->day==4&&in->hour==10&&in->minute==5&&in->second==7&&in->weekday==3);if(!civil_result)*out=1800000456;return civil_result;}
static int32_t utc_read(void *c,risc_realtime_snapshot_v1 *out){(void)c;io();out->validity=time_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET;out->epoch_seconds=time_valid?1800000000:0;return RISC_REALTIME_OK;}
static int32_t scene_open(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *n,uint64_t *token){(void)c;(void)n;io();opens++;assert(d->node_count<=RISC_COMPONENTS_MAX_NODES);*token=42;return RISC_SCENE_OK;}
static int32_t scene_update(void *c,uint64_t t,const risc_components_document_v1 *d){(void)c;io();updates++;assert(t==42&&d->revision>fw.ui.committed.revision);if(update_cancel_event){update_cancel_event=false;queue(RISC_SCENE_ACTION_EVENT,FW_CANCEL,fw.ui.committed.revision);}return RISC_SCENE_OK;}
static int32_t scene_next(void *c,uint64_t t,risc_scene_event_v1 *out){(void)c;io();assert(t==42);if(event_head==event_tail)return RISC_SCENE_IDLE;*out=events[event_head++];return RISC_SCENE_OK;}
static int32_t scene_snapshot(void *c,uint64_t t,risc_scene_navigation_v1 *out,uint32_t *flags){(void)c;io();assert(t==42);out->depth=1;out->routes[0]=1;*flags=scene_flags;if(snapshot_cancel&&!--snapshot_cancel)queue(RISC_SCENE_ACTION_EVENT,FW_CANCEL,fw.ui.committed.revision);return RISC_SCENE_OK;}
static int32_t scene_close(void *c,uint64_t t){(void)c;io();closes++;assert(t==42);return close_result;}
static int32_t scene_configure(void *c,uint64_t t,uint32_t features){(void)c;io();assert(t==42);assert(features==(resident_on?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));return RISC_SCENE_OK;}
static void queue(uint32_t kind,uint32_t action,uint32_t revision){assert(event_tail<64);events[event_tail++]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=kind,.document_revision=revision,.node=kind==RISC_SCENE_ACTION_EVENT&&action!=FW_BACK?action:0,.action=action,.sequence=++sequence};}
static bool update_status(void *c,software_update_status_v1 *out){(void)c;io();if(!status_ok)return false;*out=status_value;return true;}
static bool update_get(void *c,uint32_t i,software_update_row_v1 *out){(void)c;io();assert(i<status_value.count);*out=catalog[i];return true;}
static bool update_refresh(void *c,uint64_t utc){(void)c;io();refreshes++;last_utc=utc;status_value.state=SOFTWARE_UPDATE_CATALOG;return true;}
static bool update_step(void *c){(void)c;io();steps++;if(verify_to_ready&&status_value.state==SOFTWARE_UPDATE_VERIFYING){status_value.state=SOFTWARE_UPDATE_READY;if(inject_cancel)queue(RISC_SCENE_ACTION_EVENT,FW_CANCEL,fw.ui.committed.revision);}return true;}
static bool update_begin(void *c,uint32_t i,uint64_t utc){(void)c;io();begins++;last_index=i;last_utc=utc;status_value.state=SOFTWARE_UPDATE_PREPARING;return true;}
static bool update_cancel(void *c){(void)c;io();cancels++;if(cancel_ok&&status_value.state!=SOFTWARE_UPDATE_ACTIVATED&&status_value.state!=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN)status_value.state=SOFTWARE_UPDATE_LIST;return cancel_ok;}
static bool update_activate(void *c){(void)c;io();activates++;assert(!fw.wifi&&!fw.radio_owned);if(activate_ok)status_value.state=SOFTWARE_UPDATE_ACTIVATED;else status_value.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;return activate_ok;}
static bool update_restart(void *c){(void)c;io();restarts++;return false;}
static bool readiness(void *c,software_update_readiness_v1 *out){(void)c;io();out->configured=configured;out->network_required=configured;strcpy(out->reason,configured?"Ready":"Feed not configured");return true;}
static unsigned key_index(const char *key){if(!strcmp(key,PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY))return 4;for(unsigned i=0;i<4;i++)if(!strcmp(key,portable_wifi_credentials_chunk_key(i/2,i%2)))return i;abort();}
static int32_t kv_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){(void)c;io();reads++;unsigned i=key_index(key);*size=blob_sizes[i];if(!*size)return RISC_KEY_VALUE_NOT_FOUND;assert(capacity>=*size);memcpy(out,blobs[i],*size);return RISC_KEY_VALUE_OK;}
static int32_t kv_put(void *c,const char *key,const void *data,uint32_t size){(void)c;io();unsigned i=key_index(key);assert(size<=64);memcpy(blobs[i],data,size);blob_sizes[i]=size;return RISC_KEY_VALUE_OK;}
static bool wifi_connect(void *c,const char *ssid,const char *password){(void)c;io();connects++;assert(!strcmp(ssid,"Test network")&&!strcmp(password,"test-password"));link_state=WIFI_LINK_JOINING;return true;}
static wifi_link_t wifi_status(void *c){(void)c;io();return link_state;}
static int8_t wifi_rssi(void *c){(void)c;io();return -40;}
static bool wifi_addresses(void *c,wifi_ipv4_v1 *station,wifi_ipv4_v1 *ap){(void)c;(void)ap;io();station->address[0]=10;return true;}
static bool wifi_disconnect(void *c){(void)c;io();disconnects++;if(inject_disconnect_cancel)queue(RISC_SCENE_ACTION_EVENT,FW_CANCEL,fw.ui.committed.revision);link_state=WIFI_LINK_DOWN;return true;}
static bool scan_start(void *c){(void)c;assert(false);return false;}
static bool scan_poll(void *c,garden_radio_scan_result_v1 *out){(void)c;(void)out;assert(false);return false;}
static bool scan_cancel(void *c){(void)c;assert(false);return false;}
static int32_t checkpoint(uint64_t invocation,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply){(void)reply;io();assert(checkpoints<128);checkpoint_reasons[checkpoints++]=request->reason;assert(invocation==7);if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL)last_poll_flags=request->flags;else {assert(!fw.ui.token&&!fw.wifi);assert(!request->flags);}return checkpoint_result;}
static int32_t attention(uint64_t invocation,risc_resident_attention_v1 *out){io();assert(invocation==7);out->supported_flags=RISC_RESIDENT_ATTENTION_ALL;out->pending_flags=pending_attention;out->monotonic_ms=now_ms;return RISC_RESIDENT_OK;}
static int32_t register_attention(uint64_t i,const risc_resident_attention_callbacks_v1 *c){(void)i;(void)c;assert(false);return RISC_RESIDENT_DENIED;}
static int32_t reject_manual(uint64_t i,uint32_t bits){(void)i;(void)bits;assert(false);return RISC_RESIDENT_DENIED;}
static bool resident(risc_resident_client_v1 *out){io();if(!resident_on)return false;assert(out->struct_size==sizeof(risc_resident_attention_client_v1));risc_resident_attention_client_v1 *ext=(void*)out;*ext=(risc_resident_attention_client_v1){.base={.api_version=1,.struct_size=sizeof(*ext),.invocation=7,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint},.attention_tag=RISC_RESIDENT_ATTENTION_TAG,.attention_version=1,.register_attention=register_attention,.attention=attention,.reject_manual=reject_manual};return true;}
static void reset(void){
 memset(&fw,0,sizeof(fw));memset(&fw_document,0,sizeof(fw_document));memset(events,0,sizeof(events));memset(blobs,0,sizeof(blobs));memset(blob_sizes,0,sizeof(blob_sizes));
 apps_present=apps_changed=false;apps_available=true;app_snapshots=launches=0;
 rtc_mode=rtc_acquires=rtc_reads=rtc_utc_reads=civil_acquires=civil_reads=0;civil_present=rtc_granted=false;civil_result=0;
 event_head=event_tail=calls=trip=retains=acquires=releases=snapshot_cancel=0;update_cancel_event=false;connects=disconnects=begins=refreshes=cancels=activates=restarts=reads=steps=opens=updates=closes=checkpoints=0;
 scene_flags=now_ms=last_poll_flags=pending_attention=0;background_paused=true;background_steps=background_pauses=effect_completions=effect_bits=0;memset(checkpoint_reasons,0,sizeof(checkpoint_reasons));sequence=last_utc=0;last_index=0;
 live=ui_present=update_present=wifi_present=kv_present=time_present=time_valid=metadata=configured=cancel_ok=activate_ok=status_ok=true;
 verify_to_ready=inject_cancel=inject_disconnect_cancel=resident_on=host_exit=false;close_result=RISC_SCENE_OK;checkpoint_result=RISC_RESIDENT_OK;link_state=WIFI_LINK_DOWN;
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.yield_ms=yield_ms,.acquire=acquire,.release=release_grant,.retain_invocation=retained,.monotonic_ms=clock_ms,.exit_requested=exit_requested,.resident_shell=resident,.request_launch=request_launch};
 scene=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(scene),.next=scene_next,.snapshot=scene_snapshot,.close=scene_close},.configure=scene_configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=scene_open,.update=scene_update};
 old_service=(software_update_v1){.api_version=1,.struct_size=sizeof(old_service),.refresh=update_refresh,.step=update_step,.status=update_status,.get=update_get,.begin=update_begin,.cancel=update_cancel,.activate=update_activate,.restart=update_restart};
 service=(software_update_metadata_api_v1){.base=old_service,.tag=SOFTWARE_UPDATE_METADATA_TAG,.version=1,.readiness=readiness};service.base.struct_size=sizeof(service);
 kv=(risc_key_value_v1){.api_version=1,.struct_size=sizeof(kv),.get=kv_get,.put=kv_put};
 wifi=(wifi_api_v1){.api_version=1,.struct_size=sizeof(wifi),.connect=wifi_connect,.status=wifi_status,.rssi=wifi_rssi,.addresses=wifi_addresses,.scan_start=scan_start,.scan_poll=scan_poll,.scan_cancel=scan_cancel,.disconnect_checked=wifi_disconnect};
 apps=(risc_app_catalog_v1){.api_version=1,.struct_size=sizeof(apps),.snapshot=apps_snapshot,.get=apps_get};
 raw_rtc=(risc_rtc_clock_prefix_v2){.api_version=2,.struct_size=sizeof(raw_rtc),.read=rtc_read};
 utc_rtc=(risc_rtc_utc_api_v1){.base=raw_rtc,.utc_tag=RISC_RTC_UTC_TAG,.utc_version=1,.read_utc=rtc_utc_read};utc_rtc.base.struct_size=sizeof(utc_rtc);
 civil_policy=(risc_civil_clock_policy_v1){.api_version=1,.struct_size=sizeof(civil_policy),.policy_version=1,.policy_tag=RISC_CIVIL_CLOCK_POLICY_TAG,.project=civil_project,.interpret_rtc=civil_interpret};
 realtime=(risc_realtime_api_v1){.api_version=1,.struct_size=sizeof(realtime),.read=utc_read};
 status_value=(software_update_status_v1){.struct_size=sizeof(status_value),.state=SOFTWARE_UPDATE_LIST,.count=128};
 for(unsigned i=0;i<128;i++){catalog[i]=(software_update_row_v1){.struct_size=sizeof(catalog[i]),.availability=SOFTWARE_UPDATE_AVAILABLE,.size=4096+i};snprintf(catalog[i].id,sizeof(catalog[i].id),"release-%03u-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwx",i);strcpy(catalog[i].version,"2.0.0");strcpy(catalog[i].installed,"1.0.0");memset(catalog[i].reason,'a'+i%26,sizeof(catalog[i].reason)-1);}
 portable_wifi_credentials draft={"Test network","test-password"};assert(portable_wifi_credentials_save(&kv,&draft)==PORTABLE_WIFI_CREDENTIALS_SAVED);calls=reads=0;
}
static void start(void){assert(fw_initialize(&rt));assert(fw_publish());assert(fw.ui.token==42&&!fw.retained);}
static void test_optional(void){
 reset();ui_present=false;assert(!fw_initialize(&rt));assert(fw.clean&&!fw.retained&&!fw.service);
 reset();update_present=false;start();assert(!fw.service);fw_action(FW_CHECK);assert(!connects&&!refreshes&&!fw.retained);assert(fw_close());
 reset();kv_present=false;start();assert(!fw_connect(false));assert(!connects&&!fw.retained&&strstr(fw.message,"storage"));
 reset();wifi_present=false;start();assert(!fw_connect(false));assert(!connects&&!fw.retained&&strstr(fw.message,"Wi-Fi unavailable"));
 reset();blob_sizes[4]=0;start();assert(!fw_connect(false));assert(!connects&&disconnects==1&&!fw.retained);
 reset();time_present=false;start();assert(fw_connect(false));link_state=WIFI_LINK_UP;fw_tick();assert(!refreshes&&!fw.connecting&&!fw.wifi&&!fw.retained&&strstr(fw.message,"valid time"));
 reset();time_valid=false;start();assert(fw_connect(false));link_state=WIFI_LINK_UP;fw_tick();assert(!refreshes&&!fw.retained);
 reset();configured=false;start();assert(fw_connect(false));assert(refreshes==1&&!connects&&last_utc==0);
 reset();metadata=false;start();assert(fw_connect(false));link_state=WIFI_LINK_UP;fw_tick();assert(refreshes==1&&last_utc==1800000000&&connects==1);assert(fw_cancel());
}
static void test_utc_policy(void){
 uint64_t seconds=99;
 reset();time_present=false;rtc_mode=1;civil_present=true;start();assert(fw_utc(&seconds)&&seconds==1800000456);assert(rtc_reads==1&&!rtc_utc_reads&&civil_reads==1&&!rtc_granted);
 reset();time_present=false;rtc_mode=1;start();seconds=99;assert(!fw_utc(&seconds)&&seconds==99);assert(rtc_reads==1&&civil_acquires==1&&!civil_reads&&!fw.retained);
 reset();time_present=false;rtc_mode=2;civil_present=true;start();assert(fw_utc(&seconds)&&seconds==1800000123);assert(rtc_utc_reads==1&&!rtc_reads&&!civil_acquires);
 for(unsigned malformed=0;malformed<3;malformed++){
  reset();time_present=false;rtc_mode=2;civil_present=true;
  if(malformed==0)utc_rtc.utc_version=2;
  if(malformed==1)utc_rtc.read_utc=NULL;
  if(malformed==2)utc_rtc.base.struct_size=offsetof(risc_rtc_utc_api_v1,utc_tag)+sizeof(utc_rtc.utc_tag);
  start();seconds=99;assert(!fw_utc(&seconds)&&seconds==99&&!rtc_reads&&!rtc_utc_reads&&!civil_acquires&&!fw.retained);
 }
 reset();rtc_mode=2;civil_present=true;time_valid=false;start();seconds=99;assert(!fw_utc(&seconds)&&seconds==99&&!rtc_acquires&&!civil_acquires);
 reset();rtc_mode=2;civil_present=true;realtime.struct_size=8;start();assert(!fw_utc(&seconds)&&!rtc_acquires&&!civil_acquires);
 reset();time_present=false;rtc_mode=1;civil_present=true;civil_result=-1;start();assert(!fw_utc(&seconds)&&!fw.retained&&!rtc_granted);
 reset();time_present=false;rtc_mode=1;civil_present=true;civil_result=-9;start();assert(!fw_utc(&seconds)&&fw.retained);unsigned before=calls;fw_tick();assert(calls==before);
 /* Exhaust every callback boundary on the two-grant fallback path. */
 reset();time_present=false;rtc_mode=1;civil_present=true;start();calls=0;assert(fw_utc(&seconds));unsigned boundaries=calls;
 for(unsigned at=1;at<=boundaries;at++){reset();time_present=false;rtc_mode=1;civil_present=true;start();calls=0;trip=at;(void)fw_utc(&seconds);assert(!live);before=calls;fw_tick();assert(calls==before);}
}
static void test_workflow_launch(void){
 const char route[33]="apps/target.elf";
 reset();start();assert(!ota_update_scene_background_launch(route)&&!fw.launch_pending);
 reset();apps_present=true;start();assert(ota_update_scene_background_launch(route));assert(fw.launch_pending&&!launches);close_result=RISC_SCENE_AGAIN;assert(fw_step());assert(!launches);close_result=RISC_SCENE_OK;assert(!fw_step());assert(launches==1&&fw.clean);
 reset();apps_present=true;apps_changed=true;start();assert(!ota_update_scene_background_launch(route)&&!fw.launch_pending&&!launches);
 reset();apps_present=true;apps_available=false;start();assert(!ota_update_scene_background_launch(route)&&!fw.launch_pending&&!launches);
 reset();apps_present=true;start();assert(fw_connect(false));unsigned before=calls;assert(!ota_update_scene_background_launch(route));assert(calls==before&&!launches);
 reset();apps_present=true;start();calls=0;assert(ota_update_scene_background_launch(route));unsigned boundaries=calls;
 for(unsigned at=1;at<=boundaries;at++){reset();apps_present=true;start();calls=0;trip=at;(void)ota_update_scene_background_launch(route);assert(!live);before=calls;assert(!fw_step());assert(calls==before&&!launches);}
}
static void test_windows_identity(void){
 reset();start();for(unsigned page=0;page<8;page++){
 assert(fw.row_first==page*16&&fw.row_count==16);
 for(unsigned i=0;i<16;i++){unsigned index=page*16+i;fw_action(FW_RELEASE+index);assert(fw.confirming&&fw.selected==index&&fw_same(&fw.chosen,&catalog[index]));assert(fw_publish());char full[256]={0};for(unsigned n=0;n<fw.ui.committed.node_count;n++){const risc_scene_node_v1 *node=&fw.ui.committed.nodes[n];if(node->id>=100&&node->id<110)strcat(full,node->text);}assert(!strcmp(full,catalog[index].id));fw_action(FW_CANCEL);assert(!fw.confirming);assert(fw_publish());}
 if(page<7){fw_action(FW_NEXT);assert(fw_publish());}
 }fw_action(FW_NEXT);assert(fw.first==112);for(unsigned p=7;p;p--){fw_action(FW_PREVIOUS);assert(fw_publish());assert(fw.row_first==(p-1)*16);}assert(fw_close());
 reset();start();queue(RISC_SCENE_ACTION_EVENT,FW_RELEASE,fw.ui.committed.revision-1);assert(fw_events());assert(!fw.confirming);
 catalog[0].id[60]='Z';fw_action(FW_RELEASE);assert(!fw.confirming&&strstr(fw.message,"changed"));fw.dirty=true;assert(fw_publish());fw_action(FW_RELEASE);assert(fw.confirming);catalog[0].size++;fw_action(FW_INSTALL);assert(!fw.confirming&&!connects&&!begins);
 reset();start();fw_action(FW_RELEASE);assert(fw.confirming);fw_action(FW_INSTALL);assert(fw.connecting);catalog[0].reason[75]='Z';link_state=WIFI_LINK_UP;fw_tick();assert(!begins&&!fw.wifi&&strstr(fw.message,"changed"));
 reset();start();fw_action(FW_RELEASE+15);fw_action(FW_INSTALL);link_state=WIFI_LINK_UP;fw_tick();assert(begins==1&&last_index==15&&last_utc==1800000000);
}
static void test_cancel_ready(void){
 /* Publication callbacks can enqueue an event from the just-replaced revision. */
 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_VERIFYING;fw.dirty=true;assert(fw_publish());verify_to_ready=true;update_cancel_event=true;assert(fw_step());assert(!activates);assert(fw_step());assert(!activates&&cancels==1);
 /* A cancellation from the prior operation must not stop a later operation. */
 reset();start();assert(fw_connect(false));assert(fw_publish());uint32_t old_operation=fw.revision;assert(fw_cancel());assert(fw_publish());assert(fw_connect(false));assert(fw_publish());assert(fw.cancel_revision_floor>old_operation);unsigned old_cancels=cancels;queue(RISC_SCENE_ACTION_EVENT,FW_CANCEL,old_operation);assert(fw_events());assert(fw.connecting&&cancels==old_cancels);
 /* The relaxed stop fence never authorizes an old Install action. */
 reset();start();fw_action(FW_RELEASE);assert(fw_publish());uint32_t old_confirmation=fw.revision;fw_action(FW_INSTALL);assert(fw_publish());queue(RISC_SCENE_ACTION_EVENT,FW_INSTALL,old_confirmation);assert(fw_events());assert(fw.connecting&&!begins);

 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_VERIFYING;fw.dirty=true;assert(fw_publish());verify_to_ready=true;snapshot_cancel=2;assert(fw_step());assert(!activates);assert(fw_step());assert(!activates&&cancels==1);

 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_VERIFYING;fw.dirty=true;assert(fw_publish());verify_to_ready=inject_cancel=true;assert(fw_step());assert(!activates);assert(fw_step());assert(!activates&&cancels==1);
 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_VERIFYING;fw.dirty=true;assert(fw_publish());verify_to_ready=inject_cancel=true;scene_flags=RISC_SCENE_PRESENTING;uint32_t revision=fw.revision;assert(fw_step());assert(!activates&&fw.revision==revision);assert(fw_step());assert(!activates&&cancels==1&&fw.status.state==SOFTWARE_UPDATE_LIST);assert(event_head==event_tail);scene_flags=0;assert(fw_publish());
 reset();start();assert(fw_connect(false));link_state=WIFI_LINK_UP;fw.connecting=false;status_value.state=fw.status.state=SOFTWARE_UPDATE_READY;fw.dirty=true;assert(fw_publish());inject_disconnect_cancel=true;fw_tick();assert(!activates&&cancels==2&&fw.status.state==SOFTWARE_UPDATE_LIST);
 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_READY;activate_ok=false;fw_tick();assert(activates==1&&fw.restart_pending&&fw.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN);unsigned before=cancels;assert(!fw_cancel());fw_action(FW_CHECK);assert(cancels==before&&!connects);assert(fw_publish());fw_action(FW_RESTART);assert(restarts==1&&fw.restart_pending);fw_tick();assert(activates==1);
 reset();start();status_value.state=fw.status.state=SOFTWARE_UPDATE_READY;fw_tick();assert(fw.restart_pending&&fw.status.state==SOFTWARE_UPDATE_ACTIVATED);assert(fw_publish());
}
static void assert_terminal(void){unsigned before=calls;fw_action(FW_CHECK);fw_action(FW_CANCEL);fw_action(FW_RESTART);fw_tick();assert(!fw_step());assert(!fw_publish());assert(!fw_cancel());assert(!fw_connect(false));assert(calls==before);}
static void retention_scenario(unsigned mode){
 if(mode==0){(void)fw_initialize(&rt);if(live)(void)fw_publish();}
 else if(mode==1){(void)fw_connect(false);}
 else if(mode==2){fw_tick();}
 else if(mode==3){(void)fw_cancel();}
 else {(void)fw_step();}
}
static void retention_prepare(unsigned mode){reset();if(!mode)return;start();if(mode>=2){assert(fw_connect(false));link_state=WIFI_LINK_UP;}if(mode==4){resident_on=true;assert(files_resident_init(&fw.resident,&rt,fw_ui_alive,fw_ui_retain,fw_settle,NULL));}calls=0;}
static void test_retention(void){
 for(unsigned mode=0;mode<5;mode++){retention_prepare(mode);retention_scenario(mode);unsigned count=calls;for(unsigned at=1;at<=count;at++){retention_prepare(mode);trip=at;retention_scenario(mode);assert(!live);assert_terminal();}}
 reset();start();cancel_ok=false;assert(!fw_cancel());assert(fw.retained&&retains==1);assert_terminal();
 reset();start();status_value.state=SOFTWARE_UPDATE_RETAINED;assert(!fw_status());assert(fw.retained);assert_terminal();
}
static void test_host(void){
 reset();resident_on=true;start();pending_attention=RISC_RESIDENT_ATTENTION_ALARM;scene_flags=RISC_SCENE_PRESENTING;assert(fw_step());assert(!checkpoints&&fw.resident.pending_attention==RISC_RESIDENT_ATTENTION_ALARM);pending_attention=0;scene_flags=0;assert(fw_step());assert(checkpoints==1&&checkpoint_reasons[0]==RISC_RESIDENT_QUERY_ALARM_ONLY);assert(fw_step());assert(checkpoints==2&&checkpoint_reasons[1]==RISC_RESIDENT_CHECKPOINT_ALARM&&!fw.resident.phase&&fw.ui.token);
 reset();resident_on=true;start();fw.idle_ms=100;now_ms=101;for(unsigned i=0;i<4;i++)assert(fw_step());assert(checkpoints==4);assert(checkpoint_reasons[0]==RISC_RESIDENT_QUERY_ALARM_ONLY);assert(checkpoint_reasons[1]==RISC_RESIDENT_QUERY_IDLE_ONLY);assert(checkpoint_reasons[2]==RISC_RESIDENT_APPLY_IDLE_ONLY);assert(checkpoint_reasons[3]==RISC_RESIDENT_QUERY_LAST_IDLE_ALARM);assert(fw.ui.token&&fw.resident.idle_completed);
 reset();resident_on=true;start();effect_bits=1u<<1;scene_flags=RISC_SCENE_PRESENTING;assert(fw_step());assert(!checkpoints&&effect_bits);scene_flags=0;for(unsigned i=0;i<4;i++)assert(fw_step());assert(effect_completions==1&&!effect_bits&&checkpoints==3);assert(checkpoint_reasons[0]==RISC_SETTINGS_QUERY(1));assert(checkpoint_reasons[1]==RISC_SETTINGS_APPLY(1));assert(checkpoint_reasons[2]==RISC_SETTINGS_RESULT(1,RISC_SETTINGS_APPLIED));

 reset();resident_on=true;start();queue(RISC_SCENE_CONTROLS_EVENT,0,fw.revision);assert(fw_step());assert(checkpoints==1&&closes==1&&opens==2);
 assert(fw_connect(false));unsigned old_checkpoints=checkpoints;assert(fw_step());assert(checkpoints==old_checkpoints&&background_paused);
 host_exit=true;assert(!fw_step());assert(fw.clean&&!fw.ui.token&&!fw.service&&!fw.wifi&&!fw.credentials);
 reset();resident_on=true;start();close_result=RISC_SCENE_AGAIN;host_exit=true;assert(fw_step());assert(!fw.clean&&fw.ui.closing);close_result=RISC_SCENE_OK;assert(!fw_step());assert(fw.clean);
 reset();resident_on=true;start();checkpoint_result=RISC_RESIDENT_RETAINED;queue(RISC_SCENE_CONTROLS_EVENT,0,fw.revision);assert(!fw_step());assert(fw.retained);assert_terminal();
}
int main(void){test_optional();test_utc_policy();test_workflow_launch();test_windows_identity();test_cancel_ready();test_retention();test_host();puts("Firmware scene: optional services, 128-row windows, exact identity, stale events, Cancel/READY, pending display input, restart uncertainty, callback retention fences, host lifecycle PASS");return 0;}
