/* Native physical-size Home type. Baseline coordinates match the interactive
 * SVG. Raster inputs are generated at each requested size and weight. */
#include "../lib/PortableApps/home_fonts/text.inc"
#include "../lib/PortableApps/home_fonts/dial.inc"
#include "../lib/PortableApps/include/PortableRaster.h"
#ifdef PORTABLE_DESK_POINTS_FACE
#include "../lib/PortableApps/home_fonts/desk_time.inc"
static bool home_desk_widget,home_desk_clock,home_desk_ok;
static int home_view_width(int x){return home_desk_widget||home_desk_clock?x*app->screen_width()/800:xscale(x);}
static int home_view_x(int x){return home_desk_widget?(x+328)*app->screen_width()/800:home_desk_clock?x*app->screen_width()/800:xscale(x);}
static int home_view_y(int y){return home_desk_widget?(y-356)*app->screen_height()/480:home_desk_clock?y*app->screen_height()/480:yscale(y);}
static int home_view_height(int y){return home_desk_widget||home_desk_clock?y*app->screen_height()/480:yscale(y);}
#else
#define home_view_width xscale
#define home_view_x xscale
#define home_view_y yscale
#define home_view_height yscale
#endif
enum {HOME_NONE,HOME_TOP,HOME_DIAL,HOME_POINTS,HOME_FILES,HOME_DOCK_POINTS,HOME_CONTEXTS,HOME_SETTINGS};
static unsigned home_pressed,home_pending;
static bool home_refresh_pending=true;
static void home_press_bounds(unsigned target,int *x,int *y,int *w,int *h) {
 static const int bounds[][4]={{0,0,0,0},{0,0,480,72},{24,72,432,174},{24,254,432,364},
  {32,634,104,144},{136,634,104,144},{240,634,104,144},{344,634,104,144}};
 if(target>HOME_SETTINGS)target=HOME_NONE;
 *x=xscale(bounds[target][0]);*y=yscale(bounds[target][1]);*w=xscale(bounds[target][2]);*h=yscale(bounds[target][3]);
}
/* Capture the highlight with the glyph/shape. Touch may change Home's model
 * before deferred replay; callbacks must never consult home_pressed. */
