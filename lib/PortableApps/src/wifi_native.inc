/* Native safe-content Wi-Fi presentation; controller supplies masked entry and
 * authoritative status. This layer never changes credentials or radio state. */
#include "PortableWifiNativeLayout.h"
#include "PortableNovaUi.h"
static void wn_box(wifi_native_rect r,const char*s,bool selected) {
 portable_nova_native_fill(r.x,r.y,r.w,r.h,selected?NOVA_DIM:NOVA_LINE);
 portable_nova_native_fill(r.x,r.y,r.w,1,selected?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_text(1,r.x+4,r.y+(r.h-17)/2,r.w-8,s,NOVA_CYAN);
}
static void wn_text(wifi_native_rect r,const char*s,uint32_t c) {
 portable_nova_native_text(1,r.x,r.y+1,r.w,s,c);
}
int portable_wifi_hit(const portable_wifi_view*v,int x,int y) {
 wifi_native_layout l;if(!wifi_native_layout_open(&l,width(),height()))return -1;
 return wifi_native_layout_hit(&l,v,x,y);
}
void portable_wifi_render(const portable_wifi_view*v) {
 wifi_native_layout l;if(failed||!v||!wifi_native_layout_open(&l,width(),height()))return;
 list_mode=false;clear();portable_nova_native_fill(0,0,l.width,l.height,0);
 wn_box(l.back,"Back",false);
 portable_nova_native_text(0,l.title.x,l.title.y+12,l.title.w,v->title,NOVA_CAP);
 /* Two small original-font lines retain longer status/uncertainty notices. */
 const char *message=v->message;
 for(unsigned line=0;message&&*message&&line<2;line++){
  char text[97];unsigned n=0;
  while(message[n]&&n<96){text[n]=message[n];text[n+1]=0;if(portable_nova_measure(3,text)>l.message.w)break;n++;}
  if(!n)break;
  text[n]=0;portable_nova_native_text(3,l.message.x,l.message.y+line*12,l.message.w,text,NOVA_CAP);message+=n;
 }
 if(v->keyboard){
  wn_text(l.entry,v->entry,NOVA_CYAN);
  for(unsigned i=0;i<PWK_COUNT;i++){
   unsigned ch=portable_watch_key_character(v->key_page,i);char text[]={(char)(ch?ch:' '),0};
   const char*label=i==PWK_PAGE?"ABC/#":i==PWK_DELETE?"Delete":i==PWK_DONE?"Done":ch==32?"_":text;
   wn_box(l.keys[i],label,v->selected==i);
  }
 }else{
  wn_text(l.detail,v->detail,NOVA_CYAN);
  unsigned first=v->selected/l.row_count*l.row_count;
  for(unsigned j=0;j<l.row_count&&first+j<v->count;j++){
   unsigned i=first+j;wifi_native_rect r=l.rows[j];wn_box(r,"",i==v->selected);
   portable_nova_native_text(1,r.x+8,r.y+3,r.w-16,v->rows[i],NOVA_WHITE);
   portable_nova_native_text(3,r.x+8,r.y+24,r.w-16,v->values[i],NOVA_CYAN);
  }
  wn_box(l.footer[0],"Prev",false);wn_box(l.footer[1],"Select",false);wn_box(l.footer[2],"Next",false);
 }
 present(false);
}
