/* Explicit capability-sized Wi-Fi view. No completed 240-pixel viewport is
 * transformed: these same native rectangles drive glyphs, controls and hits. */
#if !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#error "Native Wi-Fi requires Nova, shared native canvas and custody fence"
#endif
typedef struct {int x,y,w,h;} wn_rect;
typedef struct {int w,h,margin,gap,header,top,row,bottom,footer,footer_h;} wn_layout;
static bool wn_pending=true,wn_gesture,wn_time_valid;
static uint32_t wn_presented_at;
static int wn_start_x,wn_start_y;
static wn_layout wn_geometry(void) {
  wn_layout g={0};g.w=width();g.h=height();g.margin=g.w<200?8:16;g.gap=g.w<200?4:8;
  g.header=g.h<160?20:g.h<200?24:40;g.top=g.header+18;g.row=g.h<200?28:44;
  g.footer_h=g.h<200?24:40;g.footer=g.h-6-g.footer_h;g.bottom=g.footer-18;return g;
}
static bool wn_inside(wn_rect r,int x,int y) {return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static unsigned wn_rows(void) {wn_layout g=wn_geometry();int n=(g.bottom-g.top)/g.row;return n>0?(unsigned)n:1;}
static bool wn_fits(void) {wn_layout g=wn_geometry();return g.w>=128&&g.h>=128&&g.bottom-g.top>=g.row;}
static wn_rect wn_back(void){wn_layout g=wn_geometry();return (wn_rect){g.margin,2,24,g.header-2};}
static wn_rect wn_footer(unsigned column) {wn_layout g=wn_geometry();int w=(g.w-2*g.margin-2*g.gap)/3;return (wn_rect){g.margin+(int)column*(w+g.gap),g.footer,w,g.footer_h};}
static wn_rect wn_row(unsigned row) {wn_layout g=wn_geometry();return (wn_rect){g.margin,g.top+(int)row*g.row,g.w-2*g.margin,g.row-2};}
static wn_rect wn_key(unsigned key) {
  wn_layout g=wn_geometry();if(key>=PWK_COUNT)return (wn_rect){0};
  if(key>=PWK_CHARACTERS)return wn_footer(key-PWK_CHARACTERS);
  int gap=2,cell=(g.w-2*g.margin-7*gap)/8,top=g.header+18;
  int rows=(g.footer-4-top)/4;if(rows>44)rows=44;
  return (wn_rect){g.margin+(int)(key%8)*(cell+gap),top+(int)(key/8)*rows,cell,rows-2};
}
static void wn_text(unsigned face,int x,int y,int available,const char *s,uint32_t color) {
  if(!s||available<=0)return;
  char label[100];size_t n=strlen(s);if(n>96)n=96;memcpy(label,s,n);label[n]=0;
  if(portable_nova_measure(face,label)>available) {
    int dots=portable_nova_measure(face,"...");
    while(n&&portable_nova_measure(face,label)>available-dots)label[--n]=0;
    if(available>=dots)memcpy(label+n,"...",4);else label[0]=0;
  }
  portable_nova_native_text(face,x,y,available,label,color);
}
static void wn_button(wn_rect r,const char *s,bool selected) {
  if(r.w<=0||r.h<=0)return;
  portable_nova_native_round(r.x,r.y,r.w,r.h,4,selected?NOVA_CYAN:NOVA_DIM);
  portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,3,selected?0x062d35u:0);
  /* Native glyph metrics position ink inside the cell even at compact sizes. */
  unsigned face=r.h<18?3:2;int top=100,bottom=0;
  for(unsigned i=0;s[i];i++){unsigned ch=(unsigned char)s[i];if(ch<32||ch>126)ch='?';const rps_glyph *glyph=&rps_text[face*95+ch-32];if(glyph->height){if(glyph->top<top)top=glyph->top;if(glyph->top+glyph->height>bottom)bottom=glyph->top+glyph->height;}}
  int ink=bottom-top;if(top==100){top=0;ink=0;}
  int n=portable_nova_measure(face,s);wn_text(face,n<r.w-2?r.x+(r.w-n)/2:r.x+1,r.y+(r.h-ink)/2-top,r.w-2,s,NOVA_CYAN);
}
void portable_wifi_native_invalidate(void) {
  if(pp_enabled())return;
  wn_pending=true;wn_gesture=false;wn_time_valid=false;portable_touch_cancel_gesture(&touch);
}
void portable_wifi_native_presented(void) {
  if(pp_enabled())return;
  wn_presented_at=millis_now();wn_time_valid=true;wn_gesture=false;
  portable_touch_cancel_gesture(&touch);
}
bool portable_wifi_native_ready(void) {return pp_enabled()||(!wn_pending&&wn_time_valid&&portable_paper_frame_idle());}
/* Native touch owns its header as well as list/keyboard controls. Cancelling
 * while the scene is pending prevents a held contact crossing a transition. */
