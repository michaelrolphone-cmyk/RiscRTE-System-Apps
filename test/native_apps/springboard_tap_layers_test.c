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
 /* Every honeycomb neighbor has a different color, regardless of inventory
  * size, page, duplicate icon IDs, panning, or launch scale. */
 for(unsigned inventory=1;inventory<=57;inventory++)for(unsigned first=0;first<inventory;first+=19){
  count=inventory;nova_state palette={.first=first};nova_layout(&palette);
  for(unsigned i=0;i<palette.n;i++)for(unsigned j=i+1;j<palette.n;j++){
   int dx=palette.points[i].x-palette.points[j].x,dy=palette.points[i].y-palette.points[j].y;
   if(dx*dx+dy*dy<(55*16)*(55*16))assert(nova_color(palette.points[i])!=nova_color(palette.points[j]));
   if(i<7 && j<7)assert(nova_color(palette.points[i])!=nova_color(palette.points[j]));
  }
 }
 count=19;
 for(unsigned i=0;i<19;i++){
  nova_state s={.initial=true,.pending=-1};nova_layout(&s);tap_at(&s,i);
  if(i<=6)assert(s.pending==(int)i);else assert(s.pending<0);
 }
 for(unsigned i=7;i<19;i++){
  nova_state far={.initial=true,.pending=-1};nova_layout(&far);tap_at(&far,i);
  assert(far.pending<0 && far.motion.target==i);
  for(unsigned ms=0;ms<3000 && !far.motion.settled;ms+=20)nova_tick(&far,20);
  assert(far.motion.settled && far.pending<0);
  far.visible_x=(int)(far.motion.x*16);far.visible_y=(int)(far.motion.y*16);
  tap_at(&far,i);assert(far.pending==(int)i);
 }
 nova_state s={.initial=true,.pending=-1};nova_layout(&s);
 s.visible_x=-s.points[7].x;s.visible_y=-s.points[7].y;s.motion.x=s.visible_x/16.0f;s.motion.y=s.visible_y/16.0f;
 tap_at(&s,7);assert(s.pending==7);
 nova_state moving={.initial=true,.pending=-1};nova_layout(&moving);
 moving.motion.x=20;moving.motion.y=5;tap_at(&moving,0);assert(moving.pending==0); /* displayed geometry wins */
 nova_state inherited={.initial=true,.pending=-1};nova_layout(&inherited);
 springboard_contact h={.valid=true,.down=true,.began=true,.tap_eligible=false,.x=120,.y=120};
 nova_contact_input(&inherited,&h,20);h.down=false;h.released=true;nova_contact_input(&inherited,&h,20);assert(inherited.pending<0);
 /* Opening a first-ring icon must reach its exact center continuously rather
  * than forcibly jumping the remaining spring distance on the launch frame. */
 nova_state opening={.initial=true,.pending=-1};nova_layout(&opening);tap_at(&opening,1);
 assert(opening.pending==1 && opening.pulse==160);
 float previous=0;
 for(unsigned elapsed=40;elapsed<=160;elapsed+=40){
  nova_tick(&opening,40);
  float target=opening.motion.points[1].x;
  float fraction=target?opening.motion.x/target:opening.motion.y/opening.motion.points[1].y;
  assert(fraction>previous && fraction<=1);previous=fraction;
  if(elapsed==120)assert(fraction>.98f); /* final step is less than one pixel */
 }
 assert(opening.motion.settled && !opening.pulse);
 assert(opening.motion.x==opening.motion.points[1].x && opening.motion.y==opening.motion.points[1].y);
 nova_state interrupted={.initial=true,.pending=-1};nova_layout(&interrupted);tap_at(&interrupted,1);nova_tick(&interrupted,40);
 springboard_contact grab={.valid=true,.down=true,.began=true,.tap_eligible=true,.x=120,.y=120};
 nova_contact_input(&interrupted,&grab,20);assert(interrupted.pending<0 && !interrupted.pulse && interrupted.motion.dragging);
 puts("Tap layers: center/first ring open once; far centers only; panned/moving display geometry and inherited release pass");
}
