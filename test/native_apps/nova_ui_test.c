#include "nova_peripherals.h"
#include "PortableNovaUi.h"
#include "PortableNovaKeyboard.h"
#include "../../lib/PortableApps/settings_fonts/text.inc"
static void signed_bearing_test(void) {
    /* Compare every pixel with the stored glyph, including negative bearings. */
    for(unsigned face=0;face<=6;face++)for(unsigned ch=32;ch<=126;ch++) {
        unsigned source=face==6?0:face,scale=face==6?183:256;
        const rps_glyph *g=&rps_text[source*95+ch-32];
        portable_nova_begin();char text[2]={(char)ch,0};
        portable_nova_text(face,90,80,100,text,0xffffff);
        unsigned expected=0,actual=0;
        for(int y=0;y<(int)(g->height*scale/256);y++)for(int x=0;x<(int)(g->width*scale/256);x++) {
            unsigned sx=(unsigned)x*256/scale,sy=(unsigned)y*256/scale,n=sy*g->width+sx;
            unsigned a=(g->bits[n/4]>>(6-2*(n%4)))&3;
            int px=90+(int)g->left*(int)scale/256+x,py=80+(int)g->top*(int)scale/256+y;
            if(a&&px>=90&&px<190){expected++;assert(pixels[py*244+px]!=0);}
        }
        for(unsigned y=40;y<180;y++)for(unsigned x=40;x<200;x++)if(pixels[y*244+x])actual++;
        assert(expected==actual);
    }
}
void app_main(void) {
    signed_bearing_test();
    bool seen[127]={false};unsigned count=0;
    for(unsigned page=0;page<PORTABLE_NOVA_KEY_PAGES;page++)for(unsigned key=0;key<8;key++) {
        unsigned ch=portable_nova_key_character(page,key);if(!ch)continue;
        assert(ch>=32 && ch<=126 && !seen[ch]);seen[ch]=true;count++;
    }
    assert(count==95 && !portable_nova_key_character(12,0) && !portable_nova_key_character(0,8));
    for(unsigned c=32;c<=126;c++)assert(seen[c]);
    assert(portable_nova_hit(16,56,16,56,208,54));assert(portable_nova_hit(223,109,16,56,208,54));
    assert(!portable_nova_hit(224,109,16,56,208,54));assert(!portable_nova_hit(223,110,16,56,208,54));
    portable_nova_begin();portable_nova_header("NOVA UI");
    portable_nova_row(16,56,208,54,"Settings typography","Rajdhani / Orbitron",false);
    portable_nova_button(16,116,96,44,"+ / -",false);portable_nova_button(128,116,96,44,"Selected",true);
    portable_nova_center(4,16,172,208,"12:34.56",NOVA_CYAN);
    t5_app_get_api(1)->present(false);
    assert(presents==1);
    unsigned cyan=0,white=0;for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++){uint16_t p=pixels[y*244+x];if(p==0xffff)white++;if((p&31)>8 && ((p>>5)&63)>30 && (p>>11)<10)cyan++;}
    assert(cyan>500 && white<1000); /* No accidental white-screen fallback. */
}
