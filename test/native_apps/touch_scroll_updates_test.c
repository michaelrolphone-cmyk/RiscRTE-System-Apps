/* Actual updater/controller, shared touch, renderer, async display and native
 * custody adapter. Synthetic providers only; no network or device actions. */
#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
#include "../../Apps/update_portable.inc"
bool native_system_test_open(void){return open_update();}
static const char *test_case,*frame_directory;
/* A queue GAP cancels derived gestures; only an uncertain error retains. */
static unsigned event_errors,cancelled_samples,recovered_samples;
static unsigned started,submitted,delay_ms=40,home_sent,back_sent,step_sent;
static unsigned shown,drag_frames,momentum_frames,busy_samples,quick_frames,begins,cancels,connects,activations;
static unsigned catalog_count=40,catalog_shift,catalog_version=1;
static uint32_t catalog_state=SOFTWARE_UPDATE_LIST;
static bool pending,mutated,chosen,confirmed,have_background,sent_up,radio_live,fail_cleanup;
static int maximum_offset,release_offset,expected_row=-1;
static char expected_id[65],expected_version[32];
static uint8_t pending_pixels[sizeof(pixels)],background[sizeof(pixels)];
static const void *test_service_grant,*test_radio_grant,*test_credentials_grant;
static bool tc(const char *s){return !strcmp(test_case,s);}
static unsigned elapsed(void){return started&&ticks>=started?ticks-started:0;}
static bool list_mode_now(void){return ups_mode==UPS_CATALOG&&ups_total==catalog_count+1;}
static bool test_health(risc_runtime_health_v1 *out){io();assert(ticks<15000);out->uptime_ms=ticks;return true;}
static bool test_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_CLEAN_PRESENT;return ok;}
static bool test_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!pending&&!raster_clip_active);
 if(list_mode_now()&&!started)started=ticks+delay_ms+80;
 if(list_mode_now()&&!quick_modal){++shown;if(tc("drag")&&shown>1)assert(options->intent!=RISC_DISPLAY_PRESENT_CLEAN&&n>0);unsigned at=elapsed();
  if(started&&ticks>=started&&at>=80&&at<280)++drag_frames;
  if(started&&ticks>=started&&at>=280&&at<800)++momentum_frames;
  if(!have_background){memcpy(background,pixels,sizeof(pixels));have_background=true;}
  if(tc("drag")||tc("footer-drag"))for(int y=0;y<height();y++)if(y<UPS_TOP||y>=UPS_TOP+UPS_HEIGHT)
   for(int x=0;x<width();x++){if(x>=width()-24&&x<width()-20)continue;int px=y,py=info.height-1-x;
#ifdef TEST_READER_FLIP
    px=info.width-1-px;py=info.height-1-py;
#endif
    size_t pos=(size_t)py*(TEST_TOOLBAR_NATIVE_WIDTH/8)+(unsigned)px/8;assert(!((pixels[pos]^background[pos])&(0x80u>>(px&7))));}
 }
 if(confirming){assert(selected>0);assert(!strcmp(selected_row.id,expected_id));assert(!strcmp(selected_row.version,expected_version));confirmed=true;}
 if(quick_modal){++quick_frames;/* Input cancels app momentum before another app frame. */}
 if(frame_directory){char path[1024];snprintf(path,sizeof(path),"%s/%03u-mode-%u-offset-%d.pbm",frame_directory,presents,ups_mode,portable_scroll_offset(&ups_scroll));FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n%d %d\n",TEST_TOOLBAR_NATIVE_WIDTH,TEST_TOOLBAR_NATIVE_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
 bool ok=fx_submit(c,f,r,n,options,token);pending=true;submitted=ticks;memcpy(pending_pixels,pixels,sizeof(pixels));return ok;
}
static bool test_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&token==presents&&!memcmp(pending_pixels,pixels,sizeof(pixels)));
 if(ticks-submitted<delay_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}pending=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool select_case(void){return tc("select")||tc("busy-hit")||tc("confirm-version")||tc("confirm-remove")||tc("install")||tc("connect-version")||tc("cancel")||tc("back-confirm")||tc("cancel-progress")||tc("touch-cancel-progress");}
