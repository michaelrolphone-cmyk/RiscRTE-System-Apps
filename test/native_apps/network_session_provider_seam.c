/* Actual helper + actual profile codec connected to production provider below.
 * Only authorized outer storage and the cached application owner are synthetic. */
#include "PortableNetworkSession.h"
#include <assert.h>
#include <stdio.h>
static const wifi_async_v1 *provider;
static portable_network_session session;
static struct {char key[16]; uint32_t size; uint8_t bytes[64];} cells[8];
static unsigned cell_count,read_count;
static bool owner_ok(void *unused){(void)unused;return true;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;++read_count;*n=0;for(unsigned i=0;i<cell_count;i++)if(!strcmp(cells[i].key,k)){
  *n=cells[i].size;if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*n);return RISC_KEY_VALUE_OK;
 }return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){
 (void)c;unsigned i;for(i=0;i<cell_count;i++)if(!strcmp(cells[i].key,k))break;
 if(i==cell_count){assert(cell_count<8);++cell_count;strcpy(cells[i].key,k);}assert(n<=64);cells[i].size=n;memcpy(cells[i].bytes,b,n);return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
void app_workflow_start(const wifi_async_v1 *api){
 provider=api;cell_count=read_count=0;memset(cells,0,sizeof(cells));
 portable_wifi_credentials value={"Synthetic AP","synthetic-password"};assert(portable_wifi_profile_save(&kv,0,&value)==0);
 read_count=0;portable_network_session_init(&session,&provider->base,&kv,owner_ok,NULL);
}
void app_workflow_scan(void){ /* Existing production runner's entry name. */
 assert(portable_network_session_start(&session,0)==PORTABLE_NETWORK_PENDING);
 for(unsigned i=0;i<4;i++)assert(portable_network_session_step(&session,i+1)==PORTABLE_NETWORK_PENDING);
 assert(session.operation&&read_count==4&&!session.request.kind&&!session.request.password[0]);
}
void app_workflow_blocked(void){
 assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_PENDING);
 assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSE_PENDING);
 for(unsigned i=0;i<100;i++){
  assert(portable_network_session_step(&session,5+i)==PORTABLE_NETWORK_PENDING);
  assert(session.operation&&session.view.closing&&!session.view.retained);
 }
}
void app_workflow_cleaned(void){
 assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSED);
 assert(!session.operation&&!session.service_lease&&!session.view.retained);
}
void app_workflow_unavailable(void){
 assert(portable_network_session_start(&session,0)==PORTABLE_NETWORK_PENDING);
 for(unsigned i=0;i<3;i++)assert(portable_network_session_step(&session,i+1)==PORTABLE_NETWORK_PENDING);
 assert(portable_network_session_step(&session,4)==PORTABLE_NETWORK_UNAVAILABLE);
 assert(!session.operation&&!session.request.kind&&!session.request.password[0]);
 assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSED);
}
void network_session_ready(void){
 assert(portable_network_session_step(&session,20)==PORTABLE_NETWORK_READY);
 portable_network_snapshot snapshot;portable_network_session_snapshot(&session,&snapshot);
 assert(snapshot.link==WIFI_LINK_UP&&snapshot.ipv4.address[0]==192&&snapshot.ipv4.address[3]==5&&snapshot.rssi==-42&&!snapshot.borrowed);
 assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_OK);
 assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_OK);
 assert(portable_network_session_service_end(&session)==PORTABLE_NETWORK_SERVICE_OK);
}
void network_session_end_service(void){assert(portable_network_session_service_end(&session)==PORTABLE_NETWORK_SERVICE_OK);}
void network_session_cancel(void){assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSE_PENDING&&session.operation);}
void network_session_borrow(const wifi_async_v1 *api){
 portable_network_session_init(&session,&api->base,&kv,owner_ok,NULL);unsigned before=read_count;
 assert(portable_network_session_start(&session,0)==PORTABLE_NETWORK_PENDING);
 assert(portable_network_session_step(&session,1)==PORTABLE_NETWORK_READY);
 assert(session.view.borrowed&&!session.operation&&read_count==before);
 assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_OK);
 assert(portable_network_session_service_end(&session)==PORTABLE_NETWORK_SERVICE_OK);
 assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSED&&!session.operation);
}
void network_session_retained(void){
 uint32_t id=session.operation;assert(id);
 assert(portable_network_session_step(&session,30)==PORTABLE_NETWORK_RETAINED);
 assert(portable_network_session_close(&session)==PORTABLE_NETWORK_CLOSE_RETAINED&&session.operation==id);
 assert(portable_network_session_service_begin(&session)==PORTABLE_NETWORK_SERVICE_RETAINED);
}
