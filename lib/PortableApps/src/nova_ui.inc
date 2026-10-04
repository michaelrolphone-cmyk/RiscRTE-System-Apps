/* Settings is the visual authority: its unmodified Rajdhani/Orbitron glyphs,
 * palette and six-pixel rounded outlines are shared by portable utility apps. */
#include "PortableNovaUi.h"
#include "../settings_fonts/text.inc"
static void nu_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
  if(!surface.frame || x<0 || y<0 || x>=240 || y>=240 || !alpha)return;
  x+=(width()-240)/2;y+=(height()-240)/2;
  uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+(size_t)x*2;
  uint16_t old=(uint16_t)(p[0]|p[1]<<8);
  if(alpha>255)alpha=255;
  unsigned r=(((old>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
  unsigned g=(((old>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
  unsigned b=((old&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
  uint16_t c=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
}
static void nu_rect(int x,int y,int w,int h,uint32_t rgb) {
  if(w<=0 || h<=0 || w>240 || h>240)return;
  for(int j=0;j<h;j++)for(int i=0;i<w;i++)nu_pixel(x+i,y+j,rgb,255);
}
static int nu_measure(unsigned face,const char *s) {
  int n=0;if(!s || face>5)return 0;
  for(unsigned i=0;i<96 && s[i];i++){unsigned c=(unsigned char)s[i];if(c<32 || c>126)c='?';n+=rps_text[face*95+c-32].advance_q4;}
  return (n+15)/16;
}
void portable_nova_text(unsigned face,int x,int y,int max_width,const char *s,uint32_t rgb) {
  if(!s || face>5 || max_width<=0)return;
  int pen=x*16;
  for(unsigned k=0;k<96 && s[k];k++) {
    unsigned c=(unsigned char)s[k];if(c<32 || c>126)c='?';const rps_glyph *g=&rps_text[face*95+c-32];
    if(pen/16+(g->advance_q4+15)/16>x+max_width)break;
    for(unsigned j=0;j<g->height;j++)for(unsigned i=0;i<g->width;i++) {
      unsigned n=j*g->width+i,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
      int px=pen/16+g->left+(int)i;if(px>=x && px<x+max_width)nu_pixel(px,y+g->top+(int)j,rgb,a*85);
    }
    pen+=g->advance_q4;
  }
}
void portable_nova_center(unsigned face,int x,int y,int w,const char *s,uint32_t rgb) {
  int size=nu_measure(face,s);
  if(size>w && face<=1){face=2;size=nu_measure(face,s);}
  portable_nova_text(face,x+(size<w?(w-size)/2:0),y,w,s,rgb);
}
void portable_nova_begin(void) {list_mode=false;clear_color(0);}
void portable_nova_rule(int x,int y,int w) {nu_rect(x,y,w,1,NOVA_DIM);}
void portable_nova_button(int x,int y,int w,int h,const char *s,bool selected) {
  if(w<1 || h<1 || w>240 || h>240)return;
  for(int j=0;j<h;j++)for(int i=0;i<w;i++) {
    int dx=i<6?6-i:i>=w-6?i-(w-7):0,dy=j<6?6-j:j>=h-6?j-(h-7):0;
    if(dx*dx+dy*dy>36)continue;
    bool edge=i==0 || i==w-1 || j==0 || j==h-1 || (dx && dy && dx*dx+dy*dy>22);
    nu_pixel(x+i,y+j,edge?(selected?NOVA_CYAN:NOVA_DIM):(selected?0x062d35u:0x031115u),255);
  }
  unsigned face=nu_measure(1,s)<=w-12?1:2;
  portable_nova_center(face,x+6,y+(h-(face==1?15:12))/2-2,w-12,s,NOVA_CYAN);
}
void portable_nova_row(int x,int y,int w,int h,const char *caption,const char *value,bool selected) {
  portable_nova_button(x,y,w,h,"",selected);
  nu_rect(x+8,y+9,2,h-18,selected?NOVA_CYAN:NOVA_DIM);
  portable_nova_text(1,x+18,y+7,w-30,caption,NOVA_WHITE);
  portable_nova_text(2,x+18,y+27,w-30,value,NOVA_CYAN);
}
void portable_nova_header(const char *title) {
  portable_nova_button(8,4,44,44,"<",false);
  portable_nova_text(nu_measure(0,title)<=168?0:1,62,15,168,title,NOVA_CAP);
  portable_nova_rule(16,49,208);
}
bool portable_nova_hit(int x,int y,int left,int top,int w,int h) {
  return x>=left && y>=top && x<left+w && y<top+h;
}

bool portable_nova_wrap(unsigned face,int x,int y,int w,int line_height,unsigned max_lines,const char *s,uint32_t color) {
  if(!s || face>5 || w<=0 || line_height<=0 || max_lines<1 || max_lines>4)return false;
  unsigned at=0;
  for(unsigned row=0;row<max_lines && s[at] && at<96;row++) {
    char line[97]={0};unsigned n=0,space=0;
    while(at+n<96 && s[at+n] && n<96) {
      line[n]=s[at+n];line[n+1]=0;
      if(nu_measure(face,line)>w){line[n]=0;break;}
      if(line[n]==' ')space=n;
      n++;
    }
    if(!n)return false;
    bool more=at+n<96 && s[at+n];
    if(more && row+1<max_lines && space){n=space;line[n]=0;}
    else if(more && row+1==max_lines) {
      while(n && (n+3>96 || nu_measure(face,line)>w)){line[--n]=0;}
      while(n) {line[n]='.';line[n+1]='.';line[n+2]='.';line[n+3]=0;if(nu_measure(face,line)<=w)break;line[--n]=0;}
      portable_nova_text(face,x,y+(int)row*line_height,w,line,color);return false;
    }
    portable_nova_text(face,x,y+(int)row*line_height,w,line,color);at+=n;
    while(at<96 && s[at]==' ')at++;
  }
  return at<96 && !s[at];
}