static bool test_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};unsigned at=elapsed(),b=0;
 if(started&&ticks>=started){
  if((tc("bounds")||tc("maximum"))&&step_sent<3&&at>=80+step_sent*500){b=step_sent==1?RISC_NAV_DOWN:RISC_NAV_UP;++step_sent;}
  if(tc("confirm-hidden")&&step_sent<2&&at>=950+step_sent*300){b=RISC_NAV_CONFIRM;++step_sent;}
  if(tc("busy-hit")&&chosen&&!confirmed&&step_sent==1&&at>=2100){assert(selected==(unsigned)expected_row);b=RISC_NAV_CONFIRM;++step_sent;}
  if(select_case()&&chosen&&step_sent<1&&at>=1150){b=RISC_NAV_CONFIRM;++step_sent;}
  if(select_case()&&confirmed&&!tc("select")&&!tc("busy-hit")&&!tc("back-confirm")&&!tc("cancel")&&step_sent==1&&at>=1500){b=RISC_NAV_DOWN;++step_sent;}
  if(select_case()&&confirmed&&!tc("select")&&!tc("busy-hit")&&!tc("back-confirm")&&!tc("cancel")&&step_sent==2&&at>=1750){b=RISC_NAV_CONFIRM;++step_sent;}
  if((tc("back")||tc("back-confirm")||tc("modal"))&&!back_sent&&at>=(tc("back")?180u:2100u)){b=RISC_NAV_BACK;++back_sent;}
  if(tc("cancel-progress")&&step_sent==3&&at>=1900){b=RISC_NAV_CONFIRM;++step_sent;}
  if(!home_sent&&at>=(tc("home")?180u:3200u)){b=RISC_NAV_HOME;++home_sent;}
 }
 if(tc("cancelled")&&event_errors){
  if(!input_sample.valid||input_sample.cancelled)++cancelled_samples;
  if(elapsed()>=230&&elapsed()<300){const portable_touch_scroll *s=&ups_scroll;assert(!s->contact&&!s->velocity_q8);++recovered_samples;}
 }
 out->buttons=out->pressed=b;return true;
}
static void mutate(void){
 if(mutated)return;if(tc("replace")||tc("version")||tc("remove"))assert(pending&&ups_scroll.contact);mutated=true;
 if(tc("remove")||tc("confirm-remove"))catalog_count=1;
 else if(tc("replace"))catalog_shift=50;
 else ++catalog_version;
}
static int32_t test_next(void *c,uint64_t token,risc_touch_event_v1 *out){
 (void)c;(void)token;io();
 if(tc("queued-up")&&!sent_up&&started&&elapsed()>=280){sent_up=true;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=240,.y=UPS_TOP+10};
#ifdef TEST_READER_FLIP
 out->x=width()-1-out->x;out->y=height()-1-out->y;
#endif
 return 1;}
 if((tc("cancelled")||tc("touch-retained"))&&started&&elapsed()>=180&&elapsed()<205){++event_errors;return tc("touch-retained")?-2:-1;}
 return 0;
}
static bool test_snapshot(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};if(!started||ticks<started)return true;
 unsigned at=elapsed();int offset=portable_scroll_offset(&ups_scroll);
 if(connecting||confirming||cleanup_pending||install_after_connect||restart_pending||radio_owned||(ust.state>=SOFTWARE_UPDATE_CATALOG&&ust.state!=SOFTWARE_UPDATE_LIST&&ust.state<=SOFTWARE_UPDATE_READY))assert(!portable_update_idle_ready());
 assert(offset>=0&&offset<=ups_scroll.limit);assert(ups_scroll.velocity_q8>=-1024&&ups_scroll.velocity_q8<=1024);
 if(offset>maximum_offset)maximum_offset=offset;if(at<280)release_offset=offset;
 if(getenv("SCROLL_TRACE"))fprintf(stderr,"t%u off%d v%d sel%u mode%u gen%u done%u choice%u armed%d\n",at,offset,ups_scroll.velocity_q8,selected,ups_mode,ups_generation,ups_completed.generation,confirm_choice,armed);
 if((tc("replace")||tc("version")||tc("remove"))&&at>=170)mutate();
 if((tc("confirm-version")||tc("confirm-remove"))&&confirmed&&at>=1450)mutate();
 if(tc("connect-version")&&connecting)mutate();
 bool down=false;int x=240,y=UPS_TOP+220,id=1;
 if(at>=80&&at<280&&!tc("bounds")&&!tc("maximum")){
  down=true;if(tc("horizontal")){x=100+(int)(at-80);y=UPS_TOP+100;}
  else if(tc("footer-drag"))y=height()-90-(int)(at-80);
  else y-=(int)(at-80);
  if(tc("replaced-contact")&&at>=180)id=2;
 }
 if(tc("bounds")||tc("maximum")){if(at>=220&&at<420){down=true;y=UPS_TOP+220-(int)(at-220);}if(at>=800&&at<1000){down=true;y=UPS_TOP+20+(int)(at-800);}}
 if(tc("reverse")&&at>=320&&at<520){down=true;y=UPS_TOP+30+(int)(at-320);}
 if((tc("stop-tap")||tc("busy-hit"))&&at>=300&&at<375){down=true;y=UPS_TOP+40;}
 if(tc("modal")&&at>=400&&at<700){down=true;y=20+(int)(at-400)*2;}
 unsigned choose_at=tc("busy-hit")?925:950;
 if(select_case()&&at>=choose_at&&at<choose_at+75){down=true;y=UPS_TOP+150;
  if(!chosen){chosen=true;expected_row=(y-UPS_TOP+ups_completed.q8/256)/UPS_EXTENT;
   if(tc("busy-hit"))assert(expected_row!=(y-UPS_TOP+portable_scroll_offset(&ups_scroll))/UPS_EXTENT);
   snprintf(expected_id,sizeof(expected_id),"app-%02u",(unsigned)expected_row-1+catalog_shift);snprintf(expected_version,sizeof(expected_version),"1.%u.0",catalog_version);}}
 if(tc("cancel")&&confirming&&at>=1475&&at<1550){down=true;x=40;y=height()-70;}
 if(tc("touch-cancel-progress")&&at>=1850&&at<1925){down=true;x=280;y=height()-70;}
 if(down){if(pending)++busy_samples;out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};
