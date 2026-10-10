/* Production updater, raw input reducer and display custody. This deliberately
 * observes software state before the first slow/never-completing image. */
#define UPDATE_SCROLL_NO_MAIN
#include "touch_scroll_updates_test.c"
#include "logical_touch_queue_fixture.h"
static unsigned latency,edge_buttons,confirmation_at,verified_at,busy_inputs;
static bool verified,never_complete;
static int contact_y;
static bool changed_case(void){return tc("changed")||tc("version")||tc("remove");}
static bool touch_case(void){return tc("touch")||changed_case()||tc("rapid-taps");}
static unsigned pulse(unsigned start,unsigned count,unsigned button){return ticks>=start&&ticks<start+count*60&&(ticks-start)%60<20?button:0;}
static bool logic_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 assert(!pending&&!raster_clip_active);bool ok=fx_submit(c,f,r,n,o,t);pending=true;submitted=ticks;memcpy(pending_pixels,pixels,sizeof(pixels));return ok;
}
static bool logic_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&t==presents&&!memcmp(pending_pixels,pixels,sizeof(pixels)));
 out->state=never_complete||ticks-submitted<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)pending=false;return true;
}
static void observe(void){
 if(confirming&&!confirmed){
  assert(selected==(unsigned)expected_row&&!strcmp(selected_row.id,expected_id)&&!strcmp(selected_row.version,expected_version));
  confirmed=true;confirmation_at=ticks;
 }
 if(tc("confirm-version")&&confirmed&&ticks>=1120&&!mutated){++catalog_version;mutated=true;}
 if(ticks<1550||verified)return;
 verified=true;verified_at=ticks;
 if(tc("drag")){assert(!confirmed&&portable_scroll_offset(&ups_scroll)>=200&&!ups_scroll.velocity_q8);}
 else if(changed_case()){assert(mutated&&!confirmed&&selected==0&&!begins);}
 else if(tc("rapid-taps")){assert(selected==3&&!confirmed&&!begins);}
 else if(tc("confirm-version")){assert(confirmed&&mutated&&!confirming&&!install_after_connect&&!begins);}
 else if(tc("cancel")){assert(confirmed&&!confirming&&!begins);}
 else {assert(confirmed&&confirmation_at<1100);assert(begins==(tc("install")?1u:0u));}
 if(never_complete||latency==2300)assert(pending&&presents==1);
}
static bool logic_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();assert(ticks<14000);observe();unsigned b=0;
 if(!tc("drag")&&!tc("rapid-taps"))b=pulse(100,12,RISC_NAV_DOWN);
 if(!tc("drag")&&!tc("rapid-taps")&&!changed_case())b|=pulse(touch_case()?1040:900,1,RISC_NAV_CONFIRM);
 if(tc("install")||tc("confirm-version"))b|=pulse(1100,1,RISC_NAV_DOWN)|pulse(1200,1,RISC_NAV_CONFIRM);
 if(tc("cancel"))b|=pulse(1100,1,RISC_NAV_CONFIRM);
 if(ticks>=2800&&ticks<2820)b=RISC_NAV_HOME;
 *out=(risc_input_navigation_frame_v1){.buttons=b,.pressed=b&~edge_buttons,.released=edge_buttons&~b};edge_buttons=b;return true;
}
static bool logic_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};
 bool down=false;int x=width()/2,y=0;
 if(tc("drag")&&ticks>=100&&ticks<300){down=true;y=UPS_TOP+240-(int)(ticks-100);}
 if(touch_case()&&!tc("rapid-taps")&&ticks>=900&&ticks<980){
  down=true;if(!contact_y){contact_y=portable_scroll_row_y(&ups_scroll,11)+24;assert(portable_scroll_inside(&ups_scroll.view,x,contact_y));}y=contact_y;
  if(changed_case()&&ticks>=940&&!mutated){mutated=true;if(tc("remove"))catalog_count=1;else if(tc("version"))++catalog_version;else catalog_shift=50;}
 }
 if(tc("rapid-taps")&&ticks>=100&&ticks<1060){unsigned at=(ticks-100)%60,row=(ticks-100)/60%4;
  if(at<20){down=true;y=UPS_TOP+(int)row*UPS_EXTENT+20;}}
 if(down){if(pending)++busy_inputs;out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};
#ifdef TEST_READER_FLIP
  assert(paper_flip_ui);out->contacts[0].x=out->width-1-out->contacts[0].x;out->contacts[0].y=out->height-1-out->contacts[0].y;
#endif
 }
 return true;
}
static bool logic_acquire(const char *name,uint32_t v,uint64_t i,risc_runtime_capability_v1 *out){
 if(!test_acquire(name,v,i,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=test_info;a.submit=logic_submit;a.present_status=logic_status;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.poll=logical_raw_poll;a.snapshot=logical_raw_snapshot;a.next=logical_raw_next;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=logic_nav;out->api=&a;}
 return true;
}
int main(int argc,char **argv){
 assert(argc==3);test_case=argv[1];never_complete=!strcmp(argv[2],"never");latency=never_complete?0:(unsigned)atoi(argv[2]);scenario="valid";
 expected_row=touch_case()?11:12;snprintf(expected_id,sizeof(expected_id),"app-%02u",(unsigned)expected_row-1);strcpy(expected_version,"1.1.0");
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=logic_acquire;fx_runtime.release=test_release;fx_runtime.request_launch=test_launch;fx_runtime.health=test_health;
 portable_wifi_credentials saved={.ssid="Test network"};assert(portable_wifi_credentials_save(&saved_kv,&saved)==0);
 logical_raw_init(TEST_TOOLBAR_NATIVE_HEIGHT,TEST_TOOLBAR_NATIVE_WIDTH,logic_touch);
 assert(app_module_init()==0);app_main();assert(verified&&verified_at<1700&&!activations);
 assert(logical_raw_head==logical_raw_tail);
 if(never_complete){assert(retained&&barriers==1&&pending&&presents==1&&!launches);app_module_fini();}
 else {assert(!failed&&!retained&&!pending&&launches==1);app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);}
 uint32_t raster_hash=2166136261u;for(size_t i=0;i<sizeof(pending_pixels);++i)raster_hash=(raster_hash^pending_pixels[i])*16777619u;
 printf("{\"firmware\":%d,\"case\":\"%s\",\"latency\":\"%s\",\"verified_at\":%u,\"confirmed_at\":%u,\"frames\":%u,\"busy_inputs\":%u,\"begins\":%u,\"retained\":%s,\"raster_hash\":%u}\n",PORTABLE_UPDATE_FIRMWARE,test_case,argv[2],verified_at,confirmation_at,presents,busy_inputs,begins,retained?"true":"false",raster_hash);return 0;
}
