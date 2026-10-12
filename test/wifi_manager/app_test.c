/* Real app + real scene + real shared password host; fake radio/storage only. */
#define main scene_baseline_main
#define risc_runtime_get_api fixture_runtime_get_api
#include "../scene/host_test.c"
#undef main
#undef risc_runtime_get_api
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t);
#include "RiscTextEntryV1.h"
#define WIFI_MANAGER_TEST
#include "../../Apps/wifi_manager/app.c"
extern const risc_driver_v2 *text_driver_get(uint32_t);
static const risc_driver_v2 *td;
static const risc_text_entry_api_v1 *text_api;
static unsigned active_grants,retains,joins,scans,cancels,kv_writes,db_busy,stop_delay;
static unsigned radio_ticks,radio_kind,radio_id=0;static bool stop_requested,radio_fail,old_scene,missing_text,io_fail,bad_progress;
static char sent_ssid[33],sent_password[64];
static const char *loss;
static void edge(const char *site){assert(native_alive);if(loss&&!strcmp(loss,site))native_alive=false;}
typedef struct {char key[16];uint8_t bytes[64];uint32_t size;unsigned ns;} stored;
static stored storage[80];static unsigned storage_count;
static int32_t kv_get(void *ctx,const char *key,void *buf,uint32_t cap,uint32_t *size){
    edge("storage-get");*size=0;if(db_busy){db_busy--;return RISC_KEY_VALUE_BUSY;}if(io_fail)return RISC_KEY_VALUE_IO;
    for(unsigned i=0;i<storage_count;i++)if(storage[i].ns==(unsigned)(uintptr_t)ctx&&!strcmp(storage[i].key,key)){
        *size=storage[i].size;if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(buf,storage[i].bytes,*size);return 0;}
    return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t kv_put(void *ctx,const char *key,const void *buf,uint32_t size){
    edge("storage-put");if(io_fail)return RISC_KEY_VALUE_IO;assert(size<=64);kv_writes++;
    unsigned i;for(i=0;i<storage_count;i++)if(storage[i].ns==(unsigned)(uintptr_t)ctx&&!strcmp(storage[i].key,key))break;
    if(i==storage_count){assert(storage_count<80);storage_count++;}strcpy(storage[i].key,key);storage[i].ns=(unsigned)(uintptr_t)ctx;memcpy(storage[i].bytes,buf,size);storage[i].size=size;return 0;
}
static const risc_key_value_v1 kv={1,sizeof(kv),(void*)6,kv_get,kv_put},pkv={1,sizeof(pkv),(void*)1,kv_get,kv_put};
static int32_t begin_radio(void *c,const risc_radio_request_v1 *r,uint32_t *token){
    (void)c;edge("begin");assert(!radio_id);radio_id++;*token=radio_id;radio_kind=r->kind;radio_ticks=0;stop_requested=false;
    if(r->kind==1)scans++;else{joins++;strcpy(sent_ssid,r->ssid);strcpy(sent_password,r->password);}return 0;
}
static int32_t poll_radio(void *c,uint32_t id,risc_radio_progress_v1 *p){
    (void)c;edge("radio-poll");assert(id==radio_id&&id);*p=(risc_radio_progress_v1){.struct_size=sizeof(*p),.operation_id=id,.owned=1,.phase=RISC_RADIO_STARTING,.scan={.struct_size=sizeof(p->scan)}};
    radio_ticks++;if(bad_progress){p->reserved[0]=1;return 1;}
    if(stop_requested){if(stop_delay){stop_delay--;p->phase=RISC_RADIO_STOPPING;return 1;}radio_id=0;p->owned=0;p->quiescent=1;p->phase=RISC_RADIO_IDLE;p->failure=RISC_RADIO_FAILURE_CANCELLED;return 2;}
    if(radio_fail){p->owned=0;p->quiescent=1;p->phase=RISC_RADIO_IDLE;p->failure=RISC_RADIO_FAILURE_LINK;radio_id=0;return 2;}
    if(radio_kind==1){
        if(radio_ticks<3){p->phase=RISC_RADIO_SCANNING;p->scan.state=1;}
        else{p->phase=RISC_RADIO_RESULTS;p->scan.state=2;p->scan.count=4;
            p->scan.entries[0]=(garden_radio_scan_entry_v1){"Nova Home",-48,6,2,0};
            p->scan.entries[1]=(garden_radio_scan_entry_v1){"CoffeeShop_Guest",-64,11,0,0};
            p->scan.entries[2]=(garden_radio_scan_entry_v1){"Office-Secure",-74,9,255,0};
            p->scan.entries[3]=(garden_radio_scan_entry_v1){"FarAway-AP",-90,9,2,0};}
    }else{p->phase=radio_ticks<5?RISC_RADIO_JOINING:RISC_RADIO_CONNECTED;p->station_state=radio_ticks<5?1:2;p->rssi=-48;
        const uint8_t ip[]={192,168,1,40,192,168,1,1,255,255,255,0};memcpy(p->station,ip,12);}
    return 1;
}
static int32_t cancel_radio(void *c,uint32_t id){(void)c;edge("cancel");assert(id==radio_id);stop_requested=true;cancels++;return 1;}
static int32_t lease_begin(void *c,uint32_t *token){(void)c;edge("lease-begin");*token=77;return 0;}
static int32_t lease_end(void *c,uint32_t token){(void)c;edge("lease-end");assert(token==77);return 0;}
static const wifi_async_v1 rf={.base={.api_version=1,.struct_size=sizeof(rf)},.async_tag=RISC_RADIO_ASYNC_TAG,.async_version=1,.begin=begin_radio,.poll=poll_radio,.cancel=cancel_radio,.service_begin=lease_begin,.service_end=lease_end};
static risc_scene_telemetry_api_v1 old_api;
static bool rt_acquire(const char *cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
    edge("acquire");assert(version==1);const void *value=NULL;
    if(!strcmp(cap,"ui.scene"))value=old_scene?(void*)&old_api:(void*)api;
    else if(!strcmp(cap,"ui.text-input")){if(missing_text)return false;value=text_api;}
    else if(!strcmp(cap,"net.wifi")){assert(instance==15);value=&rf;}
    else if(!strcmp(cap,"storage.key-value")){assert(instance==6||instance==1);value=instance==6?&kv:&pkv;}
    else assert(!"unexpected app capability");g->api=value;g->slot=++active_grants;g->generation=1;return true;
}
static bool rt_release(risc_runtime_capability_v1 *g){edge("release");assert(active_grants&&g->api);active_grants--;g->api=NULL;return true;}
static bool rt_clock(uint32_t *t){edge("clock");*t=(uint32_t)ms;return true;}
static bool rt_retain(void){retains++;native_alive=false;return true;}
static void rt_yield(uint32_t n){ms+=n;}
static bool rt_diag(const char *msg){assert(!strstr(msg,"password123"));return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){
    static const risc_runtime_api_v1 r={.api_version=1,.struct_size=sizeof(r),.yield_ms=rt_yield,.diagnostic=rt_diag,.acquire=rt_acquire,.release=rt_release,.retain_invocation=rt_retain,.monotonic_ms=rt_clock};
    return version==1&&native_alive?&r:NULL;
}
static void pump(unsigned n){for(unsigned i=0;i<n;i++){ms+=20;if(!wifi_manager_test_step())break;}}
static void invoke(unsigned id,int v){wifi_manager_test_action(id,v);pump(15);assert(!wm.retained);}
static void capture(const char *dir,const char *name){
    bool ready=false;
    for(unsigned tries=0;tries<2500;tries++){
        pump(1);assert(native_alive&&!wm.retained);
        uint32_t flags=RISC_SCENE_PRESENTING;
        if(wm.session){risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};assert(!api->snapshot(NULL,wm.session,&nav,&flags));}
        else if(wm.editor_session){risc_text_entry_state_v1 state={.struct_size=sizeof(state)};assert(!text_api->poll(NULL,wm.editor_session,&state));flags=(state.flags&RISC_TEXT_ENTRY_PRESENTING)?RISC_SCENE_PRESENTING:0;}
        if(!held&&!presenting&&!wm.dirty&&!wm.keyboard_pending&&!(flags&RISC_SCENE_PRESENTING)){ready=true;break;}
    }
    assert(ready);
    if(!dir)return;char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");
    for(unsigned yy=0;yy<800;yy++)for(unsigned bx=0;bx<60;bx++){unsigned b=0;for(unsigned k=0;k<8;k++){unsigned x=bx*8+k,px=yy,py=479-x;unsigned bit=(pixels[16+py*100+px/8]>>(7-px%8))&1u;b|=bit<<(7-k);}fputc((int)b,f);}fclose(f);
}
static void exit_app(void){wm.exit_requested=true;pump(200);assert(wm.finished&&!wm.retained);cleanup(&wm);assert(!active_grants&&!held&&!presenting&&!subscribed&&!radio_id);assert(driver->quiesce()&&td->quiesce());td->stop();driver->stop();}
int main(int argc,char **argv){
    assert(argc>=2);const char *mode=argv[1],*dir=argc>2?argv[2]:NULL;
    w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};
    startup();assert(!api->close(NULL,session));expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
    td=text_driver_get(2);assert(td);text_api=td->capability;risc_provider_dependency_v1 tddeps[]={ {"ui.scene",1,api} };assert(td->start(tddeps,1));
    old_api=*(const risc_scene_telemetry_api_v1*)api;((risc_scene_api_v1*)&old_api)->struct_size=sizeof(old_api);
    if(!strcmp(mode,"old-scene"))old_scene=true;
    if(!strcmp(mode,"missing-keyboard"))missing_text=true;
    if(!strcmp(mode,"storage-failure"))io_fail=true;
    if(!strcmp(mode,"busy-storage"))db_busy=5;
    assert(wifi_manager_test_init());pump(20);
    if(old_scene){assert(wm.upgrade);exit_app();puts("old scene: update message PASS");return 0;}
    if(io_fail){assert(wm.db_fault&&!wm.enabled);exit_app();puts("storage failure: no data overwritten PASS");return 0;}
    assert(wm.enabled);capture(dir,"disconnected");
    /* True touch on shared Scan action, rather than a mock render-only path. */
    touch_tap(410,58);pump(20);assert(scans==1&&wm.scan.count==4);capture(dir,"networks");
    if(!strcmp(mode,"unsupported")){invoke(SELECT_SCAN+2,0);invoke(CONNECT,0);assert(wm.screen==NOTICE&&!joins);exit_app();puts("enterprise: unsupported without radio join PASS");return 0;}
    if(!strcmp(mode,"cancel")){stop_delay=8;invoke(CANCEL,0);assert(!radio_id);exit_app();puts("asynchronous cancellation drained before release PASS");return 0;}
    if(!strcmp(mode,"loss")){loss="radio-poll";unsigned before=active_grants;pump(1);assert(!native_alive&&active_grants==before);puts("lost native invocation: no subsequent cleanup I/O PASS");return 0;}
    if(!strcmp(mode,"bad-progress")){bad_progress=true;unsigned before=active_grants;pump(1);assert(wm.retained&&active_grants==before);puts("invalid async snapshot: retained without unsafe cleanup PASS");return 0;}
    if(!strcmp(mode,"storage-loss")){loss="storage-get";wifi_manager_test_action(TAB,1);unsigned before=active_grants;pump(1);assert(wm.retained&&!native_alive&&active_grants==before);puts("revoked storage callback: reused codec stops subsequent I/O PASS");return 0;}
    if(!strcmp(mode,"hidden-network")){
        invoke(ADD_NETWORK,0);invoke(EDIT_SSID,0);assert(wm.editor_session&&!wm.session);capture(dir,"network-name");
        touch_tap(60,500);pump(10);touch_tap(410,688);pump(40);assert(!strcmp(wm.draft.ssid,"A"));
        invoke(SECURITY,0);invoke(SAVE_CONNECT,0);pump(100);assert(joins==1&&wm.link_up&&!strcmp(sent_ssid,"A")&&!sent_password[0]);
        portable_wifi_credentials got;assert(!portable_wifi_profile_load(&kv,0,&got)&&!strcmp(got.ssid,"A")&&!got.password[0]);wipe(&got,sizeof(got));
        exit_app();puts("hidden open SSID: existing text keyboard, connect then persist PASS");return 0;
    }
    invoke(SELECT_SCAN,0);assert(wm.screen==DETAIL);capture(dir,"details");invoke(CONNECT,0);
    if(missing_text){assert(wm.screen==NOTICE&&!joins);exit_app();puts("missing password keyboard: no plaintext fallback PASS");return 0;}
    assert(wm.editor_session&&!wm.session);capture(dir,"password");
    /* Existing shared keyboard: start on uppercase, cycle to lowercase. */
    touch_tap(60,598);pump(15);touch_tap(60,598);pump(15);
    /* a=lowercase row2 first; enter eight characters, then inspect masked host. */
    for(unsigned j=0;j<8;j++){touch_tap(60,500);pump(10);}capture(dir,"password-masked");
    risc_text_entry_state_v1 ts={.struct_size=sizeof(ts)};assert(!text_api->poll(NULL,wm.editor_session,&ts));assert(strlen(ts.text)==8);
    assert(ts.text[0]=='a');
    if(!strcmp(mode,"keyboard-cancel")){touch_tap(55,55);pump(100);assert(!wm.editor_session&&!joins&&!wm.draft.password[0]);capture(dir,"cancelled-password");exit_app();puts("shared keyboard cancellation discards draft without joining PASS");return 0;}
    uint32_t masked_hash=0;for(unsigned y=0;y<480;y++)for(unsigned x=0;x<38;x++)masked_hash=masked_hash*33+pixels[16+y*100+x];
    touch_tap(380,268);pump(15);capture(dir,"password-visible");
    uint32_t visible_hash=0;for(unsigned y=0;y<480;y++)for(unsigned x=0;x<38;x++)visible_hash=visible_hash*33+pixels[16+y*100+x];assert(masked_hash!=visible_hash);
    touch_tap(380,268);pump(15);capture(dir,"password-hidden");
    uint32_t hidden_hash=0;for(unsigned y=0;y<480;y++)for(unsigned x=0;x<38;x++)hidden_hash=hidden_hash*33+pixels[16+y*100+x];assert(masked_hash==hidden_hash);
    touch_tap(410,688);pump(70);assert(joins==1&&wm.link_up&&wm.screen==MAIN);assert(!strcmp(sent_ssid,"Nova Home")&&!strcmp(sent_password,"aaaaaaaa"));assert(!wm.draft.password[0]);capture(dir,"connected");
    portable_wifi_credentials saved_value;assert(portable_wifi_profile_load(&kv,0,&saved_value)==0&&!strcmp(saved_value.password,"aaaaaaaa"));wipe(&saved_value,sizeof(saved_value));
    if(!strcmp(mode,"failure")){radio_fail=true;pump(10);assert(wm.screen==FAILURE&&!wm.link_up);capture(dir,"failure");exit_app();puts("actual radio failure does not report fake success PASS");return 0;}
    invoke(SELECT_CURRENT,0);capture(dir,"connected-details");
    if(!strcmp(mode,"forget")){invoke(FORGET_ASK,0);capture(dir,"forget-confirmation");invoke(FORGET_YES,0);assert(portable_wifi_profile_load(&kv,0,&saved_value)==1);assert(!radio_id);exit_app();puts("forget: confirmation, disconnect and old codec tombstone PASS");return 0;}
    invoke(TAB,1);assert(wm.saved_rows==1);capture(dir,"saved");
    invoke(ADD_NETWORK,0);capture(dir,"add-network");
    unsigned writes=kv_writes;invoke(BACK,0);invoke(CANCEL,0);pump(250);unsigned frames=submits;pump(100);assert(submits==frames&&kv_writes==writes);
    exit_app();puts("native Wi-Fi app/shared scene/shared password touch, persistence, no idle repaint, lifecycle PASS");return 0;
}
