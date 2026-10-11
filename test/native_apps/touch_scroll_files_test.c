#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
extern const portable_touch_scroll *scroll_files_state(void);
extern void scroll_files_coordinate_checks(void);
extern unsigned scroll_files_mode(void),scroll_files_cache(void),scroll_files_selected(void),scroll_files_generation(void),scroll_files_complete_generation(void);
extern int scroll_files_complete_q8(void);
extern const char *scroll_files_name(void),*scroll_files_path(void),*scroll_files_editor(void),*scroll_files_query(void);
extern bool scroll_files_cleanup(void);
static const char *scroll_case,*scroll_frames;
/* A queue GAP cancels derived gestures; only an uncertain error retains. */
static unsigned event_errors,cancelled_samples,recovered_samples;
static const void *scroll_volume_grant,*scroll_handlers_grant;
static unsigned started,submitted,delay_ms=40,home_sent,back_sent,nav_step,next_navigation=150,confirm_sent;
static unsigned background_generation,shown,drag_frames,momentum_frames,busy_samples,quick_frames,directory_opens,directory_entries,directory_index,handler_requests;
static bool keyboard_checked,pending,first_tap,sent_up,have_background,selected_seen,close_failed;
static int maximum_offset,release_offset,minimum_offset=1000000,expected_row=-1;
static uint8_t pending_pixels[sizeof(pixels)],background[sizeof(pixels)];
static bool scase(const char *s){return !strcmp(scroll_case,s);}
static unsigned elapsed(void){return started&&ticks>=started?ticks-started:0;}
static bool keyboard_case(void){return scase("keyboard")||scase("keyboard-busy")||scase("rename-keyboard");}
static bool handlers_case(void){return scase("handlers")||scase("handler-change");}
static unsigned wanted_mode(void){return scase("rename-keyboard")?6:keyboard_case()?2:scase("options")?1:scase("actions")?5:scase("destination")?7:handlers_case()?9:0;}
static bool list_page(void){return scroll_files_mode()==wanted_mode();}
static bool scroll_health(risc_runtime_health_v1 *out){io();assert(ticks<16000);out->uptime_ms=ticks;return true;}
static bool scroll_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool scroll_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!pending&&!raster_clip_active);if(dirs)assert(scroll_files_cleanup());
 const portable_touch_scroll *s=scroll_files_state();
 if(list_page()&&!started)started=ticks+delay_ms+80;
 if(list_page()&&!quick_modal){
  ++shown;unsigned at=elapsed();
  if(started&&ticks>=started&&at>=80&&at<600)++drag_frames;
  if(started&&ticks>=started&&((at>=600&&at<950)||(height()==600&&at>=1400&&at<1750)))++momentum_frames;
  if(!have_background||background_generation!=scroll_files_generation()){memcpy(background,pixels,sizeof(pixels));have_background=true;background_generation=scroll_files_generation();}
  if(!keyboard_case()&&!scase("read-error")&&!scase("cleanup")&&!scase("directory-change")&&!scase("destination")&&!scase("handler-change"))for(int y=0;y<height();++y)if(y<s->view.y||y>=s->view.y+s->view.height)
   for(int x=0;x<width();++x){int px=y,py=info.height-1-x;
#ifdef PORTABLE_PAPER_PREFERENCES
if(paper_flip_ui){px=info.width-1-px;py=info.height-1-py;}
#endif
size_t at=(size_t)py*(TEST_TOOLBAR_NATIVE_WIDTH/8)+(unsigned)px/8;if((pixels[at]^background[at])&(0x80u>>(px&7))){fprintf(stderr,"outside viewport pixel x%d y%d mode%u offset%d time%u\n",x,y,scroll_files_mode(),portable_scroll_offset(s),ticks);abort();}}
 }
 if(quick_modal){++quick_frames;assert(!s->contact&&!s->velocity_q8);}
 if(scroll_frames){char path[1024];snprintf(path,sizeof(path),"%s/%03u-mode-%u-offset-%d.pbm",scroll_frames,presents,scroll_files_mode(),portable_scroll_offset(s));FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n%d %d\n",TEST_TOOLBAR_NATIVE_WIDTH,TEST_TOOLBAR_NATIVE_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
 bool ok=fx_submit(c,f,r,n,options,token);pending=true;submitted=ticks;memcpy(pending_pixels,pixels,sizeof(pixels));return ok;
}
static bool scroll_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&token==presents&&!memcmp(pending_pixels,pixels,sizeof(pixels)));
 if(ticks-submitted<delay_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 pending=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool scroll_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();if(dirs)assert(scroll_files_cleanup());*out=(risc_input_navigation_frame_v1){0};unsigned b=0;
 if(!started&&ticks>=next_navigation){
  if(scase("rename-keyboard")&&nav_step<5){b=nav_step==0||nav_step==4?RISC_NAV_CONFIRM:RISC_NAV_DOWN;++nav_step;}
  if(scase("actions")&&!nav_step){b=RISC_NAV_CONFIRM;++nav_step;}
  if(handlers_case()&&nav_step<3){b=nav_step==1?RISC_NAV_DOWN:RISC_NAV_CONFIRM;++nav_step;}
  if(scase("destination")&&nav_step<39){b=nav_step==32||nav_step==38?RISC_NAV_CONFIRM:RISC_NAV_DOWN;++nav_step;}
  next_navigation=ticks+80;
 }
 unsigned at=elapsed();if(started&&ticks>=started){
  if(scase("confirm-hidden")&&!confirm_sent&&at>=1400){b=RISC_NAV_CONFIRM;++confirm_sent;}
  if(scase("confirm-hidden")&&confirm_sent==1&&at>=1600){assert(scroll_files_mode()==0);b=RISC_NAV_CONFIRM;++confirm_sent;expected_row=0;}
  if(keyboard_case()&&!back_sent&&at>=2900){b=RISC_NAV_BACK;++back_sent;}
  if(scase("reentry")&&!back_sent&&at>=1800){b=RISC_NAV_BACK;++back_sent;}
  if((scase("back")||scase("modal"))&&!back_sent&&at>=(scase("back")?180u:1500u)){b=RISC_NAV_BACK;++back_sent;}
  if(!home_sent&&at>=(scase("home")?180u:keyboard_case()?3500u:2600u)){b=RISC_NAV_HOME;++home_sent;}
 }
 if(scase("cancelled")&&event_errors){
  if(!input_sample.valid||input_sample.cancelled)++cancelled_samples;
  if(elapsed()>=230&&elapsed()<300){const portable_touch_scroll *s=scroll_files_state();assert(!s->contact&&!s->velocity_q8);++recovered_samples;}
 }
 out->buttons=out->pressed=b;return true;
}
static int32_t scroll_next(void *c,uint64_t token,risc_touch_event_v1 *out){
 (void)c;(void)token;io();
 if(scase("queued-up")&&!sent_up&&started&&elapsed()>=600){sent_up=true;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=(uint16_t)(width()/2),.y=122};
#ifdef TEST_READER_FLIP
 out->x=width()-1-out->x;out->y=height()-1-out->y;
#endif
 return 1;}
 if((scase("cancelled")||scase("touch-retained"))&&started&&elapsed()>=180&&elapsed()<205){++event_errors;return scase("touch-retained")?-2:-1;}
 return 0;
}
static bool scroll_snapshot_unflipped(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};if(!started||ticks<started){
  if(keyboard_case()&&!scase("rename-keyboard")&&((ticks>=600&&ticks<675)||(ticks>=1300&&ticks<1375))){
   out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)(ticks<1000?width()-75:width()/2),.y=(uint16_t)(ticks<1000?50:156)};
  }
  if(scase("options")&&ticks>=150&&ticks<225){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)(width()-75),.y=50};}
  return true;
 }
 const portable_touch_scroll *s=scroll_files_state();unsigned at=elapsed();int offset=portable_scroll_offset(s);
 if(getenv("SCROLL_TRACE")&&at<1800)fprintf(stderr,"t%u offset%d v%d selected%u mode%u cache%u gen%u complete%u\n",at,offset,s->velocity_q8,scroll_files_selected(),scroll_files_mode(),scroll_files_cache(),scroll_files_generation(),scroll_files_complete_generation());
 if(list_page()){assert(offset>=0&&offset<=s->limit);if(offset>maximum_offset)maximum_offset=offset;if(offset<minimum_offset)minimum_offset=offset;if(at<600)release_offset=offset;}
 if(wanted_mode()==0&&!list_page()&&expected_row>=0&&!selected_seen){char want[32];snprintf(want,sizeof(want),"File %03d.txt",expected_row);assert(!strcmp(scroll_files_name(),want));selected_seen=true;}
 bool down=false;int x=width()/2,y=112+s->view.height-20,id=1;
 /* Two strokes cross the 16-row cache boundary. A long momentum coast then
  * exercises the moving cache without retaining an open directory handle. */
 if(at>=80&&at<300){down=true;y-=(int)(at-80)*2;if(y<120)y=120;}
 if(at>=350&&at<600){down=true;y-=(int)(at-350)*2;if(y<120)y=120;}
 if(scase("horizontal")&&down){x=100+(int)(at<300?at-80:at-350);y=220;}
 if(scase("footer-drag")&&down)y=height()-90-(int)(at<300?at-80:at-350);
 if(scase("replaced")&&at>=180&&at<300)id=2;
 if((scase("drag")||scase("cache-reverse")||scase("cleanup")||scase("read-error")||scase("inventory-change"))&&height()==600&&at>=1000&&at<1180){down=true;y=412-(int)(at-1000)*3/2;}
 if((scase("drag")||scase("cache-reverse")||scase("cleanup")||scase("read-error")||scase("inventory-change"))&&height()==600&&at>=1220&&at<1400){down=true;y=412-(int)(at-1220)*3/2;}
 if(scase("bounds")&&at>=800&&at<1050){down=true;y=130+(int)(at-800);}
 if(scase("cache-reverse")&&at>=1700&&at<1950){down=true;y=130+(int)(at-1700)*3/2;}
 if(scase("reverse")&&at>=700&&at<1000){down=true;y=130+(int)(at-700);}
 if((scase("stop-tap")||scase("busy-hit"))&&at>=625&&at<700){down=true;y=152;}
 if(scase("modal")&&at>=800&&at<1100){down=true;y=20+(int)(at-800)*2;}
 bool select=scase("reentry")||scase("options")||scase("select")||scase("busy-hit")||scase("actions")||scase("destination")||handlers_case()||scase("directory-change");unsigned choose_at=scase("busy-hit")?750:1400;
 if(select&&at>=choose_at&&at<choose_at+75){
  down=true;y=(scase("actions")||scase("options"))?112+s->view.height-20:212;
  if(!first_tap){first_tap=true;expected_row=(y-112+scroll_files_complete_q8()/256)/88;}
 }
 if(keyboard_case()){
  if(at>=1400&&at<1475){down=true;x=32+(width()-64)/5+10;y=240;}
  if(at>=1600&&at<1675){down=true;x=60;y=height()-172;}
  if(at>=1750&&at<1825){down=true;x=32+(width()-64)/5+10;y=240;}
  if(scase("keyboard-busy")&&at>=2700&&at<2775){down=true;x=32+(width()-64)/5+10;y=240;}
  if(at>=2825&&!keyboard_checked){
   char want[128];unsigned keys=(unsigned)(height()-448)/88;if(!keys)keys=1;keys*=5;
   snprintf(want,sizeof(want),"%s!%c",scase("rename-keyboard")?"File 000.txt":"",33+(int)keys);
   assert(!strcmp(scroll_files_editor(),want)&&!scroll_files_query()[0]);keyboard_checked=true;
  }
 }
 if(down){if(pending)++busy_samples;out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}return true;
}
static bool scroll_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 bool ok=scroll_snapshot_unflipped(c,out);
#ifdef TEST_READER_FLIP
 assert(paper_flip_ui);
 for(unsigned i=0;i<out->contact_count;++i){out->contacts[i].x=out->width-1-out->contacts[i].x;out->contacts[i].y=out->height-1-out->contacts[i].y;}