typedef struct {int x,y,w,h;} home_ink_clip;
static home_ink_clip home_ink_snapshot(void) {
 home_ink_clip p={0};if(home_pressed)home_press_bounds(home_pressed,&p.x,&p.y,&p.w,&p.h);return p;
}
static void home_ink_span(home_ink_clip p,int x,int y,int w,int h,bool black) {
 int left=x>p.x?x:p.x,top=y>p.y?y:p.y,right=x+w<p.x+p.w?x+w:p.x+p.w,bottom=y+h<p.y+p.h?y+h:p.y+p.h;
 if(left>=right||top>=bottom){app->fill_rect(x,y,w,h,black);return;}
 if(y<top)app->fill_rect(x,y,w,top-y,black);
 if(bottom<y+h)app->fill_rect(x,bottom,w,y+h-bottom,black);
 if(x<left)app->fill_rect(x,top,left-x,bottom-top,black);
 if(right<x+w)app->fill_rect(right,top,x+w-right,bottom-top,black);
 app->fill_rect(left,top,right-left,bottom-top,!black);
}
static bool home_can_defer(void) {
#ifdef PORTABLE_DESK_POINTS_FACE
 if(home_desk_widget||home_desk_clock)return false;
#endif
 return true;
}
typedef struct {const home_glyph *glyph;home_ink_clip press;int origin,baseline,pen,width,natural;bool black;} home_glyph_draw;
static void home_glyph_rows(const void *payload,int first,int last) {
 const home_glyph_draw *s=payload;const home_glyph *g=s->glyph;
 int top=s->baseline+g->top,lo=first>top?first-top:0,hi=last-top;
 if(hi>(int)g->height)hi=g->height;
 for(int row=lo;row<hi;row++) {
  unsigned col=0;
  while(col<g->width) {
   unsigned bit=(unsigned)row*g->width+col;
   if(!(g->bits[bit/8]&(0x80u>>(bit%8)))){col++;continue;}
   unsigned begin=col++;
   while(col<g->width){bit=(unsigned)row*g->width+col;if(!(g->bits[bit/8]&(0x80u>>(bit%8))))break;col++;}
   int left=((s->pen+((int)g->left+(int)begin)*64)*s->width+s->natural/2)/s->natural;
   int right=((s->pen+((int)g->left+(int)col)*64)*s->width+s->natural/2)/s->natural;
   if(right>left)home_ink_span(s->press,s->origin+left,top+row,right-left,1,s->black);
  }
 }
}
typedef struct {home_tick tick;home_ink_clip press;int sw,sh;} home_tick_draw;
/* Conservative inverse bounds preserve the original integer scaling. */
static void home_source_rows(int y,int sh,int first,int last,int *lo,int *hi) {
 int a=first*800/sh-y-1,b=(last*800+sh-1)/sh-y+1;
 if(a>*lo)*lo=a<*hi?a:*hi;
 if(b<*hi)*hi=b>*lo?b:*lo;
}
static void home_tick_rows(const void *payload,int first,int last) {
 const home_tick_draw *s=payload;const home_tick *t=&s->tick;int lo=0,hi=t->height;
 home_source_rows(t->y,s->sh,first,last,&lo,&hi);
 for(int row=lo;row<hi;row++) {
  int y=(t->y+row)*s->sh/800,h=s->sh/800;
  if(y<first){h-=first-y;y=first;}if(y+h>last)h=last-y;if(h<=0)continue;
  unsigned col=0;
  while(col<t->width) {
   unsigned bit=(unsigned)row*t->width+col;
   if(!(t->bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
   unsigned begin=col++;
   while(col<t->width){bit=(unsigned)row*t->width+col;if(!(t->bits[bit/8]&(128u>>(bit%8))))break;col++;}
   home_ink_span(s->press,(t->x+(int)begin)*s->sw/480,y,(int)(col-begin)*s->sw/480,h,true);
  }
 }
}
typedef struct {home_ink_clip press;int x,y,w,h,r,sw,sh;bool black;} home_round_draw;
static void home_round_rows(const void *payload,int first,int last) {
 const home_round_draw *s=payload;int lo=0,hi=s->h;home_source_rows(s->y,s->sh,first,last,&lo,&hi);
 for(int row=lo;row<hi;row++) {
  int y=(s->y+row)*s->sh/800,h=s->sh/800;
  if(y<first){h-=first-y;y=first;}if(y+h>last)h=last-y;if(h<=0)continue;
  int cy=row<s->r?2*s->r-2*row-1:row>=s->h-s->r?2*(row-(s->h-s->r))+1:0,inset=0;
  if(cy)while(inset<s->r){int dx=2*s->r-2*inset-1;if(dx*dx+cy*cy<=4*s->r*s->r)break;inset++;}
  home_ink_span(s->press,(s->x+inset)*s->sw/480,y,(s->w-2*inset)*s->sw/480,h,s->black);
 }
}
static void home_fill(int x,int y,int w,int h,bool black) {
#ifdef PORTABLE_DESK_POINTS_FACE
 if(home_desk_widget||home_desk_clock) {
  if(!home_desk_ok || ((desk_rectangles++&63u)==0 && desk_cancelled())){home_desk_ok=false;return;}
  app->fill_rect(x,y,w,h,black);home_desk_ok=portable_desk_adapter_ready();return;
 }
#endif
 if(!home_pressed){app->fill_rect(x,y,w,h,black);return;}
 int px,py,pw,ph;home_press_bounds(home_pressed,&px,&py,&pw,&ph);
 int left=x>px?x:px,top=y>py?y:py,right=x+w<px+pw?x+w:px+pw,bottom=y+h<py+ph?y+h:py+ph;
 if(left>=right||top>=bottom){app->fill_rect(x,y,w,h,black);return;}
 if(y<top)app->fill_rect(x,y,w,top-y,black);
 if(bottom<y+h)app->fill_rect(x,bottom,w,y+h-bottom,black);
 if(x<left)app->fill_rect(x,top,left-x,bottom-top,black);
 if(right<x+w)app->fill_rect(right,top,x+w-right,bottom-top,black);
 app->fill_rect(left,top,right-left,bottom-top,!black);
}
static void home_press_begin(void) {
 if(home_pressed){int x,y,w,h;home_press_bounds(home_pressed,&x,&y,&w,&h);app->fill_rect(x,y,w,h,true);}
}
static unsigned home_char(unsigned c) {if(c>='a'&&c<='z')c-=32;return c>=32&&c<=126?c:'?';}
static int home_kerning(const home_font *font,unsigned previous,unsigned current) {
 unsigned key=previous*128u+current,lo=0,hi=font->kern_count;
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;const home_kern *k=&font->kern[mid];unsigned value=k->first*128u+k->second;
  if(value<key)lo=mid+1;else hi=mid;}
 return lo<font->kern_count&&font->kern[lo].first==previous&&font->kern[lo].second==current?font->kern[lo].adjust_q6:0;
}
#ifndef PORTABLE_DESK_POINTS_SNAPSHOT
static int home_svg_fit(const char *value,unsigned size,unsigned family_factor,unsigned tracking,unsigned maximum) {
 unsigned length=0;while(length<96&&value[length])length++;
 return length*(size*family_factor+tracking*100u)>maximum*100u?(int)maximum:0;
}
#endif
static int home_text_width(const home_font *font,const char *value,int tracking) {
 int width=0;unsigned previous=0;
 for(unsigned i=0;i<96&&value[i];i++) {unsigned c=home_char((unsigned char)value[i]);
  if(previous)width+=home_kerning(font,previous,c)+tracking*64;
  width+=font->glyphs[c-32].advance_q6;previous=c;}
 return width;
}
/* align: 0 left, 1 right, 2 centered. A positive target enforces the mockup's
 * textLength; a positive limit fits complete dynamic labels when required. */
static void home_type_ink(const home_font *font,int x,int baseline,const char *value,int tracking,int align,int limit,int target,bool black) {
 int natural=home_text_width(font,value,home_view_width(tracking));if(natural<=0)return;
 int width=target?home_view_width(target):(natural+32)/64;
 if(limit&&width>home_view_width(limit))width=home_view_width(limit);
 int origin=home_view_x(x)-(align==1?width:align==2?width/2:0),pen=0;unsigned previous=0;
 for(unsigned k=0;k<96&&value[k];k++) {unsigned c=home_char((unsigned char)value[k]);const home_glyph *g=&font->glyphs[c-32];
  if(previous)pen+=home_kerning(font,previous,c)+home_view_width(tracking)*64;
  home_glyph_draw draw={g,home_ink_snapshot(),origin,home_view_y(baseline),pen,width,natural,black};
  if(home_can_defer()&&portable_raster_record_draw(home_glyph_rows,&draw,sizeof(draw),draw.baseline+g->top,draw.baseline+g->top+g->height)) {
   pen+=g->advance_q6;previous=c;continue;
  }
  for(unsigned row=0;row<g->height;row++) {
   unsigned col=0;
   while(col<g->width) {
    unsigned bit=row*g->width+col;
    if(!(g->bits[bit/8]&(0x80u>>(bit%8)))){col++;continue;}
    unsigned first=col++;
    while(col<g->width){bit=row*g->width+col;if(!(g->bits[bit/8]&(0x80u>>(bit%8))))break;col++;}
    int left=((pen+((int)g->left+(int)first)*64)*width+natural/2)/natural;
    int right=((pen+((int)g->left+(int)col)*64)*width+natural/2)/natural;
    if(right>left)home_fill(origin+left,home_view_y(baseline)+g->top+(int)row,right-left,1,black);
   }
  }
  pen+=g->advance_q6;previous=c;
 }
}
static void home_type(const home_font *font,int x,int baseline,const char *value,int tracking,int align,int limit,int target) {
 home_type_ink(font,x,baseline,value,tracking,align,limit,target,true);
}
static void home_round(int x,int y,int width,int height,int radius,bool black) {
 /* Scanline raster of the source's rounded rectangle; no fake-gray edges. */
 if(width<=0||height<=0)return;
 if(radius>width/2)radius=width/2;
 if(radius>height/2)radius=height/2;
 home_round_draw draw={home_ink_snapshot(),x,y,width,height,radius,app->screen_width(),app->screen_height(),black};
 if(home_can_defer()&&portable_raster_record_draw(home_round_rows,&draw,sizeof(draw),yscale(y),yscale(y+height)+1))return;
 for(int row=0;row<height;row++) {
  int cy=row<radius?2*radius-2*row-1:row>=height-radius?2*(row-(height-radius))+1:0;
  int inset=0;
  if(cy)while(inset<radius) {int dx=2*radius-2*inset-1;if(dx*dx+cy*cy<=4*radius*radius)break;inset++;}
  home_fill(home_view_x(x+inset),home_view_y(y+row),home_view_width(width-2*inset),home_view_height(1),black);
 }
}
static inline void home_battery(void) {
 char value[12];
 if(battery_status.valid)snprintf(value,sizeof(value),"%u%%%s",battery_status.percent,battery_status.charging?" +":"");
 else strcpy(value,"--%");
 home_type(&HOME_O700_20,308,47,value,0,1,86,0);
 unsigned bars=battery_status.valid?(battery_status.percent+5u)/10u:0;
 for(unsigned i=0;i<10;i++) {
  int x=321+(int)i*13;home_round(x-1,31,12,16,3,true);
  if(i>=bars)home_round(x+2,34,6,10,1,false);
 }
}

static inline void home_dial(bool known,unsigned minute) {
 for(unsigned i=0;i<60;i++) {
  const home_tick *t=&home_ticks[i*2+(known&&i<=minute?1:0)];
  home_tick_draw draw={*t,home_ink_snapshot(),app->screen_width(),app->screen_height()};
  if(home_can_defer()&&portable_raster_record_draw(home_tick_rows,&draw,sizeof(draw),yscale(t->y),yscale(t->y+t->height)+1))continue;
  for(unsigned row=0;row<t->height;row++) {
   unsigned col=0;
   while(col<t->width) {
    unsigned bit=row*t->width+col;
    if(!(t->bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
    unsigned first=col++;
    while(col<t->width){bit=row*t->width+col;if(!(t->bits[bit/8]&(128u>>(bit%8))))break;col++;}
    home_fill(xscale(t->x+(int)first),yscale(t->y+(int)row),xscale((int)(col-first)),yscale(1),true);
   }
  }
 }
}