static bool wifi_native_poll(t5_app_input_t *out) {
  if(pp_enabled())return false;
  portable_touch_sample sample;input_take(&sample);
  if(out->buttons){wn_gesture=false;portable_touch_cancel_gesture(&touch);return true;}
  if(!portable_wifi_native_ready()){wn_gesture=false;portable_touch_cancel_gesture(&touch);return true;}
  /* Captures can queue during a provider drain. Delivery after completion
   * does not make those earlier contacts belong to the newly visible scene.
   * Display waits are bounded below the signed half-range; compare modulo32. */
  if(sample.valid&&!sample.cancelled&&(sample.began||sample.down||sample.released)&&(int32_t)((uint32_t)sample.timestamp_ms-wn_presented_at)<0){wn_gesture=false;portable_touch_cancel_gesture(&touch);return true;}
  if(sample.cancelled||!sample.valid)wn_gesture=false;
  if(sample.began){wn_gesture=sample.tap_eligible;wn_start_x=sample.x;wn_start_y=sample.y;}
  if(sample.released&&wn_gesture){
    wn_gesture=false;int dx=(int)sample.x-wn_start_x,dy=(int)sample.y-wn_start_y;
    if(sample.cancelled)return true;
    if(sample.moved){
      if(dx>=36&&dx>abs(dy)*2)out->buttons=T5_APP_BUTTON_BACK;
      else if(dy>=28&&dy>abs(dx)*2)out->buttons=T5_APP_BUTTON_UP;
      else if(dy<=-28&&-dy>abs(dx)*2)out->buttons=T5_APP_BUTTON_DOWN;
    }else if(sample.tap_eligible){out->tapped=true;out->touch_x=sample.x;out->touch_y=sample.y;}
  }
  return true;
}
static int wifi_native_hit(const portable_wifi_view *v,int x,int y) {
  if(!v||!wn_fits())return -1;
  if(wn_inside(wn_back(),x,y))return WIFI_HIT_BACK;
  if(v->keyboard){for(unsigned i=0;i<PWK_COUNT;i++)if(wn_inside(wn_key(i),x,y))return (int)i;}
  else {
    for(unsigned i=0;i<3;i++)if(wn_inside(wn_footer(i),x,y))return i==0?WIFI_HIT_PREVIOUS:i==1?WIFI_HIT_SELECT:WIFI_HIT_NEXT;
    unsigned rows=wn_rows(),first=v->selected/rows*rows;
    for(unsigned j=0;j<rows&&first+j<v->count;j++)if(wn_inside(wn_row(j),x,y))return (int)(first+j);
  }
  return -1;
}
static void wifi_native_render(const portable_wifi_view *v) {
  if(failed||!v||!wn_fits()||!portable_paper_frame_ready())return;
  wn_pending=true;wn_time_valid=false;wn_layout g=wn_geometry();list_mode=false;clear();fill(0,0,g.w,g.h,0);
  wn_rect back=wn_back();(void)portable_nova_native_icon(back.x+4,back.y+(back.h-16)/2,16,"solid:f053",NOVA_CYAN);
  wn_text(2,g.margin+30,g.header<30?2:13,g.w-2*g.margin-30,v->title,NOVA_CAP);
  portable_nova_native_fill(g.margin,g.header-1,g.w-2*g.margin,1,NOVA_DIM);
  if(v->keyboard) {
    /* Keep the tail visible using actual glyph advance, never character count. */
    const char *entry=v->entry?v->entry:"";while(*entry&&portable_nova_measure(2,entry)>g.w-2*g.margin)++entry;
    wn_text(2,g.margin,g.header+2,g.w-2*g.margin,entry,NOVA_CYAN);
    for(unsigned i=0;i<PWK_COUNT;i++){
      unsigned ch=portable_watch_key_character(v->key_page,i);char s[]={(char)(ch?ch:' '),0};
      const char *label=i==PWK_PAGE?"ABC/#":i==PWK_DELETE?(g.w<200?"DEL":"DELETE"):i==PWK_DONE?"DONE":ch==32?"_":s;
      wn_button(wn_key(i),label,v->selected==i);
    }
  }else{
    wn_text(2,g.margin,g.header+2,g.w-2*g.margin,v->detail,NOVA_CYAN);
    unsigned rows=wn_rows(),first=v->selected/rows*rows;
    for(unsigned j=0;j<rows&&first+j<v->count;j++){
      unsigned i=first+j;wn_rect r=wn_row(j);
      portable_nova_native_fill(r.x,r.y,2,r.h,i==v->selected?NOVA_CYAN:NOVA_DIM);
      portable_nova_native_fill(r.x+2,r.y+r.h-1,r.w-2,1,NOVA_LINE);
      wn_text(2,r.x+8,r.y+1,r.w-12,v->rows[i],NOVA_WHITE);
      wn_text(2,r.x+8,r.y+(g.row<40?13:23),r.w-12,v->values[i],NOVA_CYAN);
    }
    wn_text(2,g.margin,g.footer-16,g.w-2*g.margin,v->message,NOVA_CAP);
    wn_button(wn_footer(0),"Prev",false);wn_button(wn_footer(1),g.w<160?"OK":"Select",false);wn_button(wn_footer(2),"Next",false);
  }
  present(false);wn_pending=false;
  /* Unchanged/synchronous frames have no outstanding completion token. */
  if(!wn_time_valid&&portable_paper_frame_idle())portable_wifi_native_presented();
}
