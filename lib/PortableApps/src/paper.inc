/* Static one-bit presentation selected from display capability information. */
#include "../../../Apps/PaperPresentation.h"
#include "../paper_fonts/text.inc"
static void pp_begin(void) { list_mode=false;clear_color(0xffff); }
static void pp_circle(int x,int y,int radius,bool black) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_PP_CIRCLE,(int32_t[8]){x,y,radius,black},NULL))return;
#endif
  if(radius<1 || radius>128)return;
  for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++)
    if(i*i+j*j<=radius*radius)fill(x+i,y+j,1,1,black?0:0xffff);
}
static int pp_measure(const char *s,bool heading) {
  int n=0;if(!s)return 0;
  for(unsigned i=0;i<96 && s[i];i++) {
    unsigned c=(unsigned char)s[i];
#ifdef PORTABLE_PAPER_PREFERENCES
    if(c==0xc2 && (unsigned char)s[i+1]==0xb0){n+=160;++i;continue;}
#endif
    if(c>='a'&&c<='z')c-=32;
    if(c<32||c>126)c='?';
    n+=rpp_text[(heading?0:95)+c-32].advance_q4;
  }
  return (n+15)/16;
}
static void pp_text(int x,int y,int max_width,const char *s,unsigned scale,bool heading,bool black) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_PP_TEXT,(int32_t[8]){x,y,max_width,(int32_t)scale,heading,black},s))return;
#endif
  if(!s || (scale&255u)<1 || (scale&255u)>4 || max_width<=0)return;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
  int zoom=(scale&PAPER_TEXT_CLOCK)?3:(scale&PAPER_TEXT_LARGE)?2:1;
  int total=pp_measure(s,heading)*zoom;
  int fitted=(scale&PAPER_TEXT_FIT)&&total>max_width?max_width:total;
#else
  int zoom=(scale&PAPER_TEXT_CLOCK)?3:1;
#endif
  int pen=x*16;
  for(unsigned k=0;k<96 && s[k];k++) {
    unsigned c=(unsigned char)s[k];
#ifdef PORTABLE_PAPER_PREFERENCES
    if(c==0xc2 && (unsigned char)s[k+1]==0xb0) {
      if((pen+160+15)/16>x+max_width)break;
      pp_circle(pen/16+4,y+4,3,black);pp_circle(pen/16+4,y+4,1,!black);
      pen+=160;++k;continue;
    }
#endif
    if(!(scale&PAPER_TEXT_LITERAL)&&c>='a'&&c<='z')c-=32;
    if(c<32||c>126)c='?';
    const rpp_glyph *g=&rpp_text[(heading?0:95)+c-32];
    int advance=(int)g->advance_q4*zoom;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
    if(!(scale&PAPER_TEXT_FIT)&&(pen+advance+15)/16>x+max_width)break;
#else
    if((pen+advance+15)/16>x+max_width)break;
#endif
    for(unsigned j=0;j<g->height;j++)for(unsigned i=0;i<g->width;i++) {
      unsigned n=j*g->width+i;
      if(g->bits[n/8]&(0x80u>>(n%8))) {
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
        int offset=pen/16-x+((int)g->left+(int)i)*zoom;
        int left=total?offset*fitted/total:offset;
        int right=total?(offset+zoom)*fitted/total:offset+zoom;
        if(right>left)fill(x+left,y+((int)g->top+(int)j)*zoom,right-left,zoom,black?0:0xffff);
#else
        fill(pen/16+((int)g->left+(int)i)*zoom,y+((int)g->top+(int)j)*zoom,zoom,zoom,black?0:0xffff);
#endif
      }
    }
    pen+=advance;
  }
}
static bool pp_battery(uint8_t *percent,bool *charging) {
  portable_perf_count(PORTABLE_PERF_BATTERY_READS);
  risc_battery_sample_v1 sample={0};
  if(!gauge || !gauge->read || !gauge->read(gauge->context,&sample) || sample.percent>100 ||
     (sample.flags&RISC_BATTERY_PROFILE_MISSING))return false;
  *percent=sample.percent;*charging=!!(sample.flags&RISC_BATTERY_CHARGING);return true;
}
static const paper_presentation pp_view={sizeof(pp_view),pp_begin,pp_text,pp_measure,pp_circle,np_contact,np_clock,pp_battery};
static bool pp_enabled(void) {
  return ((width()>=400 && width()<=600 && height()>=600 && height()>width())
#ifdef PORTABLE_DESK_LOCK_HOME
    /* The selected desk scene rotates the same native X4 surface. It must
     * remain eligible for its next minute and for restoring portrait Home. */
    || (width()==800 && height()==480)
#endif
    ) &&
    (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) &&
    (info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1));
}
const paper_presentation *paper_presentation_get(void) {
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
  if(failed || native_custody_retained)return NULL;
#endif
  if(!pp_enabled())return NULL;
#if defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
  np_rtc_grant.struct_size=sizeof(np_rtc_grant);
  if(!np_rtc_grant.api && rt->acquire("rtc.clock",TWATCH_RTC_API_V1,0,&np_rtc_grant)) {
    const twatch_rtc_api_v1 *clock=np_rtc_grant.api;
    if(clock && clock->api_version==TWATCH_RTC_API_V1 && clock->struct_size>=sizeof(*clock) && clock->read)np_rtc=clock;
    else if(np_rtc_grant.api && !rt->release(&np_rtc_grant))failed=true;
  }
#endif
  nova_mode=true;memset(&nova_contact,0,sizeof(nova_contact));return &pp_view;
}

#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
/* The selected foreground session already loaded and released app settings.
 * Reading its confirmed value adds no grant, I/O, or renderer-side custody. */
bool portable_springboard_hour_24(void) {
#ifdef PORTABLE_QUICK_ACTIONS
  return quick.loaded && quick.hour_24;
#else
  return false;
#endif
}
#endif
