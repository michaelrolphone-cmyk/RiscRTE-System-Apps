#pragma once
#include "WatchLayout.h"
/* Optional native-height layout for the shared two-row watch views. */
static inline int watch_view_footer_offset(unsigned width,unsigned height){
 watch_layout l;return watch_layout_open(&l,width,height,NULL)&&l.height>240?(int)l.height-240:0;
}
static inline bool watch_view_touch(unsigned width,unsigned height,bool keyboard,int *x,int *y){
 watch_layout l;uint16_t lx,ly;
 if(!x||!y||*x<0||*y<0||!watch_layout_open(&l,width,height,NULL)||!watch_layout_touch(&l,(unsigned)*x,(unsigned)*y,&lx,&ly))return false;
 int extra=keyboard?0:watch_view_footer_offset(width,height);
 if(ly>=175&&ly<175+extra)return false;
 *x=lx;*y=ly>=175+extra?(int)ly-extra:ly;return true;
}
