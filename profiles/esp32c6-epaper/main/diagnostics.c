#include "diagnostics.h"
#include <string.h>
#include "diagnostic_font.h"
bool diagnostic_due(diagnostic_display *d,int64_t now,bool failing,bool fatal){
 if(!failing){d->failing=false;d->shown=false;return false;}
 if(!d->failing){d->failing=true;d->failed_since=now;}
 return (!d->shown&&(fatal||now-d->failed_since>=60000))||
        (d->shown&&now-d->refreshed_at>=600000);
}
void diagnostic_shown(diagnostic_display *d,int64_t now){d->shown=true;d->refreshed_at=now;}
void diagnostic_render(uint8_t *out,const char *const *lines,unsigned count){
 memset(out,0,DIAGNOSTIC_BYTES);
 for(unsigned row=0;row<count&&row<16;row++){
  const unsigned char *s=(const unsigned char *)lines[row];
  for(unsigned col=0;s&&s[col]&&col<48;col++){
   unsigned c=s[col];if(c<32||c>126)c='?';
   for(unsigned y=0;y<12;y++)for(unsigned x=0;x<8;x++){
    if(!(diagnostic_font[c-32][y]&(0x80u>>x)))continue;
    for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++){
     unsigned px=16+col*16+x*2+dx,py=12+row*28+y*2+dy;
     out[py*100+px/8]|=(uint8_t)(0x80u>>(px%8));
    }
   }
  }
 }
}
