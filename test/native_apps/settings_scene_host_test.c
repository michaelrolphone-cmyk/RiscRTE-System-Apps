/* Actual published scene host + production Settings model and scene documents.
 * Reuse the provider's display/touch/navigation transport fixture, including
 * native-liveness assertions, pixel guards and asynchronous presentation. */
#define main inherited_scene_host_main
#include "../scene/host_test.c"
#undef main
#include "PortableSettingsScene.h"
#include "PortableRoundViewport.h"
static PortableSettingsModel settings_model;
static pss_scene settings_view;
static risc_components_document_v1 settings_document;
static const risc_scene_components_api_v1 *settings_components;
static risc_display_insets_v1 settings_insets;
static bool settings_round,settings_seeding;
static const char *settings_capture_name;
static uint8_t settings_zone_bytes[44];static unsigned settings_zone_size;
static uint32_t settings_document_hash=2166136261u;
static unsigned settings_actions,settings_frames,settings_outside_max,settings_ink_max,settings_transitions_max;
static bool settings_live(void *ctx){(void)ctx;return native_alive;}
static int32_t settings_get(void*c,const char*k,void*p,uint32_t cap,uint32_t*n){(void)c;*n=0;if(strcmp(k,"time_zone")||!settings_zone_size)return RISC_KEY_VALUE_NOT_FOUND;*n=settings_zone_size;if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(p,settings_zone_bytes,*n);return 0;}
static int32_t settings_put(void*c,const char*k,const void*p,uint32_t n){(void)c;assert(settings_seeding&&!strcmp(k,"time_zone")&&n==44);memcpy(settings_zone_bytes,p,n);settings_zone_size=n;return 0;}
static const risc_key_value_v1 settings_store={1,sizeof(settings_store),NULL,settings_get,settings_put};
static bool settings_info(void*c,risc_display_info_v1*out){bool ok=get_info(c,out);out->safe_insets=settings_insets;return ok;}
static int settings_clock_begin(void*c){(void)c;return PSM_OK;}
static int settings_clock_read(void*c,portable_timezone_civil*v){(void)c;*v=(portable_timezone_civil){2026,11,4,12,0,0,0};return PSM_OK;}
static int settings_clock_start(void*c,const portable_set_time_request*r,portable_set_time_result*v){(void)c;(void)r;(void)v;assert(!"clock writes forbidden in navigation qualification");return PSM_ERROR;}
static int settings_clock_poll(void*c,portable_set_time_result*v){(void)c;(void)v;return PSM_ERROR;}
static int settings_clock_cancel(void*c,portable_set_time_result*v){(void)c;(void)v;return PSM_CANCELLED;}
static uint16_t settings_pixel(unsigned x,unsigned y){
 if(format==RISC_DISPLAY_FORMAT_RGB565){unsigned at=16+(y*w+x)*2;return (uint16_t)pixels[at]|(uint16_t)pixels[at+1]<<8;}
 unsigned stride=(w+7)/8;return (pixels[16+y*stride+x/8]&(0x80u>>(x%8)))?0:0xffff;
}
static void settings_capture(void){
 check_guards();++settings_frames;unsigned outside=0,ink=0,transitions=0;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  uint16_t p=settings_pixel(x,y);if(x&&p!=settings_pixel(x-1,y))++transitions;bool border=x<settings_insets.left||x>=w-settings_insets.right||y<settings_insets.top||y>=h-settings_insets.bottom;
  if(border)assert(p==prof.background_rgb565);
  if(p!=prof.background_rgb565){++ink;if(settings_round&&!portable_round_contains(w,h,(int)x,(int)y,0))++outside;}
 }
 assert(transitions>20);if(transitions>settings_transitions_max)settings_transitions_max=transitions;
 const char *dir=getenv("SETTINGS_SCENE_FRAME_DIR");if(dir&&settings_capture_name){
  char filename[512];snprintf(filename,sizeof(filename),"%s/%s.ppm",dir,settings_capture_name);FILE*f=fopen(filename,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",w,h);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){uint16_t v=settings_pixel(x,y);unsigned char rgb[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}
  assert(!fclose(f));settings_capture_name=NULL;
 }
 if(outside>settings_outside_max)settings_outside_max=outside;
 if(ink>settings_ink_max)settings_ink_max=ink;
}
static void settings_close(void){
 if(!session)return;
 int rc;unsigned count=0;while((rc=api->close(NULL,session))==RISC_SCENE_AGAIN){ms+=10;assert(++count<1000);}assert(rc==0);session=0;
 assert(!presenting&&!held&&!subscribed);
}
static void settings_build(void){assert(pss_build(&settings_view,&settings_document));assert(settings_document.node_count<=RISC_COMPONENTS_MAX_NODES);for(unsigned i=0;i<settings_document.node_count;i++)if(settings_document.nodes[i].kind==RISC_COMPONENT_STEPPER){assert(settings_document.nodes[i].text[0]);assert(strtol(settings_document.nodes[i].text,NULL,10)==settings_document.nodes[i].value);}const uint8_t*p=(const uint8_t*)&settings_document;for(unsigned i=0;i<sizeof(settings_document);i++)settings_document_hash=(settings_document_hash^p[i])*16777619u;}
static unsigned settings_find(unsigned action){for(unsigned i=0;i<settings_document.node_count;i++)if(settings_document.nodes[i].action==action)return i;assert(!"semantic action missing");return 0;}
/* Select through actual provider navigation, not a fabricated scene event.
 * Focus starts at predecessor, then DOWN asks the provider to reveal target
 * using its own native scroll geometry. No caller computes a pixel location. */
