#pragma once
#include "T5AppApi.h"
#include <stdio.h>
#include <string.h>
#include "PaperPresentation.h"
/* A copied status indicator shared by paper faces. Unknown remains explicit;
 * charging is read from the capability, never guessed from percentage. */
typedef struct { bool valid,charging;uint8_t percent; } paper_battery;
static inline paper_battery paper_battery_read(const paper_presentation *p) {
 paper_battery b={0};if(p->battery)b.valid=p->battery(&b.percent,&b.charging);return b;
}
static inline bool paper_battery_changed(paper_battery a,paper_battery b) {
 return a.valid!=b.valid || (a.valid&&(a.percent!=b.percent||a.charging!=b.charging));
}
static inline void paper_battery_draw(const t5_app_api_v1 *api,const paper_presentation *p,int x,int y,paper_battery b) {
 int w=api->screen_width(),h=api->screen_height();char value[8];
 if(b.valid)snprintf(value,sizeof(value),"%u%%",b.percent);else strcpy(value,"--%");
 api->fill_rect(x*w/480,y*h/800,27*w/480,17*h/800,true);
 api->fill_rect((x+3)*w/480,(y+3)*h/800,21*w/480,11*h/800,false);
 api->fill_rect((x+27)*w/480,(y+5)*h/800,3*w/480,7*h/800,true);
 if(b.valid&&b.percent)api->fill_rect((x+4)*w/480,(y+4)*h/800,(int)(19*b.percent/100+1)*w/480,9*h/800,true);
 p->text((x+36)*w/480,(y-5)*h/800,75*w/480,value,1,false,true);
 if(b.valid&&b.charging)p->text(x*w/480,(y+18)*h/800,100*w/480,"CHARGING",1,false,true);
}
