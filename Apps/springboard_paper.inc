/* NOVA-7 E-Ink: fixed 3/2 rows, real catalog only, no animation or fades. */
#include "PaperPresentation.h"
#include "PaperBattery.h"
__attribute__((weak)) const paper_presentation *paper_presentation_get(void) {return NULL;}
#define PAPER_PAGE_APPS 15u
static int paper_x(int x){return x*api->screen_width()/480;}
static int paper_y(int y){return y*api->screen_height()/800;}
static void paper_position(unsigned cell,int *x,int *y) {
    unsigned pair=cell/5,at=cell%5;
    *x=paper_x(at<3?110+(int)at*130:175+(int)(at-3)*130);
    *y=paper_y(136+(int)pair*208+(at>=3?104:0));
}
static int paper_hit(unsigned first,int x,int y) {
    for(unsigned cell=0;cell<PAPER_PAGE_APPS&&first+cell<count;cell++) {
        int cx,cy;paper_position(cell,&cx,&cy);int dx=x-cx,dy=y-cy;
        /* The label and icon share an 88px minimum target at 480px width. */
        if(abs(dx)<=paper_x(44)&&dy>=-paper_y(44)&&dy<=paper_y(63))return (int)(first+cell);
    }
    return -1;
}
static void paper_name(const paper_presentation *v,int x,int y,const t5_app_manifest_t *app) {
    const char *name=app->display_name;
    if(!strcmp(app->file_name,"file_browser.elf"))name="FILES";
    else if(!strcmp(app->file_name,"serial_monitor.elf"))name="SERIAL";
    else if(!strcmp(app->file_name,"ble_scanner.elf"))name="BLUETOOTH";
    else if(!strcmp(app->file_name,"points_in_time.elf"))name="POINTS";
    int limit=paper_x(122),measured=v->measure(name,false);
    v->text(x-(measured<limit?measured:limit)/2,y+paper_y(43),limit,name,1,false,true);
}
static void paper_draw(const paper_presentation *v,const char *status,bool initial,
                       bool known,uint8_t hour,uint8_t minute,paper_battery battery) {
    v->begin();v->text(paper_x(32),paper_y(30),paper_x(270),"APPS",2,true,true);
    char time[12];if(known)snprintf(time,sizeof(time),"%02u:%02u",hour,minute);else strcpy(time,"--:--");
    v->text(paper_x(214),paper_y(30),paper_x(120),time,2,true,true);
    paper_battery_draw(api,v,342,40,battery);
    api->fill_rect(paper_x(32),paper_y(78),paper_x(416),3,true);
    unsigned first=selected/PAPER_PAGE_APPS*PAPER_PAGE_APPS;
    for(unsigned cell=0;cell<PAPER_PAGE_APPS && first+cell<count;cell++) {
        t5_app_manifest_t app;if(!api->installed_apps_get(first+cell,&app))continue;
        int x,y;paper_position(cell,&x,&y);bool active=first+cell==selected;
        v->circle(x,y,paper_x(active?47:38),true);
        v->circle(x,y,paper_x(active?43:34),false);
        if(active)v->circle(x,y,paper_x(36),true);
        int size=paper_x(34);
        if(!api->draw_icon(x-size/2,y-size/2,app.icon,(uint8_t)size,!active))
            v->text(x-paper_x(10),y-paper_y(12),paper_x(24),"?",2,true,!active);
        paper_name(v,x,y,&app);
    }
    if(!count)v->text(paper_x(60),paper_y(200),paper_x(360),"NO INSTALLED APPS",2,false,true);
    if(status&&*status)v->text(paper_x(32),paper_y(702),paper_x(416),status,1,false,true);
    api->fill_rect(paper_x(32),paper_y(736),paper_x(416),3,true);
    unsigned pages=count?(count+PAPER_PAGE_APPS-1)/PAPER_PAGE_APPS:1;
    int left=api->screen_width()/2-(int)(pages-1)*paper_x(26);
    for(unsigned i=0;i<pages;i++) {
        int x=left+(int)i*paper_x(52),y=paper_y(768);
        v->circle(x,y,paper_x(10),true);
        if(i!=selected/PAPER_PAGE_APPS)v->circle(x,y,paper_x(7),false);
    }
    api->present(initial);
}
static bool paper_run(void) {
    const paper_presentation *v=paper_presentation_get();
    if(!v || v->struct_size<sizeof(*v) || !v->begin || !v->text || !v->measure || !v->circle || !v->contact || !v->clock)return false;
    uint8_t hour=0,minute=0;bool known=v->clock(&hour,&minute)&&hour<24&&minute<60;
    uint32_t last_clock=api->millis(),previous=0;bool armed=false;
    paper_battery battery=paper_battery_read(v);
    char status[64]={0};paper_draw(v,status,true,known,hour,minute,battery);
    t5_app_input_t in;
    while(api->poll(&in,20)) {
        if(in.exit_requested || (in.buttons&T5_APP_BUTTON_BACK))break;
        if(!in.buttons)armed=true;
        uint32_t pressed=armed?(in.buttons&~previous):0;previous=in.buttons;
        unsigned old=selected;bool activate=false,dirty=false;
        if(count) {
            if(pressed&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))selected=selected?selected-1:count-1;
            else if(pressed&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))selected=(selected+1)%count;
            activate=!!(pressed&T5_APP_BUTTON_CONFIRM);
        }
        springboard_contact c={0};v->contact(&c);
        if(c.valid && c.released && c.tap_eligible && !c.cancelled) {
            if(c.y>=paper_y(742) && count>PAPER_PAGE_APPS) {
                selected=(selected/PAPER_PAGE_APPS+1)*PAPER_PAGE_APPS;if(selected>=count)selected=0;
            } else {
                int hit=paper_hit(selected/PAPER_PAGE_APPS*PAPER_PAGE_APPS,c.x,c.y);
                if(hit>=0){selected=(unsigned)hit;activate=true;}
            }
        }
        if(selected!=old){status[0]=0;dirty=true;}
        if(activate) {
            t5_app_manifest_t app;
            if(api->installed_apps_get(selected,&app)&&app.compatible&&api->request_app_launch(selected))return true;
            strcpy(status,"UNABLE TO OPEN APP");dirty=true;
        }
        uint32_t now=api->millis();
        if((uint32_t)(now-last_clock)>=1000) {
            uint8_t h=0,m=0;bool valid=v->clock(&h,&m)&&h<24&&m<60;
            paper_battery next=paper_battery_read(v);dirty|=paper_battery_changed(battery,next);battery=next;
            dirty|=valid!=known || (valid&&(h!=hour||m!=minute));known=valid;hour=h;minute=m;last_clock=now;
        }
        if(dirty)paper_draw(v,status,false,known,hour,minute,battery);
    }
    return true;
}
