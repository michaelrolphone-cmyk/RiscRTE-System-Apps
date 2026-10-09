/* Native physical-size Home type. Baseline coordinates match the interactive
 * SVG. Raster inputs are generated at each requested size and weight. */
#include "../lib/PortableApps/home_fonts/text.inc"
#include "../lib/PortableApps/home_fonts/dial.inc"
enum {HOME_NONE,HOME_TOP,HOME_DIAL,HOME_POINTS};
static unsigned home_pressed,home_painted,home_pending;
static bool home_refresh_pending=true;
static void home_press_bounds(unsigned target,int *x,int *y,int *w,int *h) {
 static const int bounds[][4]={{0,0,0,0},{0,0,480,72},{40,72,400,390},{24,484,432,226}};
 *x=xscale(bounds[target][0]);*y=yscale(bounds[target][1]);*w=xscale(bounds[target][2]);*h=yscale(bounds[target][3]);
}
static void home_fill(int x,int y,int w,int h,bool black) {
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
static int home_svg_fit(const char *value,unsigned size,unsigned family_factor,unsigned tracking,unsigned maximum) {
 unsigned length=0;while(length<96&&value[length])length++;
 return length*(size*family_factor+tracking*100u)>maximum*100u?(int)maximum:0;
}
static int home_text_width(const home_font *font,const char *value,int tracking) {
 int width=0;unsigned previous=0;
 for(unsigned i=0;i<96&&value[i];i++) {unsigned c=home_char((unsigned char)value[i]);
  if(previous)width+=home_kerning(font,previous,c)+tracking*64;
  width+=font->glyphs[c-32].advance_q6;previous=c;}
 return width;
}
/* align: 0 left, 1 right, 2 centered. A positive target enforces the mockup's
 * textLength; a positive limit fits complete dynamic labels when required. */
static void home_type(const home_font *font,int x,int baseline,const char *value,int tracking,int align,int limit,int target) {
 int natural=home_text_width(font,value,xscale(tracking));if(natural<=0)return;
 int width=target?xscale(target):(natural+32)/64;
 if(limit&&width>xscale(limit))width=xscale(limit);
 int origin=xscale(x)-(align==1?width:align==2?width/2:0),pen=0;unsigned previous=0;
 for(unsigned k=0;k<96&&value[k];k++) {unsigned c=home_char((unsigned char)value[k]);const home_glyph *g=&font->glyphs[c-32];
  if(previous)pen+=home_kerning(font,previous,c)+xscale(tracking)*64;
  for(unsigned row=0;row<g->height;row++) {
   unsigned col=0;
   while(col<g->width) {
    unsigned bit=row*g->width+col;
    if(!(g->bits[bit/8]&(0x80u>>(bit%8)))){col++;continue;}
    unsigned first=col++;
    while(col<g->width){bit=row*g->width+col;if(!(g->bits[bit/8]&(0x80u>>(bit%8))))break;col++;}
    int left=((pen+((int)g->left+(int)first)*64)*width+natural/2)/natural;
    int right=((pen+((int)g->left+(int)col)*64)*width+natural/2)/natural;
    if(right>left)home_fill(origin+left,yscale(baseline)+g->top+(int)row,right-left,1,true);
   }
  }
  pen+=g->advance_q6;previous=c;
 }
}
static void home_round(int x,int y,int width,int height,int radius,bool black) {
 /* Scanline raster of the source's rounded rectangle; no fake-gray edges. */
 if(width<=0||height<=0)return;
 if(radius>width/2)radius=width/2;
 if(radius>height/2)radius=height/2;
 for(int row=0;row<height;row++) {
  int cy=row<radius?2*radius-2*row-1:row>=height-radius?2*(row-(height-radius))+1:0;
  int inset=0;
  if(cy)while(inset<radius) {int dx=2*radius-2*inset-1;if(dx*dx+cy*cy<=4*radius*radius)break;inset++;}
  home_fill(xscale(x+inset),yscale(y+row),xscale(width-2*inset),yscale(1),black);
 }
}
static void home_battery(void) {
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

static void home_dial(bool known,unsigned minute) {
 for(unsigned i=0;i<60;i++) {
  const home_tick *t=&home_ticks[i*2+(known&&i<=minute?1:0)];
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
