/* Real gesture and preference session; hardware/storage are strict doubles. */
#define main legacy_quick_session_main
#include "quick_session_test.c"
#undef main

static pqa_session paper_fresh(unsigned value) {
 pqa_session s=fresh();s.ui.paper=true;saved(0,value,1);load(&s);
 assert(pqa_session_sync_brightness(&s,&display_api));
 s.ui.position_q8=s.ui.target_q8=PQA_OPEN_Q8;s.ui.neutral_gate=false;
 return s;
}
static void toggle(pqa_session *s,unsigned now) {
 assert(pqa_input(&s->ui,now,true,1,1,390,100,true));
 assert(pqa_input(&s->ui,now+1,true,0,0,0,0,true));
}
static void toggle_restore(void) {
 pqa_session s=paper_fresh(70);toggle(&s,10);
 assert(s.ui.brightness==0&&s.ui.applied_brightness==70&&hardware==70);
 apply(&s,pqa_take_action(&s.ui));
 assert(s.brightness==0&&hardware==0&&s.ui.applied_brightness==0);
 assert(records[0].bytes[0]==0&&records[4].bytes[0]==70);
 assert(write_order[0]=='4'&&write_order[1]=='0');
 pqa_close(&s.ui);assert(pqa_session_restore(&s,&display_api)&&hardware==0);
 load(&s);assert(s.ui.last_nonzero_brightness==70);
 s.ui.position_q8=s.ui.target_q8=PQA_OPEN_Q8;s.ui.neutral_gate=false;
 toggle(&s,20);apply(&s,pqa_take_action(&s.ui));
 assert(s.brightness==70&&hardware==70&&s.ui.applied_brightness==70);
 /* Fresh invocation reads OFF and its saved level; no implicit preference writes. */
 toggle(&s,30);apply(&s,pqa_take_action(&s.ui));
 pqa_session next;pqa_session_init(&next);next.ui.paper=true;load(&next);
 assert(next.brightness==0&&next.restore_brightness==70);
 assert(pqa_session_sync_brightness(&next,&display_api)&&hardware==0);
 next.ui.torch=true;apply(&next,PQA_TORCH);assert(hardware==100&&next.brightness==0);
 pqa_cancel(&next.ui);apply(&next,pqa_take_action(&next.ui));assert(hardware==0);
}
static void zero_and_rapid(void) {
 pqa_session s=paper_fresh(0);assert(s.ui.brightness_valid&&hardware==0);
 toggle(&s,10);apply(&s,pqa_take_action(&s.ui));assert(hardware==40);
 s=paper_fresh(80);toggle(&s,10);toggle(&s,12);toggle(&s,14);
 assert(s.ui.applied_brightness==80&&hardware==80);
 apply(&s,pqa_take_action(&s.ui));assert(hardware==0&&s.restore_brightness==80);
 toggle(&s,20);apply(&s,pqa_take_action(&s.ui));assert(hardware==80);
 /* The slider turns a dark backlight on at its chosen nonzero level. */
 toggle(&s,30);apply(&s,pqa_take_action(&s.ui));
 assert(pqa_input(&s.ui,40,true,1,1,364,140,true));
 assert(s.ui.brightness==100&&s.ui.applied_brightness==0);
 apply(&s,pqa_take_action(&s.ui));assert(hardware==100&&s.brightness==0);
 pqa_cancel_input(&s.ui);apply(&s,pqa_take_action(&s.ui));assert(hardware==0);
}
static void failures(void) {
 for(unsigned mode=0;mode<6;mode++) {
  pqa_session s=paper_fresh(60);toggle(&s,10);
  if(mode==0)fail_put_key=4;
  if(mode==1)fail_after_put_key=4;
  if(mode==2)fail_put_key=0;
  if(mode==3)fail_after_put_key=0;
  if(mode==4)mismatch_key=0;
  if(mode==5)committed_io_key=0;
  apply(&s,pqa_take_action(&s.ui));
  assert(hardware==(mode==5?0u:60u));assert(s.ui.applied_brightness==hardware);
  assert(!!(s.ui.error_flags&PQA_ERROR_SAVE)==(mode!=5));
 }
 pqa_session s=paper_fresh(60);toggle(&s,10);release_fail=true;bool changed=false;
 assert(!pqa_session_apply(&s,&runtime,&display_api,pqa_take_action(&s.ui),&changed));
 assert(hardware==60&&s.brightness==60&&s.ui.applied_brightness==60&&grants==1);
 s=paper_fresh(60);toggle(&s,10);hardware_fail=true;
 assert(!pqa_session_apply(&s,&runtime,&display_api,pqa_take_action(&s.ui),&changed));
 assert(hardware==60&&!s.ui.applied_brightness_valid&&(s.ui.error_flags&PQA_ERROR_BRIGHTNESS));
 s=paper_fresh(0);saved(4,0,1);load(&s);assert(!s.ui.brightness_valid);
}
static void interrupted(void) {
 pqa_session s=paper_fresh(60);
 pqa_input(&s.ui,10,true,1,1,390,100,true);
 pqa_input(&s.ui,11,true,2,1,390,100,true);
 pqa_input(&s.ui,12,true,0,0,0,0,true);
 assert(!pqa_take_action(&s.ui)&&hardware==60);
 pqa_input(&s.ui,20,true,1,1,390,100,true);pqa_close(&s.ui);
 pqa_input(&s.ui,21,true,0,0,0,0,true);
 assert(!pqa_take_action(&s.ui)&&hardware==60);
 s.ui.position_q8=s.ui.target_q8=PQA_OPEN_Q8/2;s.ui.neutral_gate=false;
 toggle(&s,30);assert(!pqa_take_action(&s.ui)&&hardware==60);
 /* Watch selection retains the legacy hit region and 10% brightness floor. */
 s=fresh();load(&s);s.ui.neutral_gate=false;s.ui.position_q8=s.ui.target_q8=PQA_OPEN_Q8;
 toggle(&s,40);assert(!pqa_take_action(&s.ui)&&hardware==40);
 saved(0,0,1);load(&s);assert(!s.ui.brightness_valid);
}
int main(void) {
 assert(legacy_quick_session_main()==0);
 toggle_restore();zero_and_rapid();failures();interrupted();
 puts("Paper backlight: off/on/restore, zero, rapid inputs, preview rollback, persistence, failed save/set/release and Watch selection passed");return 0;
}
