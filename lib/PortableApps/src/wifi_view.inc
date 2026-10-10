#define portable_wifi_hit wifi_watch_hit
#define portable_wifi_render wifi_watch_render
#ifdef PORTABLE_NOVA_UI
#include "wifi_nova.inc"
#else
/* Wi-Fi's compact client-side view shares Settings typography and palette. */
#include "PortableWifiView.h"
#include "../settings_fonts/text.inc"
#define WV_CYAN 0x19e3ffu
#define WV_DIM 0x0e4f5cu
#define WV_LINE 0x12262bu
#define WV_CAP 0x6b8288u
static void wv_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
  if(!surface.frame || x<0 || y<0 || x>=240 || y>=240 || !alpha)return;
  x+=(width()-240)/2;y+=(height()-240)/2;
  uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+(size_t)x*2;
  uint16_t old=(uint16_t)(p[0]|p[1]<<8);
  unsigned r=(((old>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
  unsigned g=(((old>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
  unsigned b=((old&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
  uint16_t color=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
  p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);
}
static void wv_rect(int x,int y,int w,int h,uint32_t color) {
  for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)wv_pixel(i,j,color,255);
}
static void wv_text(unsigned face,int x,int y,int max_width,const char *s,uint32_t color) {
  if(!s || face>5)return;
  int pen=x*16;
  for(unsigned k=0;k<96 && s[k];++k) {
    unsigned ch=(unsigned char)s[k];if(ch<32 || ch>126)ch='?';
    const rps_glyph *g=&rps_text[face*95+ch-32];
    if(pen/16+g->advance_q4/16>x+max_width)break;
    for(unsigned j=0;j<g->height;++j)for(unsigned i=0;i<g->width;++i) {
      unsigned n=j*g->width+i,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
      wv_pixel(pen/16+g->left+(int)i,y+g->top+(int)j,color,a*85);
    }
    pen+=g->advance_q4;
  }
}
static void wv_button(int x,int y,int w,int h,const char *s,bool selected) {
  wv_rect(x,y,w,h,selected?WV_DIM:WV_LINE);
  wv_rect(x,y,w,1,selected?WV_CYAN:WV_DIM);
  wv_text(2,x+5,y+7,w-10,s,WV_CYAN);
}
int portable_wifi_hit(const portable_wifi_view *v,int x,int y) {
  x-=(width()-240)/2;y-=(height()-240)/2;
  if(x<0 || x>=240 || y<0 || y>=240)return -1;
  if(v->keyboard) {
    if(x>=12 && x<228 && y>=76 && y<172)return (y-76)/24*8+(x-12)/27;
    if(y>=178 && y<207 && x>=12 && x<228)return 32+(x-12)/72;
  } else if(x>=18 && x<222 && y>=64 && y<188) {
    unsigned i=v->selected/4*4+(unsigned)(y-64)/31;
    if(i<v->count)return (int)i;
  }
  return -1;
}
void portable_wifi_render(const portable_wifi_view *v) {
  if(failed || width()<240 || height()<240)return;
  list_mode=false;clear();fill(0,0,width(),height(),0);
  wv_text(2,18,17,40,"<",WV_CYAN);
  wv_text(2,45,17,176,v->title,WV_CAP);wv_rect(18,40,204,1,WV_DIM);
  if(v->keyboard) {
    wv_text(2,18,48,204,v->entry,WV_CYAN);
    for(unsigned i=0;i<32;++i) {
      unsigned ch=32+v->key_page*32+i;
      char key[2]={(char)(ch<=126?ch:' '),0};
      wv_button(12+(int)(i%8)*27,76+(int)(i/8)*24,25,22,ch==32?"_":key,v->selected==i);
    }
    wv_button(12,178,68,29,"ABC/#",v->selected==32);
    wv_button(84,178,68,29,"DELETE",v->selected==33);
    wv_button(156,178,72,29,"DONE",v->selected==34);
    wv_text(3,18,217,204,v->message,WV_CAP);
  } else {
    wv_text(3,18,47,204,v->detail,WV_CYAN);
    unsigned first=v->selected/4*4;
    for(unsigned j=0;j<4 && first+j<v->count;++j) {
      unsigned i=first+j;int y=64+(int)j*31;
      wv_rect(18,y,2,30,i==v->selected?WV_CYAN:WV_LINE);wv_rect(20,y+30,202,1,WV_LINE);
      wv_text(2,27,y+4,194,v->rows[i],0xffffff);
      wv_text(3,27,y+18,194,v->values[i],WV_CYAN);
    }
    wv_text(3,18,192,204,v->message,WV_CAP);
    wv_button(12,212,64,26,"UP",false);wv_button(88,212,64,26,"SELECT",false);wv_button(164,212,64,26,"DOWN",false);
  }
  present(false);
}

#endif

#undef portable_wifi_hit
#undef portable_wifi_render
#include "wifi_paper.inc"
int portable_wifi_hit(const portable_wifi_view *v,int x,int y) {
  return pp_enabled()?wifi_paper_hit(v,x,y):wifi_watch_hit(v,x,y);
}
void portable_wifi_render(const portable_wifi_view *v) {
  if(pp_enabled())wifi_paper_render(v);else wifi_watch_render(v);
}
bool portable_wifi_paper(void) {return pp_enabled();}
