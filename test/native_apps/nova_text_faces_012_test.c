/* Real adapter rendering is checked against the untouched generic font. */
#include "nova_peripherals.h"
#include "PortableNovaUi.h"
#include "../../lib/PortableApps/settings_fonts/text.inc"
#include <limits.h>

static uint16_t expected_pixels[240*244];
static const unsigned faces[]={0,1,2,NOVA_FACE_ORBITRON_10};
static unsigned glyph_cases;

static uint16_t blend(uint16_t old,uint32_t color,unsigned alpha) {
    unsigned r=(((old>>11)&31)*255/31*(255-alpha)+((color>>16)&255)*alpha)/255;
    unsigned g=(((old>>5)&63)*255/63*(255-alpha)+((color>>8)&255)*alpha)/255;
    unsigned b=((old&31)*255/31*(255-alpha)+(color&255)*alpha)/255;
    return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
}
static void reset_pixels(void) {
    portable_nova_begin();
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)
        pixels[y*244+x]=(uint16_t)((x*13u+y*23u)&65535u);
    memcpy(expected_pixels,pixels,sizeof(pixels));
}
static void reference_glyph(unsigned face,unsigned ch,int x,int y,int max_width,uint32_t color) {
    unsigned source=face==NOVA_FACE_ORBITRON_10?0:face;
    unsigned scale=face==NOVA_FACE_ORBITRON_10?183:256;
    const rps_glyph *g=&rps_text[source*95+ch-32];
    unsigned advance=g->advance_q4*scale/256;
    if((int)((advance+15)/16)>max_width)return;
    unsigned w=g->width*scale/256,h=g->height*scale/256;
    if(!w&&g->width)w=1;
    if(!h&&g->height)h=1;
    for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++) {
        unsigned sx=col*256/scale,sy=row*256/scale;
        if(sx>=g->width||sy>=g->height)continue;
        unsigned n=sy*g->width+sx,a=((g->bits[n/4]>>(6-2*(n%4)))&3)*85;
        int px=x+(int)g->left*(int)scale/256+(int)col;
        int py=y+(int)g->top*(int)scale/256+(int)row;
        if(px<x||px>=x+max_width||px<0||px>=240||py<0||py>=240||!a)continue;
        expected_pixels[py*244+px]=blend(expected_pixels[py*244+px],color,a);
    }
}
static void printable_glyphs(void) {
    const struct {int x,y,w;uint32_t color;} cases[]={
        {90,80,100,NOVA_WHITE},{90,80,7,NOVA_CYAN},{0,-6,100,NOVA_CAP},
        {235,230,100,NOVA_TEXT},{-3,80,100,0xe79436u},{90,80,1,NOVA_CYAN},
    };
    for(unsigned f=0;f<sizeof(faces)/sizeof(faces[0]);f++)for(unsigned ch=32;ch<=126;ch++) {
        unsigned face=faces[f],source=face==NOVA_FACE_ORBITRON_10?0:face;
        unsigned scale=face==NOVA_FACE_ORBITRON_10?183:256;
        const rps_glyph *g=&rps_text[source*95+ch-32];
        char text[2]={(char)ch,0};
        assert(portable_nova_measure(face,text)==(int)((g->advance_q4*scale/256+15)/16));
        for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
            reset_pixels();
            reference_glyph(face,ch,cases[c].x,cases[c].y,cases[c].w,cases[c].color);
            portable_nova_text(face,cases[c].x,cases[c].y,cases[c].w,text,cases[c].color);
            assert(!memcmp(expected_pixels,pixels,sizeof(pixels)));
            glyph_cases++;
        }
        char repeated[99];memset(repeated,(int)ch,98);repeated[98]=0;
        assert(portable_nova_measure(face,repeated)==(int)((96*(g->advance_q4*scale/256)+15)/16));
    }
}
static void unsupported_faces(void) {
#ifdef PORTABLE_NOVA_TEXT_FACES_012
    const unsigned unsupported[]={3,4,5,7,UINT_MAX};
#else
    const unsigned unsupported[]={7,UINT_MAX};
    for(unsigned face=3;face<=5;face++)assert(portable_nova_measure(face,"+")>0);
#endif
    for(unsigned f=0;f<sizeof(unsupported)/sizeof(unsupported[0]);f++) {
        unsigned face=unsupported[f];reset_pixels();
        assert(!portable_nova_measure(face,"unsupported"));
        portable_nova_text(face,90,80,100,"unsupported",NOVA_CYAN);
        portable_nova_center(face,90,80,100,"unsupported",NOVA_CYAN);
        portable_nova_right(face,90,80,100,"unsupported",NOVA_CYAN);
        assert(!portable_nova_wrap(face,90,80,100,20,2,"unsupported",NOVA_CYAN));
        assert(!memcmp(expected_pixels,pixels,sizeof(pixels)));
    }
}
static void fallback_and_widgets(void) {
    for(unsigned f=0;f<sizeof(faces)/sizeof(faces[0]);f++) {
        unsigned face=faces[f];reset_pixels();reference_glyph(face,'?',90,80,100,NOVA_CYAN);
        char invalid[]={1,0};portable_nova_text(face,90,80,100,invalid,NOVA_CYAN);
        assert(!memcmp(expected_pixels,pixels,sizeof(pixels)));
        assert(portable_nova_measure(face,invalid)==portable_nova_measure(face,"?"));
        assert(!portable_nova_measure(face,NULL));
        portable_nova_text(face,0,0,100,NULL,NOVA_CYAN);
    }
    /* Save full frames so generic/profile multi-glyph layout, fallback, clipping,
     * wrapping, and alarm/LoRa-style widgets are compared byte for byte. */
    portable_nova_begin();portable_nova_header("COUNTDOWN FINISHED");
    portable_nova_center(0,16,58,208,"ALARM",NOVA_CYAN);
    portable_nova_center(1,16,88,208,"Stopping / saving",NOVA_TEXT);
    portable_nova_center(2,16,117,208,"Output stop unconfirmed",NOVA_CAP);
    portable_nova_button(24,144,192,44,"Dismiss",false);
    portable_nova_button(24,192,192,44,"Retry",false);
    t5_app_get_api(1)->present(false);
    portable_nova_begin();portable_nova_text(NOVA_FACE_ORBITRON_10,52,15,184,"LORA MESSAGES",NOVA_WHITE);
    portable_nova_right(1,12,55,216,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",NOVA_CYAN);
    portable_nova_center(0,12,82,216,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",NOVA_CAP);
    assert(portable_nova_wrap(1,12,112,216,20,3,"Every printable glyph keeps its exact original pixels.",NOVA_TEXT));
    assert(!portable_nova_wrap(2,12,176,90,17,2,"A long string must produce the same final-line ellipsis.",NOVA_CYAN));
    t5_app_get_api(1)->present(false);
}
void app_main(void) {
    printable_glyphs();unsupported_faces();fallback_and_widgets();
    assert(glyph_cases==2280);assert(presents==2);
    printf("Exact generic-font pixels and metrics: %u glyph/clipping/background cases passed\n",glyph_cases);
}
