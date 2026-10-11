/* Production Settings UI plus the shared Clock-consumer record. */
#define main original_settings_fixture_main
#include "portable_settings_test.c"
#undef main
static unsigned test_case;
#ifdef PORTABLE_INPUT_NAVIGATION
/* Navigation edges use their own provider stream; raster capture can poll
 * raw touch without dispatching navigation. */
static bool format_navigation(void *c,risc_input_navigation_frame_v1 *out) {
 (void)c;static unsigned previous,navigation_steps;unsigned event_step=++navigation_steps;*out=(risc_input_navigation_frame_v1){0};
 if(event_step==3)out->pressed=RISC_NAV_DOWN;
 if(event_step==7)out->pressed=test_case==14?RISC_NAV_BACK:RISC_NAV_CONFIRM;
 if(out->pressed)out->buttons=out->pressed;
 if(event_step==8)out->buttons=RISC_NAV_CONFIRM; /* Held confirm is never a second save. */
 out->released=previous&~out->buttons;previous=out->buttons;
 return true;
}
#endif
static void record(unsigned mode) {
 format_size=4;memcpy(format_bytes,(uint8_t[]){0x54,1,(uint8_t)mode,(uint8_t)(mode^0xa5u)},4);
}
static void boundary_labels(unsigned mode) {
 /* These are already-local civil hours. Format must not touch RTC/timezone. */
 const unsigned hours[]={0,1,9,10,12,23};
 const char *twelve[]={"12:07:03 AM","1:07:03 AM","9:07:03 AM","10:07:03 AM","12:07:03 PM","11:07:03 PM"};
 #ifdef PORTABLE_UNPADDED_HOURS
 const char *twentyfour[]={"0:07:03","1:07:03","9:07:03","10:07:03","12:07:03","23:07:03"};
#else
 const char *twentyfour[]={"00:07:03","01:07:03","09:07:03","10:07:03","12:07:03","23:07:03"};
#endif
 time_format_mode=mode;
 for(unsigned i=0;i<sizeof(hours)/sizeof(hours[0]);++i) {
  twatch_rtc_time_v1 value={2026,10,4,0,(uint8_t)hours[i],7,3};
  twatch_rtc_time_v1 before=value;char label[24];
  format_wall_time(label,sizeof(label),&value);
  assert(!strcmp(label,mode==PORTABLE_TIME_FORMAT_24?twentyfour[i]:twelve[i]));
  assert(!memcmp(&before,&value,sizeof(value)));
  settings_draft=value;editor_value_text(3,label,sizeof(label));
  if(mode==PORTABLE_TIME_FORMAT_24) {char expected[8];snprintf(expected,sizeof(expected),"%u",hours[i]);assert(!strcmp(label,expected));}
  else assert(strstr(label,hours[i]<12?"AM":"PM"));
 }
}
static void settings_row_hours(void) {
 /* Exercise the actual row and timezone path for each raw RTC hour. */
 twatch_rtc_time_v1 saved=stored;unsigned saved_mode=time_format_mode;
 for(unsigned mode=0;mode<2;++mode)for(unsigned h=0;h<24;++h){
  time_format_mode=mode;stored=(twatch_rtc_time_v1){2026,10,4,0,(uint8_t)h,7,3};
  twatch_rtc_time_v1 before=stored,local;t5_app_setting_t row;
  assert(portable_time_forward(&stored,&local)&&settings_get(0,0,&row));
  unsigned hour=mode?local.hour:(local.hour%12?local.hour%12:12);
  size_t width=hour<10?1:2;
#ifndef PORTABLE_UNPADDED_HOURS
  if(mode)width=2;
#else
  assert(row.value[0]!='0'||(hour==0&&row.value[1]==':'));
#endif
  assert(row.value[width]==':');
  assert(row.value[width+1]=='0'+local.minute/10&&row.value[width+2]=='0'+local.minute%10);
  assert(!memcmp(&before,&stored,sizeof(stored)));
 }
 stored=saved;time_format_mode=saved_mode;
}
static void record_contract(void) {
 unsigned mode=99;assert(portable_time_format_load(NULL,&mode)==PORTABLE_TIME_FORMAT_UNAVAILABLE && mode==0);
 assert(portable_time_format_load(&kv_api,&mode)==PORTABLE_TIME_FORMAT_MISSING && mode==0);
 assert(portable_time_format_save(&kv_api,0) && !format_writes);
 for(unsigned chosen=0;chosen<2;++chosen){
  record(chosen);assert(portable_time_format_load(&kv_api,&mode)==0 && mode==chosen);
  assert(portable_time_format_save(&kv_api,chosen) && !format_writes);boundary_labels(mode);
 }
 for(unsigned size=1;size<=64;++size)if(size!=4){format_size=size;assert(portable_time_format_load(&kv_api,&mode)==PORTABLE_TIME_FORMAT_INVALID && mode==0);}
 for(unsigned field=0;field<4;++field){record(1);format_bytes[field]^=0xff;assert(portable_time_format_load(&kv_api,&mode)==PORTABLE_TIME_FORMAT_INVALID && mode==0);}
 assert(portable_time_format_save(&kv_api,0) && !format_writes);
 risc_key_value_v1 bad=kv_api;bad.api_version=2;assert(portable_time_format_load(&bad,&mode)==PORTABLE_TIME_FORMAT_UNAVAILABLE && mode==0);
 bad=kv_api;bad.struct_size=8;assert(!portable_time_format_save(&bad,1));
 bad=kv_api;bad.get=NULL;assert(portable_time_format_load(&bad,&mode)==PORTABLE_TIME_FORMAT_UNAVAILABLE);
 bad=kv_api;bad.put=NULL;record(1);assert(portable_time_format_load(&bad,&mode)==0 && mode==1);assert(!portable_time_format_save(&bad,0));
 assert(!portable_time_format_save(&kv_api,2) && !format_writes);
 format_size=0;time_format_mode=0;
}
int main(int argc,char **argv) {
 assert(argc==2);test_case=(unsigned)atoi(argv[1]);scenario=50+test_case;
 record_contract();
 if(test_case==2)format_write_error=true;
 if(test_case==3)scenario=43; /* Grant denied. */
 if(test_case==4 || test_case==11 || test_case==17){format_size=4;memset(format_bytes,0xff,4);}
 if(test_case==5 || test_case==12 || test_case==15 || test_case==16)record(1);
 if(test_case==16)format_write_error=true;
 if(test_case==6)format_read_error=true;
 if(test_case==7)format_committed_error=true;
 if(test_case==8)format_verify_error=true;
 if(test_case==9)format_mismatch=true;
 bool cancel=test_case==1 || test_case==11 || test_case==14 || test_case==18;
 bool choose12=test_case==5 || test_case==10 || test_case==16 || test_case==17;
#ifdef PORTABLE_INPUT_NAVIGATION
 if(test_case==13 || test_case==14)nav_api.poll=format_navigation;
 else
#endif
 {
  tap(3,100,choose12?92:140);
  if(test_case==18){tap(7,40,170);tap(8,120,170);} /* Right-swipe Back. */
  else {tap(7,cancel?60:170,213);tap(8,cancel?60:170,213);} /* Held Save must not replay. */
 }
 if(test_case==2 || test_case==3 || test_case==15 || test_case==16 || (test_case>=6 && test_case<=9))tap(12,60,213);
 assert(app_module_init()==0);settings_row_hours();settings_render(0,0);polls=0;
 if(test_case==15)format_read_error=true;
 unsigned old_mode=time_format_mode;const char *zone=portable_time_zone(),*basis=portable_time_basis();
 t5_app_setting_t row;assert(settings_get(0,2,&row) && row.type==T5_APP_SETTING_ACTION);
 uint8_t result=settings_activate(0,2);
 bool saved=test_case==0 || test_case==4 || test_case==5 || test_case==10 || test_case==12 || test_case==13 || test_case==17;
 assert((result==T5_APP_SETTING_UPDATED)==saved);
 if(saved)assert(time_format_mode==(choose12?0:1));
 else assert(time_format_mode==old_mode);
 bool wrote=test_case==0 || test_case==2 || test_case==4 || test_case==5 || (test_case>=7 && test_case<=9) || test_case==13 || test_case==16;
 assert(format_writes==(wrote?1u:0u));
 if(test_case==2 || test_case==3 || test_case==15 || test_case==16 || (test_case>=6 && test_case<=9)) {
  assert(!strcmp(time_format_message,"Save unconfirmed - retry"));
  assert(!strcmp(settings_message,"Save unconfirmed - retry"));assert(time_format_choice==old_mode);
 }
 assert(!writes && !strcmp(zone,portable_time_zone()) && !strcmp(basis,portable_time_basis()));
 assert(sv_page==SV_ROOT);app_module_fini();assert(!grants && !subscriptions && !frame_count);
 if(saved) {
  unsigned loaded=99;int status=portable_time_format_load(&kv_api,&loaded);
  assert((status==PORTABLE_TIME_FORMAT_LOADED || status==PORTABLE_TIME_FORMAT_MISSING || status==PORTABLE_TIME_FORMAT_INVALID) && loaded==(choose12?0:1));
  polls=0;assert(app_module_init()==0);assert(time_format_mode==loaded);
  boundary_labels(loaded);
  assert(settings_get(0,0,&row));twatch_rtc_time_v1 local;
  assert(portable_time_forward(&stored,&local));char expected[24];
  if(loaded==PORTABLE_TIME_FORMAT_24)snprintf(expected,sizeof(expected),SETTINGS_HOUR_SPEC ":%02u",(unsigned)local.hour,(unsigned)local.minute);
  else snprintf(expected,sizeof(expected),"%u:%02u %s",local.hour%12?local.hour%12:12,(unsigned)local.minute,local.hour<12?"AM":"PM");
  assert(!strcmp(row.value,expected));app_module_fini();assert(!grants && !subscriptions && !frame_count);
 }
 printf("Time format scenario %u: UI, persistence, consumer labels and unchanged RTC/timezone passed\n",test_case);
}
