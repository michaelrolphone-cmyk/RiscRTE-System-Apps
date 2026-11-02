/* Native ESP-IDF C6 adapter for the existing Capabilities Service protocol.
 * All authority is outbound authenticated HTTPS; no rendering HTTP listener.
 * NVS commits the exact request before every mutation. Loss of a reply replays
 * that request. Reboot never restores a lease or inferred panel contents.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include "panel.h"
#include "frame.h"
#include "catalog.h"
#include "presentation.h"
#include "identity.h"
#include "esp_attr.h"
#include "idle_guard.h"
#include "recovery.h"
#include "esp_private/startup_internal.h"
#include "hal/modem_syscon_ll.h"
#define API_ORIGIN "https://capabilities-service.cantikash.chatgpt.site"
#define JSON_LIMIT 18000
#define JOURNAL_LIMIT 50000
static nvs_handle_t nv,journal_nv;
static uint32_t journal_sequence;
static bool journal_loaded;
static int64_t last_panel_retry,last_wifi_retry,last_wifi_repair;
static unsigned network_failures;
static char network_error[80];
#define FW_VERSION "biome-monitor/1.1.3"
static cJSON *cfg,*state;
static EventGroupHandle_t wifi_events;
static char boot_id[33],session_id[128];
static int generation;
static int64_t deadline,activation_gen,server_anchor,mono_anchor;
static bool healthy,enabled=true,offer_ready;
static uint8_t *frame;
static const char *TAG="biome-monitor";
static presentation visible;
enum { STAGE_START,STAGE_PANEL,STAGE_NVS,STAGE_WIFI,STAGE_RECOVERY,STAGE_ENROLL,STAGE_SESSION,STAGE_HTTP,STAGE_JOURNAL,STAGE_RENDER,STAGE_IDLE };
static const char *stage_names[]={"start","panel","nvs","wifi","recovery","enroll","session","http","journal","render","idle"};
RTC_NOINIT_ATTR static struct {uint32_t magic,stage;} boot_trace;
static uint32_t previous_stage;
static esp_reset_reason_t reset_reason;
static void stage(unsigned n){boot_trace.stage=n;}
/* HP watchdog/USB resets do not necessarily reset the Wi-Fi MAC domain.
 * Stop any old RX DMA before startup creates the new heap (heap priority100).
 * See espressif/esp-idf#19223; independent of the WFI avoidance below. */
