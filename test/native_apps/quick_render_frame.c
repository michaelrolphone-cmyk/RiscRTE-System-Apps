#include "PortableQuickRender.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char pixels[240*240*2];
int main(int argc,char **argv) {
    if(argc<2)return 2;
    for(int y=0;y<240;++y)for(int x=0;x<240;++x){
        unsigned v=(unsigned)((x/20+y/20)%2?0x224a:0x39f0);
        pixels[2*(y*240+x)]=(unsigned char)v;pixels[2*(y*240+x)+1]=(unsigned char)(v>>8);
    }
    pqa_state s;pqa_init(&s);pqa_set_levels(&s,true,70,true,40);
    s.position_q8=s.target_q8=(argc>2?atoi(argv[2]):240)*256;
    if(argc>3)s.volume=0;
    if(argc>4)s.torch=true;
    risc_display_surface_v1 f={1,pixels,240,240,480,sizeof(pixels),RISC_DISPLAY_FORMAT_RGB565};
    if(!pqa_render(&f,&s,"10:42",true,84))return 3;
    FILE *file=fopen(argv[1],"wb");if(!file)return 4;
    fprintf(file,"P6\n240 240\n255\n");
    for(int i=0;i<240*240;++i) {
        unsigned v=pixels[2*i]|(pixels[2*i+1]<<8);
        unsigned char rgb[3]={(unsigned char)((v>>11)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};
        fwrite(rgb,1,3,file);
    }
    fclose(file);return 0;
}
