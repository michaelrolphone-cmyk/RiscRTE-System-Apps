#include "PortableQuickActions.h"
#include <assert.h>
#include <stdio.h>
static pqa_state sheet(bool audio) {
 pqa_state s;pqa_init(&s);s.paper=true;s.audio_controls=audio;s.tone_controls=true;
 s.tone_valid=true;s.brightness_valid=true;s.volume_valid=true;s.clean_refresh_valid=true;
 s.brightness=40;s.tone=50;s.position_q8=s.target_q8=PQA_OPEN_Q8;return s;
}
int main(void) {
 for(unsigned audio=0;audio<2;audio++) {
  pqa_state s=sheet(audio!=0);int y=pqa_paper_slider_y(&s,2)+15;
  assert(pqa_paper_slider_y(&s,0)==(audio?114:126));
  assert(pqa_paper_slider_y(&s,1)==(audio?180:0));
  assert(pqa_paper_slider_y(&s,2)==(audio?246:220));
  int x,tile_y;assert(pqa_paper_tile(&s,8,&x,&tile_y));assert(tile_y+92<714);
  for(unsigned repetition=0;repetition<100;repetition++) {
   for(unsigned warm=0;warm<=100;warm+=10) {
    s=sheet(audio!=0);x=88+(int)warm*276/100;
    assert(pqa_input(&s,10,true,1,1,x,y,true));assert(s.gesture==PQA_TONE_DRAG&&s.tone==warm);
    assert(pqa_input(&s,20,true,0,0,0,0,true));
    assert(pqa_take_action(&s)==PQA_TONE_COMMIT&&s.action_tone==warm&&s.brightness==40);
   }
   s=sheet(audio!=0);assert(pqa_input(&s,10,true,1,1,364,y,true));
   assert(pqa_take_action(&s)==PQA_TONE_PREVIEW&&s.action_tone==100);
   assert(pqa_input(&s,20,true,2,1,364,y,true));
   assert(s.tone==50&&s.neutral_gate&&pqa_take_action(&s)==PQA_TONE_PREVIEW&&s.action_tone==50);
   assert(pqa_input(&s,30,true,0,0,0,0,true));
   assert(pqa_input(&s,40,true,1,2,88,y,true));assert(pqa_take_action(&s)==PQA_TONE_PREVIEW);
   pqa_close(&s);assert(s.tone==50&&s.action_tone==50&&pqa_take_action(&s)==PQA_TONE_PREVIEW);
  }
  s=sheet(audio!=0);s.tone_controls=false;assert(!pqa_paper_slider_y(&s,2));
  s=sheet(audio!=0);s.tone_valid=false;assert(pqa_input(&s,10,true,1,1,364,y,true));
  assert(s.gesture!=PQA_TONE_DRAG&&!pqa_take_action(&s));
 }
 puts("Shared tone input: audio/no-audio geometry, 100 repeated ratios, cancellation and capability gating PASS");
}