static void settings_open_focus(unsigned action){
 settings_close();settings_build();unsigned target=settings_find(action),previous=target;
 for(unsigned offset=1;offset<=settings_document.node_count;offset++){
  unsigned i=(target+settings_document.node_count-offset)%settings_document.node_count;
  if(settings_document.nodes[i].action&&!(settings_document.nodes[i].flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))){previous=i;break;}
 }
 risc_scene_navigation_v1 path={.api_version=1,.struct_size=sizeof(path),.depth=1,.routes={settings_document.root},.focus={settings_document.nodes[previous].id}};
 int rc=settings_components->open(NULL,&settings_document,&path,&session);
 if(rc)fprintf(stderr,"Settings provider rejected route=%u action=%u rc=%d nodes=%u\n",settings_document.root,action,rc,settings_document.node_count);
 assert(rc==RISC_SCENE_OK);risc_scene_event_v1 event;nav_pressed=RISC_NAV_DOWN;assert(tick(&event)==RISC_SCENE_IDLE);settle();settings_capture();
 risc_scene_navigation_v1 actual={.struct_size=sizeof(actual)};uint32_t flags=0;assert(api->snapshot(NULL,session,&actual,&flags)==0&&actual.focus[0]==settings_document.nodes[target].id&&!(flags&RISC_SCENE_PRESENTING));
}
static void settings_choose(unsigned action){
 settings_open_focus(action);risc_scene_event_v1 event;nav_pressed=RISC_NAV_CONFIRM;int rc=tick(&event);
 if(rc!=RISC_SCENE_OK||event.action!=action)fprintf(stderr,"Provider selection failed action=%u rc=%d got=%u node=%u\n",action,rc,event.action,event.node);
 assert(rc==RISC_SCENE_OK&&event.action==action&&event.document_revision==settings_document.revision);assert(pss_action(&settings_view,&event));if(settings_model.phase==PSM_CANCELLING)psm_tick(&settings_model);++settings_actions;
}
static int settings_initial(const char*id){const char*p=strchr(id,'/');p=p?p+1:id;return *p>='A'&&*p<='Z'?*p-'A':26;}
static void settings_all_zones(void){
 unsigned longest=0;for(unsigned i=1;i<portable_timezone_count();i++)if(strlen(portable_timezone_get(i)->id)>strlen(portable_timezone_get(longest)->id))longest=i;
 for(unsigned i=0;i<portable_timezone_count();i++){
  const portable_timezone_entry*z=portable_timezone_get(i);
  assert(settings_model.route==PSM_ROOT);settings_choose(100+PSM_ZONE);
  settings_choose(500+(unsigned)z->region);
  if(portable_timezone_region_count(z->region)>64)settings_choose(600+(unsigned)settings_initial(z->id));
  if(i==longest)settings_capture_name="timezone-longest";
  settings_choose(1000+i);assert(!strcmp(settings_model.draft.zone,z->id));
  /* Explicit Cancel button bypasses logical-directory Back without a save. */
  settings_choose(11);assert(settings_model.route==PSM_ROOT);
 }
}
static void settings_screens(void){
 const psm_feature features[]={PSM_TIME,PSM_FORMAT,PSM_SLEEP,PSM_ALERT,PSM_FACE,PSM_FLIP,PSM_LANGUAGE,PSM_DIRECTION,PSM_IDLE_TIMER,PSM_DEEP_TIMER,PSM_BASIS,PSM_STORAGE,PSM_ABOUT};
 for(unsigned i=0;i<sizeof(features)/sizeof(*features);i++){if(!i)settings_capture_name="root";settings_choose(100+features[i]);if(features[i]==PSM_TIME){settings_capture_name="time";settings_open_focus(300);}if(features[i]==PSM_IDLE_TIMER){settings_capture_name="timer";settings_open_focus(2000);}settings_open_focus(11);risc_scene_event_v1 event;nav_pressed=RISC_NAV_BACK;assert(tick(&event)==0);assert(pss_action(&settings_view,&event));if(settings_model.phase==PSM_CANCELLING)psm_tick(&settings_model);assert(settings_model.route==PSM_ROOT);}
 settings_choose(100+PSM_IDLE_TIMER);assert(psm_set_number(&settings_model,61000));assert(psm_save(&settings_model));settings_capture_name="save-notice";settings_choose(11);assert(settings_model.route==PSM_ROOT);
 /* Real model DST preparation produces the distinct fold document. */
 settings_choose(100+PSM_TIME);portable_timezone_civil fold={2026,11,1,1,30,0,0};assert(psm_set_civil(&settings_model,&fold));assert(!psm_save(&settings_model)&&settings_model.route==PSM_FOLD);settings_choose(400);assert(settings_model.fold_choice==0);settings_choose(11);assert(settings_model.route==PSM_ROOT);
 /* Failed document revisions are rejected without replacing a valid frame. */
 settings_open_focus(100+PSM_ZONE);if(settings_insets.left){risc_scene_event_v1 corner;touch_tap(0,0);assert(tick(&corner)==RISC_SCENE_IDLE);}
 risc_components_document_v1 stale=settings_document;assert(settings_components->update(NULL,session,&stale)==RISC_SCENE_STALE);
 /* Provider stays nonblocking while physical presentation is pending. */
 stale.revision++;assert(settings_components->update(NULL,session,&stale)==0);allow_complete=false;begin_frame();unsigned before=polls;risc_scene_event_v1 e;for(unsigned i=0;i<8;i++)assert(tick(&e)==RISC_SCENE_IDLE);assert(polls==before+8);allow_complete=true;settle();settings_close();
}
int main(int argc,char**argv){
 assert(argc==8);w=(unsigned)strtoul(argv[1],NULL,10);h=(unsigned)strtoul(argv[2],NULL,10);format=(unsigned)strtoul(argv[3],NULL,10);unsigned rotation=(unsigned)strtoul(argv[4],NULL,10),inset=(unsigned)strtoul(argv[5],NULL,10);settings_round=atoi(argv[6])!=0;bool all=atoi(argv[7])!=0;
 assert(w*h<=800u*480u&&(format==1||format==5));display_flags=settings_round?RISC_DISPLAY_INFO_ROUND:0;settings_insets=(risc_display_insets_v1){inset,inset,inset,inset};
 prof=format==1?(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,rotation,0,0}:(risc_scene_profile_v1){1,sizeof(prof),5,2,42,8,0xffff,0x0841,0x05ff,rotation,0,0};
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 risc_display_output_api_v1 display=disp;display.get_info=settings_info;
 driver=t5_driver_get(2);assert(driver);api=driver->capability;settings_components=risc_scene_components_get_v1(api);assert(settings_components);
 risc_provider_dependency_v1 deps[]={{"display.output",1,&display},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};assert(driver->start(deps,5));
 settings_seeding=true;assert(portable_timezone_preference_save(&settings_store,"America/New_York",17)==PORTABLE_TIMEZONE_SAVED);settings_seeding=false;
 psm_policy policy=psm_default_policy();psm_services services={.live=settings_live,.clock_begin=settings_clock_begin,.clock_read=settings_clock_read,.clock_start=settings_clock_start,.clock_poll=settings_clock_poll,.clock_cancel=settings_clock_cancel};assert(psm_init(&settings_model,&policy,&services,&settings_store,NULL));pss_init(&settings_view,&settings_model);
 settings_screens();if(all)settings_all_zones();settings_close();assert(driver->quiesce());driver->stop();assert(settings_ink_max&&settings_frames);
 printf("{\"width\":%u,\"height\":%u,\"format\":%u,\"rotation\":%u,\"inset\":%u,\"round\":%s,\"zones\":%u,\"actions\":%u,\"frames\":%u,\"document_hash\":%u,\"max_ink_pixels\":%u,\"max_ink_outside_circle\":%u,\"native_raster_and_navigation\":true,\"round_clipping_pass\":%s}\n",w,h,format,rotation,inset,settings_round?"true":"false",all?portable_timezone_count():0,settings_actions,settings_frames,settings_document_hash,settings_ink_max,settings_outside_max,settings_outside_max?"false":"true");
 return settings_outside_max?3:0;
}
