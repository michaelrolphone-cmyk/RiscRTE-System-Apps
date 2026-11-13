#include <assert.h>
#include <stdio.h>
#include "PortableWifiNativeLayout.h"
static void check(int w,int h){
 wifi_native_layout l;assert(wifi_native_layout_open(&l,w,h));
 portable_wifi_view v={.count=17};
 for(unsigned k=0;k<2;k++)for(unsigned selected=0;selected<(k?35u:17u);selected++){
  v.keyboard=k;v.selected=selected;
  for(int y=-1;y<=h;y++)for(int x=-1;x<=w;x++){
   int expected=-1;
   if(wifi_native_contains(l.back,x,y))expected=WIFI_HIT_BACK;
   if(k){for(unsigned i=0;i<35;i++)if(wifi_native_contains(l.keys[i],x,y)){assert(expected==-1);expected=(int)i;}}
   else{
    const int ids[]={WIFI_HIT_PREVIOUS,WIFI_HIT_SELECT,WIFI_HIT_NEXT};
    for(unsigned i=0;i<3;i++)if(wifi_native_contains(l.footer[i],x,y)){assert(expected==-1);expected=ids[i];}
    unsigned first=selected/l.row_count*l.row_count;
    for(unsigned i=0;i<l.row_count&&first+i<v.count;i++)if(wifi_native_contains(l.rows[i],x,y)){assert(expected==-1);expected=(int)(first+i);}
   }
   assert(wifi_native_layout_hit(&l,&v,x,y)==expected);
  }
 }
 for(unsigned i=0;i<35;i++){wifi_native_rect r=l.keys[i];assert(r.w>0&&r.h>0&&r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h);}
}
int main(void){wifi_native_layout l;assert(!wifi_native_layout_open(&l,239,328));assert(!wifi_native_layout_open(&l,328,239));assert(!wifi_native_layout_open(&l,2049,328));check(240,240);check(328,328);check(327,411);check(502,410);check(640,480);for(int n=0;n<8;n++)check(240+n*13,240+n*29);puts("Native Wi-Fi half-open geometry, all pages/rows/keys/gutters: PASS");}
