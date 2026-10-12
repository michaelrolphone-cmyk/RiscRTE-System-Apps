/* Production Wi-Fi controller with deterministic availability/grant fences.
 * Synthetic credentials only; no native device or source choice. */
#define PORTABLE_SETTINGS_AVAILABILITY
#define TEST_WIFI_AVAILABILITY_FIXTURE
#define WIFI_ATOMIC_WORKFLOW_LIBRARY
#include <stdbool.h>
#include "RiscKeyValueAvailabilityV1.h"
static risc_key_value_availability_v1 availability_fixture_api;
static unsigned availability_data_grants,availability_data_acquires,availability_data_releases,availability_legacy_grants;
static bool availability_denied,availability_release_denied,availability_status_release_denied;
static void test_wifi_availability_checkpoint(void);
#include "wifi_atomic_workflow_test.c"
static uint32_t current_availability=RISC_KEY_VALUE_UNAVAILABLE;
static uint64_t availability_generation=1;
static bool bound_provider_grant,invalid_snapshot;
static unsigned availability_snapshots,state_calls,get_calls;
static int32_t availability_snapshot(void*c,risc_key_value_availability_state_v1*out){
 (void)c;++availability_snapshots;
 *out=(risc_key_value_availability_state_v1){.struct_size=sizeof(*out),.namespace_id=invalid_snapshot?1:6,
  .availability=current_availability,.generation=availability_generation};return 0;
}
static void test_wifi_availability_checkpoint(void){
 if(current_availability==RISC_KEY_VALUE_RECONCILIATION_PENDING&&!availability_data_grants&&!bound_provider_grant){
  current_availability=RISC_KEY_VALUE_AVAILABLE;++availability_generation;
 }
}
static int32_t availability_atomic_state(void*c,risc_key_value_atomic_state_v1*out){
 ++state_calls;if(current_availability!=RISC_KEY_VALUE_AVAILABLE)return RISC_ATOMIC_BUSY;return atomic_state(c,out);
}
static int32_t availability_atomic_get(void*c,uint64_t revision,const char*k,void*out,uint32_t cap,uint32_t*n){
 ++get_calls;if(current_availability!=RISC_KEY_VALUE_AVAILABLE)return RISC_ATOMIC_BUSY;return atomic_get(c,revision,k,out,cap,n);
}
static void change_availability(uint32_t next){current_availability=next;++availability_generation;}
static void advance(void){ticks+=250;wifi_initial_profile_step();}
static void explicit_discard(void){wifi_page(WP_STORAGE);wifi_activate(1);}
static void availability_start(void){
 atomic_fixture_api=(risc_key_value_atomic_v1){1,sizeof(atomic_fixture_api),NULL,availability_atomic_state,availability_atomic_get,atomic_submit,atomic_collect,atomic_cancel};
 availability_fixture_api=(risc_key_value_availability_v1){1,sizeof(availability_fixture_api),NULL,availability_snapshot};
 workflow_start(false);
}
int main(int argc,char**argv){
 (void)text_done;(void)seed;(void)search_done;(void)stopped;(void)assert_record;(void)prepared;(void)drained;(void)clean_edit;
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);
 if(which==4||which==8||which==9||which==12||which==14||which==15||which==16)current_availability=RISC_KEY_VALUE_AVAILABLE;
 if(which==10)availability_denied=true;
 availability_start();assert(!availability_legacy_grants);
 if(which==0){
  assert(!credentials.ssid[0]&&!wifi_initial_profile_pending&&availability_data_grants==1&&!state_calls&&!get_calls);
  draft();draft_dirty=true;wifi_activate(6);wifi_activate(7);
  assert(!profile_work&&!submits&&!writes&&draft_dirty&&!strcmp(credentials.password,"testpass123"));
  render();assert(view.count==12&&strstr(view.values[11],"unavailable"));
 }else if(which==1){
  draft();draft_dirty=true;wifi_connect();assert(connects==1&&joining&&!remember_join&&!wifi_atomic_connect_ready);
  fake_link=WIFI_LINK_UP;tick(250);wifi_profiles_step();assert(!remember_pending&&!submits&&!writes&&draft_dirty);
  assert(wifi_disconnect());
 }else if(which==2){
  set_record(0,"Recovered AP","recovered-pass");unsigned before=writes;
  draft();draft_dirty=true;change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);
  for(unsigned i=0;i<20;i++)advance();
  assert(current_availability==RISC_KEY_VALUE_RECONCILIATION_PENDING&&availability_data_releases==0&&availability_data_grants==1);
  assert(draft_dirty&&!strcmp(credentials.password,"testpass123")&&writes==before&&!submits);
  wifi_activate(6);assert(!profile_work&&strstr(wifi_message,"pending"));
  render();assert(!wifi_back()&&page==WP_STORAGE&&draft_dirty&&availability_data_grants==1);render();
  wifi_activate(0);assert(page==WP_ROOT&&draft_dirty);
  explicit_discard();assert(current_availability==RISC_KEY_VALUE_AVAILABLE&&availability_data_releases==1&&availability_data_grants==1);
  assert(!draft_dirty&&!strcmp(credentials.ssid,"Recovered AP")&&writes==before&&!submits);
 }else if(which==3){
  set_record(0,"Recovered AP","recovered-pass");unsigned before=writes;
  change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);advance();
  assert(wifi_storage_ready()&&availability_data_releases==1&&availability_data_grants==1);
  assert(!strcmp(credentials.ssid,"Recovered AP")&&!draft_dirty&&writes==before);
 }else if(which==4){
  draft();draft_dirty=true;wifi_activate(6);prepared();assert(job_state==1&&!writes);native_worker();drained();
  assert(!draft_dirty&&native_sequence==1&&saved_state==PORTABLE_WIFI_CREDENTIALS_SAVED);
 }else if(which==5){
  draft();draft_dirty=true;change_availability(RISC_KEY_VALUE_CONFLICT);advance();wifi_activate(6);wifi_activate(7);
  assert(!profile_work&&!submits&&!writes&&availability_data_grants==1&&draft_dirty);
  render();assert(strstr(view.values[11],"conflict")&&!strcmp(credentials.password,"testpass123"));
 }else if(which==6){
  bound_provider_grant=true;change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);advance();
  assert(availability_data_releases==1&&availability_data_grants==1&&!wifi_storage_ready());
  for(unsigned i=0;i<20;i++)advance();
  assert(availability_data_releases==1&&!submits&&!writes);
  render();assert(strstr(view.values[11],"restart"));
  bound_provider_grant=false;explicit_discard();assert(wifi_storage_ready());
 }else if(which==7){
  wifi_edit(WP_PASSWORD);assert(wifi_text_active);change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);
  for(unsigned i=0;i<20;i++)advance();
  assert(!availability_data_releases&&wifi_text_active);
  text_done(RISC_TEXT_ENTRY_ACCEPTED,"temporary-pass",0);assert(draft_dirty&&!strcmp(credentials.password,"temporary-pass"));
  advance();assert(!availability_data_releases);
 }else if(which==8){
  draft();draft_dirty=true;wifi_activate(6);assert(profile_work);change_availability(RISC_KEY_VALUE_CONFLICT);wifi_profiles_step();
  assert(!profile_work&&!submits&&!writes&&draft_dirty&&!strcmp(credentials.password,"testpass123"));
 }else if(which==9){
  draft();draft_dirty=true;invalid_snapshot=true;wifi_activate(6);
  assert(!wifi_availability_valid&&!profile_work&&!submits&&!writes&&draft_dirty);
  invalid_snapshot=false;
 }else if(which==10){
  assert(!wifi_availability.api&&!wifi_storage_ready());draft();draft_dirty=true;
  wifi_activate(6);assert(!profile_work&&!submits&&!writes);wifi_connect();assert(connects==1&&!remember_join);assert(wifi_disconnect());
 }else if(which==11){
  draft();draft_dirty=true;unsigned grants=availability_data_grants;
  assert(portable_wifi_suspend()&&availability_data_grants==grants&&!strcmp(credentials.password,"testpass123"));
  portable_wifi_resume();assert(!portable_wifi_close()&&page==WP_STORAGE&&draft_dirty&&availability_data_grants==grants);
  wifi_activate(2);assert(wifi_back_return&&!draft_dirty);assert(wifi_back()&&!opened);
 }else if(which==12){
  draft();draft_dirty=true;change_availability(RISC_KEY_VALUE_AVAILABLE);advance();
  wifi_activate(6);assert(!profile_work&&!submits&&draft_dirty&&!wifi_storage_ready()&&!availability_data_releases);
 }else if(which==13){
  availability_release_denied=true;change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);advance();
  assert(wk.api&&wifi_atomic_api&&availability_data_grants==1&&!availability_data_releases&&!submits&&!writes);
  availability_release_denied=false;
 }else if(which==14){
  draft();draft_dirty=true;wifi_activate(6);prepared();assert(job_state==1);
  invalid_snapshot=true;unsigned before=collects;wifi_profiles_step();assert(collects==before&&job_state==1&&wifi_atomic_edit.token);
  invalid_snapshot=false;native_worker();drained();assert(collects==before+1&&!draft_dirty);
 }else if(which==15){
  draft();draft_dirty=true;wifi_activate(6);prepared();assert(job_state==1);
  change_availability(RISC_KEY_VALUE_CONFLICT);native_worker();drained();assert(collects==1&&!wifi_atomic_edit.token);
 }else if(which==16){
  set_record(0,"Earlier AP","earlier-pass");wifi_storage_reload();assert(!strcmp(credentials.ssid,"Earlier AP"));
  set_record(0,"Recovered AP","recovered-pass");change_availability(RISC_KEY_VALUE_AVAILABLE);temporary_store_busy=true;advance();
  assert(wifi_initial_profile_pending&&!credentials.ssid[0]&&!draft_dirty);
  temporary_store_busy=false;advance();assert(!wifi_initial_profile_pending&&!strcmp(credentials.ssid,"Recovered AP"));
 }else assert(!"unknown availability scenario");
 if(opened){
  if(wifi_text_active)text_done(RISC_TEXT_ENTRY_CANCELLED,"",0);
  assert(wifi_disconnect());wifi_storage_discard();
 }
 finish();assert(!availability_data_grants&&!availability_legacy_grants);printf("Wi-Fi availability case %u PASS\n",which);return 0;
}
