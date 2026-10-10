/* Cold/reset startup only. Paint immutable time samples through the ordinary
 * sliced framebuffer path; every wait continues input and provider service. */
#include "../lib/PortableApps/include/PortableRaster.h"
#include "../lib/PortableApps/boot_logo/RiscRteBootAnimation.h"
static bool desk_boot_animation_pending;
static bool desk_animation_run(void *context,int x,int y,int width) {
 (void)context;app->fill_rect(x,y,width,1,true);return true;
}
static void desk_animation_rows(const void *payload,int first,int last) {
 riscrte_boot_frame frame;memcpy(&frame,payload,sizeof(frame));
 (void)riscrte_boot_render_rows(&frame,desk_animation_run,NULL,first,last);
}
static bool desk_animation_poll(bool *skip) {
 t5_app_input_t input={0};if(!app->poll(&input,20))return false;
 springboard_contact contact={0};paper->contact(&contact);
 *skip|=input.exit_requested||input.buttons||(contact.valid&&contact.down);
 return portable_desk_adapter_ready();
}
static bool desk_play_boot_animation(void) {
 if(!desk_boot_animation_pending)return true;
 desk_boot_animation_pending=false;
 portable_stage_log(rt,"boot-animation-start","source=T5S3-Reader");
 if(!paper_frame_ready())return false;
 /* The first cold transfer establishes clean white. Its physical latency is
  * excluded from the animation clock so it cannot skip the opening sweep. */
 paper->begin();app->present(true);
 bool skip=false;uint32_t began=app->millis();
 while(!paper_frame_idle()) {
  if(!desk_animation_poll(&skip))return false;
  if((uint32_t)(app->millis()-began)>=8000u)return false;
 }
 began=app->millis();uint32_t last=UINT32_MAX;
 while(!skip) {
  uint32_t elapsed=(uint32_t)(app->millis()-began);
  if(elapsed>=1800u)break;
  if((last==UINT32_MAX||elapsed-last>=40u)&&paper_frame_ready()) {
   uint32_t left=elapsed<1650u?150u:1800u-elapsed;
   riscrte_boot_frame frame={app->screen_width(),app->screen_height(),elapsed,
       (uint8_t)(64u*left*left/(150u*150u))};
   paper->begin();
   if(!portable_raster_record_draw(desk_animation_rows,&frame,sizeof(frame),0,frame.height))
    desk_animation_rows(&frame,0,frame.height);
   app->present(false);last=elapsed;
  }
  if(!desk_animation_poll(&skip))return false;
 }
 /* Finish only the outstanding software/active frame. Do not wait for the
  * driver's quiet settling interval before Home becomes interactive. */
 began=app->millis();
 while(!paper_frame_idle()) {
  if(!desk_animation_poll(&skip))return false;
  if((uint32_t)(app->millis()-began)>=8000u)return false;
 }
 portable_stage_log(rt,"boot-animation-end",skip?"interrupted=1":"interrupted=0");
 return true;
}
