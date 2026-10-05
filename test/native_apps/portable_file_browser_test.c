/* Actual portable controller, adapter and NOVA raster, with a fake volume. */
#ifdef FILE_BROWSER_FULL_PROFILE
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define PORTABLE_QUICK_ACTIONS
#define PORTABLE_QUICK_RADIOS
#endif
#define PORTABLE_FILE_BROWSER_APP
#define PORTABLE_NOVA_UI
#define PORTABLE_APP_OWNS_TOUCH_CHROME
#include "../../lib/PortableApps/src/adapter.c"
static const t5_app_api_v1 *test_browser_api(uint32_t);
#define t5_app_get_api test_browser_api
#include "../../Apps/file_browser.c"
#undef t5_app_get_api
#include <assert.h>
#include <stdlib.h>
static uint16_t fb_pixels[240*244];
static unsigned test_grants,test_frames,test_subs,test_presents,test_ticks,test_entry,test_entries=8,test_offset,test_length=577,test_reads,test_closes;
static unsigned test_polls,test_launches,test_script,test_seen_modes,test_wifi_launches;
static bool test_fail_acquire,test_fail_open,test_fail_read,test_fail_close,test_fail_release,test_fail_listing,test_invalid_name;
static const char *test_directory;
static char test_error[48];
static risc_storage_volume_api_v1 test_volume;
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};const unsigned portable_catalog_count=0;
static bool test_health(risc_runtime_health_v1*h){h->uptime_ms=test_ticks;return !test_script || test_polls<50;}
static void test_yield(uint32_t n){test_ticks+=n;}
static bool test_diag(const char*s){(void)s;return true;}
static bool test_launch(const char*s){assert(!strcmp(s,FILE_BROWSER_RETURN_APP) || !strcmp(s,"wifi_settings.elf"));if(!strcmp(s,"wifi_settings.elf"))test_wifi_launches++;else assert(!fb_grant.api);test_launches++;return true;}
static bool test_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool test_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!test_frames);test_frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=fb_pixels,.width=240,.height=240,.stride_bytes=488,.size_bytes=sizeof(fb_pixels),.pixel_format=f};return true;}
static void test_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(f==1 && test_frames);test_frames=0;}
static bool test_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;assert(f==1 && test_frames);test_frames=0;*t=++test_presents;test_seen_modes|=1u<<fb_mode;
 if(test_directory){char path[512];snprintf(path,sizeof(path),"%s/frame-%02u.ppm",test_directory,test_presents);FILE*out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 240\n255\n");for(unsigned y=0;y<240;y++)for(unsigned x=0;x<244;x++){unsigned v=fb_pixels[y*244+x];if(x<240){unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,out)==3);}else assert(v==0xa5a5);}assert(!fclose(out));}return true;}
static bool test_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;(void)t;s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 test_display={.api_version=1,.struct_size=sizeof(test_display),.get_info=test_info,.acquire=test_frame,.release=test_frame_release,.submit=test_submit,.present_status=test_present};
static uint64_t test_sub(void*c){(void)c;test_subs++;return 1;}
static bool test_unsub(void*c,uint64_t n){(void)c;assert(n==1 && test_subs);test_subs--;return true;}
static bool test_poll(void*c,size_t n){(void)c;assert(n==1);test_polls++;return true;}
static int32_t test_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;(void)e;return 0;}
static bool test_snapshot(void*c,risc_touch_snapshot_v1*s){
 (void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240};int x=-1,y=-1;
 if(test_script==1 || test_script==3){if(test_polls==5){x=100;y=82;}if(test_polls==11){x=120;y=207;}if(test_polls==17||test_polls==23||test_polls==29){x=25;y=25;}}
 if(test_script==2){if(test_polls==5){x=100;y=154;}if(test_polls==6){x=100;y=82;}if(test_polls==13){x=25;y=25;}}
 if(x>=0){
#if PORTABLE_TOUCH_ROTATION == 180
  x=239-x;y=239-y;
#endif
  s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};
 }
 return true;
}
static const risc_touch_api_v1 test_touch={1,sizeof(test_touch),NULL,test_sub,test_unsub,test_poll,test_next,test_snapshot};
#ifdef FILE_BROWSER_FULL_PROFILE
static int32_t test_alarm_status(void*c,alarm_status_v1*out){(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t test_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t test_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t test_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;(void)s;return ALARM_OK;}
static const alarm_service_v1 test_alarm={1,sizeof(test_alarm),NULL,test_alarm_status,test_alarm_step,test_alarm_step,test_alarm_ack,test_alarm_prepare,test_alarm_step};
static bool test_navigation(void*c,risc_input_navigation_frame_v1*out){(void)c;*out=(risc_input_navigation_frame_v1){0};return true;}
static bool test_foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;return true;}
static bool test_navigation_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 test_nav={1,sizeof(test_nav),NULL,test_navigation,test_foreground,test_navigation_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &test_nav;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!fb_grant.api);return 1;}
#endif
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(g->struct_size==sizeof(*g));(void)id;if(!strcmp(n,PORTABLE_FILE_BROWSER_CAPABILITY)){assert(v==1);if(test_fail_acquire)return false;g->api=&test_volume;}else if(!strcmp(n,"display.output"))g->api=&test_display;else if(!strcmp(n,"input.touch.raw"))g->api=&test_touch;
#ifdef FILE_BROWSER_FULL_PROFILE
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY))g->api=&test_alarm;
#endif
 else return false;
 test_grants++;return true;}
