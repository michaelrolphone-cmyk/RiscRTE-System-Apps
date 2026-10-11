/* Pixel assertions exercise the production RGB565 caption/icon implementation,
 * with a padded stride. Optional PPMs are direct C frames, not UI mockups. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/PortableApps/src/adapter.c"

const t5_app_manifest_t portable_catalog[]={{.display_name="Stopwatch",.icon="solid:f2f2"}};
const unsigned portable_catalog_count=1;
static bool caption_health(risc_runtime_health_v1 *out){out->uptime_ms=0;return true;}
static const risc_runtime_api_v1 caption_runtime={.api_version=1,.struct_size=sizeof(caption_runtime),.health=caption_health};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&caption_runtime:NULL;}
static uint16_t pixels[240*243];
static unsigned char coverage[240*240];
static void reset_pixels(uint16_t color){
 info.width=info.height=240;
 surface=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=240,.height=240,
  .stride_bytes=486,.size_bytes=sizeof(pixels),.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 nova_mask_pending=false;
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<243;x++)pixels[y*243+x]=x<240?color:0xa55a;
}
static void guards(void){for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(pixels[y*243+x]==0xa55a);}
static void save_frame(const char *name){
 const char *dir=getenv("NOVA_VISUALS");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);
 fprintf(f,"P6\n240 240\n255\n");
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++){
  unsigned p=pixels[y*243+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
  assert(fwrite(rgb,1,3,f)==3);
 }
 assert(!fclose(f));
}
static void caption_pixels(const char *s,bool clock,int y){
 /* Independently reconstruct only text positions/coverage, without using the
  * production backing helper. The black interior includes interletter space. */
 int total=0,length=0,spacing=clock?21:34;
 while(length<95 && s[length]){
  unsigned c=(unsigned char)s[length];if(c>='a'&&c<='z')c-=32;if(c<32||c>126)c='?';
  int next=total+rpt_text[(clock?0:95)+c-32].advance_q4+(length?spacing:0);
  if(next>224*16)break;
  total=next;length++;
 }
 int x=(240*16-total)/2,l=240,t=240,r=-1,b=-1;
 memset(coverage,0,sizeof(coverage));
 for(int k=0;k<length;k++){
  unsigned c=(unsigned char)s[k];if(c>='a'&&c<='z')c-=32;if(c<32||c>126)c='?';
  const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
  if(g->width&&g->height){
   int gx=x/16+g->left,gy=y+g->top;
   if(gx<l)l=gx;
   if(gy<t)t=gy;
   if(gx+g->width-1>r)r=gx+g->width-1;
   if(gy+g->height-1>b)b=gy+g->height-1;
   for(unsigned j=0;j<g->height;j++)for(unsigned i=0;i<g->width;i++){
    unsigned n=j*g->width+i,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
    if(gx+(int)i>=0&&gx+(int)i<240&&gy+(int)j>=0&&gy+(int)j<240)coverage[(gy+j)*240+gx+i]=(unsigned char)(a*85);
   }
  }
  x+=g->advance_q4+spacing;
 }
 reset_pixels(0xffff);np_caption(y,s,clock,0xffffff);guards();
 if(r<l){for(unsigned yy=0;yy<240;yy++)for(unsigned xx=0;xx<240;xx++)assert(pixels[yy*243+xx]==0xffff);return;}
 l-=2;r+=2;t-=2;b+=2;
 unsigned whites=0,space=0,feather=0,untouched=0;
 for(int yy=0;yy<240;yy++)for(int xx=0;xx<240;xx++){
  uint16_t pixel=pixels[yy*243+xx];unsigned a=coverage[yy*240+xx];
  if(xx>=l&&xx<=r&&yy>=t&&yy<=b){
   assert(pixel==np_rgb(a*0x010101u));
   if(a==255)whites++;
   if(a==0)space++;
  }else{
   int dx=xx<l?l-xx:xx>r?xx-r:0,dy=yy<t?t-yy:yy>b?yy-b:0;
   if(dx*dx+dy*dy>=36){assert(pixel==0xffff);untouched++;}
   else{assert(pixel>0);if(pixel<0xffff)feather++;}
  }
 }
 assert(whites&&space&&feather&&untouched);
 if(t>=6){
  /* Black to transparent is smooth and monotonic, with no abrupt box outline. */
  uint16_t previous=0;int middle=(l+r)/2;
  for(int d=1;d<=6;d++){uint16_t pixel=pixels[(t-d)*243+middle];assert(pixel>previous);previous=pixel;}
  assert(previous==0xffff);
 }
}
static void bright_icon_scene(void){
 reset_pixels(0);
 static const uint32_t colors[]={0xf08c22,0xe448a4,0x22b45a,0x3978e8,0x8b5cf6};
 for(unsigned i=0;i<5;i++){
  int x=24+(int)i*48;
  np_circle(x,24,30,colors[i],255);assert(np_icon(x,24,30,"solid:f2f2",255));
  np_circle(x,204,30,colors[4-i],255);assert(np_icon(x,204,30,"solid:f017",255));
 }
 np_circle(84,116,31,0x0891b2,255);assert(np_icon(84,116,38,"solid:f017",255));
 np_circle(156,116,31,0xed4f54,255);assert(np_icon(156,116,38,"solid:f2f2",255));
 nova_mask_pending=true;np_mask();save_frame("caption-bright-icons-before");
 np_caption(16,"10:40 AM",true,0xffffff);np_caption(197,"STOPWATCH",false,0xffffff);
 guards();save_frame("caption-bright-icons-after");
}
int main(void){
 rt=&caption_runtime;
 caption_pixels("10:40 AM",true,16);caption_pixels("--:--",true,16);
 caption_pixels("STOPWATCH",false,197);caption_pixels("Long app name with many words truncated",false,197);
 caption_pixels("A  A",false,100);caption_pixels("",false,197);caption_pixels("   ",true,16);
 caption_pixels("A",false,-5);caption_pixels("A",false,235);
 reset_pixels(0);assert(np_icon(60,120,48,"solid:f017",255));assert(np_icon(180,120,48,"solid:f2f2",255));
 bool differs=false;for(unsigned y=80;y<160;y++)for(unsigned x=25;x<95;x++)if(pixels[y*243+x]!=pixels[y*243+x+120])differs=true;
 assert(differs);assert(!np_icon(120,120,24,"solid:ffff",255));guards();
 bright_icon_scene();
 puts("Caption pixels: solid full-block black, crisp white, six-pixel rounded feather, spaces, clipping, stride guards; distinct genuine stopwatch raster");
}
