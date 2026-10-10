/* Production helper and profile codec; synthetic bounded provider/storage. */
#include "PortableNetworkSession.h"
#include <assert.h>
#include <stdio.h>

static struct { char key[16]; unsigned size; uint8_t data[64]; } cells[16];
static unsigned count, reads, writes, callbacks, status_calls, begins, polls, cancels, leases, ends;
static unsigned owner_calls, lose_owner_at, context_read, busy_read;
static bool alive, lose_after, malformed, no_ip;
static int begin_rc, poll_rc, cancel_rc, lease_rc, end_rc;
static uint32_t begin_id, lease_id;
static wifi_link_t link;
static risc_radio_progress_v1 progress;
static risc_radio_request_v1 submitted;
static portable_network_session session;
static bool owner_ok(void *c) { (void)c; ++owner_calls; if (owner_calls == lose_owner_at) alive = false; return alive; }
static void called(void) { assert(alive); ++callbacks; if (lose_after) alive = false; }
static int32_t get(void *c, const char *key, void *data, uint32_t cap, uint32_t *size) {
    (void)c; called(); ++reads; *size = 0;
    if (reads == context_read) return RISC_KEY_VALUE_CONTEXT;
    if (reads == busy_read) return RISC_KEY_VALUE_BUSY;
    for (unsigned i=0; i<count; ++i) if (!strcmp(cells[i].key,key)) {
        *size=cells[i].size; if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
        memcpy(data,cells[i].data,*size); return RISC_KEY_VALUE_OK;
    }
    return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put(void *c,const char *key,const void *data,uint32_t size) {
    (void)c; called(); ++writes; assert(size<=64);
    unsigned i; for(i=0;i<count;i++)if(!strcmp(cells[i].key,key))break;
    if(i==count){assert(count<16);++count;strcpy(cells[i].key,key);}
    cells[i].size=size; memcpy(cells[i].data,data,size); return RISC_KEY_VALUE_OK;
}
static wifi_link_t status(void *c){(void)c;called();++status_calls;return link;}
static int8_t rssi(void *c){(void)c;called();return -42;}
static bool addresses(void *c,wifi_ipv4_v1 *station,wifi_ipv4_v1 *ap){
    (void)c;assert(ap);called();*station=(wifi_ipv4_v1){{10,1,2,3},{10,1,2,1},{255,255,255,0}};
    if(no_ip)memset(station,0,sizeof(*station));
    return true;
}
static bool forbidden_connect(void*c,const char*s,const char*p){(void)c;(void)s;(void)p;assert(!"synchronous connect used");return false;}
static bool forbidden_stop(void*c){(void)c;assert(!"unowned prefix stop used");return false;}
static int32_t begin(void*c,const risc_radio_request_v1*r,uint32_t*id){(void)c;called();++begins;submitted=*r;*id=begin_id;return begin_rc;}
static int32_t poll(void*c,uint32_t id,risc_radio_progress_v1*out){(void)c;called();++polls;assert(id==session.operation);*out=progress;if(malformed)out->operation_id++;return poll_rc;}
static int32_t cancel(void*c,uint32_t id){(void)c;called();++cancels;assert(id==session.operation);return cancel_rc;}
static int32_t service_begin(void*c,uint32_t*id){(void)c;called();++leases;*id=lease_id;return lease_rc;}
static int32_t service_end(void*c,uint32_t id){(void)c;called();++ends;assert(id==session.service_lease);return end_rc;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static wifi_async_v1 wifi;
static void make_progress(uint32_t phase) {
    progress=(risc_radio_progress_v1){.struct_size=sizeof(progress),.operation_id=17,.phase=phase,
        .owned=phase!=RISC_RADIO_IDLE,.quiescent=phase==RISC_RADIO_IDLE,
        .station_state=phase==RISC_RADIO_CONNECTED?WIFI_LINK_UP:WIFI_LINK_DOWN,.rssi=-51,
        .scan={.struct_size=sizeof(garden_radio_scan_result_v1)}};
    if(phase==RISC_RADIO_CONNECTED){const uint8_t address[12]={192,168,4,5,192,168,4,1,255,255,255,0};memcpy(progress.station,address,12);}
    poll_rc=phase==RISC_RADIO_IDLE?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;
}
static bool zero(const void*p,size_t n){const unsigned char*b=p;while(n--)if(*b++)return false;return true;}
static void reset(bool saved) {
    memset(cells,0,sizeof(cells));count=reads=writes=callbacks=status_calls=begins=polls=cancels=leases=ends=0;
    owner_calls=lose_owner_at=context_read=busy_read=0;
    alive=true;lose_after=malformed=no_ip=false;link=WIFI_LINK_DOWN;
    begin_rc=lease_rc=end_rc=RISC_RADIO_ACCEPTED;cancel_rc=RISC_RADIO_PENDING;begin_id=17;lease_id=29;
    wifi=(wifi_async_v1){.base={.api_version=1,.struct_size=sizeof(wifi),.connect=forbidden_connect,
        .status=status,.rssi=rssi,.addresses=addresses,.disconnect_checked=forbidden_stop,.scan_cancel=forbidden_stop},
        .async_tag=RISC_RADIO_ASYNC_TAG,.async_version=1,.begin=begin,.poll=poll,.cancel=cancel,.service_begin=service_begin,.service_end=service_end};
    if(saved){portable_wifi_credentials value={"Fixture-default","synthetic-password"};assert(portable_wifi_profile_save(&kv,0,&value)==0);}
    reads=writes=callbacks=0;make_progress(RISC_RADIO_JOINING);memset(&submitted,0,sizeof(submitted));
    portable_network_session_init(&session,&wifi.base,&kv,owner_ok,NULL);
}
static void start(void){assert(portable_network_session_start(&session,0)==PORTABLE_NETWORK_PENDING);assert(!callbacks);}
static int step(void){unsigned before=callbacks;int rc=portable_network_session_step(&session,100);assert(callbacks-before<=4);return rc;}
static void to_submit(void){start();assert(step()==PORTABLE_NETWORK_PENDING);assert(step()==PORTABLE_NETWORK_PENDING);assert(step()==PORTABLE_NETWORK_PENDING);assert(reads==4&&!begins&&session.request.kind==RISC_RADIO_REQUEST_JOIN);}
static void accepted(void){to_submit();assert(step()==PORTABLE_NETWORK_PENDING&&begins==1&&session.operation==17);assert(zero(&session.request,sizeof(session.request)));}
static void terminal(void){make_progress(RISC_RADIO_IDLE);assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSED);assert(!session.operation&&!session.view.closing&&!session.view.retained);}
static void fenced(void){unsigned before=callbacks;assert(portable_network_session_step(&session,999)==PORTABLE_NETWORK_RETAINED);assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSE_RETAINED);assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_RETAINED);assert(portable_network_session_service_end(&session)==PORTABLE_NETWORK_SERVICE_RETAINED);assert(callbacks==before&&zero(&session.request,sizeof(session.request)));}
int main(void) {
    reset(true);accepted();assert(!strcmp(submitted.ssid,"Fixture-default")&&!strcmp(submitted.password,"synthetic-password"));
    assert(step()==PORTABLE_NETWORK_PENDING);make_progress(RISC_RADIO_CONNECTED);assert(step()==PORTABLE_NETWORK_READY);
    portable_network_snapshot copy;portable_network_session_snapshot(&session,&copy);assert(copy.link==WIFI_LINK_UP&&copy.ipv4.address[3]==5&&copy.rssi==-51&&!copy.borrowed);
    memset(&progress.station,0,sizeof(progress.station));assert(copy.ipv4.address[3]==5); /* snapshot owns bytes */
    assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSE_PENDING);assert(cancels==1);assert(step()==PORTABLE_NETWORK_PENDING&&cancels==1);terminal();assert(!writes);

    /* Slot zero only. Healthy external links work even with an empty store. */
    reset(false);portable_wifi_credentials other={"Other-profile","synthetic-secret"};assert(portable_wifi_profile_save(&kv,1,&other)==0);reads=writes=callbacks=0;start();assert(step()==1);assert(step()==PORTABLE_NETWORK_EMPTY);assert(!begins&&!writes);assert(portable_network_session_close(&session)==0);
    reset(false);link=WIFI_LINK_UP;start();assert(step()==PORTABLE_NETWORK_READY&&session.view.borrowed&&!reads&&!begins);
    assert(portable_network_session_service_begin(&session)==0&&session.service_lease==29);assert(portable_network_session_service_end(&session)==0);
    assert(portable_network_session_close(&session)==0&&!cancels&&link==WIFI_LINK_UP);
    reset(true);to_submit();link=WIFI_LINK_UP;begin_rc=RISC_RADIO_BUSY;begin_id=0;assert(step()==1);assert(step()==2&&session.view.borrowed&&!session.operation);assert(portable_network_session_close(&session)==0&&!cancels);
    reset(false);link=WIFI_LINK_JOINING;start();for(unsigned i=0;i<10;++i)assert(step()==1);assert(!reads&&!begins);assert(portable_network_session_close(&session)==0&&!cancels);
    reset(false);link=WIFI_LINK_UP;no_ip=true;start();assert(step()==1&&!session.view.borrowed);no_ip=false;assert(step()==2);link=WIFI_LINK_DOWN;assert(step()==PORTABLE_NETWORK_FAILED&&!begins);assert(portable_network_session_close(&session)==0&&!cancels);

    for(unsigned i=1;i<=4;++i){reset(true);start();assert(step()==1);busy_read=i;assert(step()==1&&!session.view.retained&&!session.request.kind);busy_read=0;assert(step()==1&&session.request.kind);assert(portable_network_session_close(&session)==0);assert(zero(&session.request,sizeof(session.request)));}
    for(unsigned i=1;i<=4;++i){reset(true);start();assert(step()==1);context_read=i;assert(step()==PORTABLE_NETWORK_RETAINED&&reads==i);fenced();}
    for(unsigned i=1;i<=4;++i){reset(true);start();assert(step()==1);/* entry owner + two owner calls per read */lose_owner_at=owner_calls+1+2*i;assert(step()==PORTABLE_NETWORK_RETAINED&&reads==i);fenced();}
    reset(true);start();lose_after=true;assert(step()==PORTABLE_NETWORK_RETAINED&&callbacks==1);fenced();
    for(int rc=RISC_RADIO_BUSY;rc<=RISC_RADIO_AGAIN;rc+=4){reset(true);to_submit();begin_rc=rc;begin_id=0;assert(step()==1&&!session.operation&&session.request.kind);assert(step()==1);begin_rc=0;begin_id=17;assert(step()==1&&session.operation);assert(zero(&session.request,sizeof(session.request)));}
    reset(true);to_submit();begin_rc=RISC_RADIO_UNAVAILABLE;begin_id=0;assert(step()==PORTABLE_NETWORK_UNAVAILABLE&&!session.operation);assert(zero(&session.request,sizeof(session.request)));assert(portable_network_session_close(&session)==0);
    reset(true);wifi.base.struct_size=WIFI_MANAGEMENT_V1_SIZE;assert(portable_network_session_start(&session,0)==PORTABLE_NETWORK_UNAVAILABLE&&!callbacks);assert(portable_network_session_close(&session)==0);
    reset(true);to_submit();begin_rc=RISC_RADIO_INVALID;begin_id=0;assert(step()==PORTABLE_NETWORK_RETAINED);fenced();
    reset(true);to_submit();begin_rc=RISC_RADIO_ACCEPTED;begin_id=0;assert(step()==PORTABLE_NETWORK_RETAINED);fenced();
    reset(true);to_submit();begin_rc=RISC_RADIO_AGAIN;begin_id=17;assert(step()==PORTABLE_NETWORK_RETAINED&&session.operation==17);fenced();
    reset(true);to_submit();lose_after=true;assert(step()==PORTABLE_NETWORK_RETAINED&&session.operation==17);fenced();

    reset(true);accepted();lose_after=true;assert(step()==PORTABLE_NETWORK_RETAINED&&session.operation==17);fenced();
    reset(true);accepted();poll_rc=RISC_RADIO_BUSY;assert(step()==1&&!session.view.retained);poll_rc=RISC_RADIO_AGAIN;assert(step()==1&&!session.view.retained);make_progress(RISC_RADIO_CONNECTED);assert(step()==2);
    reset(true);accepted();malformed=true;assert(step()==PORTABLE_NETWORK_RETAINED&&session.operation==17);fenced();
    reset(true);accepted();make_progress(RISC_RADIO_IDLE);assert(step()==PORTABLE_NETWORK_FAILED&&!session.operation);assert(portable_network_session_close(&session)==0);
    reset(true);accepted();assert(portable_network_session_step(&session,30000)==1&&session.view.closing);terminal();assert(session.view.result==PORTABLE_NETWORK_FAILED);
    reset(true);accepted();cancel_rc=RISC_RADIO_BUSY;assert(portable_network_session_close(&session)==1&&!session.cancel_accepted);cancel_rc=RISC_RADIO_AGAIN;assert(portable_network_session_close(&session)==1&&!session.cancel_accepted);cancel_rc=RISC_RADIO_PENDING;assert(portable_network_session_close(&session)==1&&session.cancel_accepted);terminal();
    reset(true);accepted();cancel_rc=RISC_RADIO_QUIESCENT;assert(portable_network_session_close(&session)==1&&session.operation==17&&!polls);terminal();
    reset(true);accepted();lose_after=true;assert(portable_network_session_close(&session)==-1&&session.operation==17);fenced();
    reset(true);accepted();cancel_rc=RISC_RADIO_RETAINED;assert(portable_network_session_close(&session)==-1&&session.operation==17);fenced();
    reset(true);accepted();assert(portable_network_session_close(&session)==1);malformed=true;assert(portable_network_session_close(&session)==-1&&session.operation==17);fenced();
    reset(true);accepted();assert(portable_network_session_close(&session)==1);for(unsigned i=0;i<50;++i)assert(portable_network_session_step(&session,UINT32_MAX)==1&&!session.view.retained&&session.operation);terminal();

    reset(true);accepted();lease_rc=RISC_RADIO_BUSY;lease_id=0;assert(portable_network_session_service_begin(&session)==1&&!session.service_lease&&!session.service_depth);lease_rc=0;lease_id=29;
    assert(portable_network_session_service_begin(&session)==0);assert(portable_network_session_service_begin(&session)==0&&leases==2&&session.service_depth==2);
    unsigned before=callbacks;assert(step()==1&&callbacks==before);assert(portable_network_session_close(&session)==1&&!cancels);
    assert(portable_network_session_service_begin(&session)==1);assert(portable_network_session_service_end(&session)==0&&!ends);
    end_rc=RISC_RADIO_AGAIN;assert(portable_network_session_service_end(&session)==1&&session.service_lease==29&&session.service_depth==1);assert(portable_network_session_close(&session)==1&&!cancels);
    end_rc=0;assert(portable_network_session_service_end(&session)==0&&!session.service_lease&&!session.service_depth);assert(portable_network_session_close(&session)==1&&cancels==1);terminal();
    reset(true);accepted();assert(portable_network_session_service_begin(&session)==0);lose_after=true;assert(portable_network_session_service_end(&session)==-1&&session.service_lease==29&&session.service_depth==1&&session.operation==17);fenced();
    reset(true);accepted();assert(portable_network_session_service_begin(&session)==0);end_rc=RISC_RADIO_RETAINED;assert(portable_network_session_service_end(&session)==-1&&session.service_lease==29&&session.service_depth==1&&session.operation==17);fenced();
    reset(true);accepted();lose_after=true;assert(portable_network_session_service_begin(&session)==-1&&session.service_lease==29&&session.operation==17);fenced();
    reset(true);lease_rc=RISC_RADIO_AGAIN;assert(portable_network_session_service_begin(&session)==-1&&session.service_lease==29);fenced();
    puts("Portable network session: real codec, borrowed/owned connection, bounded retries, copied credentials/status, owner/context fences, checked cancel and lease custody PASS");
    return 0;
}
