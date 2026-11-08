/*
 * Standalone BiomeRTE display provider. Public HTTPS transport only.
 * The Pi is a commissioning/flashing convenience, not a runtime dependency.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "panel.h"
#include "idle_guard.h"
#include "diagnostics.h"
#include "bm_protocol.h"
#include "esp_private/startup_internal.h"
#include "hal/modem_syscon_ll.h"

#define VERSION "biome-monitor/2.0.1"
#define CONTRACT "display.epaper.frame:1"
#define RESPONSE_MAX 90000
#define FRAME_SIZE 48000
static const char *TAG = "biome-monitor";
static nvs_handle_t config_nv, meta_nv;
static cJSON *config, *meta;
static EventGroupHandle_t wifi_events;
static uint8_t *frame;
static bool panel_ok, panel_configured, blocked, splash_shown, diagnostic_visible;
static diagnostic_display diagnostic;
static int64_t last_panel_try, last_wifi_try, frame_expiry, last_success;
static unsigned failures;
static char error_text[64] = "Starting";
static char boot_id[33];
extern const uint8_t idle_logo[] asm("_binary_idle_logo_bin_start");

ESP_SYSTEM_INIT_FN(biome_radio_boot_reset, CORE, BIT(0), 2) {
    modem_syscon_ll_reset_wifimac(&MODEM_SYSCON);
    return ESP_OK;
}
static cJSON *get(const cJSON *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o,k); }
static const char *str(const cJSON *o,const char *k) {
    const cJSON *v=get(o,k); return cJSON_IsString(v)?v->valuestring:"";
}
static double number(const cJSON *o,const char *k) {
    const cJSON *v=get(o,k); return cJSON_IsNumber(v)?v->valuedouble:0;
}
static bool eq(const cJSON *o,const char *k,const char *v) {return strcmp(str(o,k),v)==0;}
static void set(cJSON *o,const char *k,cJSON *v) {cJSON_DeleteItemFromObjectCaseSensitive(o,k);cJSON_AddItemToObject(o,k,v);}
static void ps(cJSON *o,const char *k,const char *v) {set(o,k,cJSON_CreateString(v));}
static void pn(cJSON *o,const char *k,double v) {set(o,k,cJSON_CreateNumber(v));}
static int64_t ms(void) {return esp_timer_get_time()/1000;}
static void nonce(char out[33]) {uint8_t b[16];esp_fill_random(b,16);for(int i=0;i<16;i++)sprintf(out+2*i,"%02x",b[i]);out[32]=0;}
static void hash_bytes(const void *p,size_t n,char out[65]) {
    uint8_t h[32];mbedtls_sha256(p,n,h,0);for(int i=0;i<32;i++)sprintf(out+2*i,"%02x",h[i]);out[64]=0;
}
static void fail(const char *why) {snprintf(error_text,sizeof error_text,"%s",why);failures++;ESP_LOGW(TAG,"%s",why);}
static void halt(const char *why) {
    fail(why);blocked=true;
    /* Do not erase an uncertain effect or overwrite the retained physical image. */
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
}
static void persist(nvs_handle_t nv,const char *key,const cJSON *value) {
    char *s=bm_canonical(value);if(!s)halt("Journal encoding failed");
    size_t n=strlen(s)+1;
    if(n>12000)halt("Journal record too large");
    esp_err_t e=nvs_set_str(nv,key,s);if(e==ESP_OK)e=nvs_commit(nv);
    char *check=malloc(n);size_t size=n;
    if(!check)halt("Journal readback memory");
    if(e==ESP_OK)e=nvs_get_str(nv,key,check,&size);
    bool valid=e==ESP_OK&&size==n&&!memcmp(s,check,n);
    free(check);free(s);if(!valid)halt("Journal write/readback failed");
}
static cJSON *load(nvs_handle_t nv,const char *key) {
    size_t n=0;esp_err_t e=nvs_get_str(nv,key,NULL,&n);
    if(e==ESP_ERR_NVS_NOT_FOUND)return NULL;
    if(e!=ESP_OK||n<2||n>12000)halt("Journal/configuration invalid");
    char *s=malloc(n);if(!s)halt("Journal read memory");
    if(nvs_get_str(nv,key,s,&n)!=ESP_OK)halt("Journal/configuration read failed");
    cJSON *v=cJSON_ParseWithLength(s,n);free(s);if(!v)halt("Stored JSON invalid");
    return v;
}
static void save_meta(void) {persist(meta_nv,"state",meta);}
/* A single serialized effect may be unresolved. Completed controller receipts
 * prevent old IDs from becoming claimed again; keep only current local evidence. */
