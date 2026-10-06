/* Exercise the real shared app layout and RGB565 client, not a UI mockup.
 * A padded stride and outer sentinels check every production render. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SPRINGBOARD_SOURCE
#define SPRINGBOARD_SOURCE "../../Apps/springboard.c"
#endif
#include SPRINGBOARD_SOURCE
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"

int app_module_init(void);void app_module_fini(void);
/* First 14 entries match tests/fixtures/nova-delivered-catalog.json; the
 * remaining real glyphs extend shared-app page/boundary cases. */
const t5_app_manifest_t portable_catalog[]={
 {.display_name="Clock",.file_name="clock.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Calculator",.file_name="calculator.elf",.icon="solid:f1ec",.compatible=true},
 {.display_name="Stopwatch",.file_name="stopwatch.elf",.icon="solid:f2f2",.compatible=true},
 {.display_name="Alarms",.file_name="alarms.elf",.icon="solid:f0f3",.compatible=true},
 {.display_name="Countdown",.file_name="countdown.elf",.icon="solid:f254",.compatible=true},
 {.display_name="Wi-Fi",.file_name="wifi_settings.elf",.icon="solid:f1eb",.compatible=true},
 {.display_name="Points in Time",.file_name="points_in_time.elf",.icon="solid:f783",.compatible=true},
 {.display_name="Frequency Generator",.file_name="frequency_generator.elf",.icon="solid:f028",.compatible=true},
 {.display_name="Audio Spectrum",.file_name="audio_spectrum.elf",.icon="solid:f130",.compatible=true},
 {.display_name="File Browser",.file_name="file_browser.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="BLE Scanner",.file_name="ble_scanner.elf",.icon="solid:f7c0",.compatible=true},
 {.display_name="LoRa Messages",.file_name="lora_messages.elf",.icon="solid:f27a",.compatible=true},
 {.display_name="Clock",.file_name="clock.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Calculator",.file_name="calculator.elf",.icon="solid:f1ec",.compatible=true},
 {.display_name="Stopwatch",.file_name="stopwatch.elf",.icon="solid:f2f2",.compatible=true},
 {.display_name="Alarms",.file_name="alarms.elf",.icon="solid:f0f3",.compatible=true}};
const unsigned portable_catalog_count=14;
/* The Watch client admits 17 apps. Extend only catalog reads for shared-app
 * 19-entry page/boundary cases, using the same real glyphs and renderer. */
static t5_app_api_v1 fixture_api;
static bool fixture_get(uint32_t i,t5_app_manifest_t *out){
 if(!out||i>=sizeof(portable_catalog)/sizeof(portable_catalog[0]))return false;
 *out=portable_catalog[i];return true;
}
static unsigned fw=240,fh=240,stride,grants,subscriptions,frames,presentations;
static uint16_t *guarded,*frame_pixels,*comparison;
static const springboard_presentation *production;
static springboard_presentation observed;
static nova_state *scene;
static unsigned circle_index,icon_index;
static bool unboost;
static int focus_x,focus_y,focus_radius,focus_glyph;
static unsigned painted,white;
static bool get_info(void *c,risc_display_info_v1 *o){
 (void)c;*o=(risc_display_info_v1){.width=fw,.height=fh,
 .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
 .nominal_refresh_millihz=60000,.typical_present_latency_us=16000};return true;
}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){
 (void)c;assert(!frames);frames=1;
 *s=(risc_display_surface_v1){.frame=1,.pixels=frame_pixels,.size_bytes=fh*stride*2,
 .width=fw,.height=fh,.stride_bytes=stride*2,.pixel_format=f};return true;
}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 (void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);frames=0;*t=++presentations;return true;
}
static bool status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void *c){(void)c;++subscriptions;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subscriptions);--subscriptions;return true;}
static bool poll_touch(void *c,size_t n){(void)c;(void)n;return true;}
static int32_t next_touch(void *c,uint64_t n,risc_touch_event_v1 *e){(void)c;(void)n;(void)e;return 0;}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;memset(s,0,sizeof(*s));s->width=fw;s->height=fh;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=get_info,.acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=status};
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
static bool acquire(const char *name,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){
 assert(!id&&v==1);if(!strcmp(name,"display.output"))g->api=&display_api;
 else if(!strcmp(name,"input.touch.raw"))g->api=&touch_api;else return false;
 ++grants;return true;
}
static bool release(risc_runtime_capability_v1 *g){assert(grants&&g->api);--grants;g->api=NULL;return true;}
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=presentations*40;return true;}
static void yield_ms(uint32_t n){(void)n;}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){(void)s;assert(!"render test cannot launch");return false;}
static const risc_runtime_api_v1 runtime={1,sizeof(runtime),health,yield_ms,diagnostic,launch_app,acquire,release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static bool visible(unsigned i){
 int x=(int)fw/2+(scene->points[i].x+(int)(scene->motion.x*16))/16;
 int y=(int)fh/2+(scene->points[i].y+(int)(scene->motion.y*16))/16;
 return x>=-32&&y>=-32&&x<=(int)fw+32&&y<=(int)fh+32;
}
static unsigned visible_index(unsigned ordinal){
 for(unsigned i=0;i<scene->n;i++)if(visible(i)){if(!ordinal)return i;--ordinal;}
 assert(!"unexpected render operation");return 0;
}
static int scale_for(unsigned i){
 int dx=scene->points[i].x+(int)(scene->motion.x*16),dy=scene->points[i].y+(int)(scene->motion.y*16);
 int scale=nova_scale(abs(dx)>abs(dy)?abs(dx):abs(dy));
 if((int)i==scene->pending&&scene->pulse){int triangle=scene->pulse<=160?scene->pulse:320-scene->pulse;scale+=scale*triangle*22/(160*100);}
 return scale;
}
static void circle(int x,int y,int r,uint32_t color,uint8_t opacity){
 unsigned i=visible_index(circle_index++);int base=scale_for(i),expected=base;
#ifndef FOCUS_BASELINE
 if(i==scene->focus&&scene->pending<0)expected+=expected/8;
#endif
 assert(r==22*expected/256);
 if(i==scene->focus){focus_x=x;focus_y=y;focus_radius=r;if(unboost)r=22*base/256;}
 production->circle(x,y,r,color,opacity);
}
static bool icon(int x,int y,int size,const char *name,uint8_t opacity){
 unsigned i=visible_index(icon_index++);int base=scale_for(i),expected=base;
#ifndef FOCUS_BASELINE
 if(i==scene->focus&&scene->pending<0)expected+=expected/8;
#endif
 assert(size==24*expected/256);
 if(i==scene->focus){focus_glyph=size;if(unboost)size=24*base/256;}
 bool ok=production->icon(x,y,size,name,opacity);assert(ok);return ok;
}
static void guards(void){
 assert(guarded[0]==0xa55a&&guarded[fh*stride+1]==0xa55a);
 for(unsigned y=0;y<fh;y++)for(unsigned x=fw;x<stride;x++)assert(frame_pixels[y*stride+x]==0xa55a);
}
static void render(nova_state *s,bool original){
 scene=s;circle_index=icon_index=0;unboost=original;
 nova_render(s);guards();unsigned visible_count=0;for(unsigned i=0;i<s->n;i++)visible_count+=visible(i);
 if(circle_index!=visible_count||icon_index!=visible_count)fprintf(stderr,"w=%u h=%u count=%u first=%u n=%u focus=%u circles=%u expected=%u\n",fw,fh,count,s->first,s->n,s->focus,circle_index,visible_count);
 assert(circle_index==visible_count&&icon_index==visible_count);
}
static void save_frame(const char *name){
 const char *dir=getenv("NOVA_VISUALS");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s-%ux%u.ppm",dir,name,fw,fh);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",fw,fh);
 for(unsigned y=0;y<fh;y++)for(unsigned x=0;x<fw;x++){
  unsigned p=frame_pixels[y*stride+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
static void compare_focus(nova_state *s,bool centered){
 render(s,false);memcpy(comparison,frame_pixels,fh*stride*2);
 int radius=focus_radius,glyph=focus_glyph,x=focus_x,y=focus_y;
 assert(glyph<radius*2);
 if(s->pending<0)for(unsigned i=0;i<s->n;i++)if(i!=s->focus){
  int other_x=(int)fw/2+(s->points[i].x+(int)(s->motion.x*16))/16;
  int other_y=(int)fh/2+(s->points[i].y+(int)(s->motion.y*16))/16;
  int dx=x-other_x,dy=y-other_y,sum=radius+22*scale_for(i)/256;
  assert(dx*dx+dy*dy>=sum*sum); /* no extra overlap with any neighbour */
 }
 if(centered){
  assert(x==(int)fw/2&&y==(int)fh/2);
  assert(x-radius>0&&y-radius>0&&x+radius<(int)fw&&y+radius<(int)fh);
#ifndef FOCUS_BASELINE
  assert(radius==27&&glyph==30);
#else
  assert(radius==24&&glyph==26);
#endif
 }
 render(s,true);unsigned changed=0;painted=white=0;
 for(unsigned yy=0;yy<fh;yy++)for(unsigned xx=0;xx<fw;xx++){
  int dx=(int)xx-x,dy=(int)yy-y;uint16_t a=comparison[yy*stride+xx],b=frame_pixels[yy*stride+xx];
  if(a!=b){assert(dx*dx+dy*dy<=radius*radius);changed++;}
  if(dx*dx+dy*dy<=radius*radius){if(a)painted++;if(a==0xffff)white++;}
 }
#ifndef FOCUS_BASELINE
 if(centered)assert(changed>200&&painted>2100&&white>50);
#else
 assert(changed==0);
#endif
 /* Visual enlargement must not alter the original strict circular hit target. */
 if(centered){assert(nova_hit(s,x+25,y)==(int)s->focus);assert(nova_hit(s,x+26,y)!=(int)s->focus);}
 render(s,false);
}
static void open_display(unsigned w,unsigned h){
 fw=w;fh=h;stride=w+3;guarded=malloc((h*stride+2)*2);comparison=malloc(h*stride*2);assert(guarded&&comparison);
 for(unsigned i=0;i<h*stride+2;i++)guarded[i]=0xa55a;
 frame_pixels=guarded+1;assert(app_module_init()==0);api=t5_app_get_api(T5_APP_ABI_VERSION);assert(api);
 fixture_api=*api;fixture_api.installed_apps_get=fixture_get;api=&fixture_api;
 production=springboard_presentation_get();assert(production);observed=*production;observed.circle=circle;observed.icon=icon;
}
static void close_display(void){app_module_fini();assert(!frames&&!grants&&!subscriptions);guards();free(guarded);free(comparison);}
int main(void){
 /* Watch 240x240, compact display limits, rectangular/rotated displays. */
 const unsigned dimensions[][2]={{240,240},{160,160},{160,320},{320,160},{320,320},{399,399},{240,1024},{1024,240}};
 for(unsigned d=0;d<sizeof(dimensions)/sizeof(dimensions[0]);d++){
  open_display(dimensions[d][0],dimensions[d][1]);
  const unsigned inventories[]={1,2,3,7,14,17,19,20};
  for(unsigned c=0;c<sizeof(inventories)/sizeof(inventories[0]);c++)for(unsigned first=0;first<inventories[c];first+=19){
   count=inventories[c];nova_state s={.view=&observed,.first=first,.pending=-1,.time_known=true,.hour=10,.minute=40};nova_layout(&s);
   for(unsigned i=0;i<s.n;i++){
    s.motion.x=-(float)s.points[i].x/16;s.motion.y=-(float)s.points[i].y/16;s.focus=i;
    compare_focus(&s,true);
    if(d==0&&count==14&&(i==0||i==4)){char name[32];snprintf(name,sizeof(name),"focus-%u",i);save_frame(name);}
    if(count==14&&i==0&&d!=0)save_frame("focus-0");
   }
  }
  close_display();
 }
 /* Shared 19-app pan samples retain unchanged neighbour pixels and hit geometry.
  * All centers stay in the culling window for these between-icon movements. */
 open_display(240,240);count=19;nova_state s={.view=&observed,.pending=-1};nova_layout(&s);
 for(int x=-24;x<=24;x+=4)for(int y=-20;y<=20;y+=4){s.motion.x=(float)x;s.motion.y=(float)y;s.focus=nova_nearest(&s);compare_focus(&s,false);}
 /* No stacked focus scale during the existing pulse, including interruptions. */
 s.motion.x=s.motion.y=0;s.focus=0;s.visible_x=s.visible_y=0;
 nova_prepare_launch(&s,1,160);
 for(unsigned i=0;i<4;i++){render(&s,false);nova_tick(&s,40);}
 springboard_contact touch={.valid=true,.down=true,.began=true,.tap_eligible=true,.x=120,.y=120};
 nova_contact_input(&s,&touch,20);assert(s.pending<0);compare_focus(&s,true);
 close_display();
 puts("Springboard focus pixels: 8 display geometries; 1/2/3/7/14/17/19/20 apps and every settled focus; original hit targets, neighbour pixels, stride guards and launch pulse unchanged");
}
