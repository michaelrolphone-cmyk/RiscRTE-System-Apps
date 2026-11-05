/* Shared width-based transform; the physical device identity is irrelevant.
 * Keep typography square and anchor the existing footer to native height. */
#include "WatchLayout.h"
static watch_layout fb_layout;
static int fb_layout_extra(void){return fb_layout.height>240?(int)fb_layout.height-240:0;}
static int fb_layout_y(int y){return y>=175?y+fb_layout_extra():y;}
static bool fb_layout_touch(int x,int y,int *out_x,int *out_y){
 uint16_t lx,ly;if(x<0||y<0||!watch_layout_touch(&fb_layout,(unsigned)x,(unsigned)y,&lx,&ly))return false;
 int extra=fb_layout_extra();
 if(ly>=175&&ly<175+extra)return false;
 *out_x=lx;*out_y=ly>=175+extra?(int)ly-extra:ly;return true;
}
static void fb_layout_text(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,fb_layout_y(y),w,s,c);}
static void fb_layout_button(int x,int y,int w,int h,const char*s,bool on){portable_nova_button(x,fb_layout_y(y),w,h,s,on);}
static bool fb_layout_wrap(unsigned f,int x,int y,int w,int h,unsigned n,const char*s,uint32_t c){return portable_nova_wrap(f,x,fb_layout_y(y),w,h,n,s,c);}
#define portable_nova_text fb_layout_text
#define portable_nova_button fb_layout_button
#define portable_nova_wrap fb_layout_wrap
