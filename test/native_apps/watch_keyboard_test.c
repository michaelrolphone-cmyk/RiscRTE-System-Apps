#include "PortableWatchKeyboard.h"
#include <assert.h>
#include <stdio.h>
static int points_042_hit(int x,int y){
 if(x<0||x>=240||y<0||y>=240)return -1;
 if(x>=12&&x<228&&y>=76&&y<172)return (y-76)/24*8+(x-12)/27;
 if(y>=178&&y<207&&x>=12&&x<228)return 32+(x-12)/72;
 return -1;
}
int main(void){
 assert(PWK_INITIAL_PAGE==2&&PWK_PAGES==3&&PWK_COUNT==35);
 unsigned seen[127]={0},characters=0;
 for(unsigned p=0;p<PWK_PAGES;p++)for(unsigned k=0;k<PWK_CHARACTERS;k++){
  unsigned c=portable_watch_key_character(p,k);if(c){assert(c>=32&&c<=126&&!seen[c]);seen[c]++;characters++;}
 }
 assert(characters==95);for(unsigned c=32;c<=126;c++)assert(seen[c]);
 assert(!portable_watch_key_character(3,0)&&!portable_watch_key_character(0,32));
 for(int y=-1;y<=240;y++)for(int x=-1;x<=240;x++)assert(portable_watch_key_hit(x,y)==points_042_hit(x,y));
 for(unsigned k=0;k<PWK_COUNT;k++){
  portable_watch_key_rect r;assert(portable_watch_key_bounds(k,&r));
  assert(r.x>=12&&r.y>=76&&r.x+r.w<=228&&r.y+r.h<=207);
  assert(portable_watch_key_hit(r.x+r.w/2,r.y+r.h/2)==(int)k);
  if(k<32){assert(r.w==25&&r.h==22);}else assert(r.h==29);
 }
 assert(!portable_watch_key_bounds(35,0));
 puts("Standard Watch keyboard: all 95 ASCII characters, three pages, 35 controls and exhaustive Points0.4.2 hit-map parity passed");return 0;
}
