/* Native-test adapter only. Find the rendered control, then the test still
 * injects real DOWN/UP events; it never calls the application action handler.
 * Not compiled into firmware. Off-screen controls require real swipe events. */
#include "../../Services/scene_host/host.c"
int telemetry_test_target(unsigned action_id,int value,int*x,int*y){
 if(!active||pending||rasterizing||!view_current())return 0;
 for(unsigned i=0;i<visible.count;i++){
  const hit*h=&visible.items[i];int index=node_index(&document,h->node);
  unsigned a=index>=0?document.nodes[index].action:0;
  if(h->kind==HIT_BACK){int r=route_index(&document,current_route());a=document.flags&3u?document.cancel_action:r>=0?document.routes[r].back_action:0;}
  if(a!=action_id||(h->kind==HIT_VALUE&&h->value!=value))continue;
  if(h->h<12||h->w<12)continue;
  *x=h->x+h->w/2;*y=h->y+h->h/2;return 1;
 }
 for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==action_id){
  int xx,yy,ww,hh;nc_layout(i,&xx,&yy,&ww,&hh);yy-=model->scroll;
  if(yy<nc_header_height())return -1;
  if(yy+hh>(int)height-tm_footer())return 2;
 }
 return 0;
}
/* Deliberately test-only inspection of copied state, not application inputs. */
uint32_t telemetry_test_digest(void){const unsigned char*p=(const void*)&model->telemetry;uint32_t h=2166136261u;for(unsigned i=0;i<sizeof(model->telemetry);i++)h=(h^p[i])*16777619u;return h;}
/* Orientation test hook: renderer and touch mapping are inverse transforms. */

#include <assert.h>
void telemetry_test_orientation(unsigned expected_w,unsigned expected_h,unsigned rotation){
 assert(width==expected_w&&height==expected_h&&session_display_rotation==rotation);
 for(unsigned y=0;y<height;y+=37)for(unsigned x=0;x<width;x+=41){
  unsigned px=x,py=y;
  switch(rotation){case 90:px=info.width-1-y;py=x;break;case 180:px=info.width-1-x;py=info.height-1-y;break;case 270:px=y;py=info.height-1-x;break;default:break;}
  int tx,ty;assert(touch_point((uint16_t)px,(uint16_t)py,&tx,&ty));assert(tx==(int)x&&ty==(int)y);
 }
}