#endif
 return ok;
}

static risc_storage_dir_t scroll_dir(void *c,const char *path){(void)c;io();assert(!dirs&&path[0]=='/');++dirs;++directory_opens;directory_index=0;return 1;}
static bool scroll_dir_next(void *c,risc_storage_dir_t h,risc_storage_dirent_v1 *out){
 (void)c;io();assert(h==1&&dirs);unsigned total=scase("empty")?0:scase("destination")?48:scase("bounds")?8:96;if(directory_index>=total)return false;
 unsigned i=total-1-directory_index++;memset(out,0,sizeof(*out));
 if(scase("destination")&&i<32){snprintf(out->name,sizeof(out->name),"Folder %03u",i);out->is_directory=1;}
 else {unsigned num=scase("destination")?i-32:i;snprintf(out->name,sizeof(out->name),"File %03u.txt",num);out->size=100+num;}
 if(scase("inventory-change")&&directory_opens>=2&&i==50)strcpy(out->name,"Changed 050.txt");
 ++directory_entries;return true;
}
static bool scroll_dir_close(void *c,risc_storage_dir_t h){
 (void)c;io();assert(h==1&&dirs);
 if(scase("cleanup")&&directory_opens>=2&&!close_failed){close_failed=true;return false;}
 --dirs;return true;
}
static uint32_t scroll_handle_error(void *c,uint32_t h,bool dir){(void)c;(void)h;(void)dir;io();return scase("read-error")&&directory_opens>=2?1:0;}
static bool scroll_stat(void *c,const char *path,uint64_t *size,bool *dir){
 (void)c;io();if(scase("directory-change")&&started&&elapsed()>=1400)return false;
 const char *base=strrchr(path,'/');base=base?base+1:path;
 *dir=!strncmp(base,"Folder",6);*size=*dir?0:100+(unsigned)atoi(base+5);return true;
}
static bool scroll_rename(void *c,const char *from,const char *to){(void)c;(void)from;(void)to;assert(!"Keyboard cancellation must not rename");return false;}
static bool scroll_remove(void *c,const char *path){(void)c;(void)path;assert(!"Scrolling must not delete a file");return false;}
static unsigned scroll_handler_count(const char *path){(void)path;io();return 96;}
static bool scroll_handler_get(const char *path,unsigned i,t5_file_handler_t *out){
 (void)path;io();memset(out,0,sizeof(*out));snprintf(out->app_id,sizeof(out->app_id),"handler-%03u",i);snprintf(out->display_name,sizeof(out->display_name),"Handler %03u",i);if(scase("handler-change")&&started&&elapsed()>=1400)strcpy(out->app_id,"changed-handler");return true;
}
static bool scroll_handler_open(const char *path,const char *id,uint64_t cookie){
 io();assert(!strcmp(path,"/sd/File 000.txt")&&cookie==UINT64_C(0x4642524f57534552));char want[64];snprintf(want,sizeof(want),"handler-%03d",expected_row);assert(!strcmp(id,want));++handler_requests;return true;
}
static bool scroll_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,"storage.volume")){
  static risc_storage_volume_api_v1_ext a;a=(risc_storage_volume_api_v1_ext){0};a.base=volume;a.base.struct_size=sizeof(a);a.base.stat=scroll_stat;a.base.dir_open=scroll_dir;a.base.dir_next=scroll_dir_next;a.base.remove=scroll_remove;a.rename=scroll_rename;a.dir_close_checked=scroll_dir_close;a.handle_error=scroll_handle_error;
  assert(version==1&&instance==9);io();++acquires;++live;++storage_live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&a};scroll_volume_grant=&a;return true;
 }
 if(!app_acquire(name,version,instance,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=scroll_info;a.submit=scroll_submit;a.present_status=scroll_present;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.snapshot=scroll_snapshot;a.next=scroll_next;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=scroll_nav;out->api=&a;}
 if(!strcmp(name,"file.open")){static t5_file_open_api_v1 a;a=handlers;a.handler_count=scroll_handler_count;a.handler_get=scroll_handler_get;a.open_request=scroll_handler_open;out->api=&a;scroll_handlers_grant=&a;}
 return true;
}
static bool scroll_release(risc_runtime_capability_v1 *g){if(g->api==scroll_volume_grant){assert(storage_live&&!dirs);--storage_live;return source_release(g);}return app_release(g);}
static bool scroll_launch(const char *name){
 io();assert(!strcmp(name,(scase("back")||scase("horizontal"))?"springboard.elf":"default.elf"));assert(!storage_live&&!dirs);++launches;return true;
}
int main(int argc,char **argv){
 assert(argc==2||argc==3);scroll_case=argv[1];scroll_frames=argc>2?argv[2]:NULL;scenario="valid";delay_ms=(scase("busy-hit")||scase("keyboard-busy"))?500:40;
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=scroll_acquire;fx_runtime.release=scroll_release;fx_runtime.request_launch=scroll_launch;fx_runtime.health=scroll_health;
 assert(app_module_init()==0);const paper_presentation *retained_view=&pp_view;app_main();
 if(scase("touch-retained")||scase("cleanup")){
  assert(retained&&barriers==1&&!launches);
  /* Native uncertain cleanup keeps its owner pinned; retries are forbidden. */
  if(scase("cleanup"))assert(close_failed&&dirs==1&&storage_live==1);
  check_retained(retained_view);printf("{\"case\":\"%s\",\"retained\":true}\n",scroll_case);return 0;
 }
 const portable_touch_scroll *s=scroll_files_state();
 assert(!failed&&!retained&&!pending&&!s->contact&&!s->velocity_q8&&!raster_clip_active);
 assert(!kv_writes&&!rtc_reads&&!rtc_writes&&!dirs&&!storage_live);
 if(scase("handlers"))assert(handler_requests==1&&!launches);else assert(launches==1);
 if(scase("handler-change"))assert(!handler_requests);
 if(scase("directory-change"))assert(!selected_seen);
 if(scase("reentry")||scase("confirm-hidden")||scase("select")||scase("busy-hit"))assert(selected_seen&&expected_row>=0);
 if(scase("drag")){assert(drag_frames>=2&&momentum_frames>=1&&maximum_offset>release_offset&&directory_opens>=2);}
 if(scase("busy-hit"))assert(busy_samples>10&&shown<15);
 if(scase("queued-up"))assert(sent_up&&maximum_offset>200);
 if(scase("stop-tap"))assert(!selected_seen);
 if(scase("modal"))assert(quick_frames>0&&!quick_modal);
 if(scase("footer-drag"))assert(maximum_offset==0);
 if(scase("bounds"))assert(maximum_offset==8*88-(height()-168-112)&&minimum_offset==0);
 if(scase("actions"))assert(scroll_files_mode()==8);
 if(scase("options"))assert(scroll_files_mode()==10);
 if(scase("cache-reverse"))assert(directory_opens>=3&&portable_scroll_offset(s)<maximum_offset-300);
 if(scase("empty"))assert(!maximum_offset&&directory_opens==1);
 if(scase("inventory-change"))assert(directory_opens>=3&&scroll_files_generation()>3&&portable_scroll_offset(s)==0);
 if(scase("reentry"))assert(back_sent&&scroll_files_selected()==(unsigned)expected_row);
 if(scase("destination")){char want[64];snprintf(want,sizeof(want),"/Folder %03d",expected_row);assert(!strcmp(scroll_files_path(),want));}
 if(keyboard_case())assert(keyboard_checked&&back_sent);
 if(scase("coordinates"))scroll_files_coordinate_checks();
 if(scase("cancelled"))assert(event_errors&&cancelled_samples&&recovered_samples);
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"case\":\"%s\",\"frames\":%u,\"drag_frames\":%u,\"momentum_frames\":%u,\"max_offset\":%d,\"release_offset\":%d,\"busy_samples\":%u,\"selected_row\":%d,\"directory_opens\":%u}\n",scroll_case,shown,drag_frames,momentum_frames,maximum_offset,release_offset,busy_samples,expected_row,directory_opens);
 return 0;
}
