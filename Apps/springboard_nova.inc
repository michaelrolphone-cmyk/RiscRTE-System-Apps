/* Shared compact presentation; all gesture and layout state stays in the app. */
#include "springboard_motion.h"
#include <limits.h>
#define NOVA_PAGE_APPS 19u
#define NOVA_FRAME_MS 40u
typedef struct { int16_t x, y; } nova_point;
typedef struct {
    const springboard_presentation *view;
    nova_point points[NOVA_PAGE_APPS];
    unsigned n, first, focus, moved, frames;
    int pulse, pending, last_x, last_y,visible_x,visible_y;
    sb_motion motion;
    bool down, tap, initial, dirty;
    uint32_t last_frame, last_clock, last_input, last_motion;
    uint8_t hour, minute;
    bool time_known;
    char status[48];
} nova_state;
static unsigned nova_root(unsigned n) {
    unsigned result = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return result;
}
static int nova_distance(int x, int y) { return (int)nova_root((unsigned)(x*x + y*y)); }
static int nova_limit(int n, int limit) { return n < -limit ? -limit : n > limit ? limit : n; }
static void nova_layout(nova_state *s) {
    unsigned n = 0;
    /* Exact prototype axial spacing (54, 54*.866), stable radial order. */
    for (int q = -2; q <= 2; ++q) for (int r = -2; r <= 2; ++r) {
        if (abs(q+r) > 2) continue;
        nova_point p = {(int16_t)(864*q + 432*r), (int16_t)(748*r)};
        unsigned j = n++;
        while (j && nova_distance(s->points[j-1].x,s->points[j-1].y) > nova_distance(p.x,p.y)) {
            s->points[j] = s->points[j-1]; --j;
        }
        s->points[j] = p;
    }
    s->n = count - s->first;
    if (s->n > NOVA_PAGE_APPS) s->n = NOVA_PAGE_APPS;
    sb_point p[NOVA_PAGE_APPS];
    for (unsigned i=0;i<s->n;++i) p[i]=(sb_point){-(float)s->points[i].x/16.0f,-(float)s->points[i].y/16.0f};
    sb_motion_init(&s->motion,p,s->n);
    s->focus=0;s->pending=-1;s->pulse=0;
    s->dirty = true;
}
static unsigned nova_nearest(nova_state *s) { return sb_nearest(&s->motion); }
static int nova_scale(int distance) {
    /* Q8 approximation of 1.14 - .95*(max-axis/130)^1.5, clamped .2..1.12. */
    int ratio = distance * 256 / (130*16);
    int scale = 292 - 243 * ratio * (int)nova_root((unsigned)ratio*256) / 65536;
    return scale < 51 ? 51 : scale > 287 ? 287 : scale;
}
static int nova_axis_alpha(int p,int extent) {
    int d=p<extent-p?p:extent-p;
    if(d<=0)return 0;
    if(d<14)return d*102/14;
    if(d<46)return 102+(d-14)*153/32;
    return 255;
}
static int nova_alpha(int x,int y) {
    return nova_axis_alpha(x,api->screen_width())*nova_axis_alpha(y,api->screen_height())/255;
}
static uint32_t nova_color(const char *icon) {
    /* Presentation palette only; icons and app identity always come from catalog. */
    if (!strcmp(icon,"solid:f017")) return 0x0891b2;
    if (!strcmp(icon,"solid:f240")) return 0x22b45a;
    if (!strcmp(icon,"solid:f013")) return 0x7b8794;
    if (!strcmp(icon,"solid:f07b")) return 0xc99700;
    if (!strcmp(icon,"solid:f03e")) return 0xec4899;
    if (!strcmp(icon,"solid:f1eb")) return 0x2563eb;
    if (!strcmp(icon,"solid:f0f3")) return 0xff6a1a;
    return 0x3d9bff;
}
static void nova_render(nova_state *s) {
    s->view->begin();
    int cx = api->screen_width()/2, cy = api->screen_height()/2;
    bool unavailable_icon=false;
    for (unsigned i=0; i<s->n; ++i) {
        t5_app_manifest_t app;
        if (!api->installed_apps_get(s->first+i,&app)) continue;
        int dx=s->points[i].x+(int)(s->motion.x*16), dy=s->points[i].y+(int)(s->motion.y*16);
        int edge=abs(dx)>abs(dy)?abs(dx):abs(dy), scale=nova_scale(edge);
        if ((int)i == s->pending && s->pulse) {
            int triangle=s->pulse <= 160 ? s->pulse : 320-s->pulse;
            scale += scale*triangle*22/(160*100);
        }
        int x=cx+dx/16, y=cy+dy/16;
        if (x < -32 || y < -32 || x > api->screen_width()+32 || y > api->screen_height()+32) continue;
        s->view->circle(x,y,22*scale/256,nova_color(app.icon),255);
        if (!s->view->icon(x,y,24*scale/256,app.icon,255))
            unavailable_icon=true;
    }
    char clock[16];
    if (s->time_known) snprintf(clock,sizeof(clock),"%u:%02u %s",s->hour%12?s->hour%12:12,s->minute,s->hour<12?"AM":"PM");
    else strcpy(clock,"--:--");
    s->view->caption(16,clock,true,0x7d8c90);
    if (s->status[0]) s->view->caption(api->screen_height()-43,s->status,false,0xffffff);
    else if (s->n) {
        t5_app_manifest_t app;
        if (api->installed_apps_get(s->first+s->focus,&app))
            s->view->caption(api->screen_height()-43,app.display_name,false,0xffffff);
    } else s->view->caption(api->screen_height()/2,"NO APPS",false,0xffffff);
    if (unavailable_icon) s->view->caption(api->screen_height()-16,"ICON UNAVAILABLE",false,0x7d8c90);
    else if (count>NOVA_PAGE_APPS) {
        char page[24];snprintf(page,sizeof(page),"%u / %u",s->first/NOVA_PAGE_APPS+1,(unsigned)((count+NOVA_PAGE_APPS-1)/NOVA_PAGE_APPS));
        s->view->caption(api->screen_height()-16,page,true,0x7d8c90);
    }
    api->present(false);s->visible_x=(int)(s->motion.x*16);s->visible_y=(int)(s->motion.y*16);s->dirty=false;
}
static int nova_hit(nova_state *s,int x,int y) {
    int best=-1, nearest=10000;
    x-=api->screen_width()/2;y-=api->screen_height()/2;
    for (unsigned i=0;i<s->n;++i) {
        int dx=s->points[i].x+s->visible_x,dy=s->points[i].y+s->visible_y;
        int scale=nova_scale(abs(dx)>abs(dy)?abs(dx):abs(dy));
        int d=nova_distance(x-dx/16,y-dy/16);
        if (nova_alpha(api->screen_width()/2+dx/16,api->screen_height()/2+dy/16)<32) continue;
        if (d<24*(scale<128?128:scale)/256 && d<nearest) {best=(int)i;nearest=d;}
    }
    return best;
}
static void nova_cancel(nova_state *s) {
    s->down=s->tap=s->initial=false;sb_motion_cancel(&s->motion);
    s->pending=-1;s->pulse=0;s->dirty=true;
}
static void nova_contact_input(nova_state *s,const springboard_contact *c,uint32_t elapsed) {
    if (!c->valid || c->cancelled) { nova_cancel(s); return; }
    if (c->down) {
        if (!s->down) {
            if ((!c->began || !c->tap_eligible) && !s->initial) return;
            s->down=true;s->tap=c->tap_eligible;s->initial=false;
            s->last_x=c->x;s->last_y=c->y;s->moved=0;
            sb_motion_begin(&s->motion);s->pending=-1;s->pulse=0;
        } else {
            int dx=nova_limit(c->x-s->last_x,256),dy=nova_limit(c->y-s->last_y,256);
            s->last_x=c->x;s->last_y=c->y;
            unsigned travel=(unsigned)(abs(dx)+abs(dy));
            s->moved=s->moved+travel>1024?1024:s->moved+travel;
            sb_motion_drag(&s->motion,(float)dx,(float)dy,elapsed);s->dirty |= dx||dy;
        }
    } else {
        s->initial=false;
        if (c->released) {
            bool tap=(!s->down || (s->tap && s->moved<6)) && c->tap_eligible;
            s->down=false;sb_motion_release(&s->motion);
            if (tap) {
                int hit=nova_hit(s,c->x,c->y);
                if (hit>=0) {
                    unsigned center=0;int nearest=INT_MAX;
                    for(unsigned i=0;i<s->n;i++) {
                        int x=s->points[i].x+s->visible_x,y=s->points[i].y+s->visible_y,d=x*x+y*y;
                        if(d<nearest){nearest=d;center=i;}
                    }
                    int dx=s->points[hit].x-s->points[center].x,dy=s->points[hit].y-s->points[center].y;
                    bool first_ring=dx*dx+dy*dy<=896*896; /* center plus one54px hex ring */
                    sb_motion_target(&s->motion,(unsigned)hit);s->pending=first_ring?hit:-1;
                    s->pulse=first_ring?160:0;s->status[0]=0;s->dirty=true;
                }
                else if (count>NOVA_PAGE_APPS && c->y>=api->screen_height()-24) {
                    s->first=(s->first+NOVA_PAGE_APPS<count)?s->first+NOVA_PAGE_APPS:0;nova_layout(s);
                }
            }
            s->tap=false;
        }
    }
}
static void nova_tick(nova_state *s,uint32_t elapsed) {
    int oldx=(int)(s->motion.x*16),oldy=(int)(s->motion.y*16);
    sb_motion_step(&s->motion,elapsed);
    s->dirty |= oldx!=(int)(s->motion.x*16)||oldy!=(int)(s->motion.y*16);
    if(s->pulse){s->pulse=(unsigned)s->pulse>elapsed?s->pulse-(int)elapsed:0;s->dirty=true;}
    if(s->pending>=0 && !s->pulse) {s->motion.x=s->motion.points[s->pending].x;s->motion.y=s->motion.points[s->pending].y;s->motion.vx=s->motion.vy=0;s->motion.settled=true;s->dirty=true;}
    if(s->n){unsigned n=nova_nearest(s);s->dirty|=n!=s->focus;s->focus=n;selected=s->first+n;}
}
static bool nova_run(void) {
    const springboard_presentation *v=springboard_presentation_get();
    if (!compact || !v || v->struct_size<sizeof(*v) || !v->begin || !v->circle ||
        !v->icon || !v->caption || !v->contact || !v->clock) return false;
    nova_state s={.view=v,.initial=true,.pending=-1};
    nova_layout(&s);
    s.time_known=v->clock(&s.hour,&s.minute) && s.hour<24 && s.minute<60;
    s.last_frame=s.last_clock=s.last_input=s.last_motion=api->millis();nova_render(&s);
    t5_app_input_t in;uint32_t previous=0;bool buttons_armed=false;
    while(api->poll(&in,20)) {
        if(in.exit_requested || (in.buttons&T5_APP_BUTTON_BACK)) break;
        uint32_t now=api->millis(),elapsed=now-s.last_motion;
        /* A slow valid display frame is not an input cancellation. Keep a
         * deliberate pending launch, while bounding only physics integration. */
        if(elapsed>250)elapsed=250;
        springboard_contact c={0};v->contact(&c);nova_contact_input(&s,&c,now-s.last_input);s.last_input=now;
        if(!in.buttons) buttons_armed=true;
        uint32_t pressed=buttons_armed?(in.buttons&~previous):0;previous=in.buttons;
        if(s.n && (pressed&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN))) {
            unsigned next=s.motion.snapping?s.first+s.motion.target:selected;
            if(pressed&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP)) next=next?next-1:count-1;
            else next=(next+1)%count;
            nova_cancel(&s);
            unsigned first=next/NOVA_PAGE_APPS*NOVA_PAGE_APPS;
            if(first!=s.first){s.first=first;nova_layout(&s);}
            sb_motion_target(&s.motion,next-s.first);s.status[0]=0;
        }
        if(s.n && (pressed&T5_APP_BUTTON_CONFIRM)) {s.pending=(int)(s.motion.snapping?s.motion.target:s.focus);sb_motion_target(&s.motion,(unsigned)s.pending);s.pulse=320;s.dirty=true;}
        nova_tick(&s,elapsed);s.last_motion=now;
        if((uint32_t)(now-s.last_clock)>=1000) {
            uint8_t hour=0,minute=0;bool known=v->clock(&hour,&minute)&&hour<24&&minute<60;
            s.dirty|=known!=s.time_known||(known&&(hour!=s.hour||minute!=s.minute));
            s.hour=hour;s.minute=minute;s.time_known=known;s.last_clock=now;
        }
        if((uint32_t)(now-s.last_frame)<NOVA_FRAME_MS) continue;
        s.last_frame=now;
        if(s.dirty) nova_render(&s);
        if(s.pending>=0 && !s.pulse) {
            selected=s.first+(unsigned)s.pending;
            t5_app_manifest_t app;
            if(api->installed_apps_get(selected,&app) && app.compatible && api->request_app_launch(selected)) break;
            strcpy(s.status,"UNABLE TO OPEN APP");s.pending=-1;s.dirty=true;
        }
    }
    return true;
}