ESP_SYSTEM_INIT_FN(biome_radio_boot_reset, CORE, BIT(0), 2) {
 modem_syscon_ll_reset_wifimac(&MODEM_SYSCON);
 return ESP_OK;
}
/* ESP-IDF 5.4.2 names embedded symbols from the file basename. */
extern const uint8_t idle_logo[] asm("_binary_idle_logo_bin_start");
static cJSON *get(const cJSON *o,const char *k){return cJSON_GetObjectItemCaseSensitive(o,k);}
static const char *str(const cJSON *o,const char *k){cJSON *v=get(o,k);return cJSON_IsString(v)?v->valuestring:"";}
static double num(const cJSON *o,const char *k){cJSON *v=get(o,k);return cJSON_IsNumber(v)?v->valuedouble:0;}
static bool eq(const cJSON *o,const char *k,const char *v){return strcmp(str(o,k),v)==0;}
static void put(cJSON *o,const char *k,cJSON *v){cJSON_DeleteItemFromObjectCaseSensitive(o,k);cJSON_AddItemToObject(o,k,v);}
static void ps(cJSON *o,const char *k,const char *v){put(o,k,cJSON_CreateString(v));}
static void pn(cJSON *o,const char *k,double v){put(o,k,cJSON_CreateNumber(v));}
static cJSON *json_copy(const cJSON *v){return v?cJSON_Duplicate(v,true):cJSON_CreateObject();}
static int64_t ms(void){return esp_timer_get_time()/1000;}
static int64_t server_now(void){return server_anchor+(ms()-mono_anchor);}
/* Local fallback needs no network lease. Only this task owns the SPI panel. */
static void idle_if_due(void){
 if(!healthy||!presentation_needs_logo(&visible,ms()))return;
 if(panel_render(idle_logo)==ESP_OK){presentation_set_logo(&visible);ESP_LOGI(TAG,"Biome Monitor idle logo displayed");}
 else{healthy=false;deadline=0;ESP_LOGE(TAG,"Idle logo refresh failed; panel contents unknown");}
}
static void pause_ms(int64_t duration){int64_t until=ms()+duration;do{idle_if_due();int64_t remaining=until-ms();if(remaining<=0)break;vTaskDelay(pdMS_TO_TICKS(remaining>1000?1000:remaining));}while(ms()<until);}
static void wait_wifi(void){
 idle_if_due();
 while(!(xEventGroupWaitBits(wifi_events,1,pdFALSE,pdTRUE,pdMS_TO_TICKS(1000))&1)){
  idle_if_due();
  if(ms()-last_wifi_retry>=15000){last_wifi_retry=ms();ESP_LOGW(TAG,"Wi-Fi/IP unavailable; retrying association");if(ms()-last_wifi_repair>=120000){last_wifi_repair=ms();esp_wifi_disconnect();}esp_wifi_connect();}
 }
}
static void stop(const char *why){deadline=0;ESP_LOGE(TAG,"Stopped: %s",why);for(;;)pause_ms(10000);}
static void hash(const void *p,size_t n,char out[65]);
static void save(void){
 stage(STAGE_JOURNAL);char *s=cJSON_PrintUnformatted(state);
 if(!s||strlen(s)+1>JOURNAL_LIMIT)stop("journal bound");
 size_t length=strlen(s)+1;uint32_t next=journal_sequence+1;
 size_t packed_length=length+RECOVERY_HEADER_BYTES;uint8_t *packed=malloc(packed_length);
 if(!packed)stop("journal memory");
 recovery_pack(packed,s,length,next);free(s);
 const char *key=next%2?"snap1":"snap0";
 esp_err_t e=nvs_set_blob(journal_nv,key,packed,packed_length);
 if(e==ESP_OK)e=nvs_commit(journal_nv);
 /* Do not send an external mutation until its exact intent is durable. */
 if(e==ESP_OK){uint8_t *check=malloc(packed_length);size_t n=packed_length;
  if(!check)e=ESP_ERR_NO_MEM;
  else{e=nvs_get_blob(journal_nv,key,check,&n);if(e==ESP_OK&&(n!=packed_length||memcmp(check,packed,n)))e=ESP_FAIL;free(check);}}
 free(packed);
 if(e!=ESP_OK){ESP_LOGE(TAG,"Journal write failed: %s; rebooting without sending mutation",esp_err_to_name(e));pause_ms(5000);esp_restart();}
 journal_sequence=next;journal_loaded=true;
}
static cJSON *load_json(const char *key){
 size_t n=0;bool blob=!strcmp(key,"journal");esp_err_t e=blob?nvs_get_blob(nv,key,NULL,&n):nvs_get_str(nv,key,NULL,&n);
 if(e==ESP_ERR_NVS_NOT_FOUND)return NULL;
 if(e!=ESP_OK)stop("provisioning read failed; retained");
 if(n>JOURNAL_LIMIT||!n)stop("stored data bound");
 char *s=malloc(n);if(!s)stop("memory");
 e=blob?nvs_get_blob(nv,key,s,&n):nvs_get_str(nv,key,s,&n);if(e!=ESP_OK)stop("provisioning read failed; retained");
 cJSON *v=cJSON_ParseWithLength(s,n);free(s);if(!v)stop("stored JSON invalid; retained");return v;
}
static cJSON *load_journal(void){
 cJSON *best=NULL;bool present=false;
 for(unsigned slot=0;slot<2;slot++){
  const char *key=slot?"snap1":"snap0";size_t n=0;esp_err_t e=nvs_get_blob(journal_nv,key,NULL,&n);
  if(e==ESP_ERR_NVS_NOT_FOUND)continue;
  present=true;
  if(e!=ESP_OK||n<RECOVERY_HEADER_BYTES||n>JOURNAL_LIMIT+RECOVERY_HEADER_BYTES)continue;
  uint8_t *b=malloc(n);if(!b)stop("journal read memory");
  uint32_t sequence=0;size_t length=0;
  if(nvs_get_blob(journal_nv,key,b,&n)==ESP_OK&&recovery_valid(b,n,&sequence,&length)){
   cJSON *v=cJSON_ParseWithLength((char*)b+RECOVERY_HEADER_BYTES,length);
   if(v&&(!best||recovery_newer(sequence,journal_sequence))){cJSON_Delete(best);best=v;journal_sequence=sequence;journal_loaded=true;}else cJSON_Delete(v);
  }free(b);
 }
 if(best)return best;
 if(present)stop("Both runtime journal slots invalid; provisioning retained, unknown intent not discarded");
 /* First upgrade only: preserve pending request identities/cursor from1.1.2. */
 return load_json("journal");
}
/* Enrollment tickets are consumable. Retain the resulting identity separately
 * from the request journal so a journal repair never spends a second ticket. */
