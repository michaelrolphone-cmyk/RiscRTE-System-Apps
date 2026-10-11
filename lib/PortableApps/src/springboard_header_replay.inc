/* Private linked-app hook: retain a whole immutable header, then replay rows.
 * Original font coordinates, tracking and rounding are unchanged. */
#include "../paper_fonts/springboard_header.inc"
static const sbh_glyph *raster_sbh_lookup(const sbh_font *font,unsigned ch) {
 for(unsigned i=0;i<font->count;i++)if(font->glyphs[i].code==ch)return &font->glyphs[i];
 return NULL;
}
bool portable_springboard_header_text(unsigned face,int x,int baseline,const char *text,int tracking,bool right) {
 if(raster_record(RS_SBH_TEXT,(int32_t[8]){(int32_t)face,x,baseline,tracking,right},text))return true;
 const sbh_font *font=face?&SBH_700:&SBH_900;
 int width=0;
 for(unsigned i=0;text[i];i++){const sbh_glyph*g=raster_sbh_lookup(font,(unsigned char)text[i]);if(g)width+=(int)g->advance+(i?tracking*64:0);}
 int pen=x*64-(right?width:0);
 for(unsigned i=0;text[i];i++) {
  const sbh_glyph*g=raster_sbh_lookup(font,(unsigned char)text[i]);if(!g)continue;if(i)pen+=tracking*64;
  int first=0,end=g->height;raster_rows((int64_t)baseline+g->top,&first,&end);
  for(int y=first;y<end;y++) {
   unsigned run=0;
   while(run<g->width) {
    unsigned bit=(unsigned)y*g->width+run;
    if(!(g->bits[bit/8]&(128u>>(bit%8)))){run++;continue;}
    unsigned start=run++;
    while(run<g->width){bit=(unsigned)y*g->width+run;if(!(g->bits[bit/8]&(128u>>(bit%8))))break;run++;}
    fill((pen+32)/64+g->left+(int)start,baseline+g->top+y,(int)(run-start),1,0);
   }
  }
  pen+=g->advance;
 }
 return true;
}
