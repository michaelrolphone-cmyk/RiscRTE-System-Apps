#include WIFI_SYNC_FIXTURE
static unsigned sample_close_calls,sample_release_calls;
static int sample_close_result=RISC_TIME_SAMPLE_PENDING;
static int32_t sample_close(void *p,uint64_t t){(void)p;assert(t==7&&wifi_service_depth);sample_close_calls++;return sample_close_result;}
static bool sample_release(risc_runtime_capability_v1 *g){assert(!wifi_time.token&&wifi_service_depth);sample_release_calls++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_time_sample_api_v1 sampler={.api_version=1,.struct_size=sizeof(sampler),.close=sample_close};
static risc_runtime_api_v1 sample_runtime;
int portable_wifi_time_commit(portable_time_sync *c,const char **m){(void)c;(void)m;assert(0);return -1;}
static void sampling(void){
 assert(portable_wifi_services_begin());wifi_time_lease=true;wifi_time_attempted=wifi_time_ip=true;
 sample_runtime=runtime_api;sample_runtime.release=sample_release;
 wifi_time=(portable_time_sync){.runtime=&sample_runtime,.guard=wifi_time_guard,.api=&sampler,.token=7,.state=PORTABLE_TIME_SYNC_PENDING,
 .grant={.struct_size=sizeof(wifi_time.grant),.slot=99,.generation=1,.api=&sampler}};wifi_time.self=&wifi_time;
}
int main(int argc,char **argv){assert(argc==2);int mode=atoi(argv[1]);workflow_start(mode>=2);draft();wifi_connect();
 if(mode>=2){progress.phase=RISC_RADIO_CONNECTED;progress.station_state=WIFI_LINK_UP;progress.station[0]=10;wifi_async_tick();}
 else {fake_link=WIFI_LINK_UP;tick(250);}
 assert(link_state==WIFI_LINK_UP);sampling();unsigned before=disconnects;
 if(mode==0){wifi_activate(5);assert(wifi_time_disconnect_pending);}
 if(mode==1){assert(!portable_wifi_suspend());assert(wifi_time_disconnect_pending);}
 if(mode==2){wifi_scan_start();assert(wifi_time_radio_stop);}
 if(mode==3){wifi_make_view();wifi_activate(wifi_time_row);assert(wifi_time_cancelling);}
 assert(wifi_time_lease&&wifi_service_depth&&sample_close_calls&&!sample_release_calls&&disconnects==before);
 sample_close_result=0;
 if(mode==3)wifi_time_update();else wifi_tick();
 assert(!wifi_time_lease&&!wifi_service_depth&&sample_release_calls==1);
 if(mode==0||mode==1)assert(disconnects==before+1&&link_state==WIFI_LINK_DOWN);
 if(mode==2)assert(wifi_stopping||wifi_operation_kind==RISC_RADIO_REQUEST_SCAN);
 if(mode==3){assert(link_state==WIFI_LINK_UP&&wifi_time_attempted);wifi_time_update();assert(sample_release_calls==1);}
 printf("PASS actual Wi-Fi controller pending-close action %d\n",mode);return 0;
}