static cJSON *read_record(const char *id) {
    cJSON *r=get(meta,"current_record");
    return r&&eq(r,"id",id)?cJSON_Duplicate(r,true):NULL;
}
static void save_record(cJSON *r) {
    const cJSON *prior=get(meta,"current_record");
    if(prior&&(eq(prior,"state","uncertain")||!cJSON_IsTrue(get(prior,"controller_settled")))&&!eq(prior,"id",str(r,"id")))
        halt("Cannot replace unresolved effect");
    set(meta,"current_record",cJSON_Duplicate(r,true));
    ps(meta,"last_operation",str(r,"id"));save_meta();
}
static void fallback(void) {
    if(blocked)return;
    if(!panel_configured) {
        panel_pins pins={2,6,3,7,4,5};
        if(panel_init(pins)!=ESP_OK)halt("Panel initialization failed");
        panel_configured=true;panel_ok=panel_probe()==ESP_OK;last_panel_try=ms();
    }
    if(!panel_ok&&ms()-last_panel_try>=60000) {
        last_panel_try=ms();panel_ok=panel_probe()==ESP_OK;
    }
    bool failing=error_text[0]!=0;
    if(panel_ok&&diagnostic_due(&diagnostic,ms(),failing,false)) {
        uint8_t *image=malloc(FRAME_SIZE);
        if(image) {
            char version[49],error[49],age[49];
            snprintf(version,sizeof version,"%s",VERSION);
            snprintf(error,sizeof error,"%.48s",error_text);
            snprintf(age,sizeof age,"Last sync: %lld seconds ago",(long long)((ms()-last_success)/1000));
            const char *lines[]={"BIOME - LOCAL DIAGNOSTICS",version,error,
                "Standalone public gateway over Wi-Fi",age,"Automatic recovery retries continue",
                "No Pi LAN connection is required"};
            diagnostic_render(image,lines,7);
            if(panel_render(image)==ESP_OK){diagnostic_shown(&diagnostic,ms());diagnostic_visible=true;splash_shown=false;}
            else{panel_ok=false;last_panel_try=ms();}
            free(image);
        }
    }
    if(diagnostic_visible) {
        if(failing)return;
        diagnostic_visible=false;frame_expiry=0;
    }
    if(panel_ok&&!splash_shown&&(!frame_expiry||ms()>=frame_expiry)) {
        if(panel_render(idle_logo)==ESP_OK){splash_shown=true;frame_expiry=0;}
        else{panel_ok=false;last_panel_try=ms();fail("Panel refresh failed");}
    }
}
static void pause_ms(unsigned duration) {
    int64_t until=ms()+duration;do {
        fallback();vTaskDelay(pdMS_TO_TICKS(200));
    }while(ms()<until);
}
static void network_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)data;
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED)xEventGroupClearBits(wifi_events,1);
    if(base==IP_EVENT&&id==IP_EVENT_STA_LOST_IP)xEventGroupClearBits(wifi_events,1);
    if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP)xEventGroupSetBits(wifi_events,1);
}
static void network_start(void) {
    wifi_events=xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,network_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,network_event,NULL));
    wifi_config_t w={0};
    memcpy(w.sta.ssid,str(config,"ssid"),strlen(str(config,"ssid")));
    memcpy(w.sta.password,str(config,"password"),strlen(str(config,"password")));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_connect());
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");
    esp_sntp_setservername(1,"time.google.com");esp_sntp_init();
}
static bool network_ready(void) {
    if(!(xEventGroupGetBits(wifi_events)&1)) {
        if(ms()-last_wifi_try>=15000){last_wifi_try=ms();esp_wifi_connect();}
        fail("Wi-Fi/IP unavailable");return false;
    }
    if(time(NULL)<1700000000){fail("Waiting for network time");return false;}
    return true;
}
/* Gateway accepts the unchanged signed native envelope and returns {result}.
 * Certificate bundle validation stays enabled; redirects are never followed.
 */
