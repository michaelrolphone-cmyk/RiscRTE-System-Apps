/* Wi-Fi keeps its controller and credential lifecycle. Only app-local view and
 * hit geometry change; every visible action uses a >=44px target. */
#include "PortableWifiView.h"
#include <stdio.h>
#include "PortableNovaKeyboard.h"
int portable_wifi_hit(const portable_wifi_view *v,int x,int y) {
  x-=(width()-240)/2;y-=(height()-240)/2;
  if(v->keyboard) {
    for(unsigned i=0;i<8;i++)if(portable_nova_hit(x,y,16+(int)(i%4)*53,84+(int)(i/4)*48,49,44))
      return portable_nova_key_character(v->key_page,i)!=0?(int)i:-1;
    for(unsigned i=0;i<4;i++)if(portable_nova_hit(x,y,16+(int)i*53,188,49,44))return (int)(8+i);
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
    portable_nova_text(1,18,52,204,v->entry,NOVA_CYAN);
    char status[40];snprintf(status,sizeof(status),"Characters %u / 12",v->key_page+1);
    portable_nova_center(3,18,69,204,status,NOVA_CAP);
    for(unsigned i=0;i<8;i++) {
      unsigned ch=portable_nova_key_character(v->key_page,i);if(!ch)continue;
      char key[]={(char)ch,0};portable_nova_button(16+(int)(i%4)*53,84+(int)(i/4)*48,49,44,ch==32?"Space":key,v->selected==i);
    }
    const char *labels[]={"Prev","Next","Del","Done"};
    for(unsigned i=0;i<4;i++)portable_nova_button(16+(int)i*53,188,49,44,labels[i],v->selected==8+i);
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
