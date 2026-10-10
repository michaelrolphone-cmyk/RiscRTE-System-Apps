#pragma once
#include "../lib/PortableApps/paper_fonts/springboard_header.inc"
/* Reference header only. Grid/paging, input, launch and Home battery ownership
 * are independent. Text uses physical-size font rasters at a shared baseline. */
static const sbh_glyph *sbh_lookup(const sbh_font *font,unsigned ch) {
 for(unsigned i=0;i<font->count;i++){if(font->glyphs[i].code==ch)return &font->glyphs[i];}
 return NULL;
}
static void sbh_text(const sbh_font *font,int x,int baseline,const char *text,int tracking,bool right) {
 int width=0;for(unsigned i=0;text[i];i++){const sbh_glyph *g=sbh_lookup(font,(unsigned char)text[i]);if(g)width+=(int)g->advance+(i?tracking*64:0);}
 int pen=x*64-(right?width:0);
 for(unsigned i=0;text[i];i++) {
  const sbh_glyph *g=sbh_lookup(font,(unsigned char)text[i]);if(!g)continue;if(i)pen+=tracking*64;
  for(unsigned y=0;y<g->height;y++)for(unsigned xg=0;xg<g->width;xg++) {
   unsigned bit=y*g->width+xg;if(g->bits[bit/8]&(128u>>(bit%8)))api->fill_rect((pen+32)/64+g->left+(int)xg,baseline+g->top+(int)y,1,1,true);
  }
  pen+=g->advance;
 }
}
static void springboard_paper_header(const char *time) {
 sbh_text(&SBH_900,paper_x(32),paper_y(58),"APPS",4,false);
 sbh_text(&SBH_700,paper_x(448),paper_y(58),time,0,true);
 api->fill_rect(paper_x(32),paper_y(78),paper_x(416),paper_y(4),true);
}
