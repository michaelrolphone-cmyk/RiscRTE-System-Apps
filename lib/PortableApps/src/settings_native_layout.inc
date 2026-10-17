/* Explicit native Settings presentation. Legacy rectangle arguments below are
 * controller control identities, not a viewport transform. Render and hit
 * testing resolve those identities through this one capability-sized layout. */
#if defined(PORTABLE_SETTINGS_NATIVE_UI) && (!defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS))
#error "Native Settings requires Nova and the shared native canvas"
#endif
static bool sn_enabled(void) {
#ifdef PORTABLE_SETTINGS_NATIVE_UI
  return !pp_enabled();
#else
  return false;
#endif
}
typedef struct {int x,y,w,h;} sn_rect;
typedef struct {int w,h,margin,gap,top,bottom,row,footer,footer_h,message;} sn_layout;
static sn_layout sn_geometry(void) {
  sn_layout g={0};g.w=width();g.h=height();
  g.margin=g.w<200?8:20;g.gap=g.w<200?6:12;
  g.top=g.h<200?28:56;g.row=g.h<200?32:44;
  g.footer_h=g.h<200?28:44;g.footer=g.h-8-g.footer_h;
  g.message=g.footer-15;g.bottom=g.message-3;return g;
}
/* This is an app-content fit test, not a Runtime display ABI minimum.
 * Reserve a complete list row and the compact value/AM-PM/slider stack. */
static int sv_measure(unsigned,const char *,int);
static bool sn_fits(void) {
  sn_layout g=sn_geometry();
  int button=(g.w-2*g.margin-g.gap)/2;
  return g.bottom-g.top>=46 && g.w-2*g.margin>=100 &&
    button>=sv_measure(2,"CANCEL",0)+8;
}
static sn_rect sn_slider_rect(void) {
  sn_layout g=sn_geometry();
  return (sn_rect){g.margin,g.h<160?g.bottom-6:g.top+(g.w<200?32:44)+12,g.w-2*g.margin,6};
}
static bool sn_hit(sn_rect r,int x,int y) {
  return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;
}
static int sn_choice_scroll;
static unsigned sn_last_choice;
static unsigned sn_choices(void) {
  if(sv_page==SV_TIME_FORMAT||sv_page==SV_FOLD)return 2;
#ifdef PORTABLE_SLEEP_SETTINGS
  if(sv_page==SV_SLEEP)return settings_sleep_count();
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  if(sv_page==SV_ALERT)return settings_visual_alerts()?0:3;
#endif
  return 0;
}
static unsigned sn_visible_rows(void){sn_layout g=sn_geometry();int rows=(g.bottom-g.top)/g.row;return rows>0?(unsigned)rows:1;}
static int sn_limit(unsigned count) {return count>sn_visible_rows()?(int)(count-sn_visible_rows())*sn_geometry().row:0;}
static int sn_scroll_value(int value,unsigned count) {
  int limit=sn_limit(count),row=sn_geometry().row;
  if(value<0)return 0;
  value=(value+row/2)/row*row;return value>limit?limit:value;
}
static int sn_reveal(unsigned row,int scroll) {
  unsigned first=(unsigned)scroll/(unsigned)sn_geometry().row,rows=sn_visible_rows();
  if(row<first)first=row;else if(row>=first+rows)first=row-rows+1;
  return (int)first*sn_geometry().row;
}
/* Selection reveal belongs to the controller layout, including when a busy
 * provider defers the next paint. It must not depend on raster completion. */
static void sn_update_choice(void) {
  if(sn_choices()) {
    unsigned chosen=sv_page==SV_FOLD?sv_fold_choice:sv_page==SV_TIME_FORMAT?time_format_choice:0;
#ifdef PORTABLE_SLEEP_SETTINGS
    if(sv_page==SV_SLEEP)for(unsigned i=0;i<settings_sleep_count();i++)if(settings_sleep_at(i)==sleep_choice)chosen=i;
#endif
#ifdef PORTABLE_ALARM_SETTINGS
    if(sv_page==SV_ALERT)chosen=alert_choice?alert_choice-1:0;
#endif
    if(sn_last_choice!=chosen){sn_choice_scroll=sn_reveal(chosen,sn_choice_scroll);sn_last_choice=chosen;}
  }
 }
static sn_rect sn_row(unsigned row,int scroll) {
  sn_layout g=sn_geometry();sn_rect r={g.margin,g.top+(int)row*g.row-scroll,g.w-2*g.margin,g.row};
  /* Only whole visible rows are actionable; partial content never hides a
   * footer or becomes a tap target under it. Dragging reveals the other rows. */
  if(r.y<g.top||r.y+r.h>g.bottom)r.w=0;
  return r;
}
static sn_rect sn_footer(unsigned slot,unsigned columns) {
  sn_layout g=sn_geometry();int w=(g.w-2*g.margin-((int)columns-1)*g.gap)/(int)columns;
  return (sn_rect){g.margin+(int)slot*(w+g.gap),g.footer,w,g.footer_h};
}
static sn_rect sn_control(int x,int y,int w,int h) {
  sn_layout g=sn_geometry();
  if(h==26&&(y==192||y==200)) {
    if(x==24&&w==86)return sn_footer(0,2);
    if(x==130&&w==86)return sn_footer(1,2);
    return sn_footer(0,1);
  }
  if((sv_page==SV_VALUE&&h==36&&y==66)
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
     ||(sv_page==SV_TIMER&&h==44&&y==88)
#endif
    ) {
    int side=g.w<200?32:44;
    return (sn_rect){x<100?g.margin:g.w-g.margin-side,g.top,side,side};
  }
  if(sv_page==SV_VALUE&&x==76&&y==140) {
    int rh=g.h<200?20:32;
    return (sn_rect){g.w/2-24,g.h<160?g.top+18:g.bottom-rh,48,rh};
  }
  if(sv_page==SV_TIME_FORMAT&&h==32)return sn_row(y==76?0:1,sn_choice_scroll);
  if(sv_page==SV_FOLD&&h==32)return sn_row(y==100?0:1,sn_choice_scroll);
  if(h==30&&(y==66||y==104||y==142))return sn_row((unsigned)(y-66)/38u,sn_choice_scroll);
  return (sn_rect){0,0,0,0};
}
/* Raw contacts cannot act against a page/layout which is still queued or
 * painting. Navigation/Back and runtime exit are handled independently. */
static bool sn_touch_ready(void){return !sp_dirty&&portable_paper_frame_idle();}
static void sn_cancel_pending_touch(void){sv_gesture=false;sv_need_neutral=true;portable_touch_cancel_gesture(&touch);}

/* Completion is an input ownership boundary, including synchronous display
 * waits which collect raw edges before the controller can dispatch them.
 * Use the raw ABI's sequence watermark instead of dispatch time or guessed
 * timestamp epochs. Held contacts cancel; newer queued edges remain ordered. */
static void sn_presented(void) {
  if(!sv_active||!sn_enabled()||failed)return;
  sv_gesture=false;sv_need_neutral=true;
  input_reset();
}
