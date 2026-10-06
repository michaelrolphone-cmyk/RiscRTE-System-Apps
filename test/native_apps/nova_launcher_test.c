#define main original_capture_main
#include "nova_peripherals.h"
#undef main
#include "PortableNovaUi.h"
int main(int argc,char **argv) {
  assert(argc==2);directory=argv[1];memset(pixels,0xa5,sizeof(pixels));assert(portable_catalog_count==14);
  assert(app_module_init()==0);const t5_app_api_v1 *api=t5_app_get_api(1);portable_nova_begin();
  for(unsigned i=0;i<portable_catalog_count;i++) {
    for(unsigned j=0;j<i;j++)assert(strcmp(portable_catalog[i].icon,portable_catalog[j].icon));
    int x=10+(int)(i%4)*57,y=10+(int)(i/4)*56;assert(api->draw_icon(x,y,portable_catalog[i].icon,40,false));
    unsigned painted=0;for(int yy=y;yy<y+40;yy++)for(int xx=x;xx<x+40;xx++)painted+=pixels[yy*244+xx]!=0;assert(painted>12);
  }
  api->present(false);app_main();app_module_fini();assert(!grants && !frames && !subs);
  puts("Fourteen distinct real glyphs and complete production launcher render/lifetime passed");return 0;
}
