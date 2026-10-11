/* Frozen per-pixel text from 0.1.61 / 80bf78a, used only as a pixel oracle. */
static void reference_paper_text(int x,int y,int max_width,const char *s,unsigned scale,bool heading,bool black) {
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
    int row_first=0,row_end=g->height;
    for(unsigned j=(unsigned)row_first;j<(unsigned)row_end;j++)for(unsigned i=0;i<g->width;i++) {
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
