#include "PortableQuickRender.h"
#include <stddef.h>
#include <string.h>

typedef struct { uint8_t code; int8_t left,top; uint8_t width,height; uint16_t advance_q8; uint32_t offset; } pqa_glyph;
typedef struct { uint8_t width,height; uint32_t offset; } pqa_icon;
#include "quick_actions_assets.inc"
typedef struct { uint16_t pixels[240]; int y; } pqa_row;
#define RGB(r,g,b) (uint16_t)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3))
#define CYAN RGB(25,227,255)
#define MUTED RGB(138,164,170)
#define LINE RGB(18,38,43)
#define TILE RGB(12,16,19)
#define GREEN RGB(61,255,154)
static uint16_t mix(uint16_t a,uint16_t b,unsigned n) {
    unsigned m=256u-n;
    return (uint16_t)(((((a>>11)*m+(b>>11)*n+128u)>>8)<<11)|
        (((((a>>5)&63u)*m+((b>>5)&63u)*n+128u)>>8)<<5)|
        (((a&31u)*m+(b&31u)*n+128u)>>8));
}
static void pixel(pqa_row *r,int x,uint16_t color,unsigned alpha) {
    if(x>=0 && x<240) r->pixels[x]=mix(r->pixels[x],color,alpha);
}
static void rect(pqa_row *r,int x,int y,int w,int h,int radius,uint16_t color,unsigned alpha) {
    if(r->y<y || r->y>=y+h) return;
    int yy=r->y-y, inset=0;
    /* Antialiased circular corners from four subpixel samples per pixel. */
    for(int xx=0;xx<w;++xx) {
        unsigned coverage=4;
        int cx=xx<radius?radius-1:xx>=w-radius?w-radius:xx;
        int cy=yy<radius?radius-1:yy>=h-radius?h-radius:yy;
        if(cx!=xx && cy!=yy) {
            coverage=0;
            for(int sy=1;sy<=3;sy+=2) for(int sx=1;sx<=3;sx+=2) {
                int dx=4*xx+sx-(4*cx+2),dy=4*yy+sy-(4*cy+2);
                if(dx*dx+dy*dy<=16*radius*radius) ++coverage;
            }
        }
        if(coverage) pixel(r,x+xx+inset,color,alpha*coverage/4);
    }
}
static const pqa_glyph *glyph(const pqa_glyph *g,size_t n,char ch) {
    for(size_t i=0;i<n;++i) if(g[i].code==(uint8_t)ch)return g+i;
    return NULL;
}
static int text_width(const pqa_glyph *g,size_t n,const char *s,unsigned limit) {
    int width=0;
    for(unsigned i=0;i<limit && s[i];++i) { const pqa_glyph *c=glyph(g,n,s[i]); if(c)width+=c->advance_q8; }
    return (width+128)/256;
}
static void text(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,unsigned limit,
                 int x,int baseline,uint16_t color,unsigned alpha) {
    int cursor=x*256;
    for(unsigned i=0;i<limit && s[i];++i) {
        const pqa_glyph *c=glyph(g,n,s[i]);if(!c)continue;
        int yy=r->y-baseline-c->top;
        if(yy>=0 && yy<c->height) for(int xx=0;xx<c->width;++xx) {
            unsigned a=pqa_alpha[c->offset+(unsigned)yy*c->width+(unsigned)xx];
            if(a) pixel(r,(cursor+128)/256+c->left+xx,color,(a*alpha+127)/255);
        }
        cursor+=c->advance_q8;
    }
}
#define GLYPHS(set) pqa_##set##_glyphs,sizeof(pqa_##set##_glyphs)/sizeof(pqa_##set##_glyphs[0])
static void center_text(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,int cx,int baseline,uint16_t color,unsigned alpha) {
    text(r,g,n,s,32,cx-text_width(g,n,s,32)/2,baseline,color,alpha);
}
static void icon(pqa_row *r,const pqa_icon *ic,int cx,int y,uint16_t color,unsigned alpha) {
    int yy=r->y-y;
    if(yy<0 || yy>=ic->height)return;
    for(int xx=0;xx<ic->width;++xx) {
        unsigned a=pqa_alpha[ic->offset+(unsigned)yy*ic->width+(unsigned)xx];
        if(a)pixel(r,cx-ic->width/2+xx,color,(a*alpha+127)/255);
    }
}
static void percentage(char out[5],bool valid,unsigned value) {
    if(!valid || value>100u){memcpy(out,"--%",4);return;}
    unsigned i=0;
    if(value>=100)out[i++]='1';
    if(value>=10)out[i++]=(char)('0'+(value/10)%10);
    out[i++]=(char)('0'+value%10);out[i++]='%';out[i]=0;
}
static void slider(pqa_row *r,int y,const pqa_icon *ic,unsigned value,bool valid,bool muted,bool error) {
    unsigned a=muted?115u:256u;
    icon(r,ic,28,y-1,error?RGB(255,135,65):CYAN,a);
    for(int i=0;i<10;++i) {
        /* Exactly 138px flex track, 3px gaps, 11.1px bars. */
        int x=44+(i*141)/10, next=44+((i+1)*141)/10;
        rect(r,x,y,next-x-3,14,3,CYAN,(valid && (unsigned)i*10u<value)?a:a*41u/256u);
    }
    char label[5];percentage(label,valid,value);
    int width=text_width(GLYPHS(percent),label,4);
    text(r,GLYPHS(percent),label,4,220-width,y+11,error?RGB(255,135,65):RGB(207,233,238),a);
}
static void panel_row(pqa_row *r,const pqa_state *s,const char *time,bool bv,unsigned bp) {
    text(r,GLYPHS(time),time?time:"--:--",8,20,31,RGB(255,255,255),256);
    char battery[5]; percentage(battery,bv,bp);
    int bw=text_width(GLYPHS(percent),battery,4);
    text(r,GLYPHS(percent),battery,4,146-bw,30,bv?GREEN:MUTED,256);
    for(int i=0;i<10;++i)rect(r,152+i*7,24,5,4,1,GREEN,bv && (unsigned)i*10<bp?256:41);
    slider(r,55,&pqa_icon_sun,s->brightness,s->brightness_valid,false,(s->error_flags&PQA_ERROR_BRIGHTNESS)!=0);
    text(r,GLYPHS(caption),"NOTIFICATIONS",13,44,80,MUTED,210);
    slider(r,85,s->volume_valid && !s->volume?&pqa_icon_muted:&pqa_icon_volume,
           s->volume,s->volume_valid,s->volume_valid && !s->volume,(s->error_flags&PQA_ERROR_VOLUME)!=0);
    rect(r,20,108,200,1,0,LINE,256);
    static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI SETUP","BLUETOOTH","TORCH"};
    const pqa_icon *const icons[]={&pqa_icon_silent,&pqa_icon_dnd,&pqa_icon_airplane,&pqa_icon_wifi,&pqa_icon_bluetooth,&pqa_icon_torch};
    for(int i=0;i<6;++i) {
        int x=20+(i%3)*71,y=118+(i/3)*48;
        bool disabled=(i==1 && !s->dnd_valid) || ((i==2||i==4||(i==3&&s->radio_controls)) && !s->radios_valid) || (i==0 && !s->volume_valid);
        bool on=(i==1 && s->dnd_valid && s->dnd_enabled) || (i==0 && s->volume_valid && !s->volume) || (i==5 && s->torch) || (s->radios_valid && ((i==2&&s->airplane)||(i==3&&s->wifi_enabled)||(i==4&&s->bluetooth_enabled)));
        rect(r,x,y,62,42,12,on?CYAN:LINE,256);
        if(!on)rect(r,x+1,y+1,60,40,11,TILE,256);
        uint16_t color=on?RGB(0,20,24):disabled?RGB(66,82,88):MUTED;
        if(i==3 && (s->error_flags&PQA_ERROR_WIFI))color=RGB(255,135,65);
        if(i==1 && (s->error_flags&PQA_ERROR_DND))color=RGB(255,135,65);
        icon(r,icons[i],x+31,y+6+(18-icons[i]->height)/2,color,256);
        center_text(r,GLYPHS(label),(i==3&&s->radio_controls)?"WI-FI":labels[i],x+31,y+36,color,256);
        /* Small unavailable dash reinforces dim disabled tiles, no fake state. */
        if(disabled)rect(r,x+49,y+6,5,1,0,RGB(66,82,88),256);
    }
    rect(r,104,216,32,4,2,CYAN,128);
    if(s->error_flags&PQA_ERROR_SAVE)
        center_text(r,GLYPHS(caption),"SAVE UNCONFIRMED",120,231,RGB(255,135,65),256);
}
static uint16_t read_pixel(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static void write_pixel(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned torch_edge(int v) {
    int d=v<120?v:239-v;
    if(d>=46)return 256;
    if(d<14)return (unsigned)d*102u/14u;
    return 102u+(unsigned)(d-14)*154u/32u;
}
bool pqa_render(risc_display_surface_v1 *f,const pqa_state *s,const char *time,bool bv,uint8_t bp) {
    if(!f || !s || !f->pixels || f->pixel_format!=RISC_DISPLAY_FORMAT_RGB565 ||
       f->width!=240 || f->height!=240 || f->stride_bytes<480u)return false;
    uint64_t span=(uint64_t)239*f->stride_bytes+480u;
    if(span>f->size_bytes)return false;
    if(!pqa_visible(s))return true;
    bv=bv && bp<=100;
    int position=s->position_q8<0?0:s->position_q8>PQA_OPEN_Q8?PQA_OPEN_Q8:s->position_q8;
    pqa_row row;
    if(s->torch) {
        for(int y=0;y<240;++y) {
            row.y=y;
            for(int x=0;x<240;++x)row.pixels[x]=RGB(255,243,214);
            center_text(&row,GLYPHS(torch),"TAP TO TURN OFF",120,210,RGB(106,90,48),256);
            uint8_t *dst=(uint8_t*)f->pixels+(size_t)y*f->stride_bytes;
            for(int x=0;x<240;++x) {
                unsigned a=torch_edge(x)*torch_edge(y)/256u;
                /* Fade to black, not potentially bright underlying app pixels. */
                write_pixel(dst+2*x,mix(0,row.pixels[x],a));
            }
        }
        return true;
    }
    int offset=(position-PQA_OPEN_Q8)/256;
    unsigned face_alpha=(unsigned)(position>=190*256?0:256-position/190);
    for(int y=0;y<240;++y) {
        uint8_t *dst=(uint8_t*)f->pixels+(size_t)y*f->stride_bytes;
        int local=y-offset;
        bool in_panel=local>=0 && local<240;
        unsigned a=in_panel?(local<218?256u:(unsigned)(240-local)*256u/22u):0;
        if(in_panel){memset(row.pixels,0,sizeof(row.pixels));row.y=local;panel_row(&row,s,time,bv,bp);}
        for(int x=0;x<240;++x) {
            uint16_t behind=mix(0,read_pixel(dst+2*x),face_alpha);
            write_pixel(dst+2*x,in_panel?mix(behind,row.pixels[x],a):behind);
        }
    }
    return true;
}

void pqa_paper_icon(unsigned index,int x,int y,int size,bool black,
                    void (*draw)(int,int,uint32_t,unsigned)) {
    const pqa_icon *const icons[]={&pqa_icon_silent,&pqa_icon_dnd,&pqa_icon_airplane,
      &pqa_icon_wifi,&pqa_icon_bluetooth,&pqa_icon_torch,&pqa_icon_sun,&pqa_icon_volume};
    if(index>=sizeof(icons)/sizeof(icons[0]) || !draw || size<1 || size>64)return;
    const pqa_icon *ic=icons[index];int largest=ic->width>ic->height?ic->width:ic->height;
    int w=ic->width*size/largest,h=ic->height*size/largest;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)
      if(pqa_alpha[ic->offset+(unsigned)(j*ic->height/h)*ic->width+(unsigned)(i*ic->width/w)]>=128)
        draw(x+(size-w)/2+i,y+(size-h)/2+j,black?0:0xffffff,255);
}