typedef struct { char *data; size_t used; bool overflow; } response_buffer;
static esp_http_client_handle_t gateway_http;
static esp_err_t gateway_event(esp_http_client_event_t *event) {
    response_buffer *r=event->user_data;
    if(event->event_id==HTTP_EVENT_ON_DATA&&r&&event->data_len>0) {
        if((size_t)event->data_len>RESPONSE_MAX-r->used){r->overflow=true;return ESP_FAIL;}
        memcpy(r->data+r->used,event->data,event->data_len);r->used+=(size_t)event->data_len;
    }
    return ESP_OK;
}
static cJSON *transport(const cJSON *body,const char *path,int *http_status) {
    *http_status=0;
    if(!network_ready())return NULL;
    char *wire=bm_canonical(body);if(!wire){fail("Request encoding failed");return NULL;}
    char url[256];snprintf(url,sizeof url,"%s%s",str(config,"origin"),path);
    if(!gateway_http) {
        esp_http_client_config_t hc={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,
            .timeout_ms=45000,.disable_auto_redirect=true,.buffer_size=2048,.buffer_size_tx=2048,
            .keep_alive_enable=true,.event_handler=gateway_event};
        gateway_http=esp_http_client_init(&hc);
    }
    if(!gateway_http){free(wire);fail("HTTP allocation failed");return NULL;}
    response_buffer response={.data=malloc(RESPONSE_MAX+1)};
    if(!response.data){free(wire);fail("Response allocation failed");return NULL;}
    esp_err_t setup=esp_http_client_set_user_data(gateway_http,&response);
    if(setup==ESP_OK)setup=esp_http_client_set_url(gateway_http,url);
    if(setup==ESP_OK)setup=esp_http_client_set_method(gateway_http,HTTP_METHOD_POST);
    if(setup==ESP_OK)setup=esp_http_client_set_header(gateway_http,"Content-Type","application/json");
    if(setup==ESP_OK)setup=esp_http_client_set_post_field(gateway_http,wire,(int)strlen(wire));
    /* perform consumes the complete response and preserves an eligible HTTP/1.1
     * connection. On uncertainty discard it; rpc replays the same signed ID. */
    esp_err_t e=setup==ESP_OK?esp_http_client_perform(gateway_http):setup;
    int status=esp_http_client_get_status_code(gateway_http);
    *http_status=status;
    cJSON *out=NULL;
    bool complete=e==ESP_OK&&esp_http_client_is_complete_data_received(gateway_http);
    if(complete&&!response.overflow) {
        response.data[response.used]=0;
        if(status!=202)out=cJSON_ParseWithLength(response.data,response.used+1);
        if(!out&&status>=400&&status<500&&status!=408&&status!=429) {
            out=cJSON_CreateObject();pn(out,"_rpc_error",status);
        }
    }
    esp_http_client_set_user_data(gateway_http,NULL);
    esp_http_client_set_post_field(gateway_http,NULL,0);
    free(response.data);free(wire);
    if(!complete||response.overflow) {
        esp_http_client_cleanup(gateway_http);gateway_http=NULL;
    }
    if(!out){if(status!=202)fail("Gateway response unavailable");return NULL;}
    if(status==200){last_success=ms();failures=0;error_text[0]=0;}
    else if(status!=202){snprintf(error_text,sizeof error_text,"Gateway HTTP %d",status);failures++;}
    return out;
}
/* Forward the existing native protocol unchanged. Every retry uses the same
 * journaled request ID and signature; controller receipts deduplicate writes. */
