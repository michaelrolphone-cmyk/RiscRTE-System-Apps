#pragma once
/* Empirical calibration against the sensor's REAL double-tap decisions.
 * Test encoded values least-sensitive first. Accept only three detections in
 * separate prompted trials, each followed by a quiet detector window, then
 * five seconds of observed ordinary wrist movement with zero tap detections.
 * No conversion from acceleration to Bosch's encoded sensitivity is invented.
 * This characterizes sampled gestures; it is not a new Deep-wake classifier. */
#include "PortableMotionTap.h"
#include <string.h>
enum { TAP_CAL_SETTLE,TAP_CAL_PAIR,TAP_CAL_QUIET,TAP_CAL_MOVE,TAP_CAL_NEXT,
       TAP_CAL_READY,TAP_CAL_NOISY,TAP_CAL_INSUFFICIENT,TAP_CAL_ERROR };
typedef struct {
    unsigned phase,value,minimum,pairs,samples,stalled;
    uint32_t phase_at,last_sample,last_poll;
    int16_t low[3],high[3];
    bool have_sample;
} portable_tap_calibration;
static inline void portable_tap_calibration_candidate(portable_tap_calibration *c,uint32_t now){
    c->phase=TAP_CAL_SETTLE;c->pairs=c->samples=c->stalled=0;c->phase_at=c->last_sample=c->last_poll=now;c->have_sample=false;
}
static inline bool portable_tap_calibration_start(portable_tap_calibration *c,const twatch_tap_info_v1 *info,uint32_t now){
    if(!c || !info || info->struct_size<sizeof(*info) || info->minimum>info->maximum ||
       info->maximum>15 || (info->profile!=TWATCH_TAP_BMA423 && info->profile!=TWATCH_TAP_BMA456H))return false;
    memset(c,0,sizeof(*c));c->minimum=info->minimum;c->value=info->maximum;
    portable_tap_calibration_candidate(c,now);return true;
}
static inline void portable_tap_calibration_feed(portable_tap_calibration *c,uint32_t now,const twatch_tap_observation_v1 *s){
    if(c->phase>=TAP_CAL_NEXT)return;
    /* A delayed UI/poll cannot certify an unobserved quiet window. */
    if(!s || s->struct_size<sizeof(*s) || (uint32_t)(now-c->last_poll)>500){c->phase=TAP_CAL_ERROR;return;}
    if(now==c->last_poll){if(++c->stalled>=100){c->phase=TAP_CAL_ERROR;return;}}else c->stalled=0;
    c->last_poll=now;
    if(s->sample_ready){
        c->last_sample=now;c->samples++;
        const int16_t xyz[]={s->x_mg,s->y_mg,s->z_mg};
        for(unsigned i=0;i<3;i++){
            if(!c->have_sample || xyz[i]<c->low[i])c->low[i]=xyz[i];
            if(!c->have_sample || xyz[i]>c->high[i])c->high[i]=xyz[i];
        }
        c->have_sample=true;
    }
    if((uint32_t)(now-c->last_sample)>500){c->phase=TAP_CAL_INSUFFICIENT;return;}
    uint32_t elapsed=now-c->phase_at;
    if(c->phase==TAP_CAL_SETTLE){
        /* Start-button knocks and feature setup are outside measured trials. */
        if(elapsed>=1500){c->phase=TAP_CAL_PAIR;c->phase_at=now;c->samples=0;}
    }else if(c->phase==TAP_CAL_PAIR){
        if(s->double_tap){c->pairs++;c->phase=TAP_CAL_QUIET;c->phase_at=now;}
        else if(elapsed>=5000){
            if(c->value>c->minimum){c->value--;c->phase=TAP_CAL_NEXT;}
            else c->phase=TAP_CAL_INSUFFICIENT;
        }
    }else if(c->phase==TAP_CAL_QUIET){
        if(s->double_tap)c->phase=TAP_CAL_NOISY;
        else if(elapsed>=1000){
            c->phase=c->pairs==3?TAP_CAL_MOVE:TAP_CAL_PAIR;c->phase_at=now;
            c->samples=0;c->have_sample=false;
        }
    }else if(c->phase==TAP_CAL_MOVE){
        if(s->double_tap)c->phase=TAP_CAL_NOISY;
        else if(elapsed>=5000){
            unsigned span=0;
            for(unsigned i=0;i<3;i++){unsigned n=(unsigned)((int)c->high[i]-c->low[i]);if(n>span)span=n;}
            /* Require evidence that this was a movement trial, not a watch
             * left still. 150 mg span is a validity gate, not tap sensitivity. */
            c->phase=c->samples>=25 && span>=150?TAP_CAL_READY:TAP_CAL_INSUFFICIENT;
        }
    }
}
