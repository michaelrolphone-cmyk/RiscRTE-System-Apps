/* Same ordered reducer and Quick Controls router, with exact native coordinates. */
#define PORTABLE_QUICK_FIXTURE_MAIN inherited_quick_main
#include "quick_adapter_test.c"
static unsigned px(unsigned logical){return (logical*SETTINGS_FIXTURE_WIDTH+239u)/240u;}
static portable_touch_sample take(void){portable_touch_sample sample;input_take_native(&sample);assert(!failed);return sample;}
static void position(portable_touch_sample s,unsigned x,unsigned y){assert(s.valid&&s.x==px(x)&&s.y==px(y));}
int main(void) {
 setup();(void)take();
 /* A top-edge horizontal gesture becomes replay, with this MOVE's physical
  * coordinates and the existing began flag, never Quick's logical start. */
 unsigned at=polls+1;tap(at,150,10);tap(at+1,180,10);
 portable_touch_sample s=take();assert(!s.valid&&quick.ui.route==PQA_RESERVED);
 s=take();position(s,180,10);assert(s.began&&s.down&&quick.ui.route==PQA_REPLAY);
 assert(input_sample.x==180 && input_sample.y==10);
 s=take();position(s,180,10);assert(s.released);
 assert(s.moved);(void)take();
 /* A reserved tap is replayed on release at its original native location. */
 at=polls+1;tap(at,150,10);s=take();assert(!s.valid);
 s=take();position(s,150,10);assert(s.began&&s.released&&!s.down&&!s.moved);
 (void)take();
 /* Reset cancels the inherited contact; stale native x/y cannot turn the
  * invalid cancellation into a new valid contact or preserve a pending edge. */
 at=polls+1;tap(at,180,140);s=take();position(s,180,140);assert(s.began);
 portable_touch_reset(&touch);input_pending=false;s=take();assert((!s.valid||s.cancelled)&&!s.began&&!s.released);
 (void)take();
 /* Open and dismiss the real Controls modal, then accept a new native drag. */
 unsigned end=polls+160;opening(polls+3);tap(polls+80,120,222);drain_to(end);
 assert(!pqa_visible(&quick.ui)&&!quick_modal);
 at=polls+1;tap(at,180,140);tap(at+1,190,150);
 s=take();position(s,180,140);assert(s.began&&s.down);
 s=take();position(s,190,150);assert(s.down&&s.moved&&!s.began);
 s=take();position(s,190,150);assert(s.released&&s.moved);
 /* Generic Springboard fallback still routes Controls in logical units, but
  * a top-edge replay must restore the raw origin before physical deltas. At
  * 410px this 20-logical-pixel drag is 34 physical pixels, below Back's36. */
 sv_active=false;nova_mode=false;springboard_native_input=true;
 at=polls+1;tap(at,150,10);tap(at+1,170,10);
 t5_app_input_t in={0};assert(poll_input(&in,8)&&!in.buttons&&!in.tapped);
 assert(poll_input(&in,8)&&!in.buttons&&!in.tapped);
 assert(nu_start_x==(int)px(150)&&nu_start_y==(int)px(10));
 assert(poll_input(&in,8)&&!in.buttons&&!in.tapped&&!in.exit_requested);
 cleanup();puts("Native input: replay/release, reset, Controls dismissal, native drag and fallback replay origin PASS");return 0;
}
