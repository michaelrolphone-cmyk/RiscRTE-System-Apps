#include <assert.h>
#include <stdio.h>
#include "../../Apps/springboard.c"
static int32_t dimension(void){return 240;}
const t5_app_api_v1*t5_app_get_api(uint32_t v){(void)v;return NULL;}
const t5_video_api_v1*t5_video_get_api(uint32_t v){(void)v;return NULL;}
const t5_storage_api_v1*t5_storage_get_api(uint32_t v){(void)v;return NULL;}
static const t5_app_api_v1 fixture={.screen_width=dimension,.screen_height=dimension};
static void tap_at(nova_state*s,unsigned hit){
 int x=120+(s->points[hit].x+s->visible_x)/16,y=120+(s->points[hit].y+s->visible_y)/16;
 springboard_contact down={.valid=true,.down=true,.began=true,.tap_eligible=true,.x=x,.y=y};
 springboard_contact up=down;up.down=false;up.began=false;up.released=true;
 nova_contact_input(s,&down,20);nova_contact_input(s,&up,20);
}
int main(void){api=&fixture;count=19;
 for(unsigned i=0;i<19;i++){
  nova_state s={.initial=true,.pending=-1};nova_layout(&s);tap_at(&s,i);
  if(i<=6)assert(s.pending==(int)i);else assert(s.pending<0);
 }
 nova_state s={.initial=true,.pending=-1};nova_layout(&s);
 s.visible_x=-s.points[7].x;s.visible_y=-s.points[7].y;s.motion.x=s.visible_x/16.0f;s.motion.y=s.visible_y/16.0f;
 tap_at(&s,7);assert(s.pending==7);
 nova_state moving={.initial=true,.pending=-1};nova_layout(&moving);
 moving.motion.x=20;moving.motion.y=5;tap_at(&moving,0);assert(moving.pending==0); /* displayed geometry wins */
 nova_state inherited={.initial=true,.pending=-1};nova_layout(&inherited);
 springboard_contact h={.valid=true,.down=true,.began=true,.tap_eligible=false,.x=120,.y=120};
 nova_contact_input(&inherited,&h,20);h.down=false;h.released=true;nova_contact_input(&inherited,&h,20);assert(inherited.pending<0);
 puts("Tap layers: center/first ring open once; far centers only; panned/moving display geometry and inherited release pass");
}
