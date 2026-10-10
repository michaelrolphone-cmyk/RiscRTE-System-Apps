/* Actual Home raster and retained Points scene, with bounded pixel transport. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/paper_clock.c"
static unsigned fill_calls;
static unsigned char pixels[800*480];static int width=480,height=800;static unsigned flipped;
static int screen_width(void){return width;}static int screen_height(void){return height;}
static void fill(int x,int y,int w,int h,bool black) {
 ++fill_calls;assert(w>=0&&h>=0);
 for(int row=y;row<y+h;row++)for(int col=x;col<x+w;col++) {
  assert(row>=0&&row<height&&col>=0&&col<width);
  int at=row*width+col;if(flipped)at=width*height-1-at;pixels[at]=black;
 }
}
void portable_desk_adapter_retain(void){assert(!"unexpected retained rendering result");}
bool portable_desk_adapter_ready(void){return true;}
bool portable_desk_adapter_landscape(bool value,unsigned direction){width=value?800:480;height=value?480:800;flipped=direction;return true;}
void portable_desk_adapter_begin(void){memset(pixels,0,sizeof(pixels));}
static int cancelled(void *unused){(void)unused;return 0;}
static void save(const char *directory,const char *name) {
 char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",directory,name);FILE *f=fopen(path,"wb");assert(f);
 fprintf(f,"P4\n%d %d\n",width,height);
 for(int i=0;i<width*height;i+=8){unsigned char b=0;for(int n=0;n<8;n++)b|=pixels[i+n]<<(7-n);assert(fwrite(&b,1,1,f)==1);}fclose(f);
}
/* Exercise the production draw_clock cache decision with a simple surface
 * transport. Adapter token/rotation custody has separate integration tests. */