static void remember_identity(const char *id){
 if(!identity_valid(id))stop("invalid participant identity");
 if(eq(cfg,"participant_id",id))return;
 ps(cfg,"participant_id",id);char *s=cJSON_PrintUnformatted(cfg);
 if(!s||strlen(s)>JOURNAL_LIMIT)stop("provisioning bound");
 esp_err_t e=nvs_set_str(nv,"config",s);free(s);
 if(e!=ESP_OK||nvs_commit(nv)!=ESP_OK)stop("identity commit");
}
static void hash(const void *p,size_t n,char out[65]){uint8_t d[32];mbedtls_sha256(p,n,d,0);for(int i=0;i<32;i++)sprintf(out+i*2,"%02x",d[i]);out[64]=0;}
static void nonce(char *out){uint8_t b[16];esp_fill_random(b,sizeof b);for(int i=0;i<16;i++)sprintf(out+i*2,"%02x",b[i]);out[32]=0;}
static void net_event(void *arg,esp_event_base_t base,int32_t id,void *data){(void)arg;(void)data;if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){xEventGroupClearBits(wifi_events,1);}if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){xEventGroupSetBits(wifi_events,1);ESP_LOGI(TAG,"Wi-Fi connected");}}
static void wifi(void){
 wifi_events=xEventGroupCreate();ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_sta();
 wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,net_event,NULL));ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,net_event,NULL));
 wifi_config_t w={0};snprintf((char*)w.sta.ssid,sizeof w.sta.ssid,"%s",str(cfg,"ssid"));snprintf((char*)w.sta.password,sizeof w.sta.password,"%s",str(cfg,"password"));
 ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_connect());
 wait_wifi();
 esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_setservername(1,"time.google.com");esp_sntp_init();
 /* Time sync is retried by SNTP; never trap startup here forever. TLS still
  * validates certificates and retries normally until trustworthy time exists. */
 int64_t until=ms()+30000;
 while(time(NULL)<1700000000&&ms()<until)pause_ms(1000);
 if(time(NULL)<1700000000)ESP_LOGW(TAG,"Network time unavailable; background sync continues");
}
/* Read a bounded authenticated response. Redirects and arbitrary destinations
 * are refused. Binary artifacts stay within this same configured service. */
static int request(const char *path,const char *body,bool bootstrap,uint8_t *out,size_t capacity){
 stage(STAGE_HTTP);
 if(path[0]!='/'||path[1]=='/'||strlen(path)>300)return -1;
 wait_wifi();
 char url[400];snprintf(url,sizeof url,"%s%s",API_ORIGIN,path);
 esp_http_client_config_t hc={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=45000,.disable_auto_redirect=true,.buffer_size=2048,.buffer_size_tx=2048};
 esp_http_client_handle_t h=esp_http_client_init(&hc);if(!h)return -1;
 char auth[1100];snprintf(auth,sizeof auth,"Bearer %s",bootstrap?str(cfg,"bootstrap"):str(cfg,"participant_credential"));esp_http_client_set_header(h,"Authorization",auth);
 snprintf(auth,sizeof auth,"Bearer %s",str(cfg,"site_access"));esp_http_client_set_header(h,"OAI-Sites-Authorization",auth);memset(auth,0,sizeof auth);
 if(bootstrap)esp_http_client_set_header(h,"x-enrollment-id",str(cfg,"enrollment_id"));
 if(body){esp_http_client_set_method(h,HTTP_METHOD_POST);esp_http_client_set_header(h,"Content-Type","application/json");}
 int result=-1;size_t sent=0,len=body?strlen(body):0;
 esp_err_t opened=esp_http_client_open(h,len);if(opened!=ESP_OK){snprintf(network_error,sizeof network_error,"http_open:%s",esp_err_to_name(opened));goto done;}
 while(sent<len){int n=esp_http_client_write(h,body+sent,len-sent);if(n<=0)goto done;sent+=n;}
 int64_t declared=esp_http_client_fetch_headers(h);int status=esp_http_client_get_status_code(h);
 if(status<200||status>=500||status==429||(status>=300&&status<400)||declared>(int64_t)capacity){snprintf(network_error,sizeof network_error,"http_status:%d",status);goto done;}
 size_t total=0;while(total<capacity){int n=esp_http_client_read(h,(char*)out+total,capacity-total);if(n<0)goto done;if(!n)break;total+=n;}
 if(total==capacity){char extra;int n=esp_http_client_read(h,&extra,1);if(n!=0)goto done;}
 if(!esp_http_client_is_complete_data_received(h))goto done;
 result=(int)total;
 done:esp_http_client_close(h);esp_http_client_cleanup(h);if(result<0){network_failures++;ESP_LOGW(TAG,"Transport retry %u: %s",network_failures,network_error);if(network_failures%8==0){esp_wifi_disconnect();esp_wifi_connect();}}else{network_failures=0;network_error[0]=0;}return result;
}
static cJSON *exchange(const char *wire,bool bootstrap){
 char *buf=malloc(JSON_LIMIT+1);if(!buf)stop("response memory");int n=request("/v1/call",wire,bootstrap,(uint8_t*)buf,JSON_LIMIT);cJSON *r=NULL;if(n>=0){buf[n]=0;r=cJSON_ParseWithLength(buf,n+1);if(r&&(!eq(r,"protocol_version","1.0")||(!eq(r,"status","completed")&&!eq(r,"status","rejected")))){cJSON_Delete(r);r=NULL;}}free(buf);return r;
}
/* Pending replies are finalized atomically with their durable consequences.
 * A replayed lease is never used; callers obtain a fresh sync after recovery. */