#ifdef TEST_READER_FLIP
  assert(paper_flip_ui);out->contacts[0].x=out->width-1-out->contacts[0].x;out->contacts[0].y=out->height-1-out->contacts[0].y;
#endif
 }return true;
}
static bool catalog_status(void *c,software_update_status_v1 *out){(void)c;io();*out=(software_update_status_v1){.struct_size=sizeof(*out),.state=catalog_state,.count=catalog_count};return true;}
static bool catalog_get(void *c,uint32_t i,software_update_row_v1 *out){(void)c;io();assert(i<catalog_count);*out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_AVAILABLE,.size=1234};snprintf(out->id,sizeof(out->id),"app-%02u",i+catalog_shift);snprintf(out->version,sizeof(out->version),"1.%u.0",catalog_version);strcpy(out->installed,"1.0.0");strcpy(out->reason,"Update available");return true;}
static bool catalog_begin(void *c,uint32_t i,uint64_t seconds){(void)c;io();assert(seconds>1704067200&&i+1==(unsigned)expected_row&&confirmed);assert(!mutated);++begins;if(tc("cancel-progress")||tc("touch-cancel-progress"))delay_ms=500;catalog_state=SOFTWARE_UPDATE_DOWNLOAD;return true;}
static bool catalog_cancel(void *c){(void)c;io();++cancels;if(fail_cleanup){portable_adapter_retain();return false;}catalog_state=SOFTWARE_UPDATE_LIST;return true;}
static bool catalog_refresh(void *c,uint64_t seconds){(void)c;(void)seconds;io();++update_checks;return true;}
static bool catalog_step(void *c){(void)c;io();if((tc("cancel-progress")||tc("touch-cancel-progress"))&&catalog_state==SOFTWARE_UPDATE_DOWNLOAD&&elapsed()>=1950)catalog_state=SOFTWARE_UPDATE_READY;return true;}
static bool catalog_activate(void *c){(void)c;io();++activations;catalog_state=SOFTWARE_UPDATE_ACTIVATED;return true;}
static const software_update_v1 catalog_service={1,sizeof(catalog_service),NULL,catalog_refresh,catalog_step,catalog_status,catalog_get,catalog_begin,catalog_cancel,catalog_activate,volume_ok};
static struct {char key[16];uint8_t bytes[64];uint32_t size;} saved_cells[5];
static int32_t saved_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){(void)c;io();*size=0;for(unsigned i=0;i<5;i++)if(!strcmp(saved_cells[i].key,key)){assert(cap>=saved_cells[i].size);*size=saved_cells[i].size;memcpy(out,saved_cells[i].bytes,*size);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t saved_put(void *c,const char *key,const void *bytes,uint32_t size){(void)c;assert(size<=64);unsigned i=0;while(i<5&&saved_cells[i].key[0]&&strcmp(saved_cells[i].key,key))++i;assert(i<5);strcpy(saved_cells[i].key,key);memcpy(saved_cells[i].bytes,bytes,size);saved_cells[i].size=size;return 0;}
static const risc_key_value_v1 saved_kv={1,sizeof(saved_kv),NULL,saved_get,saved_put};
static bool radio_connect(void *c,const char *ssid,const char *password){(void)c;io();assert(!strcmp(ssid,"Test network")&&!password[0]&&!radio_live);++connects;radio_live=true;return true;}
static wifi_link_t radio_status(void *c){(void)c;io();return radio_live?WIFI_LINK_UP:WIFI_LINK_DOWN;}
static bool radio_addresses(void *c,wifi_ipv4_v1 *s,wifi_ipv4_v1 *a){(void)c;io();*s=(wifi_ipv4_v1){.address={192,0,2,10}};*a=(wifi_ipv4_v1){0};return true;}
static bool radio_disconnect(void *c){(void)c;io();radio_live=false;return true;}
static bool test_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!app_acquire(name,version,instance,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=test_info;a.submit=test_submit;a.present_status=test_present;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.snapshot=test_snapshot;a.next=test_next;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=test_nav;out->api=&a;}
 if(!strcmp(name,SOFTWARE_UPDATE_FIRMWARE_CAPABILITY)||!strcmp(name,SOFTWARE_UPDATE_APPS_CAPABILITY)){out->api=&catalog_service;test_service_grant=out->api;}
 if(!strcmp(name,"net.wifi")){static wifi_api_v1 a;a=app_wifi;a.connect=radio_connect;a.status=radio_status;a.addresses=radio_addresses;a.disconnect_checked=radio_disconnect;out->api=&a;test_radio_grant=out->api;}
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==6){out->api=&saved_kv;test_credentials_grant=out->api;}
 return true;
}
static bool test_release(risc_runtime_capability_v1 *g){if(g->api==test_service_grant)g->api=&update_service;if(g->api==test_radio_grant)g->api=&app_wifi;if(g->api==test_credentials_grant)g->api=&credential_kv;return app_release(g);}
static bool test_launch(const char *name){io();assert(!strcmp(name,"default.elf")||!strcmp(name,"springboard.elf"));assert(!opened&&!wifi_live&&!credentials_live&&!update_live&&!radio_live);++launches;return true;}

