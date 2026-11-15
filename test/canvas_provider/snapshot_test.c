#include <assert.h>
#include <stdio.h>
#include "canvas_provider.h"
static unsigned copies;static bool refuse;
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=7,.height=11,.supported_formats=1};return true;}
static bool acquire(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;(void)f;(void)out;assert(0);return false;}
static void release(void*c,uint64_t f){(void)c;(void)f;assert(0);}
static bool submit(void*c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)f;(void)d;(void)n;(void)o;(void)t;assert(0);return false;}
static bool status(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;(void)t;(void)out;assert(0);return false;}
static bool seed(void*c,uint64_t f){(void)c;(void)f;return false;}
static int32_t power(void*c,uint32_t n){(void)c;(void)n;return 0;}
static bool metrics(void*c,void*p){(void)c;(void)p;return false;}
static bool copy(void*c,uint32_t format,void*out,size_t capacity,uint32_t stride){(void)c;copies++;assert(format==1&&stride>=1&&capacity>=stride*11);if(refuse)return false;for(unsigned y=0;y<11;y++)((unsigned char*)out)[y*stride]=(unsigned char)(y%2?0xaa:0x54);return true;}
int main(void){
 canvas_native_snapshot d={.history={.base={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=seed},.power_tag=0x44505731,.power_version=1,.prepare=power,.resume=power,.metrics_tag=0x444d5431,.metrics_version=1,.metrics_snapshot=metrics,.snapshot_tag=0x44534e31,.snapshot_version=1,.copy_completed=copy};
 for(unsigned rotation=0;rotation<4;rotation++)for(unsigned encoding=1;encoding<=3;encoding++){
 if(encoding==2)continue;
 canvas_provider p;canvas_provider_init(&p,&d.history.base,NULL,rotation);
 const risc_canvas_presentation_v1*x=NULL;risc_canvas_open_presentation_v1 q={.base={sizeof(q),encoding,RISC_CANVAS_RESPONSIVE,0,0},.presentation_tag=RISC_CANVAS_PRESENTATION_TAG,.presentation_out=&x};uint64_t session;assert(!p.api.open(&p,&q.base,&session));
 risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};assert(!p.api.geometry(&p,session,&g));
 unsigned row=encoding==1?(g.width+7)/8:g.width*2,stride=row+3;unsigned char buffer[512];memset(buffer,0xcc,sizeof(buffer));
 refuse=true;assert(x->copy_completed(&p,session,g.revision,buffer,sizeof(buffer),stride)==RISC_CANVAS_UNAVAILABLE);for(unsigned i=0;i<sizeof(buffer);i++)assert(buffer[i]==0xcc);refuse=false;
 assert(!x->copy_completed(&p,session,g.revision,buffer,sizeof(buffer),stride));
 for(unsigned y=0;y<g.height;y++)for(unsigned xx=0;xx<g.width;xx++){unsigned px,py;canvas_mapping_physical(&p.frames.mapping,xx,y,&px,&py);bool white=!((py%2?0xaa:0x54)&(0x80u>>(px&7)));if(encoding==1)assert(!!(buffer[y*stride+xx/8]&(0x80u>>(xx&7)))==white);else assert(buffer[y*stride+xx*2]==(white?255:0)&&buffer[y*stride+xx*2+1]==(white?255:0));}
 for(unsigned y=0;y<g.height;y++)for(unsigned k=row;k<stride;k++)assert(buffer[y*stride+k]==0xcc);
 unsigned before=copies;assert(x->copy_completed(&p,session,g.revision+1,buffer,sizeof(buffer),stride)==RISC_CANVAS_STALE&&copies==before);
 assert(!p.api.close(&p,session));
 }
 puts("Canvas completed snapshot:8 logical encoding/rotation cases, exact completed pixels, unchanged refusal/padding, no native lease/submit and stale revision rejection PASS");
}
