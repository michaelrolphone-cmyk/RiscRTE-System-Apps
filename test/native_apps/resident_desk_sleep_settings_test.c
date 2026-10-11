/* Selected resident Settings entry, controller, records and real paper rasters.
 * Only capability providers are simulated. Editor changes use physical input. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main

static const char *const preference_keys[]={PORTABLE_DESK_FACE_KEY,PORTABLE_DESK_DIRECTION_KEY,
  PORTABLE_SLEEP_IDLE_KEY,PORTABLE_SLEEP_DEEP_KEY,PORTABLE_SLEEP_KEY};
static uint8_t preference_records[5][5];
static unsigned preference_sizes[5],preference_reads[5],preference_writes[5],page_visits[32];
static unsigned choice_seen,seconds_seen,root_frames;
static bool read_fault,write_fault,verify_fault,saw_unconfirmed;
static int preference_index(const char *key) {
  for(unsigned i=0;i<5;++i)if(!strcmp(key,preference_keys[i]))return (int)i;
  return -1;
}
static int32_t preferences_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size) {
  int i=preference_index(key);if(i<0)return kv_get(ctx,key,out,cap,size);
  io();assert(ctx==leases&&kind_live(K_KV));++preference_reads[i];*size=preference_sizes[i];
  if(read_fault&&i<3)return RISC_KEY_VALUE_IO;
  if(!*size)return RISC_KEY_VALUE_NOT_FOUND;
  if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
  memcpy(out,preference_records[i],*size);
  if(verify_fault&&preference_writes[i])((uint8_t *)out)[*size-1]^=1;
  return RISC_KEY_VALUE_OK;
}
static int32_t preferences_put(void *ctx,const char *key,const void *data,uint32_t size) {
  int i=preference_index(key);if(i<0)return kv_put(ctx,key,data,size);
  io();assert(ctx==leases&&kind_live(K_KV)&&i<3&&size==(i==2?5u:4u));++preference_writes[i];
  if(write_fault)return RISC_KEY_VALUE_IO;
  memcpy(preference_records[i],data,size);preference_sizes[i]=size;return RISC_KEY_VALUE_OK;
}
static bool preferences_show(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  assert(sv_page<32);++page_visits[sv_page];
  /* Every Settings interaction, including full replacement and scrolling, uses
   * the fast intent selected by the production adapter. */
  assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
  if(sv_page==SV_ROOT) {
    ++root_frames;assert(width()==LOGICAL_WIDTH&&height()==LOGICAL_HEIGHT);
    assert(sp_scroll[0].view.y==112&&sp_scroll[0].view.height==LOGICAL_HEIGHT-144);
  }
  if(sv_page==SV_DESK_FACE)choice_seen|=1u<<face_choice;
  if(sv_page==SV_DESK_DIRECTION)choice_seen|=1u<<desk_direction_choice;
  if(sv_page==SV_TIMER)seconds_seen=timer_choice/1000u;
  saw_unconfirmed|=face_unconfirmed||desk_direction_unconfirmed||strstr(timer_message,"unconfirmed")!=NULL;
  return frame_show(ctx,frame,damage,count,options,token);
}
static bool preferences_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
  if(!acquire(name,version,instance,out))return false;
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
    static risc_key_value_v1 api;api=kv_api;api.get=preferences_get;api.put=preferences_put;out->api=&api;
  } else if(!strcmp(name,"display.output")) {
    static risc_display_output_api_v1 api;api=display_api;api.submit=preferences_show;out->api=&api;
  }
  return true;
}
static bool preferences_launch(const char *path) {
  io();assert(!strcmp(path,PORTABLE_RETURN_APP)||!strcmp(path,PORTABLE_HOME_APP));++launches;return true;
}
static void record_choice(unsigned i,unsigned value) {
  preference_records[i][0]=i==0?0x46:0x44;preference_records[i][1]=1;
  preference_records[i][2]=(uint8_t)value;preference_records[i][3]=(uint8_t)(value^0xa5);
  preference_sizes[i]=4;
}
static void record_seconds(unsigned i,unsigned seconds) {
  preference_records[i][0]=0x54;preference_records[i][1]=1;
  preference_records[i][2]=(uint8_t)seconds;preference_records[i][3]=(uint8_t)(seconds>>8);
  preference_records[i][4]=(uint8_t)(seconds^(seconds>>8)^0xa5);preference_sizes[i]=5;
}
static void inputs(void){memset(nav_buttons,0,sizeof(nav_buttons));contact_count=0;next_at=polls+5;}
static void open_row(unsigned row) {
  inputs();for(unsigned i=0;i<=row;++i)nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);
}
int main(int argc,char **argv) {
  assert(argc==3);test_name=argv[1];unsigned value=(unsigned)atoi(argv[2]);zone_id="UTC";native_epoch=1768505696;
  configure_records();runtime.acquire=preferences_acquire;runtime.request_launch=preferences_launch;
  assert(PORTABLE_DESK_FACE_COUNT==7&&PORTABLE_DESK_FACE_DEFAULT==6&&SETTINGS_ROW_COUNT==11);
  assert(SETTINGS_FACE_ROW==6&&SETTINGS_DESK_DIRECTION_ROW==9&&SETTINGS_TIMER_ROW==10);
  /* Preserve an old Hybrid preference and a independently edited deep timer. */
  preference_records[4][0]=0x53;preference_records[4][1]=1;preference_records[4][2]=2;
  preference_records[4][3]=2^0xa5;preference_sizes[4]=4;record_seconds(3,720);
  unsigned row=SETTINGS_FACE_ROW,index=0,page=SV_DESK_FACE;
  bool timer=!strncmp(test_name,"timer-",6),direction=!strncmp(test_name,"direction-",10);
  bool saved=strstr(test_name,"save")!=NULL,loaded=strstr(test_name,"read")!=NULL;
  bool cancel=strstr(test_name,"cancel")!=NULL,home=strstr(test_name,"home")!=NULL;
  if(timer){row=SETTINGS_TIMER_ROW;index=2;page=SV_TIMER;}
  if(direction){row=SETTINGS_DESK_DIRECTION_ROW;index=1;page=SV_DESK_DIRECTION;}
  if(loaded||which("timer-down")||which("timer-up")) {
    if(timer)record_seconds(index,value);else record_choice(index,value);
  }
  if(strstr(test_name,"invalid")){record_choice(index,0);preference_records[index][3]^=1;}
  if(strstr(test_name,"unavailable"))read_fault=true;
  write_fault=strstr(test_name,"write-fail")!=NULL;verify_fault=strstr(test_name,"verify-fail")!=NULL;
  uint8_t before[5][5];memcpy(before,preference_records,sizeof(before));
  assert(app_module_init()==0);in_main=true;
  const t5_app_api_v1 *api=t5_app_get_api(1);assert(api);
  const char *const labels[]={"Set Time","Time Zone","Time Format","RTC Basis","Top-right key","About",
    "Clock face","Flip UI 180°","Language","Desk orientation","Time to sleep"};
  for(unsigned i=0;i<SETTINGS_ROW_COUNT;++i) {
    t5_app_setting_t item;assert(api->settings_get(0,i,&item)&&!strcmp(item.label,labels[i]));
    if(i==4)assert(item.type==T5_APP_SETTING_VALUE&&!strcmp(item.value,"Desk clock"));
    if(i==row) {
      if(read_fault)assert(!strcmp(item.value,timer?"Unconfirmed":"Unavailable"));
      else if(loaded&&index==0)assert(!strcmp(item.value,portable_desk_face_name(value)));
      else if(loaded&&index==1)assert(!strcmp(item.value,settings_desk_direction_name(value)));
      else if(loaded&&timer){char text[32];snprintf(text,sizeof(text),"%u seconds",value);assert(!strcmp(item.value,text));}
      else if(!strcmp(test_name,"defaults"))assert(!strcmp(item.value,"Points in Time (default)"));
    }
  }
  if(which("root-positions")) {
    inputs();for(unsigned i=0;i<SETTINGS_ROW_COUNT;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_HOME);
  } else if(which("timer-bottom-tap")) {
    inputs();nav(RISC_NAV_UP);tap(LOGICAL_WIDTH/2,LOGICAL_HEIGHT-76);nav(RISC_NAV_BACK);nav(RISC_NAV_HOME);
  } else {
    open_row(row);
    if(saved||write_fault||verify_fault||cancel||home) {
      if(timer)tap(LOGICAL_WIDTH-88,290);
      else if(direction)tap(LOGICAL_WIDTH/2,value?300:180);
      else {unsigned columns=sp_face_columns();tap(32+(value%columns)*(sp_face_width()+16)+sp_face_width()/2,
          120+(value/columns)*sp_face_step()+(sp_face_step()-12)/2);}
      if(cancel)tap(88,FOOTER_Y);
      else if(home)nav(RISC_NAV_HOME);
      else tap(SAVE_X,FOOTER_Y);
      if(write_fault||verify_fault)nav(RISC_NAV_BACK);
      else if(!cancel&&!home){nav(RISC_NAV_CONFIRM);nav(RISC_NAV_BACK);} /* Reopen persisted selection. */
    } else if(which("timer-down")||which("timer-up")) {
      tap(which("timer-down")?88:LOGICAL_WIDTH-88,290);tap(SAVE_X,FOOTER_Y);
    } else nav(RISC_NAV_BACK);
    nav(RISC_NAV_HOME);
  }
  app_main();
  if(failed||retained||launches!=0||sv_page!=SV_ROOT||!resident_checks||!root_frames)fprintf(stderr,"failed=%u retained=%u launches=%u page=%u checkpoints=%u roots=%u polls=%u\n",failed,retained,launches,sv_page,resident_checks,root_frames,polls);
  assert(!failed&&!retained&&launches==0&&sv_page==SV_ROOT&&resident_checks>0&&root_frames>0);
  assert(!rtc_reads&&!rtc_writes&&!seeds&&!basis_puts&&!zone_puts&&!other_puts);
  assert(!preference_writes[3]&&!preference_writes[4]&&!preference_reads[3]&&!preference_reads[4]);
  assert(!memcmp(before[3],preference_records[3],5)&&!memcmp(before[4],preference_records[4],5));
  if(!which("root-positions"))assert(page_visits[page]);
  if(saved||write_fault||verify_fault||which("timer-down")||which("timer-up")) {
    unsigned expected=timer?(which("timer-down")?(value<=10?5:value-5):which("timer-up")?(value>=3595?3600:value+5):65):value;
    if(write_fault){assert(preference_writes[index]==1&&saw_unconfirmed&&!memcmp(before[index],preference_records[index],5));}
    else if(verify_fault)assert(preference_writes[index]==1&&saw_unconfirmed);
    else {
      assert(preference_writes[index]==((loaded||which("timer-down")||which("timer-up"))&&expected==value?0u:1u));
      unsigned stored=timer?(unsigned)preference_records[index][2]|((unsigned)preference_records[index][3]<<8):preference_records[index][2];
      assert(stored==expected&&!saw_unconfirmed);
      if(timer)assert(seconds_seen==expected);
    }
  } else for(unsigned i=0;i<5;++i)assert(!preference_writes[i]);
  if(!timer&&!direction&&!which("root-positions"))assert(choice_seen&(1u<<(loaded?value:6)));
  if(direction)assert(choice_seen&(1u<<(loaded?value:0)));
  in_main=false;in_fini=true;app_module_fini();in_fini=false;assert(!live&&!subscriptions&&!frames&&!barriers);
  printf("{\"case\":\"%s\",\"value\":%u,\"writes\":%u,\"choice_mask\":%u,\"timer_seconds\":%u,\"frames\":%u,\"resident_checkpoints\":%u}\n",
    test_name,value,preference_writes[index],choice_seen,seconds_seen,presents,resident_checks);
  return 0;
}