static void receipt(cJSON *req,cJSON *r){
 cJSON *d=get(r,"data"),*a=get(req,"arguments");
 if(eq(req,"operation","enroll")&&get(d,"id")){remember_identity(str(d,"id"));ps(state,"participant_id",str(d,"id"));}
 if(eq(req,"operation","sync")&&cJSON_IsArray(get(d,"controls"))){
  cJSON *queue=get(state,"controls");if(!queue){queue=cJSON_CreateArray();put(state,"controls",queue);}
  int64_t last=(int64_t)num(state,"cursor");cJSON *control;
  cJSON_ArrayForEach(control,get(d,"controls")){double raw=num(control,"seq");if(!frame_cursor_new(raw,-1))stop("invalid control sequence");int64_t seq=(int64_t)raw;if(seq<=last)continue;
   if(!frame_cursor_new(num(control,"seq"),last)||cJSON_GetArraySize(queue)>=32)stop("invalid control sequence or backlog; cursor retained");
   cJSON_AddItemToArray(queue,json_copy(control));last=seq;
  }pn(state,"cursor",last);
 }
 if(eq(req,"operation","observe")&&get(d,"id")){
  cJSON *queue=get(state,"controls");cJSON *first=cJSON_GetArrayItem(queue,0);
  if(first&&eq(get(first,"data"),"desired_id",str(a,"desired_id")))cJSON_DeleteItemFromArray(queue,0);
 }
 cJSON_DeleteItemFromObjectCaseSensitive(state,"pending");save();
}
static cJSON *rpc(const char *family,const char *op,cJSON *args,bool write,int revision){
 cJSON *q=cJSON_CreateObject();ps(q,"protocol_version","1.0");ps(q,"family",family);ps(q,"operation",op);cJSON_AddItemToObject(q,"arguments",args);
 char id[33];nonce(id);ps(q,"operation_id",id);if(revision>=0)pn(q,"expected_revision",revision);
 if(write){if(get(state,"pending"))stop("unresolved mutation");put(state,"pending",json_copy(q));save();}
 char *wire=cJSON_PrintUnformatted(q);if(!wire)stop("request memory");cJSON *r=NULL;
 do{r=exchange(wire,!strcmp(op,"enroll"));if(r&&eq(r,"mutation_outcome","unknown")){cJSON_Delete(r);r=NULL;}if(!r)pause_ms(3000);}while(write&&!r);
 free(wire);
 if(r&&eq(r,"status","rejected")){
  /* A rejection is a known non-execution; retained controls remain actionable. */
  ESP_LOGW(TAG,"%s.%s rejected: %s",family,op,str(r,"code"));
  if(write){cJSON_DeleteItemFromObjectCaseSensitive(state,"pending");save();}
  cJSON_Delete(r);r=NULL;
 }else if(r&&write)receipt(q,r);
 cJSON_Delete(q);return r;
}
static void recover(void){cJSON *q=get(state,"pending");if(!q)return;char *wire=cJSON_PrintUnformatted(q);if(!wire)stop("memory");for(;;){cJSON *r=exchange(wire,eq(q,"operation","enroll"));if(!r||eq(r,"mutation_outcome","unknown")){cJSON_Delete(r);pause_ms(3000);continue;}if(eq(r,"status","rejected")){cJSON_DeleteItemFromObjectCaseSensitive(state,"pending");save();}else receipt(q,r);cJSON_Delete(r);break;}free(wire);}
static cJSON *activation(void){cJSON *a=cJSON_CreateObject();ps(a,"protocol_version","provider-activation/1");cJSON_AddBoolToObject(a,"automatic",true);cJSON_AddBoolToObject(a,"available",healthy&&enabled);cJSON *profiles=cJSON_CreateArray();cJSON_AddItemToArray(profiles,cJSON_Parse(PROFILE_JSON));cJSON_AddItemToObject(a,"profiles",profiles);cJSON *report=cJSON_CreateObject();pn(report,"generation",activation_gen);ps(report,"profile_id",str(state,"activation_profile"));ps(report,"state",deadline>ms()?"applied":"idle");cJSON_AddItemToObject(a,"report",report);return a;}
static void activation_accept(cJSON *d,int64_t started){
 deadline=0;offer_ready=false;cJSON *g=get(d,"provider_activation");
 double server=num(d,"server_time");if(server<=0||ms()-started>30000)return;server_anchor=(int64_t)server;mono_anchor=ms();
 if(eq(g,"protocol_version","provider-activation/1")&&eq(g,"action","deactivate")&&eq(g,"state","idle")&&num(g,"generation")>=activation_gen){activation_gen=(int64_t)num(g,"generation");pn(state,"activation_generation",activation_gen);ps(state,"activation_profile","");save();return;}
 if(!eq(g,"protocol_version","provider-activation/1")||!eq(g,"action","activate")||!eq(g,"state","assigned")||!eq(g,"profile_id","epaper-800x480")||num(g,"profile_revision")!=1||!eq(g,"profile_digest",PROFILE_DIGEST)||num(g,"generation")<activation_gen||num(g,"lease_ms")>120000||num(g,"lease_ms")<=0)return;
 cJSON *caps=get(g,"capabilities"),*c;if(!cJSON_IsArray(caps)||!cJSON_IsArray(get(g,"services"))||cJSON_GetArraySize(get(g,"services")))return;
 cJSON_ArrayForEach(c,caps){if(!eq(c,"capability_id","display.epaper.frame")||(get(c,"api_version")&&(num(c,"api_version")!=1||!eq(c,"contract_digest",CONTRACT_DIGEST))))return;}
 int64_t rem=(int64_t)num(g,"expires_at")-(int64_t)server-(ms()-started)-2000;
 int64_t limit=started+(int64_t)num(g,"lease_ms")-2000;if(rem<=0||limit<=ms())return;
 activation_gen=(int64_t)num(g,"generation");if(num(state,"activation_generation")!=activation_gen||!eq(state,"activation_profile","epaper-800x480")){pn(state,"activation_generation",activation_gen);ps(state,"activation_profile","epaper-800x480");save();}
 if(healthy&&enabled)deadline=limit<ms()+rem?limit:ms()+rem;
}
static cJSON *session_args(void){cJSON *a=cJSON_CreateObject();ps(a,"participant_id",str(state,"participant_id"));ps(a,"session_id",session_id);pn(a,"generation",generation);return a;}
static bool open_session(void){
 cJSON *a=cJSON_CreateObject();ps(a,"participant_id",str(state,"participant_id"));ps(a,"boot_id",boot_id);cJSON *p=cJSON_CreateArray();cJSON_AddItemToArray(p,cJSON_CreateString("1.0"));cJSON_AddItemToObject(a,"adapter_protocols",p);ps(a,"adapter_version",FW_VERSION);pn(a,"contact_interval_ms",15000);pn(a,"response_bytes",12000);cJSON *diag=cJSON_CreateObject();pn(diag,"reset_reason",reset_reason);ps(diag,"previous_stage",stage_names[previous_stage]);pn(diag,"uptime_ms",ms());ps(diag,"firmware",FW_VERSION);cJSON_AddBoolToObject(diag,"panel_healthy",healthy);pn(diag,"network_failures",network_failures);cJSON_AddItemToObject(a,"journal_summary",diag);cJSON_AddItemToObject(a,"activation_request",activation());
 int64_t started=ms();cJSON *r=rpc("participant","open_session",a,true,-1);if(!r)return false;cJSON *d=get(r,"data");snprintf(session_id,sizeof session_id,"%s",str(d,"id"));generation=(int)num(d,"generation");activation_accept(d,started);cJSON_Delete(r);return session_id[0]&&generation>0;
}
static bool sync_now(void){cJSON *a=cJSON_CreateObject();ps(a,"session_id",session_id);pn(a,"generation",generation);char cursor[32];snprintf(cursor,sizeof cursor,"%.0f",num(state,"cursor"));ps(a,"cursor",cursor);pn(a,"requested_work",0);cJSON_AddItemToObject(a,"activation_request",activation());int64_t started=ms();cJSON *r=rpc("participant","sync",a,true,-1);if(!r){deadline=0;return false;}activation_accept(get(r,"data"),started);cJSON_Delete(r);return true;}
static void offer_id(char out[256]){snprintf(out,256,"%s:riscrte.esp32c6.epaper@1:panel",str(state,"participant_id"));}
static bool publish_offer(void){
 if(deadline<=ms())return false;
 char id[256];offer_id(id);cJSON *a=cJSON_CreateObject();ps(a,"kind","offer");ps(a,"entity_id",id);cJSON *existing=rpc("catalog","get",a,false,-1);int rev=existing?(int)num(get(existing,"data"),"revision"):0;cJSON_Delete(existing);
 a=session_args();cJSON *o=cJSON_CreateObject();ps(o,"offer_id",id);ps(o,"provider_id","riscrte.esp32c6.epaper");pn(o,"provider_revision",1);ps(o,"local_instance_id","panel");pn(o,"offer_generation",generation);pn(o,"valid_until",server_now()+60000);ps(o,"installed_state","active");ps(o,"observed_health","healthy");cJSON *e=cJSON_CreateObject();ps(e,"controller","busy-low-then-high power-on handshake");ps(e,"boot_id",boot_id);cJSON_AddBoolToObject(e,"optical_readback",false);cJSON_AddItemToObject(o,"probe_evidence",e);cJSON_AddItemToObject(o,"pool_bindings",cJSON_CreateObject());cJSON_AddItemToObject(o,"frame_spec",cJSON_Parse(FRAME_JSON));pn(o,"activation_generation",activation_gen);ps(o,"activation_profile","epaper-800x480");cJSON_AddItemToObject(a,"offer",o);
 cJSON *r=rpc("catalog","offer_upsert",a,true,rev);bool ok=r&&cJSON_IsTrue(get(get(r,"data"),"ready"));cJSON_Delete(r);return ok;
}
/* 1 verified, 0 invalid input/content, -1 transient transport/service failure. */
static int download(cJSON *artifact){
 const char *id=str(artifact,"artifact_id");cJSON *a=cJSON_CreateObject();ps(a,"artifact_id",id);cJSON *r=rpc("artifact","get",a,false,-1);if(!r)return -1;cJSON *d=get(r,"data");bool valid=eq(d,"state","verified")&&eq(d,"sha256",str(artifact,"sha256"))&&num(d,"size_bytes")==FRAME_BYTES;cJSON_Delete(r);if(!valid)return 0;
 a=cJSON_CreateObject();ps(a,"artifact_id",id);r=rpc("artifact","download_grant",a,true,-1);if(!r)return -1;d=get(r,"data");const char *path=str(d,"download_path");bool ok=frame_path(path,id);int n=ok?request(path,NULL,false,frame,FRAME_BYTES):-1;cJSON_Delete(r);if(!ok)return 0;if(n!=FRAME_BYTES)return -1;char sha[65];hash(frame,FRAME_BYTES,sha);return strcmp(sha,str(artifact,"sha256"))==0;
}
static void pop_control(void){cJSON_DeleteItemFromArray(get(state,"controls"),0);save();}
static void desired(cJSON *control){
 cJSON *a=cJSON_CreateObject();ps(a,"desired_id",str(get(control,"data"),"desired_id"));cJSON *r=rpc("desired","get",a,false,-1);if(!r)return;cJSON *d=get(r,"data"),*input=get(d,"desired"),*artifact=get(input,"artifact");
 char own[256];offer_id(own);int revision=(int)num(d,"desired_revision");
 if(!eq(d,"provider_offer_id",own)||!eq(d,"slot","frame")){cJSON_Delete(r);pop_control();return;}
 if(eq(d,"state","withdrawn")||num(d,"expires_at")<=server_now()){
  presentation_withdraw(&visible,str(d,"id"),revision);idle_if_due();cJSON_Delete(r);pop_control();return;
 }
 /* Reads/withdrawal remain actionable without activation; new remote effects do not. */
 if(!frame_can_render(ms(),deadline,healthy&&enabled,offer_ready)){cJSON_Delete(r);return;}
 frame_spec spec={(unsigned)num(input,"width"),(unsigned)num(input,"height"),(unsigned)num(input,"stride_bytes"),(unsigned)num(artifact,"size_bytes"),str(input,"format"),str(artifact,"sha256")};
 const char *status="failed",*reason="invalid_frame_spec";bool rendered=false;
 if(strlen(str(d,"id"))>0&&strlen(str(d,"id"))<sizeof visible.desired_id&&frame_valid(&spec)&&num(input,"width")==800&&num(input,"height")==480&&num(input,"stride_bytes")==100&&num(artifact,"size_bytes")==48000){
  int downloaded=download(artifact);if(downloaded<0){cJSON_Delete(r);return;}if(!downloaded){reason="artifact_verification_failed";goto report;}
  /* Refresh lease and re-read desired state after transfer. A superseded or
   * withdrawn image must not be committed just because its download finished. */
  if(!sync_now()||!publish_offer()){cJSON_Delete(r);return;}
  a=cJSON_CreateObject();ps(a,"desired_id",str(d,"id"));cJSON *fresh=rpc("desired","get",a,false,-1);cJSON *v=fresh?get(fresh,"data"):NULL;
  if(v&&eq(v,"provider_offer_id",own)&&eq(v,"slot","frame")&&(eq(v,"state","withdrawn")||num(v,"expires_at")<=server_now())){
   presentation_withdraw(&visible,str(v,"id"),(int)num(v,"desired_revision"));idle_if_due();
  }
  int64_t remaining=v?(int64_t)num(v,"expires_at")-server_now():0;
  bool current=v&&eq(v,"id",str(d,"id"))&&eq(v,"provider_offer_id",own)&&eq(v,"slot","frame")&&num(v,"desired_revision")==revision&&!eq(v,"state","withdrawn")&&remaining>30000;
  int64_t expires_ms=ms()+remaining;
  cJSON_Delete(fresh);
  if(!current||!frame_can_render(ms(),deadline,healthy&&enabled,true)){cJSON_Delete(r);return;}
  /* Durable physical-effect intent. On crash this remains unknown; convergence
   * safely repeats the latest complete frame under a new live lease. */
  if(presentation_same_frame(&visible,spec.sha256,ms())){
   presentation_set_frame(&visible,str(d,"id"),revision,spec.sha256,expires_ms);
   rendered=true;status="rendered";reason="same_frame_still_displayed";goto report;
  }
  cJSON *intent=cJSON_CreateObject();ps(intent,"boot_id",boot_id);ps(intent,"desired_id",str(d,"id"));pn(intent,"revision",revision);ps(intent,"sha256",spec.sha256);ps(intent,"status","unknown");put(state,"last_render",intent);save();
  stage(STAGE_RENDER);esp_err_t e=panel_render(frame);rendered=e==ESP_OK;status=rendered?"rendered":"unknown";reason=rendered?"controller_busy_completed":"controller_error_or_busy_timeout";
  if(rendered)presentation_set_frame(&visible,str(d,"id"),revision,spec.sha256,expires_ms);
  else{visible=(presentation){0};healthy=false;deadline=0;}
  ps(get(state,"last_render"),"status",status);save();
 }
 report:
 a=cJSON_CreateObject();ps(a,"desired_id",str(d,"id"));pn(a,"desired_revision",revision);pn(a,"provider_generation",generation);ps(a,"participant_id",str(state,"participant_id"));cJSON *actual=cJSON_CreateObject();ps(actual,"status",status);ps(actual,"reason",reason);ps(actual,"sha256",spec.sha256);ps(actual,"boot_id",boot_id);cJSON_AddBoolToObject(actual,"controller_refresh_completed",rendered);cJSON_AddBoolToObject(actual,"optical_readback",false);cJSON_AddItemToObject(a,"actual",actual);
 cJSON *observed=rpc("desired","observe",a,true,-1);if(observed){/* stale observations have no id; still retire the handled control */if(!get(get(observed,"data"),"id"))pop_control();cJSON_Delete(observed);}cJSON_Delete(r);
}
static void retry_panel(void){
 if(healthy||ms()-last_panel_retry<60000)return;
 last_panel_retry=ms();stage(STAGE_PANEL);healthy=panel_probe()==ESP_OK;
 if(healthy){visible=(presentation){0};ESP_LOGW(TAG,"Panel recovered; local Biome splash and fresh remote frame allowed");idle_if_due();}
}
static void report_boot(void){
 cJSON *a=cJSON_CreateObject(),*report=cJSON_CreateObject();ps(a,"participant_id",str(state,"participant_id"));
 ps(report,"phase",healthy?"connected":"panel_retry");ps(report,"adapter_version",FW_VERSION);ps(report,"boot_id",boot_id);
 char detail[160];snprintf(detail,sizeof detail,"reset=%d previous_stage=%s panel=%s journal=%s",(int)reset_reason,stage_names[previous_stage],healthy?"ready":"retrying",journal_loaded?"verified":"new");
 ps(report,"message",detail);cJSON_AddItemToObject(a,"report",report);
 cJSON *r=rpc("participant","report_status",a,true,-1);cJSON_Delete(r);
}
static void process_control(void){cJSON *c=cJSON_GetArrayItem(get(state,"controls"),0);if(!c)return;
 if(eq(c,"kind","desired")){desired(c);return;}
 if(eq(c,"kind","lifecycle")){enabled=eq(get(c,"data"),"lifecycle","enabled");if(!enabled)deadline=0;pop_control();return;}
 if(eq(c,"kind","recovery_requested")){pop_control();return;}
 /* No assignment executor is advertised. An unknown/control_reference record
  * remains durably queued, prevents admission, and needs operator diagnosis. */
 deadline=0;enabled=false;ESP_LOGE(TAG,"Unsupported control retained: %s",str(c,"kind"));
}
void app_main(void){
 reset_reason=esp_reset_reason();previous_stage=boot_trace.magic==0x42494f4d&&boot_trace.stage<=STAGE_IDLE?boot_trace.stage:STAGE_START;
 boot_trace.magic=0x42494f4d;stage(STAGE_START);
 ESP_LOGW(TAG,"%s boot reset=%d previous_stage=%s",FW_VERSION,(int)reset_reason,stage_names[previous_stage]);
 /* Let an intermittent supply settle before flash writes or panel charge pump. */
 ESP_ERROR_CHECK(biome_idle_guard_install());
 vTaskDelay(pdMS_TO_TICKS(reset_reason==ESP_RST_BROWNOUT?5000:2000));
 ESP_LOGW(TAG,"C6 idle guard active; interrupt and task watchdogs remain enabled");
 /* Show the built-in logo before NVS, credentials, Wi-Fi or service availability. */
 /* Seengreat ESP32-C6 E-Paper Driver Board v1.4: recovered source is authoritative. */
 stage(STAGE_PANEL);panel_pins pins={2,6,3,7,4,5};ESP_ERROR_CHECK(panel_init(pins));healthy=panel_probe()==ESP_OK;
 idle_if_due();
 stage(STAGE_NVS);ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nvs_open("epaper",NVS_READWRITE,&nv));ESP_ERROR_CHECK(nvs_flash_init_partition("runtime"));ESP_ERROR_CHECK(nvs_open_from_partition("runtime","epaper",NVS_READWRITE,&journal_nv));cfg=load_json("config");if(!cfg)stop("Provision config NVS first; see README");
 if(!strlen(str(cfg,"ssid"))||strlen(str(cfg,"ssid"))>32||strlen(str(cfg,"password"))>63)stop("invalid Wi-Fi provisioning");
 state=load_journal();if(!state)state=cJSON_CreateObject();if(!journal_loaded)save();nonce(boot_id);activation_gen=(int64_t)num(state,"activation_generation");
 identity_action identity=identity_resolve(str(state,"participant_id"),str(cfg,"participant_id"));
 if(identity==IDENTITY_INVALID)stop("provisioned identity conflicts with journal");
 if(identity==IDENTITY_RESTORE){ps(state,"participant_id",str(cfg,"participant_id"));save();ESP_LOGW(TAG,"Restored provisioned participant identity");}
 if(identity==IDENTITY_JOURNAL)remember_identity(str(state,"participant_id"));
 frame=malloc(FRAME_BYTES);if(!frame)stop("frame memory");stage(STAGE_WIFI);wifi();
 /* A Wi-Fi-only installation still joins its configured network and shows the
  * logo. Service credentials can be provisioned later without changing code. */
 if(strlen(str(cfg,"participant_credential"))<32||strlen(str(cfg,"participant_credential"))>1024||strlen(str(cfg,"site_access"))<1||strlen(str(cfg,"site_access"))>1024)stop("Wi-Fi connected; Capabilities Service provisioning required");
 stage(STAGE_RECOVERY);recover();
 while(!strlen(str(state,"participant_id"))){stage(STAGE_ENROLL);cJSON *a=cJSON_CreateObject();ps(a,"bootstrap_credential",str(cfg,"bootstrap"));ps(a,"display_name","Biome Monitor");ps(a,"class","physical_device");ps(a,"adapter_kind","esp32");cJSON *binding=cJSON_CreateObject();char sha[65];hash(str(cfg,"participant_credential"),strlen(str(cfg,"participant_credential")),sha);ps(binding,"credential_sha256",sha);cJSON_AddItemToObject(a,"identity_binding",binding);cJSON *labels=cJSON_CreateObject();ps(labels,"arch","riscv32");ps(labels,"display","uc8179-800x480-bw");ps(labels,"firmware",FW_VERSION);cJSON_AddItemToObject(a,"labels",labels);cJSON *r=rpc("participant","enroll",a,true,-1);cJSON_Delete(r);if(!strlen(str(state,"participant_id"))){ESP_LOGE(TAG,"Enrollment unavailable; keeping Wi-Fi alive and retrying in 30 seconds. Existing devices need participant_id provisioning.");pause_ms(30000);}}
 for(;;){retry_panel();stage(STAGE_SESSION);if(!open_session()){pause_ms(30000);continue;}report_boot();while(sync_now()){
   retry_panel();offer_ready=deadline>ms()&&publish_offer();process_control();
   stage(STAGE_IDLE);pause_ms(15000);
  }}
}