static cJSON *exchange(const cJSON *request) {
    int status=0;cJSON *root=transport(request,"/v1/call",&status),*out=NULL;
    if(root) {
        if(status==200&&get(root,"result"))out=cJSON_DetachItemFromObjectCaseSensitive(root,"result");
        else if(status>=400&&status<500&&status!=408&&status!=429) {
            out=cJSON_CreateObject();pn(out,"_rpc_error",status);
        }
        cJSON_Delete(root);
    }
    return out;
}
static cJSON *new_request(const char *op,cJSON *args) {
    cJSON *q=cJSON_CreateObject();char id[33];nonce(id);
    ps(q,"id",id);ps(q,"operation",op);ps(q,"realm","site");
    ps(q,"site",str(config,"site"));set(q,"arguments",args);
    if(!bm_sign_request(q,str(config,"private_key"),str(config,"provider"))){cJSON_Delete(q);halt("Signing failed");}
    return q;
}
static cJSON *rpc(const char *op,cJSON *args,bool mutation) {
    if(get(meta,"pending")){cJSON_Delete(args);return NULL;}
    cJSON *q=new_request(op,args);
    (void)mutation;set(meta,"pending",cJSON_Duplicate(q,true));save_meta();
    cJSON *r=exchange(q);cJSON_Delete(q);
    if(r){cJSON_DeleteItemFromObjectCaseSensitive(meta,"pending");save_meta();}
    return r;
}
static bool replay(void) {
    cJSON *pending=get(meta,"pending");if(!pending)return true;
    cJSON *r=exchange(pending);if(!r)return false;
    cJSON_Delete(r);cJSON_DeleteItemFromObjectCaseSensitive(meta,"pending");save_meta();return true;
}
static cJSON *provider_args(void) {cJSON *a=cJSON_CreateObject();ps(a,"provider",str(config,"provider"));return a;}
/* Inbox reads have no effect to recover across power loss. Keep the exact
 * signed request in RAM for transport retries instead of wearing NVS each second. */