struct portable_raster_layer {unsigned char bits[800*480];int width,height;unsigned flipped;};
static struct portable_raster_layer layer;
static unsigned captures,blits;
static bool frame_ready=true,cache_unavailable;
bool portable_app_sleep_retained(void){return false;}
int portable_home_points_catalog(portable_points_catalog_view *view){(void)view;assert(!"unexpected service I/O");return 0;}
bool portable_paper_frame_ready(void){return frame_ready;}
bool portable_raster_layer_valid(const portable_raster_layer *p){return p&&!cache_unavailable&&p->width==width&&p->height==height&&p->flipped==flipped;}
bool portable_raster_layer_begin(portable_raster_layer **out){
 if(cache_unavailable)return false;
 *out=&layer;layer.width=width;layer.height=height;layer.flipped=flipped;captures++;
 memset(pixels,0,sizeof(pixels));return true;
}
void portable_raster_layer_end(void){memcpy(layer.bits,pixels,sizeof(pixels));}
void portable_raster_layer_release(portable_raster_layer **p){*p=NULL;}
void portable_raster_layer_blit(portable_raster_layer *p,int sx,int sy,int w,int h,int dx,int dy){
 assert(portable_raster_layer_valid(p)&&!sx&&!sy&&!dx&&!dy&&w==width&&h==height);
 memcpy(pixels,p->bits,sizeof(pixels));blits++;
}
static void begin(void){memset(pixels,0,sizeof(pixels));}
static void submit(bool clean){(void)clean;}
static void cache_check(twatch_rtc_time_v1 *time,bool changed,const char *notice) {
 static unsigned char expected[sizeof(pixels)];unsigned before=captures;fill_calls=0;
 clock_dirty=true;assert(draw_clock(time,true,notice,false));
 assert(captures==before+(changed?1:0));if(!changed)assert(!fill_calls);
 memcpy(expected,pixels,sizeof(pixels));
 /* Independently redraw without a cache and compare every output pixel. */
 cache_unavailable=true;clock_dirty=true;assert(draw_clock(time,true,notice,false));
 assert(!memcmp(expected,pixels,sizeof(pixels)));cache_unavailable=false;
 /* Restore the selected image without invalidating its cached key. */
 home_scene_layer=&layer;
}
static void cache_cases(twatch_rtc_time_v1 time,home_point_state state) {
 static const paper_presentation presentation={.struct_size=sizeof(presentation),.begin=begin};paper=&presentation;
 home_points=state;home_refresh_pending=false;home_pressed=HOME_NONE;
 home_scene_saved_valid=false;clock_dirty=true;
 cache_check(&time,true,"");
 for(unsigned i=0;i<12;i++)cache_check(&time,false,"");
 home_points.revision++;home_points.next[3].event_id++;cache_check(&time,false,"");
 time.second++;cache_check(&time,false,"");
 time.minute++;cache_check(&time,true,"");
 battery_status=(paper_battery){true,false,83};cache_check(&time,true,"");
 battery_status.charging=true;cache_check(&time,true,"");
 home_points.next[0].label[0]='B';cache_check(&time,true,"");
 home_points.now_rtc+=61;cache_check(&time,true,"");
 format=PORTABLE_TIME_FORMAT_12;cache_check(&time,true,"");
 home_pressed=HOME_POINTS;cache_check(&time,true,"");home_pressed=HOME_NONE;cache_check(&time,true,"");
 cache_check(&time,true,"RETRY");cache_check(&time,false,"RETRY");
 home_points.status=NOVA_POINTS_PENDING;cache_check(&time,true,"");
 home_points.status=NOVA_POINTS_EMPTY;cache_check(&time,true,"");
 flipped=1;cache_check(&time,true,"");flipped=0;cache_check(&time,true,"");
 frame_ready=false;clock_dirty=false;unsigned before=captures;
 assert(draw_clock(&time,true,"NEW NOTICE",true)&&clock_dirty&&captures==before);
 frame_ready=true;cache_check(&time,true,"NEW NOTICE");
 printf("Home cache: %u captures, %u blits; unchanged returns use zero paint spans; visible changes, orientation, deferred dirty and fallback have exact pixels PASS\n",captures,blits);
}
int main(int argc,char **argv) {
 assert(argc==2);static t5_app_api_v1 api={.screen_width=screen_width,.screen_height=screen_height,.fill_rect=fill,.present=submit};
 app=&api;static const portable_desk_sleep_ops ops={.cancelled=cancelled};desk_ops=&ops;
 portable_desk_config config={.face=PORTABLE_DESK_POINTS,.time_format=PORTABLE_TIME_FORMAT_24,.rtc_stores_utc=1};
 strcpy(config.time_zone,"UTC");portable_points_catalog_view *v=&config.points.view;
 uint32_t now;assert(points_utc_from_unix(1791369720,&now));
 config.points.status=PORTABLE_DESK_POINTS_READY;
 *v=(portable_points_catalog_view){.struct_size=sizeof(*v),.catalog_revision=3,.snapshot=1,.seconds=now,.count=4,.has_previous=1,.valid_until=now+3600};
 v->previous=(portable_points_catalog_event){.event_id=1,.type_id=90000,.revision=2,.deadline=now-3600,.parent_day=9777};strcpy(v->previous.label,"Previous catalog event");
 const char *labels[]={"A long catalog title with words","WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW","1234567890123456789012345678901","Final fourth event with a label"};
 for(unsigned i=0;i<4;i++){v->next[i]=v->previous;v->next[i].event_id=10+i;v->next[i].deadline=now+600+i*600;snprintf(v->next[i].label,32,"%s",labels[i]);}
 assert(portable_points_catalog_view_valid(v));
 portable_timezone_rule rule;assert(portable_timezone_resolve("UTC",4,&rule)==PORTABLE_TIMEZONE_OK);
 home_point_state state;assert(paper_catalog_project(v,&rule,1791369720,&state));
 assert(state.next_count==4&&state.next[3].event_id==13&&state.next[1].type_id==90000);
 assert(!strcmp(state.next[1].label,labels[1]));
 char lines[2][32];
 for(unsigned c=32;c<127;c++){char label[32];memset(label,(int)c,31);label[31]=0;
  assert(home_label_lines(&HOME_R700_22,label,284,lines));
  assert(home_text_width(&HOME_R700_22,lines[0],0)<=284*64);
  assert(home_text_width(&HOME_R700_22,lines[1],0)<=284*64);
 }
 assert(home_label_lines(&HOME_R700_22,"A WWWWWWWWWWWWWWWWWWWWWWWWWWWWW",284,lines));
 assert(home_label_lines(&HOME_R700_22,"Long word ABCDEFGHIJKLMNOPQRSTU",284,lines));
 home_points_render(&state,PORTABLE_TIME_FORMAT_24,NULL);save(argv[1],"portrait-full-labels");
 v->seconds+=7;
 assert(desk_points_scene(&config,1791369720));save(argv[1],"landscape-full-labels");
 unsigned char forward[sizeof(pixels)];memcpy(forward,pixels,sizeof(pixels));config.flip_ui=1;
 assert(desk_points_scene(&config,1791369720));save(argv[1],"landscape-reverse");
 for(unsigned i=0;i<sizeof(pixels);i++)assert(pixels[i]==forward[sizeof(pixels)-1-i]);
 config.flip_ui=0;config.time_format=PORTABLE_TIME_FORMAT_12;
 assert(desk_points_scene(&config,1791331200));save(argv[1],"landscape-midnight-12h");
 char value[20];home_point_event event={.hour=0,.minute=5};home_time(value,sizeof(value),&event,false,PORTABLE_TIME_FORMAT_24);assert(!strcmp(value,"0:05"));
 home_time(value,sizeof(value),&event,false,PORTABLE_TIME_FORMAT_12);assert(!strcmp(value,"12:05 AM"));
 assert(portable_desk_adapter_landscape(false,0));format=PORTABLE_TIME_FORMAT_24;
 twatch_rtc_time_v1 time={.year=2026,.month=10,.day=7,.weekday=3,.hour=9,.minute=22};
 const nova_points_status statuses[]={NOVA_POINTS_PENDING,NOVA_POINTS_EMPTY,NOVA_POINTS_ERROR,NOVA_POINTS_UNAVAILABLE,NOVA_POINTS_READY};
 const char *names[]={"home-loading","home-empty","home-error","home-unavailable","home-ready"};
 for(unsigned i=0;i<sizeof(statuses)/sizeof(statuses[0]);i++) {
  portable_desk_adapter_begin();home_reference_header(&time,true);
  home_point_state shown=state;shown.status=statuses[i];home_reference_points(&shown,format,NULL);save(argv[1],names[i]);
 }
 cache_cases(time,state);
 puts("Actual Home catalog raster: all 31-byte ASCII labels fit two measured lines, four rows, full IDs, both orientations, unpadded hours passed");return 0;
}
