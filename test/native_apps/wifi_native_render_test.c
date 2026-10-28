#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RiscDisplayOutputV1.h"
#include "PortableSafeViewport.h"
#include "PortableNovaUi.h"
#define PORTABLE_RF_NATIVE_CANVAS
#define PORTABLE_WATCH_SAFE_INSETS
#define PORTABLE_RASTER_SNAPSHOT
static risc_display_surface_v1 surface;
static portable_safe_viewport content_view;
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
static bool failed,list_mode,raster_replaying,raster_band_columns;
static int raster_band_top,raster_band_bottom;
static unsigned raster_pixels;
static int width(void){return (int)content_view.width;}
static int height(void){return (int)content_view.height;}
static bool raster_checkpoint(void){return true;}
static bool raster_surface_writable(void){return surface.pixels!=NULL;}
static void native_point(int*x,int*y){*x+=(int)content_view.x;*y+=(int)content_view.y;}
static void np_pixel(int x,int y,uint32_t c,unsigned a){(void)x;(void)y;(void)c;(void)a;assert(0);}
static void raster_rows(int64_t origin,int*first,int*end){if(!raster_replaying)return;int64_t t=raster_band_top-origin,b=raster_band_bottom-origin;if(t>*first)*first=t<*end?(int)t:*end;if(b<*end)*end=b>*first?(int)b:*first;}
enum {RS_NU_NATIVE_ROUND,RS_NU_NATIVE_TEXT};
static bool raster_record(int kind,int32_t a[8],const char*s){(void)kind;(void)a;(void)s;return false;}
static void clear(void){}
static void present(bool ignored){(void)ignored;}
#include "../../lib/PortableApps/settings_fonts/text.inc"
int portable_nova_measure(unsigned face,const char*s){int advance=0;for(unsigned k=0;s&&s[k]&&k<96;k++){unsigned c=(unsigned char)s[k];if(c<32||c>126)c='?';advance+=rps_text[face*95+c-32].advance_q4;}return (advance+15)/16;}
#include "../../lib/PortableApps/src/nova_native.inc"
#include "PortableWifiNativeLayout.h"
static const portable_wifi_view *observed_view;
static unsigned observed_keys;
static void observed_native_text(unsigned face,int x,int y,int max_width,const char*s,uint32_t c){
 if(observed_view&&observed_view->keyboard){
  wifi_native_layout l;assert(wifi_native_layout_open(&l,width(),height()));
  for(unsigned key=0;key<PWK_CHARACTERS;key++){
   wifi_native_rect r=l.keys[key];
   if(x==r.x+4&&y==r.y+(r.h-17)/2){
    unsigned ch=portable_watch_key_character(observed_view->key_page,key);
    char expected=(char)(ch==32?'_':ch?ch:' ');
    assert(face==1&&max_width==r.w-8&&s&&s[0]==expected&&s[1]==0);
    assert(wifi_native_layout_hit(&l,observed_view,r.x+r.w/2,r.y+r.h/2)==(int)key);observed_keys++;
   }
  }
 }
 portable_nova_native_text(face,x,y,max_width,s,c);
}
#define portable_nova_native_text observed_native_text
#include "../../lib/PortableApps/src/wifi_native.inc"
#undef portable_nova_native_text
static void check(int w,int h,int inset,const char*dir){
 int pw=w+2*inset,ph=h+2*inset,stride=pw*2+7;size_t size=(size_t)stride*ph;
 unsigned char*base=malloc(size+2),*reference=malloc(size);assert(base&&reference);
 surface=(risc_display_surface_v1){.pixels=base+1,.width=pw,.height=ph,.stride_bytes=stride,.size_bytes=size,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 assert(portable_safe_viewport_open(&content_view,pw,ph,inset,inset,inset,inset));
 portable_wifi_view v={.title="Wi-Fi",.detail="Disconnected",.message="Not saved; explicit Save required",.entry="****************",.count=17};
 for(unsigned i=0;i<17;i++){v.rows[i]="Long synthetic network name 12345";v.values[i]="Unavailable; retry explicitly";}
 for(unsigned state=0;state<8;state++){
  v.keyboard=state>=3;v.selected=state%3==0?16:state%3==1?0:34;v.key_page=state%3;
  if(!v.keyboard&&v.selected>=17)v.selected=4;
  if(state==1){v.count=0;v.message="Scanning...";}else v.count=17;
  if(state==2)v.message="Storage outcome uncertain";
  observed_view=&v;observed_keys=0;memset(base,0xa5,size+2);raster_replaying=false;portable_wifi_render(&v);assert(observed_keys==(v.keyboard?32u:0u));memcpy(reference,base+1,size);
  wifi_native_layout layout;assert(wifi_native_layout_open(&layout,w,h));unsigned title_pixels=0;
  for(int y=layout.title.y;y<layout.title.y+layout.title.h;y++)for(int x=layout.title.x;x<layout.title.x+layout.title.w;x++){unsigned char *pixel=reference+(size_t)(y+inset)*stride+(x+inset)*2;if(pixel[0]||pixel[1])title_pixels++;}
  assert(title_pixels>10);
  if(v.keyboard)for(unsigned key=0;key<32;key++){
   wifi_native_rect r=layout.keys[key];int next=key%8==7?w-8:layout.keys[key+1].x;
   for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x+r.w;x<next;x++){
    unsigned char*pixel=reference+(size_t)(y+inset)*stride+(x+inset)*2;assert(!pixel[0]&&!pixel[1]);
   }
  }
  if(dir&&w==328&&h==328){char path[512];snprintf(path,sizeof(path),"%s/wifi-native-%u.ppm",dir,state);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",pw,ph);for(int y=0;y<ph;y++)for(int x=0;x<pw;x++){unsigned char*p=reference+(size_t)y*stride+x*2;unsigned c=p[0]|p[1]<<8;unsigned char rgb[]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};/* Circular provider aperture, outside physical circle is background. */if(x<inset||x>=pw-inset||y<inset||y>=ph-inset||(2*x-pw+1)*(2*x-pw+1)+(2*y-ph+1)*(2*y-ph+1)>pw*pw)memset(rgb,0,3);fwrite(rgb,1,3,f);}fclose(f);}
  for(unsigned axis=0;axis<2;axis++){
   memset(base,0xa5,size+2);raster_replaying=true;raster_band_columns=axis;
   int limit=axis?pw:ph;for(int n=0;n<limit;n+=13){raster_band_top=n;raster_band_bottom=n+13;
    unsigned char *before=malloc(size);assert(before);memcpy(before,base+1,size);
    portable_wifi_render(&v);
    for(int y=0;y<ph;y++)for(int x=0;x<pw;x++)if((axis?x:y)<n||(axis?x:y)>=n+13)assert(!memcmp(before+(size_t)y*stride+x*2,base+1+(size_t)y*stride+x*2,2));
    free(before);}
   assert(!memcmp(reference,base+1,size));assert(base[0]==0xa5&&base[size+1]==0xa5);
  }
  for(int y=0;y<ph;y++)for(int x=0;x<stride;x++)if(y<inset||y>=ph-inset||x<inset*2||x>=(pw-inset)*2)assert(reference[(size_t)y*stride+x]==0xa5);
 }
 free(base);free(reference);
}
int main(int argc,char**argv){check(240,240,0,NULL);check(328,328,69,argc>1?argv[1]:NULL);check(327,411,11,NULL);check(502,410,7,NULL);puts("Production Wi-Fi native renderer: unaligned/strided guards, row/column sliced parity, masked entry, long SSID and states PASS");}
