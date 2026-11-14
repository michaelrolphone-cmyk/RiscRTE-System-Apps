#define main historical_main
#include "quick_native_test.c"
#undef main
static pqa_state safe_make(unsigned w,unsigned h,unsigned x,unsigned y,unsigned cw,unsigned ch) {
 pqa_state s;pqa_init(&s);assert(pqa_set_dimensions(&s,w,h));
 assert(pqa_set_safe_content(&s,x,y,cw,ch));pqa_set_levels(&s,true,40,true,50);
 s.dnd_valid=true;s.radio_controls=true;s.wifi_valid=true;s.contexts_controls=true;
 s.contexts_valid=true;s.position_q8=s.target_q8=pqa_open_q8(&s);return s;
}
static void inside(pqa_rect r,unsigned x,unsigned y,unsigned w,unsigned h) {
 assert(r.w>0&&r.h>0&&r.x>=(int)x&&r.y>=(int)y&&r.x+r.w<=(int)(x+w)&&r.y+r.h<=(int)(y+h));
}
static void matrix(void) {
 for(unsigned n=0;n<257;n++) {
  unsigned cw=240+n%137,ch=240+n%181,x=n%71,y=n%79;
  pqa_state s=safe_make(cw+x+43,ch+y+29,x,y,cw,ch);pqa_layout l=pqa_layout_for(&s);
  for(int i=0;i<6;i++)inside(l.tiles[i],x,y,cw,ch);
  inside(l.contexts,x,y,cw,ch);inside(l.handle,x,y,cw,ch);
  for(int i=0;i<2;i++)inside(l.slider_hit[i],x,y,cw,ch);
  tap(&s,l.tiles[3]);assert(pqa_take_action(&s)==PQA_WIFI);
  tap(&s,l.tiles[4]);assert(!pqa_take_action(&s));
  tap(&s,l.tiles[2]);assert(!pqa_take_action(&s));
  tap(&s,l.contexts);assert(!pqa_take_action(&s));
  s.bluetooth_valid=s.radios_valid=s.contexts_available=true;
  tap(&s,l.tiles[4]);assert(pqa_take_action(&s)==PQA_BLUETOOTH);
  tap(&s,l.tiles[2]);assert(pqa_take_action(&s)==PQA_AIRPLANE);
  tap(&s,l.contexts);assert(pqa_take_action(&s)==PQA_CONTEXTS);
  down(&s,200,l.slider_left,l.slider_y[0]);assert(s.gesture==PQA_BRIGHTNESS_DRAG);pqa_take_action(&s);
  assert(pqa_input(&s,210,false,0,0,0,0,true));assert(s.neutral_gate);pqa_take_action(&s);up(&s,220);
  down(&s,230,l.tiles[1].x+4,l.tiles[1].y+4);assert(pqa_input(&s,240,true,1,72,0,0,true));up(&s,250);assert(!pqa_take_action(&s));
  s.position_q8=s.target_q8=0;s.gesture=PQA_IDLE;
  down(&s,260,(int)pqa_width(&s)/2,0);
  assert(!pqa_input(&s,270,true,0,71,0,0,true)&&s.route==PQA_REPLAY);
  assert(!pqa_input(&s,280,true,1,71,0,(int)pqa_height(&s)-1,true));
  assert(!pqa_input(&s,290,true,1,71,0,0,true));assert(!s.target_q8);(void)pqa_input(&s,300,true,0,71,0,0,true);
  down(&s,100,(int)pqa_width(&s)/2,0);assert(s.route==PQA_RESERVED);
  down(&s,120,(int)pqa_width(&s)/2,(int)pqa_height(&s)-1);up(&s,140);assert(s.target_q8==pqa_open_q8(&s));
 }
}
static void captures(const char *dir) {
 unsigned w=466,h=466,stride=w*2+6,size=stride*h;
 uint8_t *a=malloc(size+32),*b=malloc(size+32);assert(a&&b);
 risc_display_surface_v1 f={1,a+16,w,h,stride,size,RISC_DISPLAY_FORMAT_RGB565},g=f;g.pixels=b+16;
 for(int phase=0;phase<9;phase++) {
  pqa_state s=safe_make(w,h,69,69,328,328);
  if(phase==1)s.bluetooth_valid=s.radios_valid=s.contexts_available=true;
  if(phase==2)s.error_flags=PQA_ERROR_SAVE;
  if(phase==3)s.position_q8/=2;
  if(phase==4)s.torch=true;
  if(phase==5)s.wifi_valid=false;
  if(phase==6){s.save_pending=true;s.error_flags=PQA_ERROR_SAVE;}
  if(phase==7)s.gesture=PQA_BRIGHTNESS_DRAG;
  if(phase==8)s.gesture=PQA_VOLUME_DRAG;
  memset(a,0,size+32);memset(b,0,size+32);
  assert(pqa_render(&f,&s,"12:34",true,74));
  for(unsigned y=0;y<h;y++)assert(pqa_render_rows(&g,&s,"12:34",true,74,y,1));
  assert(!memcmp(a,b,size+32));
  for(unsigned i=0;i<16;i++)assert(!a[i]&&!a[size+16+i]);
  for(unsigned y=0;y<h;y++)for(unsigned x=w*2;x<stride;x++)assert(!a[16+y*stride+x]);
  if(phase!=3&&phase!=4)for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
   if(x<69||x>=397||y<69||y>=397)assert(!a[16+y*stride+2*x]&&!a[17+y*stride+2*x]);
  }
  if(dir){char path[512];snprintf(path,sizeof(path),"%s/quick-safe-%d.ppm",dir,phase);save(path,&f);}
 }
 free(a);free(b);
}
static void pending_input(void){
 for(unsigned kind=0;kind<6;kind++){
  pqa_state s=safe_make(466,466,69,69,328,328);
  s.save_pending=true;s.pending=PQA_DND;s.action_dnd=true;s.action_brightness=37;s.action_volume=19;
  uint32_t pending=s.pending;int position=s.position_q8,target=s.target_q8;
  if(kind<3)assert(pqa_input(&s,100,kind!=2,kind==1?1:0,9,200,200,true));
  if(kind==3)assert(pqa_input(&s,100,true,2,9,200,200,true));
  if(kind==4){pqa_cancel_input(&s);}if(kind==5){pqa_cancel(&s);}
  assert(s.save_pending&&s.save_cancelled==(kind!=0)&&s.pending==pending&&s.action_dnd&&s.action_brightness==37&&s.action_volume==19&&s.position_q8==position&&s.target_q8==target);
 }
}
int main(int argc,char **argv) {
 pqa_state s;pqa_init(&s);assert(!pqa_set_safe_content(&s,0,0,240,240));assert(pqa_set_dimensions(&s,466,466));
 assert(!pqa_set_safe_content(&s,69,69,398,328));assert(!pqa_set_safe_content(&s,69,69,239,328));
 assert(pqa_set_safe_content(&s,69,69,328,328));s.position_q8=1;assert(!pqa_set_safe_content(&s,0,0,466,466));
 pending_input();matrix();captures(argc>1?argv[1]:NULL);puts("Quick safe-content: 257 layouts, independent capabilities, screen-edge gesture, sliced raster and guard checks passed");return 0;
}
