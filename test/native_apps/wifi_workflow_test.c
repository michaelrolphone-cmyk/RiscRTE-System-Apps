/* Production Wi-Fi controller and adapter, copied text/radio provider tables,
 * deterministic input/storage boundary. No host network or real credentials. */
#define PORTABLE_TEXT_INPUT_CLIENT
#define PORTABLE_WIFI_PROFILES
#define TEST_WIFI_CELL_COUNT 256u
#define main legacy_fixture_main
#include "portable_wifi_test.c"
#undef main
static unsigned text_opens,text_closes,text_polls,text_close_wait,grant_serial;
static bool text_live;
static risc_text_entry_request_v1 text_request;
static risc_text_entry_state_v1 text_result;
static int32_t text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*out){(void)c;assert(!surface.frame&&!sub_count&&!text_live);text_request=*q;text_live=true;++text_opens;*out=77;return 0;}
static int32_t text_poll(void*c,uint64_t id,risc_text_entry_state_v1*out){(void)c;assert(text_live&&id==77&&!surface.frame&&!sub_count);++text_polls;*out=text_result;return 0;}
static int32_t text_close(void*c,uint64_t id){(void)c;assert(text_live&&id==77&&!surface.frame&&!sub_count);++text_closes;if(text_close_wait){--text_close_wait;return RISC_TEXT_ENTRY_AGAIN;}text_live=false;return 0;}
static risc_text_entry_api_v1_masked texts={{{1,sizeof(texts),NULL,text_open,text_poll,text_close},RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION},RISC_TEXT_ENTRY_MASKED_TAG,RISC_TEXT_ENTRY_MASKED_VERSION};
static const risc_text_entry_api_v1 *offered_text=&texts.base.base;
static risc_radio_progress_v1 progress;
static risc_radio_request_v1 copied_request;
static unsigned async_starts,async_polls,async_cancels;
static uint32_t async_serial;
static int32_t async_begin(void*c,const risc_radio_request_v1*q,uint32_t*out){(void)c;assert(!native_active&&q->struct_size==sizeof(*q));copied_request=*q;native_active=true;++async_starts;*out=++async_serial;progress=(risc_radio_progress_v1){.struct_size=sizeof(progress),.operation_id=*out,.phase=RISC_RADIO_STARTING,.owned=1,.scan={.struct_size=sizeof(progress.scan)}};return RISC_RADIO_ACCEPTED;}
static int32_t async_poll(void*c,uint32_t id,risc_radio_progress_v1*out){(void)c;assert(id==progress.operation_id);++async_polls;*out=progress;return progress.quiescent?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;}
static int32_t async_cancel(void*c,uint32_t id){(void)c;assert(id==progress.operation_id);++async_cancels;return progress.quiescent?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;}
static wifi_async_v1 radios;
static bool use_async;
static bool workflow_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){assert(v==1&&!id);g->api=offered_text;++grant_count;}
 else if(!fake_acquire(n,v,id,g))return false;
 if(use_async&&!strcmp(n,"net.wifi"))g->api=&radios;
 g->struct_size=sizeof(*g);g->slot=++grant_serial;g->generation=1;return true;
}
static bool workflow_release(risc_runtime_capability_v1*g){if(g->api==offered_text)assert(!text_live);if(!fake_release(g))return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void workflow_start(bool async){start();assert(portable_wifi_suspend());radios=(wifi_async_v1){.base=radio_api,.async_tag=RISC_RADIO_ASYNC_TAG,.async_version=1,.begin=async_begin,.poll=async_poll,.cancel=async_cancel};radios.base.struct_size=sizeof(radios);use_async=async;runtime_api.acquire=workflow_acquire;runtime_api.release=workflow_release;assert(wifi_acquire());}
static void stopped(void){progress.phase=RISC_RADIO_IDLE;progress.owned=0;progress.quiescent=1;native_active=false;tick(250);}
static void search_done(void){unsigned n=0;while(profile_work){wifi_profiles_step();assert(++n<=256);}}
static void seed(unsigned count){for(unsigned i=0;i<count;i++){portable_wifi_credentials v={{0},{0}};snprintf(v.ssid,sizeof(v.ssid),"Synthetic-%u",i);strcpy(v.password,"synthetic-pass");assert(portable_wifi_profile_save(wifi_store,i,&v)==0);}}
static void text_done(unsigned state,const char*value,unsigned flags){text_result=(risc_text_entry_state_v1){.struct_size=sizeof(text_result),.revision=1,.state=state,.flags=flags};strcpy(text_result.text,value);wifi_text_step();}
int main(int argc,char**argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);workflow_start(which>=10);draft();
 if(which==0){
  wifi_edit(WP_PASSWORD);assert(wifi_text_active&&text_live&&text_opens==1&&(text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED));
  unsigned before=writes;text_close_wait=2;text_done(RISC_TEXT_ENTRY_ACCEPTED,"new-synthetic-password",0);assert(wifi_text_active&&!strcmp(credentials.password,"testpass123")&&writes==before);
  wifi_text_step();assert(wifi_text_active);wifi_text_step();assert(!wifi_text_active&&!text_live&&!strcmp(credentials.password,"new-synthetic-password")&&draft_dirty&&writes==before);
 }else if(which==1){wifi_edit(WP_SSID);assert(!(text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED));text_done(RISC_TEXT_ENTRY_ACCEPTED,"Updated network",0);assert(!strcmp(credentials.ssid,"Updated network")&&!text_live);}
 else if(which==2){wifi_edit(WP_PASSWORD);text_done(RISC_TEXT_ENTRY_CANCELLED,"discarded-secret",0);assert(!strcmp(credentials.password,"testpass123")&&!text_live&&!writes);}
 else if(which==3){wifi_edit(WP_PASSWORD);text_done(RISC_TEXT_ENTRY_CANCELLED,"discarded-secret",RISC_TEXT_ENTRY_HOME_CANCEL);assert(wifi_home_return&&!strcmp(credentials.password,"testpass123")&&!writes&&!text_live);}
 else if(which==4){risc_text_entry_api_v1 old=texts.base.base;old.struct_size=sizeof(old);offered_text=&old;wifi_edit(WP_PASSWORD);assert(!wifi_text_active&&!text_opens&&!strcmp(credentials.password,"testpass123"));}
 else if(which==5){seed(20);wifi_saved_open(8);unsigned before=writes;for(unsigned i=0;i<8;i++)wifi_profiles_step();assert(!saved_loading_active&&saved_rows==8&&!strcmp(saved_names[4],"Synthetic-12")&&writes==before);wifi_activate(4);assert(!strcmp(credentials.ssid,"Synthetic-12")&&!draft_dirty&&!connects);}
 else if(which==6){seed(20);unsigned before=writes;wifi_activate(6);assert(profile_work==WIFI_PROFILE_SAVE);wifi_profiles_step();assert(!wifi_back()&&!profile_work&&writes==before);}
 else if(which==7){seed(20);strcpy(credentials.ssid,"Synthetic-12");strcpy(credentials.password,"replacement-pass");wifi_activate(6);search_done();portable_wifi_credentials check;assert(portable_wifi_profile_load(wifi_store,12,&check)==0&&!strcmp(check.password,"replacement-pass"));assert(portable_wifi_profile_load(wifi_store,0,&check)==0&&!strcmp(check.password,"synthetic-pass"));}
 else if(which==8){seed(20);strcpy(credentials.ssid,"Synthetic-12");wifi_activate(7);search_done();assert(page==WP_FORGET&&forget_index==12);wifi_activate(1);portable_wifi_credentials check;assert(portable_wifi_profile_load(wifi_store,12,&check)==PORTABLE_WIFI_CREDENTIALS_EMPTY);assert(portable_wifi_profile_load(wifi_store,0,&check)==0);}
 else if(which==9){seed(20);wifi_activate(6);search_done();assert(saved_state==0);portable_wifi_credentials check;assert(portable_wifi_profile_load(wifi_store,20,&check)==0&&!strcmp(check.ssid,"Fixture network"));}
 else if(which==10){wifi_scan_start();assert(async_starts==1&&scanning&&page==WP_SCAN);assert(!wifi_back()&&portable_wifi_stop_pending()&&!cleanup_pending);for(unsigned i=0;i<100;i++){tick(250);assert(opened&&wg.api&&!portable_text_adapter_retained());}assert(!portable_wifi_suspend());stopped();assert(!portable_wifi_stop_pending()&&portable_wifi_suspend());}
 else if(which==11){wifi_connect();assert(async_starts==1&&joining&&!strcmp(copied_request.password,"testpass123"));wifi_zero(credentials.password,sizeof(credentials.password));assert(!strcmp(copied_request.password,"testpass123"));progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;progress.station[0]=192;progress.station[1]=0;progress.station[2]=2;progress.station[3]=55;progress.rssi=-44;tick(250);assert(link_state==WIFI_LINK_UP&&!joining);assert(!wifi_disconnect()&&portable_wifi_stop_pending());stopped();}
 else if(which==12){wifi_scan_start();progress.phase=RISC_RADIO_RESULTS;progress.scan.state=GARDEN_RADIO_SCAN_DONE;progress.scan.count=1;strcpy(progress.scan.entries[0].ssid,"Selected AP");progress.scan.entries[0].auth=GARDEN_RADIO_AUTH_WPA2_PSK;tick(250);wifi_activate(0);assert(wifi_select_pending&&!text_live);stopped();search_done();assert(text_live&&wifi_text_active&&!strcmp(credentials.ssid,"Selected AP"));text_done(RISC_TEXT_ENTRY_ACCEPTED,"selected-pass",0);assert(!text_live&&!strcmp(credentials.password,"selected-pass"));}
 else if(which==13){wifi_connect();wifi_scan_start();assert(wifi_next_request.kind==RISC_RADIO_REQUEST_SCAN&&async_starts==1);stopped();assert(async_starts==2&&scanning);assert(!wifi_cancel_scan());stopped();}
 else if(which==14){wifi_scan_start();tick(15000);assert(portable_wifi_stop_pending()&&async_cancels&&!cleanup_pending);stopped();}
 else if(which==15){wifi_connect();wifi_scan_start();assert(wifi_next_request.kind);(void)wifi_back();assert(!wifi_next_request.kind);stopped();assert(async_starts==1);}
 else assert(!"unknown workflow");
 assert(!wifi_operation&&!text_live);finish();printf("Wi-Fi shared keyboard/profiles/async workflow %u PASS\n",which);return 0;
}
