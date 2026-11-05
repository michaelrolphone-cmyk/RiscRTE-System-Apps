#pragma once
/* Capability-pixel geometry shared by alarm drawing and hit testing. No
 * service, input, allocation or display policy is owned by this helper. */
#include <stdbool.h>
#include <stdint.h>

typedef struct { int x,y,w,h; } portable_alarm_rect;
typedef struct {
    portable_alarm_rect title,state,warning,rule,dismiss,retry;
} portable_alarm_layout;

static inline bool portable_alarm_hit(portable_alarm_rect r,int x,int y) {
    return r.w>0 && r.h>0 && x>=r.x && y>=r.y &&
        (int64_t)x<(int64_t)r.x+r.w && (int64_t)y<(int64_t)r.y+r.h;
}

static inline bool portable_alarm_layout_for(uint32_t width,uint32_t height,
        bool blocked,portable_alarm_layout *out) {
    if(!out)return false;
    *out=(portable_alarm_layout){0};
    if(!width || !height || width>UINT16_MAX || height>UINT16_MAX)return false;
    int w=(int)width,h=(int)height,small=w<h?w:h;
    int margin=small/20;if(margin>24)margin=24;
    int gap=small/40;if(gap>12)gap=12;
    int content=w-2*margin,available=h-2*margin;
    bool side=blocked && w>=2*h && w>=240;
    int button=h/6;if(button<32)button=32;if(button>56)button=56;
    int maximum=(available-(blocked&&!side?gap:0))/3;
    if(button>maximum)button=maximum;
    if(button<0)button=0;
    out->dismiss=(portable_alarm_rect){margin,h-margin-button,content,button};
    if(blocked) {
        if(side) {
            int left=(content-gap)/2;
            out->dismiss.w=left;
            out->retry=(portable_alarm_rect){margin+left+gap,h-margin-button,content-left-gap,button};
        } else {
            out->retry=out->dismiss;
            out->dismiss.y-=button+gap;
        }
    }
    int header=out->dismiss.y-margin-gap;
    if(header<0)header=0;
    int row=header/3;
    out->title=(portable_alarm_rect){margin,margin,content,row};
    out->state=(portable_alarm_rect){margin,margin+row,content,row};
    out->warning=(portable_alarm_rect){margin,margin+2*row,content,header-2*row};
    if(row>0)out->rule=(portable_alarm_rect){margin,margin+row-1,content,1};
    return true;
}
