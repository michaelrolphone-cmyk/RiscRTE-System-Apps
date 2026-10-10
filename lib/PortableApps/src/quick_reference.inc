/* Host-owned paper sheet from the supplied refined Quick Actions reference.
 * All coordinates use the same 480x800 interaction model as quick_actions.c. */
typedef struct {uint8_t width,height;int8_t left,top;uint16_t advance_q6;const uint8_t *bits;} quick_ref_glyph;
typedef struct {uint8_t first,second;int16_t adjust_q6;} quick_ref_kern;
typedef struct {const quick_ref_glyph *glyphs;const quick_ref_kern *kern;unsigned kern_count;} quick_ref_font;
typedef struct {const quick_ref_font *font;const char *text;int tracking,target;unsigned width,height;int left,top;const uint8_t *bits;} quick_ref_word;
#include "../quick_fonts/reference.inc"
static int qr_kern(const quick_ref_font *font,unsigned first,unsigned second) {
 unsigned key=first*128u+second,lo=0,hi=font->kern_count;
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;const quick_ref_kern *k=&font->kern[mid];
  if(k->first*128u+k->second<key)lo=mid+1;else hi=mid;}
 return lo<font->kern_count&&font->kern[lo].first==first&&font->kern[lo].second==second?font->kern[lo].adjust_q6:0;
}
static unsigned qr_char(unsigned c){return c>=32&&c<=126?c:'?';}
static void qr_text(const quick_ref_font *font,int x,int baseline,const char *text,int tracking_q6,int align,int target,bool black) {
 if(!align)for(unsigned i=0;i<sizeof(quick_ref_words)/sizeof(*quick_ref_words);i++) {
  const quick_ref_word *word=&quick_ref_words[i];
  if(word->font!=font||word->tracking!=tracking_q6||word->target!=target||strcmp(word->text,text))continue;
  for(unsigned row=0;row<word->height;row++)for(unsigned col=0;col<word->width;) {
   unsigned bit=row*word->width+col;
   if(!(word->bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
   unsigned first=col++;while(col<word->width){bit=row*word->width+col;if(!(word->bits[bit/8]&(128u>>(bit%8))))break;col++;}
   qp_rect(x+word->left+(int)first,baseline+word->top+(int)row,(int)(col-first),1,black);
  }
  return;
 }
 int natural=0;unsigned previous=0;
 for(unsigned i=0;i<64&&text[i];i++){unsigned c=qr_char((unsigned char)text[i]);
  if(previous)natural+=qr_kern(font,previous,c)+tracking_q6;
  natural+=font->glyphs[c-32].advance_q6;previous=c;}
 if(natural<=0)return;
 int width=target?target:(natural+32)/64,origin=x-(align==1?width:align==2?width/2:0),pen=0;previous=0;
 for(unsigned i=0;i<64&&text[i];i++){unsigned c=qr_char((unsigned char)text[i]);const quick_ref_glyph *g=&font->glyphs[c-32];
  if(previous)pen+=qr_kern(font,previous,c)+tracking_q6;
  for(unsigned row=0;row<g->height;row++)for(unsigned col=0;col<g->width;){
   unsigned bit=row*g->width+col;if(!(g->bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
   unsigned first=col++;while(col<g->width){bit=row*g->width+col;if(!(g->bits[bit/8]&(128u>>(bit%8))))break;col++;}
   int left=((pen+((int)g->left+(int)first)*64)*width+natural/2)/natural;
   int right=((pen+((int)g->left+(int)col)*64)*width+natural/2)/natural;
   if(right>left)qp_rect(origin+left,baseline+g->top+(int)row,right-left,1,black);
  }
  pen+=g->advance_q6;previous=c;
 }
}
static void qr_round(int x,int y,int w,int h,int radius,bool black) {
 for(int row=0;row<h;row++) {
  int cy=row<radius?2*radius-2*row-1:row>=h-radius?2*(row-(h-radius))+1:0,inset=0;
  if(cy)while(inset<radius){int dx=2*radius-2*inset-1;if(dx*dx+cy*cy<=4*radius*radius)break;inset++;}
  qp_rect(x+inset,y+row,w-2*inset,1,black);
 }
}
static void qr_icon(unsigned icon,int x,int y,bool black) {
 const uint8_t *bits=quick_ref_icons[icon];
 for(unsigned row=0;row<40;row++)for(unsigned col=0;col<40;){
  unsigned bit=row*40+col;if(!(bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
  unsigned first=col++;while(col<40){bit=row*40+col;if(!(bits[bit/8]&(128u>>(bit%8))))break;col++;}
  qp_rect(x+(int)first,y+(int)row,(int)(col-first),1,black);
 }
}
static void qr_slider(int y,const char *label,unsigned icon,unsigned value,bool valid) {
 qr_text(&QUICK_REF_LABEL,32,y-10,label,4*64,0,0,true);qr_icon(icon,32,y+2,true);
 for(unsigned i=0;i<10;i++){int x=88+(int)i*28;qr_round(x-2,y-2,28,48,5,true);
  if(!valid||i*10>=value)qr_round(x+2,y+2,20,40,1,false);}
 char percent[8];if(valid)snprintf(percent,sizeof(percent),"%u%%",value);else strcpy(percent,"--%");
 qr_text(&QUICK_REF_NUMBER,448,y+33,percent,0,1,0,true);
}
static void quick_paper_render(const char *time,bool battery_valid,unsigned battery) {
#ifdef PORTABLE_PAPER_TRANSITIONS
 qp_offset=(quick.ui.position_q8*800/PQA_OPEN_Q8)-800;
 if(!quick_copy_background()){failed=true;return;}
 fill(0,qp_y(0),width(),height(),0xffff);
#else
 pp_begin();
#endif
 qr_text(&QUICK_REF_TITLE,32,58,"QUICK ACTIONS",2*64,0,260,true);
 qr_text(&QUICK_REF_TIME,448,58,time,0,1,0,true);qp_rect(32,78,416,4,true);
#ifdef PORTABLE_PAPER_TRANSITIONS
 bool light_known=quick.ui.brightness_valid&&quick.ui.applied_brightness_valid;
 unsigned light=quick.ui.applied_brightness;
#else
 bool light_known=quick.ui.brightness_valid;unsigned light=quick.ui.brightness;
#endif
 qr_slider(126,"FRONTLIGHT",6,light,light_known);
 if(quick.ui.audio_controls)qr_slider(220,"VOLUME",7,quick.ui.volume,quick.ui.volume_valid);
 qp_rect(32,quick.ui.audio_controls?283:187,416,3,true);
 static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI","BLUETOOTH","FRONTLIGHT"};
 for(unsigned i=0;i<9;i++) {
  int x,y;if(!pqa_paper_tile(&quick.ui,i,&x,&y))continue;
  bool enabled=i==0?quick.ui.volume_valid:i==1?quick.ui.dnd_valid:i==5?light_known:i>=7?true:quick.ui.radios_valid;
  bool on=enabled&&(i==0?!quick.ui.volume:i==1?quick.ui.dnd_enabled:i==2?quick.ui.airplane:i==3?quick.ui.wifi_enabled:i==4?quick.ui.bluetooth_enabled:i==5?light!=0:false);
  qr_round(x-2,y-2,208,96,20,true);if(!on)qr_round(x+2,y+2,200,88,16,false);
  if(i==7){qr_text(&QUICK_REF_TILE,x+102,y+42,"USB",32,2,0,!on);qr_text(&QUICK_REF_TILE,x+102,y+66,"TRANSFER",32,2,0,!on);}
  else {
   qr_icon(i==8?8:i,x+22,y+26,!on);
   if(i==8){qr_text(&QUICK_REF_TILE,x+78,y+44,"CLEAN",32,0,0,!on);qr_text(&QUICK_REF_TILE,x+78,y+68,"REFRESH",32,0,0,!on);}
   else {qr_text(&QUICK_REF_TILE,x+78,y+(enabled?54:42),labels[i],32,0,0,!on);
    if(!enabled)qr_text(&QUICK_REF_SMALL,x+78,y+65,"UNAVAILABLE",0,0,0,true);}
  }
 }
#ifdef PORTABLE_QUICK_USB_TRANSFER
 if(!quick.ui.audio_controls){qr_round(30,516,420,68,20,true);qr_round(34,520,412,60,16,false);
  qr_text(&QUICK_REF_TILE,240,557,"USB SD TRANSFER",32,2,0,true);}
#endif
 qr_text(&QUICK_REF_SMALL,240,731,quick.ui.error_flags?"CHANGE UNCONFIRMED":"SWIPE UP TO CLOSE",0,2,0,true);
 qr_text(&QUICK_REF_BATTERY,32,764,"BATT",4*64,0,0,true);
 for(unsigned i=0;i<10;i++){int x=104+(int)i*23;qr_round(x-2,748,22,18,4,true);
  if(!battery_valid||i*10>=battery)qr_round(x+2,752,14,10,1,false);}
 char percent[8];if(battery_valid)snprintf(percent,sizeof(percent),"%u%%",battery);else strcpy(percent,"--%");
 qr_text(&QUICK_REF_NUMBER,448,764,percent,0,1,0,true);
}
