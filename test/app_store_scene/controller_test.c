#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_store_controller.h"
static app_store_controller asc;
static bool as_can_launch,as_feed_available,as_feed_enabled;
static char as_selected_identity[96],as_selected_version[64],as_selected_route[193];
#include "app_store_presenter.inc"
static risc_runtime_api_v1 runtime,replacement;
static const risc_runtime_api_v1 *current;
static unsigned reads,writes,releases,retains,pauses,connects,disconnects,checks,begins,cancels,activations,restarts,rows,clock_reads,now;
static unsigned fail_get,fail_acquire,replace_on,grant_fault;
static bool closed,cancel_ok,connect_ok,disconnect_ok,activate_ok,restart_ok,read_clock_ok,valid_clock,status_ok,step_ok,get_row_ok;
static wifi_link_t link;
static software_update_status_v1 state;
static software_update_row_v1 row;
static int64_t clock_epoch;
static unsigned radio_flags;static int radio_status,credential_status,clock_status;
static struct {char key[16];uint8_t data[64];uint32_t size;} cells[8];
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return current;}
static void check(void){assert(current==&runtime);}
static bool alive(void*c){(void)c;return current==&runtime;}
static void retain(void*c){(void)c;check();retains++;}
static bool pause_app(void*c){(void)c;check();pauses++;return true;}
static void yield_ms(uint32_t ms){check();now+=ms;}
static bool mono(uint32_t*v){check();*v=now;return true;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){check();reads++;*n=0;if(replace_on==1)current=&replacement;if((uintptr_t)c==1){assert(!strcmp(k,"quick_radio"));if(radio_status)return radio_status;assert(cap>=4);uint8_t q[]={0x51,1,(uint8_t)radio_flags,(uint8_t)(radio_flags^0xa5)};memcpy(b,q,4);*n=4;return 0;}if(credential_status)return credential_status;if(fail_get)return RISC_KEY_VALUE_IO;for(unsigned i=0;i<8;i++)if(!strcmp(cells[i].key,k)){*n=cells[i].size;if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].data,*n);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;check();writes++;assert(n<=64);unsigned i=0;while(i<8&&cells[i].key[0]&&strcmp(cells[i].key,k))i++;assert(i<8);snprintf(cells[i].key,sizeof(cells[i].key),"%s",k);memcpy(cells[i].data,b,n);cells[i].size=n;return 0;}
static risc_key_value_v1 credentials={1,sizeof(credentials),NULL,get,put},policy={1,sizeof(policy),(void*)1,get,put};
static bool connect_wifi(void*c,const char*s,const char*p){(void)c;check();assert(!strcmp(s,"Saved network")&&!strcmp(p,"testpass"));connects++;closed=false;link=WIFI_LINK_JOINING;if(replace_on==2)current=&replacement;return connect_ok;}
static bool disconnect_wifi(void*c){(void)c;check();disconnects++;if(disconnect_ok){closed=true;link=WIFI_LINK_DOWN;}return disconnect_ok;}
static wifi_link_t wifi_status(void*c){(void)c;check();return link;}
static int8_t rssi(void*c){(void)c;return -30;}
static bool addresses(void*c,wifi_ipv4_v1*s,wifi_ipv4_v1*a){(void)c;check();*s=(wifi_ipv4_v1){.address={192,0,2,1}};*a=(wifi_ipv4_v1){0};return true;}
static bool scan_start(void*c){(void)c;assert(!"No scanning in saved network client");return false;}
static bool scan_poll(void*c,garden_radio_scan_result_v1*s){(void)c;(void)s;assert(!"No scanning");return false;}
static bool scan_cancel(void*c){(void)c;assert(!"No scanning");return false;}
static wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.connect=connect_wifi,.status=wifi_status,.rssi=rssi,.addresses=addresses,.scan_start=scan_start,.scan_poll=scan_poll,.scan_cancel=scan_cancel,.disconnect_checked=disconnect_wifi};
static bool refresh(void*c,uint64_t utc){(void)c;check();assert(utc==1800000000);checks++;state.state=SOFTWARE_UPDATE_CATALOG;state.resources_open=true;return true;}
static bool step(void*c){(void)c;check();return step_ok;}
static bool status(void*c,software_update_status_v1*s){(void)c;check();assert(s->struct_size==sizeof(*s));*s=state;return status_ok;}
static bool get_row(void*c,uint32_t i,software_update_row_v1*r){(void)c;check();rows++;assert(i<state.count&&r->struct_size==sizeof(*r));*r=row;snprintf(r->id,sizeof(r->id),"app-%u",i);return get_row_ok;}
static bool begin(void*c,uint32_t i,uint64_t utc){(void)c;check();assert(i<state.count&&utc==1800000000);begins++;state.state=SOFTWARE_UPDATE_PREPARING;state.resources_open=true;return true;}
static bool cancel(void*c){(void)c;check();cancels++;if(!cancel_ok)return false;state.state=SOFTWARE_UPDATE_LIST;state.resources_open=false;return true;}
static bool activate(void*c){(void)c;check();assert(closed&&!asc.wifi_grant.api);activations++;state.state=activate_ok?SOFTWARE_UPDATE_ACTIVATED:SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;return activate_ok;}
static bool restart(void*c){(void)c;check();restarts++;return restart_ok;}
static software_update_v1 service={1,sizeof(service),NULL,refresh,step,status,get_row,begin,cancel,activate,restart};
static int32_t read_clock(void*c,risc_realtime_snapshot_v1*s){(void)c;check();clock_reads++;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=valid_clock?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,.epoch_seconds=clock_epoch,.monotonic_before_us=100,.monotonic_after_us=100};return clock_status?clock_status:read_clock_ok?RISC_REALTIME_OK:RISC_REALTIME_IO;}
static risc_realtime_api_v1 realtime={1,sizeof(realtime),NULL,read_clock};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){check();assert(v==1&&g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation);unsigned kind=0;const void*api=NULL;if(!strcmp(n,SOFTWARE_UPDATE_APPS_CAPABILITY)){assert(!i);kind=1;api=&service;}else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(i==1||i==6);kind=i==1?2:3;api=i==1?&policy:&credentials;}else if(!strcmp(n,"net.wifi")){assert(!i);kind=4;api=&wifi;}else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!i);kind=5;api=&realtime;}else assert(!"Unexpected capability");if(fail_acquire==kind){if(grant_fault)g->api=api;return false;}g->slot=kind;g->generation=1;g->api=api;if(grant_fault==2)g->generation=0;return true;}
static bool release(risc_runtime_capability_v1*g){check();assert(g->api&&g->slot&&g->generation);releases++;g->api=NULL;g->slot=g->generation=0;if(grant_fault==3)g->struct_size=0;return true;}
static void reset(void){
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.monotonic_ms=mono,.yield_ms=yield_ms};current=&runtime;
 clock_epoch=1800000000;reads=writes=releases=retains=pauses=connects=disconnects=checks=begins=cancels=activations=restarts=rows=clock_reads=now=0;fail_get=fail_acquire=replace_on=grant_fault=0;radio_flags=PORTABLE_RADIO_WIFI;radio_status=credential_status=clock_status=0;link=WIFI_LINK_DOWN;closed=cancel_ok=connect_ok=disconnect_ok=activate_ok=restart_ok=read_clock_ok=valid_clock=status_ok=step_ok=get_row_ok=true;
 memset(cells,0,sizeof(cells));portable_wifi_credentials value={"Saved network","testpass"};assert(portable_wifi_credentials_save(&credentials,&value)==PORTABLE_WIFI_CREDENTIALS_SAVED);writes=reads=0;
 as_can_launch=as_feed_available=as_feed_enabled=false;asp_confirmation=false;as_selected_identity[0]=as_selected_version[0]=as_selected_route[0]=0;state=(software_update_status_v1){.struct_size=sizeof(state),.state=SOFTWARE_UPDATE_LIST,.count=1};row=(software_update_row_v1){.struct_size=sizeof(row),.availability=SOFTWARE_UPDATE_AVAILABLE,.size=12345};strcpy(row.version,"2.0");strcpy(row.installed,"1.0");strcpy(row.reason,"New release available");assert(asc_init(&asc,&runtime,(app_store_hooks){.alive=alive,.retain=retain,.pause=pause_app}));asp_candidate=asp_presented=(app_store_presenter_view){0};
}
static risc_components_document_v1 document;
static void build(void){asp_build(&document);assert(document.root==1&&document.route_count==1&&document.node_count<=96);unsigned headers=0;for(unsigned i=0;i<document.node_count;i++){assert(document.nodes[i].id==i+1&&document.nodes[i].route==1);headers+=document.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION;}assert(headers<=2);if(document.flags&RISC_COMPONENTS_CONFIRM){unsigned texts=0,actions=0;for(unsigned i=0;i<document.node_count;i++){texts+=document.nodes[i].kind==RISC_SCENE_TEXT_NODE;actions+=document.nodes[i].kind==RISC_SCENE_ACTION;}assert(texts==1&&actions>=1&&actions<=3&&document.cancel_action==ASP_CANCEL);}asp_commit();}
static bool has(unsigned action){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==action)return true;return false;}
int main(int argc,char**argv){unsigned cases=0;
 if(argc>1){reset();unsigned before=releases;
  if(!strcmp(argv[1],"policy-context"))radio_status=RISC_KEY_VALUE_CONTEXT;
  else if(!strcmp(argv[1],"policy-unknown"))radio_status=-99;
  else if(!strcmp(argv[1],"credentials-context"))credential_status=RISC_KEY_VALUE_CONTEXT;
  else if(!strcmp(argv[1],"credentials-unknown"))credential_status=-99;
  else if(!strcmp(argv[1],"dirty-release"))grant_fault=3;
  else assert(!"Unknown regression");
  bool result=asc_connect(&asc,false);
  printf("%s result=%d terminal=%d retains=%u releases_after=%u connects=%u\n",argv[1],result,asc.terminal,retains,releases-before,connects);
  return asc.terminal?0:1;
 }

 reset();assert(!writes&&!connects&&!checks&&!begins);build();assert(has(ASP_CHECK)&&has(ASP_SELECT_BASE)&&!has(ASP_CONFIRM));cases++;
 reset();assert(asc_select(&asc,0));build();assert(has(ASP_REVIEW)&&!has(ASP_CONFIRM));assert(asp_action(ASP_REVIEW));build();assert(document.flags&RISC_COMPONENTS_CONFIRM);assert(document.cancel_action==ASP_CANCEL&&has(ASP_CANCEL)&&has(ASP_CONFIRM)&&!begins&&!connects);cases++;
 reset();assert(asc_select(&asc,0));build();strcpy(row.version,"3.0");assert(!asc_confirm(&asc)&&!connects&&!begins&&strstr(asc.message,"changed"));cases++;
 reset();assert(asc_select(&asc,0));assert(asc_confirm(&asc)&&asc.connecting&&!begins);strcpy(row.version,"3.0");link=WIFI_LINK_UP;assert(asc_tick(&asc)&&!begins&&closed);cases++;
 reset();assert(asc_select(&asc,0));assert(asc_confirm(&asc));link=WIFI_LINK_UP;assert(asc_tick(&asc)&&begins==1&&!checks);state.state=SOFTWARE_UPDATE_READY;assert(asc_tick(&asc)&&activations==1&&asc.restart_pending&&closed&&!asc.wifi_grant.api);assert(!asc_cancel(&asc));build();assert(has(ASP_RESTART)&&!has(ASP_CHECK));assert(asc_restart(&asc)&&restarts==1&&!writes);cases++;
 reset();assert(asc_connect(&asc,false));link=WIFI_LINK_UP;assert(asc_tick(&asc)&&checks==1&&!begins);state.state=SOFTWARE_UPDATE_LIST;assert(asc_tick(&asc)&&closed&&!asc.wifi_grant.api);cases++;
 reset();assert(asc_connect(&asc,false));now=30000;assert(asc_tick(&asc)&&!asc.connecting&&closed&&!checks);cases++;
 reset();assert(asc_connect(&asc,false));cancel_ok=false;unsigned before=disconnects;assert(!asc_cancel(&asc)&&asc.cleanup_pending&&disconnects==before&&asc.wifi_grant.api);build();assert(has(ASP_CANCEL)&&!has(ASP_CHECK));cancel_ok=true;assert(asc_cancel(&asc)&&closed);cases++;
 reset();assert(asc_connect(&asc,false));disconnect_ok=false;assert(!asc_cancel(&asc)&&asc.terminal&&retains==1);unsigned after=releases;assert(!asc_close(&asc)&&releases==after);cases++;
 reset();state.state=SOFTWARE_UPDATE_READY;activate_ok=false;assert(asc_tick(&asc)&&asc.restart_pending&&asc.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN);build();assert(has(ASP_RESTART)&&!has(ASP_CANCEL));cases++;
 reset();radio_flags=0;assert(!asc_connect(&asc,false)&&!connects&&!writes);cases++;
 reset();radio_status=RISC_KEY_VALUE_IO;assert(!asc_connect(&asc,false)&&!connects);cases++;
 reset();radio_status=RISC_KEY_VALUE_NOT_FOUND;assert(asc_connect(&asc,false)&&connects==1);cases++;
 reset();fail_get=1;assert(!asc_connect(&asc,false)&&!connects&&!writes);cases++;
 reset();link=WIFI_LINK_UP;assert(!asc_connect(&asc,false)&&!connects&&!disconnects);cases++;
 reset();connect_ok=false;assert(!asc_connect(&asc,false)&&connects==1&&disconnects==1&&closed);cases++;
 reset();valid_clock=false;assert(asc_connect(&asc,false));link=WIFI_LINK_UP;assert(asc_tick(&asc)&&!checks&&!begins&&closed);cases++;
 reset();state.count=SOFTWARE_UPDATE_ROWS_MAX+1;assert(!asc_status(&asc)&&asc.terminal);cases++;
 reset();state.state=99;assert(!asc_status(&asc)&&asc.terminal);cases++;
 reset();state.state=SOFTWARE_UPDATE_RETAINED;assert(!asc_status(&asc)&&asc.terminal);cases++;
 reset();status_ok=false;assert(!asc_status(&asc)&&asc.cleanup_pending&&!asc.terminal);cases++;
 reset();replace_on=1;assert(!asc_connect(&asc,false)&&asc.terminal&&!retains&&!connects);cases++;
 reset();replace_on=2;assert(!asc_connect(&asc,false)&&asc.terminal&&!retains&&!disconnects);cases++;
 reset();fail_acquire=4;grant_fault=1;assert(!asc_connect(&asc,false)&&asc.terminal&&retains==1);cases++;
 reset();state.count=128;assert(asc_status(&asc));for(unsigned page=0;page<16;page++){build();assert(asp_presented.page==page);for(unsigned slot=0;slot<8;slot++)assert(has(ASP_SELECT_BASE+page*8+slot)&&asp_action(ASP_SELECT_BASE+page*8+slot));if(page<15)assert(asp_action(ASP_NEXT));else assert(!has(ASP_NEXT)&&!asp_action(ASP_NEXT));}assert(asp_action(ASP_PREVIOUS)&&asc.page==14);cases++;
 reset();state.count=9;assert(asc_status(&asc));build();assert(!asp_action(ASP_SELECT_BASE+8));assert(asp_action(ASP_NEXT));assert(!asp_action(ASP_SELECT_BASE));build();assert(has(ASP_SELECT_BASE+8)&&!has(ASP_SELECT_BASE+9));cases++;
 reset();state.count=0;assert(asc_status(&asc));build();assert(!has(ASP_NEXT)&&!has(ASP_SELECT_BASE));cases++;
 reset();asc.page=999;build();assert(!asc.page&&asp_presented.page==0);cases++;
 reset();assert(asc_select(&asc,0));build();asc.selected_row.size++;assert(!asp_action(ASP_CONFIRM));cases++;
 reset();for(unsigned availability=1;availability<=3;availability++){row.availability=availability;assert(asc_select(&asc,0));build();assert(!has(ASP_CONFIRM)&&!asp_action(ASP_CONFIRM));asc.confirming=false;}cases++;
 reset();for(unsigned availability=4;availability<=5;availability++){row.availability=availability;assert(asc_select(&asc,0));build();assert(has(ASP_REVIEW)&&asp_action(ASP_REVIEW));build();assert(has(ASP_CONFIRM)&&asp_action(ASP_CONFIRM));asc.confirming=false;}cases++;
 reset();state.state=SOFTWARE_UPDATE_DOWNLOAD;state.total=500;state.done=250;assert(asc_status(&asc));build();bool progress=false;for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].kind==RISC_COMPONENT_PROGRESS){assert(document.nodes[i].maximum==500&&document.nodes[i].value==250);progress=true;}assert(progress&&has(ASP_CANCEL)&&!has(ASP_CHECK));cases++;
 reset();state.state=SOFTWARE_UPDATE_DOWNLOAD;state.total=UINT32_MAX;state.done=UINT32_MAX;assert(asc_status(&asc));build();for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].kind==RISC_COMPONENT_PROGRESS)assert(document.nodes[i].maximum<=1000&&document.nodes[i].value==document.nodes[i].maximum);cases++;
 reset();get_row_ok=false;build();assert(!has(ASP_SELECT_BASE)&&!asp_action(ASP_SELECT_BASE));cases++;
 reset();unsigned closed_before=releases;assert(asc_close(&asc)&&releases==closed_before+2&&!asc.opened&&!writes);cases++;
 reset();clock_status=-99;uint64_t utc=0;assert(!asc_clock(&asc,&utc)&&asc.terminal&&retains==1);cases++;
 reset();clock_status=RISC_REALTIME_CONTEXT;assert(!asc_clock(&asc,&utc)&&asc.terminal&&retains==1);cases++;
 reset();clock_status=RISC_REALTIME_IO;assert(!asc_clock(&asc,&utc)&&!asc.terminal);cases++;
 reset();assert(asc_select(&asc,0));assert(asc_confirm(&asc));assert(asc_cancel(&asc)&&!asc.install_after_connect&&!asc.confirming&&!asc.connecting&&closed&&!begins&&!activations);cases++;
 reset();assert(asc_select(&asc,0));build();assert(asp_action(ASP_BACK)&&asp_action(ASP_CANCEL)&&!begins&&!activations);cases++;
 reset();radio_status=RISC_KEY_VALUE_CONTEXT;unsigned released_before=releases;assert(!asc_connect(&asc,false)&&asc.terminal&&releases==released_before&&!connects);cases++;
 reset();radio_status=-99;released_before=releases;assert(!asc_connect(&asc,false)&&asc.terminal&&releases==released_before&&!connects);cases++;
 reset();credential_status=RISC_KEY_VALUE_CONTEXT;released_before=releases;assert(!asc_connect(&asc,false)&&asc.terminal&&releases==released_before+1&&!connects);cases++;
 reset();credential_status=-99;released_before=releases;assert(!asc_connect(&asc,false)&&asc.terminal&&releases==released_before+1&&!connects);cases++;
 reset();grant_fault=3;assert(!asc_connect(&asc,false)&&asc.terminal&&!connects);cases++;
 reset();row.availability=4;assert(asc_select(&asc,0));as_can_launch=true;build();assert(has(ASP_OPEN)&&asp_action(ASP_OPEN));asc.selected_row.size++;assert(!asp_action(ASP_OPEN));cases++;
 reset();assert(asc_select(&asc,0));build();assert(!has(ASP_OPEN)&&!asp_action(ASP_OPEN));cases++;
 reset();state.state=SOFTWARE_UPDATE_DOWNLOAD;assert(asc_status(&asc));asc.network_wait=true;build();assert(has(ASP_RETRY_NETWORK)&&asp_action(ASP_RETRY_NETWORK));asc.connecting=true;assert(!asp_action(ASP_RETRY_NETWORK));cases++;
 reset();state.state=SOFTWARE_UPDATE_DOWNLOAD;assert(asc_status(&asc));build();assert(!has(ASP_RETRY_NETWORK)&&!asp_action(ASP_RETRY_NETWORK));cases++;
 reset();assert(asc_select(&asc,0));memset(as_selected_identity,'I',95);as_selected_identity[95]=0;memset(as_selected_version,'V',63);as_selected_version[63]=0;memset(as_selected_route,'R',192);as_selected_route[192]=0;build();unsigned bytes=0;for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].kind==RISC_SCENE_TEXT_NODE)bytes+=strlen(document.nodes[i].text);assert(bytes>=350&&!has(ASP_CONFIRM)&&has(ASP_REVIEW));assert(asp_action(ASP_REVIEW));build();assert(has(ASP_CONFIRM)&&asp_action(ASP_CONFIRM));cases++;
 reset();clock_epoch=INT64_C(2147483648);assert(asc_clock(&asc,&utc)&&utc==UINT64_C(2147483648));cases++;
 reset();clock_epoch=INT64_C(4102444799);assert(asc_clock(&asc,&utc)&&utc==UINT64_C(4102444799));cases++;
 reset();clock_epoch=INT64_C(4102444800);assert(!asc_clock(&asc,&utc)&&!asc.terminal);cases++;
 reset();build();assert(!has(ASP_SOURCE)&&!asp_action(ASP_SOURCE));cases++;
 reset();as_feed_available=true;build();assert(has(ASP_SOURCE)&&asp_action(ASP_SOURCE)&&!checks&&!connects);as_feed_enabled=true;assert(!asp_action(ASP_SOURCE));build();assert(asp_action(ASP_SOURCE));cases++;
 reset();as_feed_available=true;build();asc.radio_owned=true;assert(!asp_action(ASP_SOURCE));asc.radio_owned=false;asc.network_wait=true;assert(!asp_action(ASP_SOURCE));asc.network_wait=false;asc.status.resources_open=true;assert(!asp_action(ASP_SOURCE));cases++;
 reset();as_feed_available=true;assert(asc_select(&asc,0));build();assert(!has(ASP_SOURCE)&&!asp_action(ASP_SOURCE));cases++;
 printf("App Store controller/presenter: %u cases passed\n",cases);
}
