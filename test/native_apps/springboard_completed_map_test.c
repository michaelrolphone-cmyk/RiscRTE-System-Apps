#include <assert.h>
#include <stdio.h>
#define PORTABLE_NOVA_SLICED_FRAMES
#define PORTABLE_FRAME_RECEIPTS
#define PORTABLE_RASTER_SNAPSHOT
#define PORTABLE_NATIVE_CUSTODY_FENCE
#include "../../Apps/springboard.c"
static bool idle=true,broken;
static unsigned paints;static uint64_t requested,completed;
uint64_t portable_frame_requested_id(void){return requested;}
uint64_t portable_frame_completed_id(void){return completed;}
static int32_t dimension(void){return 466;}
bool portable_paper_frame_idle(void){return idle&&!broken;}
bool portable_paper_frame_ready(void){return idle&&!broken;}
bool portable_paper_frame_drain(void){return !broken;}
const t5_app_api_v1*t5_app_get_api(uint32_t v){(void)v;return NULL;}
const t5_video_api_v1*t5_video_get_api(uint32_t v){(void)v;return NULL;}
const t5_storage_api_v1*t5_storage_get_api(uint32_t v){(void)v;return NULL;}
static void begin(void){paints++;}
static void circle(int x,int y,int r,uint32_t c,uint8_t a){(void)x;(void)y;(void)r;(void)c;(void)a;}
static bool icon(int x,int y,int n,const char*s,uint8_t a){(void)x;(void)y;(void)n;(void)s;(void)a;return true;}
static void caption(int y,const char*s,bool c,uint32_t rgb){(void)y;(void)s;(void)c;(void)rgb;}
static bool get(unsigned i,t5_app_manifest_t*out){(void)i;memset(out,0,sizeof(*out));strcpy(out->display_name,"App");strcpy(out->icon,"solid:f017");return true;}
static void present(bool full){(void)full;idle=false;requested++;}
static const springboard_presentation view={.struct_size=sizeof(view),.begin=begin,.circle=circle,.icon=icon,.caption=caption};
static const t5_app_api_v1 fixture={.struct_size=sizeof(fixture),.screen_width=dimension,.screen_height=dimension,.installed_apps_get=get,.present=present,.frame_ready=portable_paper_frame_ready};
int main(void){
 api=&fixture;count=19;nova_state s={.view=&view,.initial=true,.pending=-1};nova_layout(&s);
 assert(nova_hit(&s,233,233)==-1);nova_render(&s);assert(paints==1&&s.image_pending&&!s.visible_valid);
 s.motion.x=90;s.motion.y=30;nova_render(&s);assert(paints==1);nova_completed(&s);assert(!s.visible_valid);
 idle=true;completed=requested;nova_completed(&s);assert(s.visible_valid&&s.visible_x==0&&s.visible_y==0&&!s.image_pending);
 /* Model motion is ahead; the user still sees the completed old image. */
 assert(nova_hit(&s,233,233)==0);
 nova_render(&s);assert(paints==2&&s.image_pending&&s.visible_x==0);
 broken=true;idle=true;completed=requested;nova_completed(&s);assert(s.image_pending&&s.visible_x==0);
 broken=false;nova_completed(&s);assert(!s.image_pending&&s.visible_x==1440&&s.visible_y==480);
 s.first=19;count=38;nova_layout(&s);assert(nova_hit(&s,233,233)==-1);
 nova_render(&s);assert(s.image_pending);nova_cancel(&s);assert(s.pending==-1&&!s.down);
 idle=true;completed=requested;nova_completed(&s);assert(s.visible_first==19&&s.visible_n==19);
 s.motion.x=45;s.motion.y=20;nova_render(&s);assert(s.image_pending);
 int old_x=s.visible_x;uint64_t abandoned=requested;idle=true;completed=abandoned+1;
 nova_completed(&s);assert(!s.image_pending&&s.visible_x==old_x&&s.dirty);
 for(unsigned hit=0;hit<7;hit++){
  nova_state q={.view=&view,.initial=true,.pending=-1};count=19;nova_layout(&q);
  q.visible_valid=true;q.visible_n=q.n;q.motion.x=100;q.motion.y=-50;
  int x=233+nova_pixels(q.points[hit].x)/16,y=233+nova_pixels(q.points[hit].y)/16;
  springboard_contact c={.valid=true,.began=true,.down=true,.tap_eligible=true,.x=x,.y=y};
  nova_contact_input(&q,&c,20);c.began=c.down=false;c.released=true;nova_contact_input(&q,&c,20);
  assert(q.pending==(int)hit);nova_cancel(&q);assert(q.pending<0);
 }
 puts("Completed-map: capture/busy/failure/page/tap/cancel preserved (7 first-ring identities)");
}
