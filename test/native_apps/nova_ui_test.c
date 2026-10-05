#include "nova_peripherals.h"
#include "PortableNovaUi.h"
#include "PortableNovaKeyboard.h"
void app_main(void) {
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