#ifndef UPDATE_SCROLL_NO_MAIN
int main(int argc,char **argv){
 assert(argc==2||argc==3);test_case=argv[1];frame_directory=argc>2?argv[2]:NULL;scenario="valid";
 delay_ms=tc("busy-hit")?500:40;if(tc("fit"))catalog_count=2;if(tc("maximum"))catalog_count=SOFTWARE_UPDATE_ROWS_MAX;
 fail_cleanup=tc("cleanup-retained");
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=test_acquire;fx_runtime.release=test_release;fx_runtime.request_launch=test_launch;fx_runtime.health=test_health;
 portable_wifi_credentials saved={.ssid="Test network"};assert(portable_wifi_credentials_save(&saved_kv,&saved)==0);
 assert(app_module_init()==0);app_main();
 if(tc("touch-retained")||tc("cleanup-retained")){assert(retained&&barriers==1&&!launches);app_module_fini();printf("{\"case\":\"%s\",\"retained\":true}\n",test_case);return 0;}
 assert(!failed&&!retained&&!pending&&launches==1&&!ups_scroll.contact&&!ups_scroll.velocity_q8&&!raster_clip_active);
 assert(!kv_writes&&!rtc_reads&&!rtc_writes);
 assert(!activations);
 if(tc("install")||tc("cancel-progress")||tc("touch-cancel-progress")){assert(begins==1&&connects==1);}else assert(!begins);
 if(tc("drag")||tc("select")){assert(drag_frames>=2&&momentum_frames>=1&&maximum_offset>release_offset);}
 if(tc("bounds")||tc("maximum"))assert(maximum_offset==ups_scroll.limit||maximum_offset>3000);
 if(tc("fit")||tc("footer-drag"))assert(maximum_offset==0);
 if(tc("busy-hit"))assert(busy_samples>10&&shown<20&&confirmed);
 if(tc("queued-up"))assert(sent_up&&maximum_offset>200);
 if(tc("select")||tc("confirm-version")||tc("confirm-remove")||tc("connect-version")||tc("back-confirm"))assert(confirmed);
 if(tc("replace")||tc("version")||tc("remove"))assert(mutated&&!confirmed&&selected==0);
 if(tc("confirm-version")||tc("confirm-remove")||tc("connect-version"))assert(mutated&&!confirming&&!install_after_connect);
 if(tc("connect-version"))assert(connects==1);
 if(tc("modal"))assert(quick_frames>0&&!quick_modal);
 if(tc("cancelled"))assert(event_errors&&cancelled_samples&&recovered_samples);
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"max_offset\":%d,\"release_offset\":%d,\"busy_samples\":%u,\"selected_row\":%d,\"begins\":%u}\n",test_case,shown,drag_frames,momentum_frames,maximum_offset,release_offset,busy_samples,expected_row,begins);return 0;
}

#endif
