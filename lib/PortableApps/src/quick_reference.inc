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
/* Clip the source rows before scanning bitmaps or calculating rounded edges.
 * The final fill guard still owns exact coverage at scaled/rotated sizes. */
static void qr_rows(int origin,int *first,int *end) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_replaying)return;
#ifdef PORTABLE_PAPER_TRANSITIONS
 origin+=qp_offset;
#endif
 if(height()==800){raster_rows(origin,first,end);return;}
 int top=raster_band_top*800/height()-origin-2;
 int bottom=(raster_band_bottom+1)*800/height()-origin+2;
 if(top>*first)*first=top<*end?top:*end;
 if(bottom<*end)*end=bottom>*first?bottom:*first;
#else
 (void)origin;(void)first;(void)end;
#endif
}
static void qr_text(const quick_ref_font *font,int x,int baseline,const char *text,int tracking_q6,int align,int target,bool black) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 const quick_ref_font *fonts[]={&QUICK_REF_TITLE,&QUICK_REF_TIME,&QUICK_REF_NUMBER,&QUICK_REF_LABEL,&QUICK_REF_TILE,&QUICK_REF_SMALL,&QUICK_REF_BATTERY};
 for(unsigned f=0;f<sizeof(fonts)/sizeof(*fonts);f++)if(font==fonts[f]){
  if(raster_record(RS_QR_TEXT,(int32_t[8]){(int32_t)f,x,baseline,tracking_q6,align,target,black},text))return;
  break;
 }