static cJSON *poll_request;
static cJSON *provider_poll(void) {
    if(get(meta,"pending"))return NULL;
    if(!poll_request)poll_request=new_request("provider.poll",provider_args());
    cJSON *r=exchange(poll_request);
    if(r){cJSON_Delete(poll_request);poll_request=NULL;}
    return r;
}
static bool identity(const cJSON *a) {
    return eq(a,"provider",str(config,"provider"))&&eq(a,"resource",str(config,"resource"))&&
        eq(a,"guard",str(config,"resource"))&&eq(a,"contract",CONTRACT)&&
        bm_json_integers_only(a)&&number(a,"generation")>=1&&strlen(str(a,"lease"))==64&&
        strlen(str(a,"id"))>0&&strlen(str(a,"id"))<=100;
}
static char *action_signature(const cJSON *a) {
    cJSON *v=cJSON_CreateArray();
    cJSON_AddItemToArray(v,cJSON_CreateString(str(a,"contract")));
    cJSON_AddItemToArray(v,cJSON_Duplicate(get(a,"inputs"),true));
    cJSON_AddItemToArray(v,cJSON_CreateString(str(a,"resource")));
    cJSON_AddItemToArray(v,cJSON_CreateNumber(number(a,"generation")));
    cJSON_AddItemToArray(v,cJSON_CreateString(str(a,"lease")));
    char *s=bm_canonical(v);cJSON_Delete(v);if(!s)return NULL;
    char *h=malloc(65);if(h)hash_bytes(s,strlen(s),h);free(s);return h;
}
static bool report(cJSON *record) {
    cJSON *a=provider_args();ps(a,"operation",str(record,"id"));
    pn(a,"generation",number(record,"generation"));ps(a,"state",str(record,"state"));
    set(a,"result",cJSON_Duplicate(get(record,"result"),true));
    cJSON *r=rpc("provider.result",a,true);bool ok=r&&eq(r,"state",str(record,"state"));cJSON_Delete(r);
    if(ok&&!eq(record,"state","uncertain")){set(record,"controller_settled",cJSON_CreateBool(true));save_record(record);}
    return ok;
}
static bool reconcile(void) {
    const char *id=str(meta,"last_operation");if(!*id){blocked=false;return true;}
    cJSON *r=read_record(id);if(!r){halt("Missing operation journal");}
    if(!eq(r,"state","uncertain")){
        blocked=false;bool settled=cJSON_IsTrue(get(r,"controller_settled"))||report(r);
        cJSON_Delete(r);return settled;
    }
    blocked=true;cJSON *a=cJSON_CreateObject();ps(a,"operation",id);
    cJSON *current=rpc("cap.status",a,false);
    if(current&&identity(current)) {
        char *sig=action_signature(current);
        cJSON *result=get(current,"result");
        bool settled=eq(current,"state","succeeded")||eq(current,"state","failed");
        bool quiescent=cJSON_IsTrue(get(get(result,"evidence"),"quiescent"))||
                       cJSON_IsTrue(get(get(result,"reconciliation"),"quiescent"));
        if(sig&&eq(r,"signature",sig)&&settled&&quiescent) {
            ps(r,"state",str(current,"state"));set(r,"result",cJSON_Duplicate(result,true));set(r,"controller_settled",cJSON_CreateBool(true));save_record(r);blocked=false;
        }
        free(sig);
    }
    cJSON_Delete(current);
    if(blocked)report(r);
    cJSON_Delete(r);return !blocked;
}
static bool advertise(void) {
    cJSON *a=provider_args();ps(a,"contract",CONTRACT);ps(a,"resource",str(config,"resource"));
    ps(a,"service","biome-monitor");ps(a,"state",panel_ok&&!blocked?"ready":"offline");pn(a,"capacity",1);
    cJSON *info=cJSON_CreateObject();ps(info,"firmware",VERSION);ps(info,"boot_id",boot_id);
    ps(info,"format","mono1-msb-black1");pn(info,"width",800);pn(info,"height",480);
    set(a,"configuration",info);
    cJSON *r=rpc("cap.offer",a,true);bool ok=r&&eq(r,"contract",CONTRACT);cJSON_Delete(r);return ok;
}
static bool fence(const cJSON *l) {
    if(!eq(l,"provider",str(config,"provider"))||!eq(l,"resource",str(config,"resource"))||
       !eq(l,"guard",str(config,"resource"))||!eq(l,"contract",CONTRACT)||
       !bm_json_integers_only(l)||number(l,"generation")<1||strlen(str(l,"id"))!=64)return false;
    if(!reconcile())return false;
    cJSON *old=get(meta,"fence");double gen=number(l,"generation");
    if(old&&(number(old,"generation")>gen||
       (number(old,"generation")==gen&&!eq(old,"lease",str(l,"id")))))return false;
    cJSON *f=cJSON_CreateObject();pn(f,"generation",gen);ps(f,"lease",str(l,"id"));set(meta,"fence",f);save_meta();
    cJSON *a=provider_args();pn(a,"generation",gen);ps(a,"lease",str(l,"id"));
    cJSON *r=rpc("cap.fenced",a,true);bool ok=r&&eq(r,"state","bound");cJSON_Delete(r);return ok;
}
static cJSON *claim(const cJSON *action) {
    cJSON *a=provider_args();ps(a,"operation",str(action,"id"));pn(a,"generation",number(action,"generation"));
    return rpc("provider.claim",a,true);
}
static bool valid_sha(const char *s) {
    if(strlen(s)!=64)return false;
    for(unsigned i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
static void reject_action(const cJSON *action,const char *signature,const char *reason) {
    cJSON *claimed=claim(action);
    if(!claimed||!eq(claimed,"state","claimed")){cJSON_Delete(claimed);return;}
    cJSON_Delete(claimed);
    cJSON *r=cJSON_CreateObject();ps(r,"id",str(action,"id"));ps(r,"signature",signature);
    pn(r,"generation",number(action,"generation"));ps(r,"state","failed");
    cJSON *result=cJSON_CreateObject();ps(result,"reason",reason);
    cJSON *e=cJSON_CreateObject();cJSON_AddBoolToObject(e,"quiescent",true);cJSON_AddBoolToObject(e,"executed",false);
    set(result,"evidence",e);set(r,"result",result);save_record(r);report(r);cJSON_Delete(r);
}
static bool download_frame(const char *sha) {
    cJSON *a=cJSON_CreateObject();ps(a,"sha256",sha);
    cJSON *r=rpc("artifact.get",a,false);if(!r)return false;
    size_t n=0;const char *data=str(r,"data");
    bool ok=eq(r,"id",sha)&&strlen(data)==64000&&
        mbedtls_base64_decode(frame,FRAME_SIZE,&n,(const unsigned char*)data,strlen(data))==0&&n==FRAME_SIZE;
    char actual[65];if(ok){hash_bytes(frame,n,actual);ok=!strcmp(actual,sha);}
    cJSON_Delete(r);if(!ok)fail("Frame integrity check failed");return ok;
}
static void perform(cJSON *action) {
    if(!identity(action)||!reconcile())return;
    cJSON *f=get(meta,"fence");
    if(!f||number(f,"generation")!=number(action,"generation")||!eq(f,"lease",str(action,"lease")))return;
    char *sig=action_signature(action);if(!sig)return;
    cJSON *record=read_record(str(action,"id"));
    if(record) {
        if(!eq(record,"signature",sig))halt("Operation identity conflict");
        report(record);cJSON_Delete(record);free(sig);return;
    }
    const cJSON *input=get(action,"inputs");
    const char *sha=str(input,"sha256");
    double expiry=number(input,"expires_at_ms");
    bool valid=cJSON_IsObject(input)&&valid_sha(sha)&&eq(input,"format","mono1-msb-black1")&&
        number(input,"width")==800&&number(input,"height")==480&&number(input,"stride_bytes")==100&&
        number(input,"size_bytes")==FRAME_SIZE&&expiry>=(double)time(NULL)*1000+60000&&
        expiry<=(double)time(NULL)*1000+86400000;
    if(!valid){reject_action(action,sig,"Invalid or expired frame specification");free(sig);fail("Rejected frame specification");return;}
    cJSON *claimed=claim(action);
    if(!claimed||!eq(claimed,"state","claimed")||!identity(claimed)){cJSON_Delete(claimed);free(sig);return;}
    char *claimed_sig=action_signature(claimed);
    bool same=claimed_sig&&!strcmp(sig,claimed_sig);free(claimed_sig);cJSON_Delete(claimed);
    if(!same){free(sig);halt("Claim binding mismatch");}
    if(!panel_ok||!download_frame(sha)){free(sig);return;}
    /* A second fresh claim validates current authority after the potentially slow download. */
    claimed=claim(action);same=claimed&&eq(claimed,"state","claimed")&&identity(claimed);
    claimed_sig=claimed?action_signature(claimed):NULL;same=same&&claimed_sig&&!strcmp(sig,claimed_sig);
    free(claimed_sig);cJSON_Delete(claimed);
    if(!same){free(sig);return;}
    if(expiry<(double)time(NULL)*1000+30000){reject_action(action,sig,"Frame expired before render");free(sig);return;}
    record=cJSON_CreateObject();ps(record,"id",str(action,"id"));ps(record,"signature",sig);free(sig);
    pn(record,"generation",number(action,"generation"));ps(record,"state","uncertain");
    cJSON *result=cJSON_CreateObject();ps(result,"reason","Panel effect may have occurred; reconcile before retry");
    set(record,"result",result);save_record(record);
    blocked=true; /* No fallback can overwrite an unresolved effect. */
    esp_err_t e=panel_render(frame);
    if(e==ESP_OK) {
        result=cJSON_CreateObject();ps(result,"status","rendered");ps(result,"sha256",sha);
        ps(result,"firmware",VERSION);ps(result,"boot_id",boot_id);
        cJSON *evidence=cJSON_CreateObject();cJSON_AddBoolToObject(evidence,"quiescent",true);
        cJSON_AddBoolToObject(evidence,"controller_refresh_completed",true);
        cJSON_AddBoolToObject(evidence,"optical_readback",false);set(result,"evidence",evidence);
        ps(record,"state","succeeded");set(record,"result",result);save_record(record);
        blocked=false;splash_shown=false;diagnostic_visible=false;
        frame_expiry=ms()+(int64_t)(expiry-(double)time(NULL)*1000);
        ESP_LOGW(TAG,"BiomeRTE frame rendered sha256=%s",sha);
    } else {panel_ok=false;fail("Panel outcome uncertain");}
    report(record);cJSON_Delete(record);
}
void app_main(void) {
    /* Seengreat v1.4 D2 user LED is active-low on GPIO8. Leave charge LEDs alone. */
    gpio_reset_pin(GPIO_NUM_8);gpio_set_level(GPIO_NUM_8,1);gpio_set_direction(GPIO_NUM_8,GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(biome_idle_guard_install());
    vTaskDelay(pdMS_TO_TICKS(esp_reset_reason()==ESP_RST_BROWNOUT?5000:2000));
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nvs_open("biomerte",NVS_READONLY,&config_nv));
    config=load(config_nv,"config");if(!config)halt("BiomeRTE provisioning required");
    const char *origin=str(config,"origin");
    if(strncmp(origin,"https://",8)||strlen(origin)>210||strchr(origin+8,'/')||
       strchr(origin,'@')||strchr(origin,'?')||strchr(origin,'#')||
       !valid_sha(str(config,"provider"))||strlen(str(config,"resource"))>150||
       !*str(config,"site")||!*str(config,"resource")||!*str(config,"private_key")||
       !*str(config,"ssid")||strlen(str(config,"ssid"))>32||strlen(str(config,"password"))>64)
        halt("Invalid BiomeRTE configuration");
    ESP_ERROR_CHECK(nvs_flash_init_partition("runtime"));
    ESP_ERROR_CHECK(nvs_open_from_partition("runtime","biomerte_meta",NVS_READWRITE,&meta_nv));
    meta=load(meta_nv,"state");if(!meta)meta=cJSON_CreateObject();
    char authority[400];snprintf(authority,sizeof authority,"%s/site/%s/%s",str(config,"site"),str(config,"provider"),str(config,"resource"));
    if(*str(meta,"authority")&&!eq(meta,"authority",authority))halt("Journal authority mismatch");
    ps(meta,"authority",authority);save_meta();nonce(boot_id);
    cJSON *last=*str(meta,"last_operation")?read_record(str(meta,"last_operation")):NULL;
    blocked=last&&eq(last,"state","uncertain");cJSON_Delete(last);
    frame=malloc(FRAME_SIZE);if(!frame)halt("Frame allocation failed");
    fallback();network_start();
    ESP_LOGW(TAG,"%s provider=%s resource=%s",VERSION,str(config,"provider"),str(config,"resource"));
    int64_t advertised=0;
    for(;;) {
        if(!replay()){pause_ms(3000);continue;}
        reconcile();
        if(ms()-advertised>60000||!advertised) {
            if(advertise())advertised=ms();
            if(get(meta,"pending")){pause_ms(3000);continue;}
        }
        cJSON *queue=provider_poll();
        if(queue) {
            cJSON *item;
            cJSON_ArrayForEach(item,get(queue,"fences")) {
                fence(item);if(get(meta,"pending"))break;
            }
            if(!get(meta,"pending")&&!blocked)cJSON_ArrayForEach(item,get(queue,"actions")) {
                perform(item);if(get(meta,"pending")||blocked)break;
            }
            cJSON_Delete(queue);
        }
        pause_ms(1000);
    }
}