static bool test_release(risc_runtime_capability_v1*g){assert(g->api && test_grants);if(g->api==&test_volume && test_fail_release)return false;g->api=NULL;test_grants--;return true;}
static const risc_runtime_api_v1 test_runtime={1,sizeof(test_runtime),test_health,test_yield,test_diag,test_launch,test_acquire,test_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&test_runtime:NULL;}
static bool test_browser_poll(t5_app_input_t*out,uint32_t wait){
 bool ok=t5_app_get_api(1)->poll(out,wait);
 if(ok && test_script==3 && test_polls>=17){assert(fb_mode==FB_PREVIEW);assert(test_launch("wifi_settings.elf"));out->exit_requested=true;}
 return ok;
}
static const t5_app_api_v1 *test_browser_api(uint32_t v){
 static t5_app_api_v1 wrapped;const t5_app_api_v1*original=t5_app_get_api(v);if(!original)return NULL;wrapped=*original;wrapped.poll=test_browser_poll;return &wrapped;
}
static bool test_ready(void*c){(void)c;return true;}
static bool test_label(void*c,char*s,size_t n){(void)c;snprintf(s,n,"Installed files (read-only)");return true;}
static bool test_stat(void*c,const char*p,uint64_t*s,bool*d){(void)c;(void)p;*s=test_length;*d=false;return !test_fail_open;}
static risc_storage_dir_t test_diropen(void*c,const char*p){(void)c;(void)p;test_entry=0;test_error[0]=0;return test_fail_open?0:1;}
static bool test_dirnext(void*c,risc_storage_dir_t h,risc_storage_dirent_v1*e){(void)c;assert(h==1);if(test_fail_listing){strcpy(test_error,"Read failed");return false;}if(test_entry==test_entries)return false;memset(e,0,sizeof(*e));unsigned i=test_entries-test_entry++;snprintf(e->name,sizeof(e->name),"file%u.txt",i);e->size=test_length;if(test_invalid_name)memset(e->name,'X',sizeof(e->name));return true;}
static void test_dirclose(void*c,risc_storage_dir_t h){(void)c;assert(h==1);}
static risc_storage_file_t test_open(void*c,const char*p,uint64_t*s){(void)c;(void)p;test_offset=0;*s=test_length;return test_fail_open?0:2;}
static size_t test_read(void*c,risc_storage_file_t h,void*out,size_t n){(void)c;assert(h==2);test_reads++;if(test_fail_read)return 0;if(n>17)n=17;if(n>test_length-test_offset)n=test_length-test_offset;for(size_t i=0;i<n;i++)((unsigned char*)out)[i]=(unsigned char)('A'+(test_offset+i)%26);test_offset+=(unsigned)n;return n;}
static bool test_close(void*c,risc_storage_file_t h,bool commit){(void)c;assert(h==2 && commit);test_closes++;return !test_fail_close;}
static bool test_error_get(void*c,char*out,size_t n){(void)c;snprintf(out,n,"%s",test_error);return true;}
static void check_model(void){char path[512]="/";assert(fb_name_compare("file2",false,"file10",false)<0);assert(fb_name_compare("z",true,"a",false)<0);assert(fb_join(path,"folder",path,sizeof(path)));assert(!strcmp(path,"/folder"));assert(fb_parent(path) && !strcmp(path,"/"));assert(!fb_parent(path));assert(!fb_join("/","..",path,sizeof(path)));assert(!fb_join("/","../secret",path,sizeof(path)));assert(!fb_join("/","a\\b",path,sizeof(path)));assert(fb_contains("Manifest.JSON","json"));assert(!fb_contains("JSON","jsno"));char longdir[510];memset(longdir,'x',sizeof(longdir));longdir[0]='/';longdir[509]=0;assert(!fb_join(longdir,"long",path,sizeof(path)));}
int main(int argc,char**argv){
 test_directory=argc>1?argv[1]:NULL;memset(fb_pixels,0xa5,sizeof(fb_pixels));check_model();
 test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 assert(app_module_init()==0);assert(fb_open());assert(fb_count==2 && !strcmp(fb_rows[0].name,"file1.txt") && !strcmp(fb_rows[1].name,"file2.txt"));fb_draw();
 fb_page_next();assert(fb_page==1 && !strcmp(fb_rows[0].name,"file3.txt"));fb_page_previous();assert(fb_page==0 && !strcmp(fb_rows[0].name,"file1.txt"));
 test_entries=513;(void)fb_load(0);for(unsigned i=0;i<256;i++)fb_page_next();assert(fb_page==256 && fb_count==1 && !strcmp(fb_rows[0].name,"file513.txt") && !fb_more);fb_page_previous();assert(!strcmp(fb_rows[0].name,"file511.txt"));
 test_entries=8;(void)fb_load(0);fb_mode=FB_OPTIONS;fb_choice=0;fb_draw();fb_option(0);fb_draw();
 strcpy(fb_editor,"FILE8");fb_filter_key(PWK_DONE);assert(fb_count==1 && !strcmp(fb_rows[0].name,"file8.txt"));strcpy(fb_query,"missing");assert(fb_load(0) && !fb_count);fb_draw();
 fb_query[0]=0;assert(fb_load(0));fb_activate_file(0);assert(fb_mode==FB_DETAIL);fb_draw();fb_activate();assert(fb_mode==FB_PREVIEW && fb_preview_count==192 && fb_preview[0]=='A');fb_draw();
 fb_preview_page(1);assert(fb_offset==192 && fb_preview[0]=='K');fb_preview_page(1);assert(fb_offset==384 && fb_preview_count==192);fb_preview_page(1);assert(fb_offset==576 && fb_preview_count==1);fb_preview_page(1);assert(fb_offset==576);fb_preview_page(-1);assert(fb_offset==384);
 fb_activate();assert(fb_hex && !fb_offset && fb_preview_count==64);fb_draw();
 test_fail_read=true;assert(!fb_preview_read() && !fb_preview_count);fb_draw();test_fail_read=false;
 test_fail_close=true;assert(!fb_preview_read() && fb_retained_file);unsigned reads=test_reads;assert(!fb_preview_read() && reads==test_reads);assert(!portable_file_browser_close());test_fail_close=false;assert(portable_file_browser_close() && !fb_retained_file);
 assert(fb_back()==false);assert(fb_mode==FB_DETAIL);assert(fb_back()==false);assert(fb_mode==FB_LIST);
 test_fail_listing=true;assert(!fb_load(0) && !fb_count);fb_draw();test_fail_listing=false;test_invalid_name=true;assert(!fb_load(0));test_invalid_name=false;
 test_entries=0;assert(fb_load(0) && fb_loaded && !fb_count);fb_draw();test_entries=8;
 assert(portable_file_browser_close());test_fail_acquire=true;assert(!fb_load(0));fb_draw();test_fail_acquire=false;assert(fb_load(0));
 test_length=0;fb_activate_file(0);fb_activate();assert(!fb_preview_count && !strcmp(fb_status,"Empty file"));fb_draw();
 memset(fb_selected.name,'W',127);fb_selected.name[127]=0;fb_mode=FB_DETAIL;fb_detail_offset=0;fb_draw();fb_move(1);assert(fb_detail_offset==64);fb_draw();fb_move(-1);assert(!fb_detail_offset);
 fb_mode=FB_LIST;test_fail_release=true;assert(!fb_back());test_fail_release=false;assert(fb_back());app_module_fini();assert(!test_grants && !test_frames && !test_subs);
 test_entries=8;test_length=577;
 for(unsigned mode=1;mode<=3;mode++){
  test_polls=0;test_script=mode;test_seen_modes=0;unsigned before=test_launches;
  assert(app_module_init()==0);app_main();app_module_fini();assert(test_launches==before+1);
  if(mode==1)assert((test_seen_modes&((1u<<FB_DETAIL)|(1u<<FB_PREVIEW)))==((1u<<FB_DETAIL)|(1u<<FB_PREVIEW)));
  else if(mode==2)assert(fb_page==1 && fb_mode==FB_LIST);
  else assert(fb_mode==FB_PREVIEW && test_wifi_launches==1);
  assert(!test_grants && !test_frames && !test_subs);
 }
 puts("File Browser: streaming513-item pagination, natural ordering, filter/keyboard, preview boundaries, long names, empty/missing/invalid/failing storage, retry and cleanup passed");return 0;
}