#endif
 if(!align)for(unsigned i=0;i<sizeof(quick_ref_words)/sizeof(*quick_ref_words);i++) {
  const quick_ref_word *word=&quick_ref_words[i];
  if(word->font!=font||word->tracking!=tracking_q6||word->target!=target||strcmp(word->text,text))continue;
  int row_first=0,row_end=(int)word->height;qr_rows(baseline+word->top,&row_first,&row_end);
  for(int row=row_first;row<row_end;row++)for(unsigned col=0;col<word->width;) {
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
  int row_first=0,row_end=(int)g->height;qr_rows(baseline+g->top,&row_first,&row_end);
  for(int row=row_first;row<row_end;row++)for(unsigned col=0;col<g->width;){
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
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_QR_ROUND,(int32_t[8]){x,y,w,h,radius,black},NULL))return;
#endif
 int row_first=0,row_end=h;qr_rows(y,&row_first,&row_end);
 for(int row=row_first;row<row_end;row++) {
  int cy=row<radius?2*radius-2*row-1:row>=h-radius?2*(row-(h-radius))+1:0,inset=0;
  if(cy)while(inset<radius){int dx=2*radius-2*inset-1;if(dx*dx+cy*cy<=4*radius*radius)break;inset++;}
  qp_rect(x+inset,y+row,w-2*inset,1,black);
 }
}
static void qr_icon(unsigned icon,int x,int y,bool black) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_QR_ICON,(int32_t[8]){(int32_t)icon,x,y,black},NULL))return;
#endif
 const uint8_t *bits=quick_ref_icons[icon];
 int row_first=0,row_end=40;qr_rows(y,&row_first,&row_end);
 for(int row=row_first;row<row_end;row++)for(unsigned col=0;col<40;){
  unsigned bit=row*40+col;if(!(bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
  unsigned first=col++;while(col<40){bit=row*40+col;if(!(bits[bit/8]&(128u>>(bit%8))))break;col++;}
  qp_rect(x+(int)first,y+(int)row,(int)(col-first),1,black);
 }
}
static void qr_slider(int y,const char *label,unsigned icon,unsigned value,bool valid) {
 const bool compact=pqa_paper_compact_sliders(&quick.ui);const int height=compact?30:44;
 qr_text(&QUICK_REF_LABEL,32,y-10,label,4*64,0,0,true);qr_icon(icon,32,y+(compact?-5:2),true);
 for(unsigned i=0;i<10;i++){int x=88+(int)i*28;qr_round(x-2,y-2,28,height+4,5,true);
  if(!valid||i*10>=value)qr_round(x+2,y+2,20,height-4,1,false);}
 char percent[8];if(valid)snprintf(percent,sizeof(percent),"%u%%",value);else strcpy(percent,"--%");
 qr_text(&QUICK_REF_NUMBER,448,y+(compact?26:33),percent,0,1,0,true);
}
static void quick_paper_paint(const char *time,bool battery_valid,unsigned battery) {
 qr_text(&QUICK_REF_TITLE,32,58,"QUICK ACTIONS",2*64,0,260,true);
 qr_text(&QUICK_REF_TIME,448,58,time,0,1,0,true);qp_rect(32,78,416,4,true);
#ifdef PORTABLE_PAPER_TRANSITIONS
 bool light_known=quick.ui.brightness_valid&&quick.ui.applied_brightness_valid;
 unsigned light=quick.ui.applied_brightness;
#else
 bool light_known=quick.ui.brightness_valid;unsigned light=quick.ui.brightness;
#endif
 qr_slider(pqa_paper_slider_y(&quick.ui,0),"FRONTLIGHT",6,light,light_known);
 if(quick.ui.audio_controls)qr_slider(pqa_paper_slider_y(&quick.ui,1),"VOLUME",7,quick.ui.volume,quick.ui.volume_valid);
 int divider=quick.ui.audio_controls?283:187;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(quick.ui.tone_controls){
  qr_slider(pqa_paper_slider_y(&quick.ui,2),"COOL / WARM",6,quick.ui.applied_tone,quick.ui.tone_valid&&quick.ui.applied_tone_valid);
  divider=quick.ui.audio_controls?287:283;
 }
#endif
 qp_rect(32,divider,416,3,true);
 static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI","BLUETOOTH","FRONTLIGHT"};
 for(unsigned i=0;i<10;i++) {
  int x,y;if(!pqa_paper_tile(&quick.ui,i,&x,&y))continue;
  bool enabled=i==0?quick.ui.volume_valid:i==1?quick.ui.dnd_valid:i==5?light_known:i==9?quick.ui.contexts_valid:i>=7?true:quick.ui.radios_valid;
  bool on=enabled&&(i==0?!quick.ui.volume:i==1?quick.ui.dnd_enabled:i==2?quick.ui.airplane:i==3?quick.ui.wifi_enabled:i==4?quick.ui.bluetooth_enabled:i==5?light!=0:i==9?quick.ui.contexts_enabled:false);
  qr_round(x-2,y-2,208,96,20,true);if(!on)qr_round(x+2,y+2,200,88,16,false);
  if(i==9){qr_text(&QUICK_REF_TILE,x+102,y+32,"CONTEXT",32,2,0,!on);qr_text(&QUICK_REF_TILE,x+102,y+55,"DETECTION",32,2,0,!on);qr_text(&QUICK_REF_SMALL,x+102,y+78,enabled?(on?"ON":"OFF"):"UNAVAILABLE",0,2,0,!on);}
  else if(i==7){qr_text(&QUICK_REF_TILE,x+102,y+42,"USB",32,2,0,!on);qr_text(&QUICK_REF_TILE,x+102,y+66,"TRANSFER",32,2,0,!on);}
  else {
   qr_icon(i==8?8:i,x+22,y+26,!on);
   if(i==8){qr_text(&QUICK_REF_TILE,x+78,y+44,"CLEAN",32,0,0,!on);qr_text(&QUICK_REF_TILE,x+78,y+68,"REFRESH",32,0,0,!on);}
   else {qr_text(&QUICK_REF_TILE,x+78,y+(enabled?54:42),labels[i],32,0,0,!on);
    if(!enabled)qr_text(&QUICK_REF_SMALL,x+78,y+65,"UNAVAILABLE",0,0,0,true);}
  }
 }
 qr_text(&QUICK_REF_SMALL,240,731,quick.ui.error_flags?"CHANGE UNCONFIRMED":"SWIPE UP TO CLOSE",0,2,0,true);
 qr_text(&QUICK_REF_BATTERY,32,764,"BATT",4*64,0,0,true);
 for(unsigned i=0;i<10;i++){int x=104+(int)i*23;qr_round(x-2,748,22,18,4,true);
  if(!battery_valid||i*10>=battery)qr_round(x+2,752,14,10,1,false);}
 char percent[8];if(battery_valid)snprintf(percent,sizeof(percent),"%u%%",battery);else strcpy(percent,"--%");
 qr_text(&QUICK_REF_NUMBER,448,764,percent,0,1,0,true);
}

#if defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_PAPER_TRANSITIONS)
static portable_raster_layer *quick_sheet_layer;
static pqa_state quick_sheet_state;
static char quick_sheet_time[6];
static bool quick_sheet_battery_valid;
static unsigned quick_sheet_battery;
static bool quick_sheet_changed(void) {
 pqa_state prior=quick_sheet_state;
 prior.position_q8=quick.ui.position_q8;prior.target_q8=quick.ui.target_q8;
 return quick_paper_changed(&prior,&quick.ui) ||
  prior.audio_controls!=quick.ui.audio_controls || prior.clean_refresh_valid!=quick.ui.clean_refresh_valid;
}
#endif
static void quick_paper_render(const char *time,bool battery_valid,unsigned battery) {
#ifdef PORTABLE_PAPER_TRANSITIONS
 qp_offset=(quick.ui.position_q8*800/PQA_OPEN_Q8)-800;
 if(!quick_copy_background()){failed=true;return;}

#else
 pp_begin();
#endif

#if defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_PAPER_TRANSITIONS)
 /* Exact reference geometry; other scaled profiles retain their rounding. */
 if(width()==480&&height()==800&&raster_recording) {
  bool changed=!portable_layer_valid(quick_sheet_layer)||quick_sheet_changed()||
   strcmp(quick_sheet_time,time)||quick_sheet_battery_valid!=battery_valid||quick_sheet_battery!=battery;
  if(changed) {
   int offset=qp_offset;
   if(portable_layer_begin(&quick_sheet_layer)) {
    qp_offset=0;quick_paper_paint(time,battery_valid,battery);portable_raster_layer_end();qp_offset=offset;
    quick_sheet_state=quick.ui;memcpy(quick_sheet_time,time,6);quick_sheet_battery_valid=battery_valid;quick_sheet_battery=battery;
   } else portable_layer_release(&quick_sheet_layer);
  }
  if(portable_layer_valid(quick_sheet_layer)) {
   portable_raster_layer_blit(quick_sheet_layer,0,0,width(),height(),0,qp_offset);return;
  }
 }
#endif
#ifdef PORTABLE_PAPER_TRANSITIONS
 fill(0,qp_y(0),width(),height(),0xffff);
#endif
 quick_paper_paint(time,battery_valid,battery);
}
