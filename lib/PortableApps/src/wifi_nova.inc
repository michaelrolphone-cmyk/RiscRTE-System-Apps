/* Wi-Fi keeps its controller and credential lifecycle. Only app-local view and
 * hit geometry change; text entry retains the standard 32-key Watch cells. */
#include "PortableWifiView.h"
#include <stdio.h>
#include "PortableWatchKeyboard.h"
int portable_wifi_hit(const portable_wifi_view *v,int x,int y) {
  x-=(width()-240)/2;y-=(height()-240)/2;
  if(v->keyboard) {
    return portable_watch_key_hit(x,y);
  } else {
    for(unsigned j=0;j<2;j++)if(portable_nova_hit(x,y,16,62+(int)j*58,208,54)) {
      unsigned i=v->selected/2*2+j;return i<v->count?(int)i:-1;
    }
  }
  return -1;
}
void portable_wifi_render(const portable_wifi_view *v) {
  if(failed || width()<240 || height()<240)return;
  portable_nova_begin();portable_nova_header(v->title);
  if(v->keyboard) {
    portable_nova_text(2,18,48,204,v->entry,NOVA_CYAN);
    for(unsigned i=0;i<PWK_COUNT;i++) {
      portable_watch_key_rect r;if(!portable_watch_key_bounds(i,&r))continue;
      unsigned ch=portable_watch_key_character(v->key_page,i);
      char text[]={(char)(ch?ch:' '),0};
      const char *label=i==PWK_PAGE?"ABC/#":i==PWK_DELETE?"DELETE":i==PWK_DONE?"DONE":ch==32?"_":text;
      portable_nova_fill(r.x,r.y,r.w,r.h,v->selected==i?NOVA_DIM:NOVA_LINE);
      portable_nova_fill(r.x,r.y,r.w,1,v->selected==i?NOVA_CYAN:NOVA_DIM);
      portable_nova_text(2,r.x+5,r.y+7,r.w-10,label,NOVA_CYAN);
    }
  } else {
    portable_nova_text(3,18,50,204,v->detail,NOVA_CYAN);
    unsigned first=v->selected/2*2;
    for(unsigned j=0;j<2 && first+j<v->count;j++) {
      unsigned i=first+j;int y=62+(int)j*58;
      portable_nova_button(16,y,208,54,"",i==v->selected);
      (void)portable_nova_wrap(1,28,y+4,184,15,1,v->rows[i],NOVA_WHITE);
      (void)portable_nova_wrap(3,28,y+25,184,11,2,v->values[i],NOVA_CYAN);
    }
    portable_nova_text(3,18,175,204,v->message,NOVA_CAP);
    portable_nova_button(16,188,64,44,"Prev",false);portable_nova_button(88,188,64,44,"Select",false);portable_nova_button(160,188,64,44,"Next",false);
  }
  present(false);
}
