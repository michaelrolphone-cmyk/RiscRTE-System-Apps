/* Selected X4 paper grid: standard 3 by 4 geometry, 12 apps per horizontal page.
 * Input snapshots logical identities; presentation never gates interaction. */
#include "PaperPresentation.h"
#include "PaperFrame.h"
#include "PaperBattery.h"
#include "PortablePerformance.h"
#include "PortablePaperScroll.h"
#include "springboard_pages.h"
#define SBS_PAGE_APPS 12u
__attribute__((weak)) const paper_presentation *paper_presentation_get(void) {return NULL;}
/* Compile only enough identity slots for the selected deployment catalog. */
#include "PortableSpringboardCatalog.h"
#define SBS_CATALOG_BOUND PORTABLE_SPRINGBOARD_CATALOG_BOUND
__attribute__((weak)) bool portable_springboard_hour_24(void) {return false;}
static void sbs_time_text(char out[12],bool known,uint8_t hour,uint8_t minute) {
    if(!known){strcpy(out,"--:--");return;}
    unsigned shown=portable_springboard_hour_24()?hour:hour%12?hour%12:12;
    snprintf(out,12,"%u:%02u",shown,minute);
}
static int paper_x(int x){return x*api->screen_width()/480;}
static int paper_y(int y){return y*api->screen_height()/800;}
#include "SpringboardPaperHeader.h"
typedef struct {
    bool valid,highlight;
    unsigned count,selected;
    int q8;
    char names[SBS_CATALOG_BOUND][128];
} sbs_image;
static sbs_image sbs_submitted,sbs_completed;
static springboard_pages sbs_scroll;
static char sbs_names[SBS_CATALOG_BOUND][128],sbs_hit_name[128],sbs_launch_name[128];
static int sbs_hit_q8,sbs_hit_cell,sbs_dot_page;
static bool sbs_pressed,sbs_pending;
bool portable_app_ui_active(void) {return sbs_scroll.contact||sbs_scroll.animating||sbs_pending;}
static void sbs_visible(void) {
    if(portable_paper_scroll_settled())sbs_completed=sbs_submitted;
}
static void sbs_position(unsigned cell,int offset,int *x,int *y) {
    unsigned row=(cell%SBS_PAGE_APPS)/3,col=cell%3;
    *x=paper_x(80+(int)col*160)+(int)(cell/SBS_PAGE_APPS)*api->screen_width()-offset;
    *y=paper_y(160+(int)row*160);
}
static void sbs_configure(void) {
    unsigned pages=count?(count+SBS_PAGE_APPS-1)/SBS_PAGE_APPS:1;
    sbs_pages_configure(&sbs_scroll,(portable_scroll_viewport){0,paper_y(88),api->screen_width(),paper_y(640)},
                        api->screen_width(),pages);
}
static int sbs_resolve(const char *name) {
    if(!*name)return -1;
    for(unsigned i=0;i<count;i++)if(!strcmp(name,sbs_names[i]))return (int)i;
    return -1;
}
static bool sbs_catalog(void) {
    char selection[128]={0};if(selected<count&&selected<SBS_CATALOG_BOUND)strcpy(selection,sbs_names[selected]);
    unsigned next=api->installed_apps_count();
    if(next>SBS_CATALOG_BOUND)next=0; /* Reject a broken explicit build contract. */
    bool changed=count!=next;count=next;
    for(unsigned i=0;i<SBS_CATALOG_BOUND;i++) {
        t5_app_manifest_t app={0};
        if(i<count&&!api->installed_apps_get(i,&app))app.file_name[0]=0;
        changed|=strcmp(sbs_names[i],app.file_name)!=0;
        memcpy(sbs_names[i],app.file_name,sizeof(sbs_names[i]));
    }
    int same=sbs_resolve(selection);if(same>=0)selected=(unsigned)same;
    else if(selected>=count)selected=0;
    if(sbs_pressed&&sbs_resolve(sbs_hit_name)<0)sbs_pressed=false;
    sbs_configure();return changed;
}
static int sbs_hit(int q8,int x,int y) {
    if(!portable_scroll_inside(&sbs_scroll.view,x,y))return -1;
    for(unsigned i=0;i<count;i++) {
        int cx,cy;sbs_position(i,q8/256,&cx,&cy);
        if(abs(x-cx)<=paper_x(44)&&y-cy>=-paper_y(44)&&y-cy<=paper_y(63)&&sbs_names[i][0])return (int)i;
    }
    return -1;
}
static void sbs_reveal(unsigned cell) {
    sbs_scroll.position_q8=(int)(cell/SBS_PAGE_APPS)*sbs_scroll.width*256;
    sbs_scroll.velocity_q8=0;sbs_scroll.animating=false;
}
static void sbs_cancel(void) {
    sbs_pages_cancel(&sbs_scroll);sbs_pressed=sbs_pending=false;
    sbs_hit_name[0]=sbs_launch_name[0]=0;sbs_dot_page=-1;
}
void portable_springboard_scroll_interrupt(void) {sbs_cancel();}
static unsigned sbs_touch(const portable_touch_sample *sample,uint32_t now) {
    return sbs_pages_touch(&sbs_scroll,sample,now);
}
static bool sbs_draw(const paper_presentation *v,const char *status,bool initial,
                     bool known,uint8_t hour,uint8_t minute,paper_battery battery) {
    sbs_visible();if(!paper_frame_ready()||!portable_paper_scroll_available())return false;
    v->begin();(void)battery;
    char time[12];sbs_time_text(time,known,hour,minute);
    springboard_paper_header(time);
    portable_paper_scroll_clip(&sbs_scroll.view);
    bool highlight=sbs_pressed||sbs_pending;
    for(unsigned i=0;i<count;i++) {
        t5_app_manifest_t app;if(!api->installed_apps_get(i,&app))continue;
        int x,y;sbs_position(i,sbs_pages_offset(&sbs_scroll),&x,&y);
        if(x+paper_x(62)<0||x-paper_x(62)>=api->screen_width())continue;
        bool active=i==selected;v->circle(x,y,paper_x(active?47:38),true);
        if(!active||!highlight)v->circle(x,y,paper_x(active?43:34),false);
        if(active)v->circle(x,y,paper_x(36),true);
        int size=paper_x(34);
        if(!api->draw_icon(x-size/2,y-size/2,app.icon,(uint8_t)size,!active))
            v->text(x-paper_x(10),y-paper_y(12),paper_x(24),"?",2,true,!active);
        const char *name=app.display_name;
        if(!strcmp(app.file_name,"file_browser.elf"))name="FILES";
        else if(!strcmp(app.file_name,"serial_monitor.elf"))name="SERIAL";
        else if(!strcmp(app.file_name,"ble_scanner.elf"))name="BLUETOOTH";
        else if(!strcmp(app.file_name,"points_in_time.elf"))name="POINTS";
        else if(!strcmp(app.file_name,"ble_touchpad.elf"))name="TOUCHPAD";
        else if(!strcmp(app.file_name,"ble_buttons.elf"))name="BT BUTTONS";
        else if(!strcmp(app.file_name,"waterfall.elf"))name="RF SPECTRUM";
        else if(!strcmp(app.file_name,"contexts.elf"))name="CONTEXTS";
        else if(!strcmp(app.file_name,"usb_sd_transfer.elf"))name="USB SD";
        int limit=paper_x(112),measured=v->measure(name,false);
        v->text(x-(measured<limit?measured:limit)/2,y+paper_y(43),limit,name,1,false,true);
    }
    portable_paper_scroll_clip(NULL);
    if(!count)v->text(paper_x(60),paper_y(200),paper_x(360),"NO INSTALLED APPS",2,false,true);
    api->fill_rect(paper_x(32),paper_y(736),paper_x(416),3,true);
    if(status&&*status)v->text(paper_x(32),paper_y(754),paper_x(416),status,1,false,true);
    else {
        unsigned pages=count?(count+SBS_PAGE_APPS-1)/SBS_PAGE_APPS:1;
        unsigned page=(unsigned)sbs_pages_nearest(&sbs_scroll);
        int left=api->screen_width()/2-(int)(pages-1)*paper_x(26);
        for(unsigned i=0;i<pages;i++) {
            int x=left+(int)i*paper_x(52),y=paper_y(768);
            v->circle(x,y,paper_x(10),true);
            if(i!=page)v->circle(x,y,paper_x(7),false);
        }
    }
    sbs_submitted=(sbs_image){.valid=true,.highlight=highlight,.selected=selected,.count=count,.q8=sbs_scroll.position_q8};
    memcpy(sbs_submitted.names,sbs_names,sizeof(sbs_names));
    api->present(initial);sbs_visible();return true;
}
static bool paper_run(void) {
    const paper_presentation *v=paper_presentation_get();
    if(!v||v->struct_size<sizeof(*v)||!v->begin||!v->text||!v->measure||!v->circle||!v->clock)return false;
    memset(&sbs_scroll,0,sizeof(sbs_scroll));memset(&sbs_submitted,0,sizeof(sbs_submitted));
    memset(&sbs_completed,0,sizeof(sbs_completed));memset(sbs_names,0,sizeof(sbs_names));sbs_cancel();sbs_catalog();
    uint8_t hour=0,minute=0;bool known=v->clock(&hour,&minute)&&hour<24&&minute<60;
    uint32_t last_clock=api->millis(),previous=0;bool armed=false,dirty=true,initial=true;
    paper_battery battery=paper_battery_read(v);char status[64]={0};
    paper_transition_begin();
    if(sbs_draw(v,status,initial,known,hour,minute,battery))dirty=initial=false;
    t5_app_input_t in;
    while(api->poll(&in,20)) {
        if(in.exit_requested||(in.buttons&T5_APP_BUTTON_BACK))break;
        if(!portable_paper_scroll_available()){sbs_cancel();dirty=true;continue;}
        dirty|=sbs_catalog();
        if(!in.buttons)armed=true;
        uint32_t pressed=armed?(in.buttons&~previous):0;previous=in.buttons;
        if(pressed){sbs_cancel();paper_transition_cancel();dirty=true;status[0]=0;}
        if(count) {
            if(pressed&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))selected=selected?selected-1:count-1;
            else if(pressed&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))selected=(selected+1)%count;
            if(pressed)sbs_reveal(selected);
            if(pressed&T5_APP_BUTTON_CONFIRM){strcpy(sbs_launch_name,sbs_names[selected]);sbs_pending=true;}
        }
        portable_touch_sample sample;uint32_t now;portable_paper_scroll_sample(&sample,&now);
        if(sample.began) {
            bool moving=sbs_scroll.animating;
            sbs_pending=sbs_pressed=false;sbs_launch_name[0]=sbs_hit_name[0]=0;sbs_dot_page=-1;
            paper_transition_cancel();dirty=true;
            /* Never rewind newest input to an older BUSY/completed frame. */
            sbs_configure();
            int hit=sbs_hit(sbs_scroll.position_q8,sample.x,sample.y);
            if(hit>=0&&!moving&&sample.tap_eligible&&!sample.cancelled) {
                strcpy(sbs_hit_name,sbs_names[hit]);sbs_hit_q8=sbs_scroll.position_q8;sbs_hit_cell=hit;
                int index=sbs_resolve(sbs_hit_name);
                if(index>=0){selected=(unsigned)index;sbs_pressed=true;status[0]=0;}
            }
            if(!moving&&sample.tap_eligible&&!sample.cancelled&&sample.y>=paper_y(742)) {
                unsigned pages=count?(count+SBS_PAGE_APPS-1)/SBS_PAGE_APPS:1;
                int left=api->screen_width()/2-(int)(pages-1)*paper_x(26);
                for(unsigned i=0;i<pages;i++)if(abs((int)sample.x-left-(int)i*paper_x(52))<=paper_x(22))sbs_dot_page=(int)i;
            }
            unsigned result=sbs_touch(&sample,now);
            sbs_scroll.stop_tap|=moving;dirty|=!!(result&PORTABLE_SCROLL_CHANGED);
        } else {
            unsigned result=sbs_touch(&sample,now);
            dirty|=!!(result&PORTABLE_SCROLL_CHANGED);
            if(!sample.valid||sample.cancelled||sbs_scroll.dragged) {
                if(sbs_pressed||sbs_pending)dirty=true;
                sbs_pressed=sbs_pending=false;sbs_hit_name[0]=sbs_launch_name[0]=0;sbs_dot_page=-1;
            }
            if(result&PORTABLE_SCROLL_TAP) {
                if(sbs_dot_page>=0&&sample.y>=paper_y(742)) {
                    sbs_pages_snap(&sbs_scroll,sbs_dot_page,now);dirty=true;
                    portable_perf_action(PORTABLE_PERF_SPRINGBOARD_PAGE,false);
                }
                int x,y;sbs_position((unsigned)sbs_hit_cell,sbs_hit_q8/256,&x,&y);
                int index=sbs_resolve(sbs_hit_name);
                if(sbs_pressed&&index>=0&&portable_scroll_inside(&sbs_scroll.view,sample.x,sample.y)&&
                   abs((int)sample.x-x)<=paper_x(44)&&(int)sample.y-y>=-paper_y(44)&&(int)sample.y-y<=paper_y(63)) {
                    selected=(unsigned)index;strcpy(sbs_launch_name,sbs_hit_name);sbs_pending=true;
                    portable_perf_action(PORTABLE_PERF_LAUNCH,true);
                }
            }
            if(sample.released&&sbs_pressed){sbs_pressed=false;dirty=true;}
        }
        if(!sbs_scroll.contact&&!sbs_scroll.animating&&count&&
           selected/SBS_PAGE_APPS!=(unsigned)sbs_pages_nearest(&sbs_scroll)) {
            selected=(unsigned)sbs_pages_nearest(&sbs_scroll)*SBS_PAGE_APPS;
            dirty=true;status[0]=0;portable_perf_action(PORTABLE_PERF_SPRINGBOARD_PAGE,false);
        }
        if(sbs_pending) {
            int index=sbs_resolve(sbs_launch_name);
            if(index<0){sbs_pending=false;dirty=true;}
            else if(selected!=(unsigned)index){selected=(unsigned)index;dirty=true;}
        }
        if((uint32_t)(now-last_clock)>=1000) {
            uint8_t h=0,m=0;bool valid=v->clock(&h,&m)&&h<24&&m<60;
            paper_battery next=paper_battery_read(v);dirty|=paper_battery_changed(battery,next);battery=next;
            dirty|=valid!=known||(valid&&(h!=hour||m!=minute));known=valid;hour=h;minute=m;last_clock=now;
        }
        if((dirty||paper_transition_active())&&sbs_draw(v,status,initial,known,hour,minute,battery))dirty=initial=false;
        /* Admission consumes current intent. The adapter owns any outstanding
         * display lease; a completed highlight is never an input prerequisite. */
        if(sbs_pending&&!sample.down) {
            t5_app_manifest_t app;int index=sbs_resolve(sbs_launch_name);
            if(index>=0&&api->installed_apps_get((unsigned)index,&app)&&app.compatible&&!strcmp(app.file_name,sbs_launch_name)&&api->request_app_launch((unsigned)index))return true;
            portable_perf_action(PORTABLE_PERF_LAUNCH_FAILED,false);sbs_cancel();strcpy(status,"UNABLE TO OPEN APP");dirty=true;
        }
    }
    sbs_cancel();paper_transition_cancel();(void)paper_frame_drain();return true;
}
