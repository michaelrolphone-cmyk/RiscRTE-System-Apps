/* Both selectors exercise the same real Settings instance and namespace grant. */
#define PORTABLE_SLEEP_SETTINGS
#define main original_alert_fixture_main
#include "portable_alarm_settings_test.c"
#undef main
static uint8_t format_record[4]={0x54,1,0,0xa5};
static uint8_t sleep_record[4]={0x53,1,1,0xa4};
static unsigned test_case,format_writes,sleep_writes;
static bool fault;
static int32_t combined_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size) {
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY) || !strcmp(key,PORTABLE_SLEEP_KEY)) {
    assert(cap==4);*size=4;
    if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)) {
      if(fault && test_case==4 && format_writes)return RISC_KEY_VALUE_IO;
      memcpy(data,format_record,4);
    } else memcpy(data,sleep_record,4);
    return RISC_KEY_VALUE_OK;
  }
  return kv_get(c,key,data,cap,size);
}
static int32_t combined_put(void *c,const char *key,const void *data,uint32_t size) {
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)) {
    assert(size==4);++format_writes;
    if(fault && test_case==2)return RISC_KEY_VALUE_IO;
    memcpy(format_record,data,4);
    if(fault && test_case==8){format_record[2]=0;format_record[3]=0xa5;}
    return fault && test_case==3?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
  }
  if(!strcmp(key,PORTABLE_SLEEP_KEY)) {
    assert(size==4);++sleep_writes;memcpy(sleep_record,data,4);return RISC_KEY_VALUE_OK;
  }
  return kv_put(c,key,data,size);
}
static uint8_t choose(unsigned row,unsigned y,bool cancel,bool pick) {
  contact_count=0;polls=0;
  if(pick)tap(3,100,y);
  tap(7,cancel?60:170,213);tap(8,cancel?60:170,213);
  /* Errors remain open until an explicit Cancel; a held Save never retries. */
  tap(12,60,213);
  settings_render(0,(int32_t)row+1);
  assert(sv_row_y(row)-sv_scroll[0]>=52 && sv_row_y(row)-sv_scroll[0]+40<=174);
  uint32_t category=0;int32_t selected=0;
  uint8_t result=settings_touch(100,(int16_t)(sv_row_y(row)-sv_scroll[0]+20),&category,&selected);
  assert(category==0 && selected==(int32_t)row+1 && sv_page==SV_ROOT && !settings_editing);
  return result;
}
int main(int argc,char **argv) {
  assert(argc==2);test_case=(unsigned)atoi(argv[1]);assert(test_case<=8);
  exists=true;blob_size=1;bytes[0]=PORTABLE_ALERT_SOUND;
  kv_api.get=combined_get;kv_api.put=combined_put;fault=true;
  assert(app_module_init()==0 && kv_grants==1 && alert_store==settings_store);
  assert(SETTINGS_ALERT_ROW==6 && SETTINGS_ABOUT_ROW==5 && settings_count(0)==7);
  const char *zone=portable_time_zone(),*basis=portable_time_basis();
  bool cancelled=test_case==1;
  bool format_error=test_case==2 || test_case==3 || test_case==4 || test_case==8;
  uint8_t result=choose(2,140,cancelled,true);
  assert(result==(cancelled||format_error?T5_APP_SETTING_NO_CHANGE:T5_APP_SETTING_UPDATED));
  assert(time_format_mode==(cancelled||format_error?PORTABLE_TIME_FORMAT_12:PORTABLE_TIME_FORMAT_24));
  assert(bytes[0]==PORTABLE_ALERT_SOUND && !kv_writes && !sleep_writes);
  assert(!memcmp(sleep_record,(uint8_t[]){0x53,1,1,0xa4},4));
  if(format_error) {
    assert(format_writes==1 && time_format_choice==PORTABLE_TIME_FORMAT_12);
    assert(!strcmp(time_format_message,"Save unconfirmed - retry"));
    fault=false;
    assert(choose(2,140,false,true)==T5_APP_SETTING_UPDATED && time_format_mode==PORTABLE_TIME_FORMAT_24);
    assert(format_writes==(test_case==3||test_case==4?1u:2u));
  }
  uint8_t confirmed_format[4];memcpy(confirmed_format,format_record,4);
  unsigned saved_format_writes=format_writes;
  scenario=test_case==5?4:test_case==6?6:test_case==7?5:0;
  result=choose(SETTINGS_ALERT_ROW,154,cancelled,true);
  bool alert_error=test_case==5 || test_case==6;
  assert(result==(cancelled||alert_error?T5_APP_SETTING_NO_CHANGE:T5_APP_SETTING_UPDATED));
  assert(!memcmp(confirmed_format,format_record,4) && format_writes==saved_format_writes && !sleep_writes);
  if(alert_error) {
    assert(alert_unconfirmed && alert_choice==PORTABLE_ALERT_BOTH && kv_writes==1);
    assert(bytes[0]==PORTABLE_ALERT_SOUND);
    check_row("Unconfirmed");scenario=0;
    assert(choose(SETTINGS_ALERT_ROW,154,false,false)==T5_APP_SETTING_UPDATED);
    assert(kv_writes==2 && !alert_unconfirmed);
  }
  check_row(cancelled?"Sound":"Both");
  assert(choose(4,118,false,true)==T5_APP_SETTING_UPDATED && !sleep_writes);
  assert(!memcmp(confirmed_format,format_record,4));
  assert(bytes[0]==(cancelled?PORTABLE_ALERT_SOUND:PORTABLE_ALERT_BOTH));
  assert(!rtc_writes && !strcmp(zone,portable_time_zone()) && !strcmp(basis,portable_time_basis()));
  app_module_fini();assert(!grants && !subscriptions && !frames && kv_releases==1);
  polls=0;kv_grants=kv_releases=0;scenario=0;
  assert(app_module_init()==0 && kv_grants==1);
  assert(time_format_mode==(cancelled?PORTABLE_TIME_FORMAT_12:PORTABLE_TIME_FORMAT_24));
  check_row(cancelled?"Sound":"Both");
  app_module_fini();assert(!grants && !subscriptions && !frames && kv_releases==1);
  printf("Combined Settings scenario %u: one grant, exact independent keys, UI routing, retry and reopen passed\n",test_case);
}
