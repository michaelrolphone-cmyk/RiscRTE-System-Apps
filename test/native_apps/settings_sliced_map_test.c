#define PORTABLE_SETTINGS_BOTH_TIMERS
#define main inherited_main
#include "settings_sleep_defaults_test.c"
#undef main
static unsigned steps;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
static size_t deny_malloc_size;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size){if(deny_malloc_size&&size==deny_malloc_size){deny_malloc_size=0;return NULL;}return __real_malloc(size);}
#endif
static void finish_frame(void){for(unsigned i=0;i<20000;++i){
 if(!portable_paper_frame_idle()){++steps;assert(paper_present_progress());ticks++;}
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
 sv_frame_completed();
 if(portable_paper_frame_idle()&&sp_dirty){settings_view_redraw();continue;}
#endif
 if(portable_paper_frame_idle())break;
 }assert(portable_paper_frame_idle());
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
 assert(!sv_image_pending);assert(sv_frame_current());
#endif
}
static unsigned long long hash(void){unsigned long long h=1469598103934665603ULL;for(unsigned i=0;i<sizeof(framebuffer);i++)h=(h^((unsigned char*)framebuffer)[i])*1099511628211ULL;return h;}
static t5_app_input_t sample(bool down,bool began,bool release,int x,int y){input_progressed=true;input_pending=true;input_sample=(portable_touch_sample){.valid=true,.down=down,.began=began,.released=release,.tap_eligible=true,.x=(uint16_t)sv_native_x(x),.y=(uint16_t)sv_native_y(y)};t5_app_input_t o={0};assert(settings_view_poll(&o));return o;}
int main(void){begin();finish_frame();unsigned pages[]={SV_ROOT,SV_FIELDS,SV_VALUE,SV_FOLD,SV_ABOUT,SV_SLEEP,SV_TIME_FORMAT,SV_ALERT,SV_TIMER};
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){sv_page=pages[i];settings_draft=stored;editor_field=2;editor_message="Draft only - use SAVE";sv_scroll[0]=sv_scroll[1]=0;settings_view_redraw();finish_frame();printf("%u %016llx\n",pages[i],hash());}
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
 sv_switch(SV_ROOT);finish_frame();uint64_t visible=sv_visible_map.receipt;
 /* A new logical scroll may not retarget a tap against still-visible rows. */
 sv_scroll[0]=120;settings_view_redraw();assert(sv_image_pending&&sv_visible_map.receipt==visible);
 (void)sample(true,true,false,30,80);assert(sv_gesture_map.scroll[0]==0);
 t5_app_input_t o=sample(false,false,true,30,80);assert(o.tapped&&sv_action_scroll(0)==0);
 finish_frame();assert(sv_visible_map.scroll[0]==120);
 /* Completion of different pixels between DOWN and UP cancels the old action. */
 (void)sample(true,true,false,30,80);sv_scroll[0]=0;settings_view_redraw();finish_frame();o=sample(false,false,true,30,80);assert(!o.tapped&&!o.buttons);
 /* Even revisiting the same page does not revive an older frame/gesture. */
 (void)sample(true,true,false,30,80);sv_switch(SV_ABOUT);sv_switch(SV_ROOT);o=sample(false,false,true,30,80);assert(!o.tapped&&!o.buttons);finish_frame();
 /* Idle with a replaced/discarded receipt is not completion authority. */
 sv_scroll[0]=40;settings_view_redraw();assert(sv_image_pending);raster_discard();sv_frame_completed();assert(!sv_image_pending&&sp_dirty);assert(sv_visible_map.scroll[0]==0);
 /* Existing immediate compatibility fallback must preserve every pixel and map. */
 sv_scroll[0]=0;settings_view_redraw();finish_frame();unsigned long long expected=hash();
 size_t allocations[]={sizeof(raster_command_block),(size_t)info.width*info.height*2};
 for(unsigned i=0;i<2;i++){raster_discard();deny_malloc_size=allocations[i];settings_view_redraw();finish_frame();assert(!deny_malloc_size&&!failed&&hash()==expected);}
 fprintf(stderr,"cooperative maps and allocation fallback PASS steps=%u\n",steps);
#endif
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count);return 0;}
