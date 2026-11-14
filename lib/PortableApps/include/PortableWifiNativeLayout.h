#pragma once
#include "PortableWifiView.h"
/* Pure content-local geometry. Provider safe origin is applied only by adapter.
 * Half-open rectangles leave all painted gutters inert; IDs preserve local
 * keyboard semantics. No device identity, scaling, state or provider access. */
typedef struct { int x,y,w,h; } wifi_native_rect;
typedef struct {
 int width,height; unsigned row_count;
 wifi_native_rect back,title,detail,message,entry,rows[8],footer[3],keys[PWK_COUNT];
} wifi_native_layout;
static inline bool wifi_native_contains(wifi_native_rect r,int x,int y) {
 return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
}
static inline bool wifi_native_layout_open(wifi_native_layout *l,int w,int h) {
 if(!l||w<240||h<240||w>2048||h>2048)return false;
 *l=(wifi_native_layout){.width=w,.height=h};
 int m=8,g=4,inner=w-2*m;
 l->back=(wifi_native_rect){m,m,44,40};
 l->title=(wifi_native_rect){m+52,m,inner-52,40};
 l->detail=(wifi_native_rect){m,52,inner,20};l->entry=l->detail;
 l->message=(wifi_native_rect){m,h-76,inner,24};
 int available=h-80-76;l->row_count=(unsigned)(available/44);
 if(l->row_count<2)l->row_count=2;
 if(l->row_count>8)l->row_count=8;
 for(unsigned j=0;j<l->row_count;j++){
  int top=76+available*(int)j/(int)l->row_count;
  int bottom=76+available*(int)(j+1)/(int)l->row_count;
  l->rows[j]=(wifi_native_rect){m,top,inner,bottom-top-g};
 }
 for(unsigned j=0;j<3;j++){
  int left=m+inner*(int)j/3,right=m+inner*(int)(j+1)/3;
  l->footer[j]=(wifi_native_rect){left,h-48,right-left-g,40};
  l->keys[PWK_PAGE+j]=l->footer[j];
 }
 int kh=h-80-76;
 for(unsigned j=0;j<PWK_CHARACTERS;j++){
  int col=(int)(j%8),row=(int)(j/8);
  int left=m+inner*col/8,right=m+inner*(col+1)/8;
  int top=76+kh*row/4,bottom=76+kh*(row+1)/4;
  l->keys[j]=(wifi_native_rect){left,top,right-left-g,bottom-top-g};
 }
 return true;
}
static inline int wifi_native_layout_hit(const wifi_native_layout*l,const portable_wifi_view*v,int x,int y) {
 if(!l||!v||x<0||y<0||x>=l->width||y>=l->height)return -1;
 if(wifi_native_contains(l->back,x,y))return WIFI_HIT_BACK;
 if(v->keyboard){for(unsigned j=0;j<PWK_COUNT;j++)if(wifi_native_contains(l->keys[j],x,y))return (int)j;}
 else {
  const int ids[]={WIFI_HIT_PREVIOUS,WIFI_HIT_SELECT,WIFI_HIT_NEXT};
  for(unsigned j=0;j<3;j++)if(wifi_native_contains(l->footer[j],x,y))return ids[j];
  unsigned first=v->selected/l->row_count*l->row_count;
  for(unsigned j=0;j<l->row_count&&first+j<v->count;j++)if(wifi_native_contains(l->rows[j],x,y))return (int)(first+j);
 }
 return -1;
}
