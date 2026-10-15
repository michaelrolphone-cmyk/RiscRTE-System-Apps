#define SETTINGS_AVAILABILITY_FIXTURE
#include "settings_atomic_workflow_test.c"
static uint32_t availability=RISC_KEY_VALUE_UNAVAILABLE;
static uint64_t availability_generation=1;
static unsigned status_grants,status_releases;
static bool provider_holds;
static int32_t availability_snapshot(void *ctx,risc_key_value_availability_state_v1 *out) {
 (void)ctx;io();assert(kind_live(14));
 *out=(risc_key_value_availability_state_v1){sizeof(*out),1,availability,0,availability_generation};
 if(which("malformed"))out->namespace_id=6;
 return 0;
}
static const risc_key_value_availability_v1 availability_api={1,sizeof(availability_api),NULL,availability_snapshot};
static bool availability_acquire(const char *name,uint32_t version,uint64_t ns,risc_runtime_capability_v1 *out) {
 if(strcmp(name,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY))return selected_acquire(name,version,ns,out);
 io();assert(version==1&&ns==1);++status_grants;
 unsigned slot=1;while(leases[slot].kind)++slot;assert(slot<32);
 leases[slot]=(lease){14,++generation};++live;
 *out=(risc_runtime_capability_v1){sizeof(*out),slot,generation,&availability_api};return true;
}
static bool availability_release(risc_runtime_capability_v1 *grant) {
 unsigned kind=leases[grant->slot].kind;
 if(kind==14){++status_releases;if(which("status-release-fails")){io();hidden=true;return false;}}
 bool ok=selected_release(grant);
 if(ok&&kind==13&&availability==RISC_KEY_VALUE_RECONCILIATION_PENDING&&!provider_holds) {
  availability=RISC_KEY_VALUE_AVAILABLE;++availability_generation;
 }
 return ok;
}
static void availability_discard(bool keep) {
 t5_app_input_t input={.buttons=T5_APP_BUTTON_BACK};
 assert(settings_availability_filter(&input)&&!input.buttons&&settings_availability_discard);
 input=(t5_app_input_t){.buttons=keep?T5_APP_BUTTON_BACK:T5_APP_BUTTON_CONFIRM};
 assert(settings_availability_filter(&input)&&!settings_availability_discard);
 assert(settings_availability_draft==keep);
}
int main(int argc,char **argv) {
 assert(argc==2);test_name=argv[1];zone_id="UTC";native_epoch=expected_epoch=1768505696;
 configure_records();memcpy(values[1],zone_record,zone_size);sizes[1]=zone_size;
 memcpy(values[7],basis_record,basis_size);sizes[7]=basis_size;
 if(which("conflict")||which("storage-conflict"))availability=RISC_KEY_VALUE_CONFLICT;
 if(which("reload")||which("reload-modal")||which("storage-pending")||which("provider-pending"))availability=RISC_KEY_VALUE_RECONCILIATION_PENDING;
 if(which("available-save")||which("failed-save-draft"))availability=RISC_KEY_VALUE_AVAILABLE;
 runtime.acquire=availability_acquire;runtime.release=availability_release;runtime.resident_shell=selected_resident;
 assert(app_module_init()==0);in_main=true;const t5_app_api_v1 *api=t5_app_get_api(1);
 if(which("malformed")){assert(!api&&retained);cleanup();puts(test_name);return 0;}
 assert(api&&!retained&&status_grants==1);
 t5_app_setting_t row;assert(settings_get(0,SETTINGS_STORAGE_ROW,&row)&&strstr(row.label,"storage"));
 if(!strncmp(test_name,"storage-",8)) {
  api->settings_render(0,1);sv_switch(SV_STORAGE);assert(page_visits[SV_STORAGE]);
 } else if(which("reload-modal")) {
  inputs();nav(RISC_NAV_CONFIRM);nav(RISC_NAV_BACK);
  assert(settings_availability_activate()==T5_APP_SETTING_NO_CHANGE);
  assert(settings_availability.state.availability==RISC_KEY_VALUE_AVAILABLE&&atomic_acquires==2);
 } else if(which("available-save")) {
  api->settings_render(0,3);inputs();nav(RISC_NAV_RIGHT);nav(RISC_NAV_CONFIRM);next_at+=40;end_app();
  assert(api->settings_activate(0,2)==T5_APP_SETTING_UPDATED&&submitted==1);
 } else if(which("zone-touch-cancel")) {
  api->settings_render(0,2);inputs();next_at+=20;nav(RISC_NAV_CONFIRM);next_at+=20;
  tap(LOGICAL_WIDTH/2,FOOTER_Y);next_at+=20;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);nav(RISC_NAV_CONFIRM);
  assert(api->settings_activate(0,1)!=T5_APP_SETTING_UPDATED);
  assert(!submitted&&!settings_availability_draft&&page_visits[SV_DISCARD]>=2);
 } else if(which("date-discard")) {
  assert(settings_atomic_begin("rtc_basis"));api->settings_render(0,1);sv_switch(SV_FIELDS);editor_field=7;
  t5_app_input_t input={.buttons=T5_APP_BUTTON_CONFIRM};assert(settings_availability_filter(&input)&&settings_availability_discard);
  input=(t5_app_input_t){.buttons=T5_APP_BUTTON_CONFIRM};assert(settings_availability_filter(&input)&&input.exit_requested&&!settings_availability_draft);
 } else if(which("unavailable-modal")) {
  api->settings_render(0,3);inputs();nav(RISC_NAV_RIGHT);nav(RISC_NAV_CONFIRM);
  nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);nav(RISC_NAV_CONFIRM);
  assert(api->settings_activate(0,2)!=T5_APP_SETTING_UPDATED);
  assert(time_format_choice==1&&!submitted&&!settings_availability_draft&&page_visits[SV_DISCARD]>=2);
 } else if(which("failed-save-draft")) {
  assert(settings_atomic_begin("time_format"));sv_switch(SV_TIME_FORMAT);fault_status=RISC_ATOMIC_IO;
  const char *message="";time_format_choice=1;
  assert(!settings_atomic_choice("time_format",0x54,1,2,&message));
  assert(submitted==1&&settings_availability_draft&&!settings_atomic_exit&&time_format_choice==1);
  availability_discard(false);
 } else if(which("reload")||which("provider-pending")) {
  provider_holds=which("provider-pending");
  values[0][0]=0x54;values[0][1]=1;values[0][2]=1;values[0][3]=0xa4;sizes[0]=4;
  assert(settings_atomic_close());rt->yield_ms(1);assert(settings_atomic_open());settings_time_format_load();
  assert(settings_availability.state.availability==(provider_holds?RISC_KEY_VALUE_RECONCILIATION_PENDING:RISC_KEY_VALUE_AVAILABLE));
  assert(time_format_mode==(provider_holds?0:1)&&atomic_acquires==2);
 } else if(which("status-release-fails")) {
  cleanup();assert(retained&&status_releases==1);puts(test_name);return 0;
 } else {
  unsigned before=atomic_reads;settings_time_format_load();assert(time_format_mode==0&&atomic_reads==before);
  assert(settings_atomic_begin("time_format")&&settings_availability_temporary&&settings_atomic_owned());
  time_format_choice=1;sv_switch(SV_TIME_FORMAT);const char *message="";
  assert(!settings_atomic_choice("time_format",0x54,1,2,&message)&&strstr(message,"Draft kept"));
  assert(!submitted&&!legacy_puts&&time_format_choice==1);
  assert(!snt_save(-1)&&!rtc_writes&&!seeds);
  if(which("draft-pending")) {availability=RISC_KEY_VALUE_RECONCILIATION_PENDING;++availability_generation;assert(settings_availability_refresh());}
  if(which("invalid-publication")) {availability=RISC_KEY_VALUE_AVAILABLE;++availability_generation;assert(!settings_availability_refresh()&&retained);cleanup();puts(test_name);return 0;}
  assert(!settings_atomic_close()&&atomic_releases==0);
  availability_discard(true);assert(time_format_choice==1&&atomic_releases==0);
  home_pending=true;t5_app_input_t input={0};assert(settings_availability_filter(&input)&&!home_pending);
  input=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};assert(settings_availability_filter(&input)&&settings_availability_draft);
  availability_discard(false);assert(!settings_atomic_owned());
 }
 cleanup();assert(!retained&&!live&&!legacy_puts);printf("Settings availability %s passed\n",test_name);return 0;
}
