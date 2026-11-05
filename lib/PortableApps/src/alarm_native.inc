/* Selected RGB presentation only. The service/modal/custody state machine
 * remains in alarm.inc; these helpers neither acquire nor submit a frame. */
#include "PortableAlarmLayout.h"
static void alarm_native_text(unsigned face,portable_alarm_rect r,
        const char *text,uint32_t color) {
    int size=portable_nova_measure(face,text);
    if(size>r.w && face<=1){face=2;size=portable_nova_measure(face,text);}
    /* At ABI-valid but visually tiny extents, omit text that cannot fit its
     * row rather than painting across another action. Buttons remain bounded. */
    int line=face==2?16:20;
    if(r.w<=0 || r.h<line)return;
    char fitted[96];
    if(size>r.w) {
        unsigned n=0;
        while(n<sizeof(fitted)-4 && text[n]){fitted[n]=text[n];n++;}
        for(;;) {
            fitted[n]='.';fitted[n+1]='.';fitted[n+2]='.';fitted[n+3]=0;
            size=portable_nova_measure(face,fitted);
            if(size<=r.w)break;
            if(!n)return;
            n--;
        }
        text=fitted;
    }
    int inset=size<r.w?(r.w-size)/2:0;
    portable_nova_native_text(face,r.x+inset,r.y+(r.h-line)/2,
        r.w-inset,text,color);
}
static void alarm_native_button(portable_alarm_rect r,const char *text) {
    portable_nova_native_round(r.x,r.y,r.w,r.h,6,NOVA_DIM);
    if(r.w>2 && r.h>2)portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,5,0x031115u);
    if(r.w>8){r.x+=4;r.w-=8;alarm_native_text(1,r,text,NOVA_CYAN);}
}
static void alarm_native_draw(const alarm_status_v1 *s,const char *title) {
    portable_alarm_layout l;
    if(!portable_alarm_layout_for(width(),height(),s->state==ALARM_STATE_BLOCKED,&l))return;
    portable_nova_begin();
    alarm_native_text(0,l.title,title,NOVA_CYAN);
    portable_nova_native_fill(l.rule.x,l.rule.y,l.rule.w,l.rule.h,NOVA_DIM);
    alarm_native_text(1,l.state,s->state==ALARM_STATE_BLOCKED?
        (s->error==ALARM_RTC?"RTC error":"Service error"):
        s->state==ALARM_STATE_DISMISSING?"Stopping / saving":"Time is up",NOVA_TEXT);
    const char *warning=portable_nova_measure(2,"Output stop unconfirmed")<=l.warning.w?
        "Output stop unconfirmed":"Stop unconfirmed";
    alarm_native_text(2,l.warning,s->output_uncertain?warning:"",NOVA_CAP);
    alarm_native_button(l.dismiss,s->occurrence.generation?"Dismiss":"Back");
    if(s->state==ALARM_STATE_BLOCKED)alarm_native_button(l.retry,"Retry");
}
