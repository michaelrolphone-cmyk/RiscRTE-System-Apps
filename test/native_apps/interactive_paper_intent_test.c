/* Real adapter: page coverage and transition renderer selection must not select
 * the slow waveform. Provider callbacks record the submitted contract. */
#define main native_toolbar_original_main
#include "portable_native_toolbar_test.c"
#undef main
static unsigned submits;
static bool info_clean(void *c,risc_display_info_v1 *out) {
 bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;
}
static bool capture(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 assert(o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 if(submits==0||submits==2)assert(n==0);else assert(n==1&&r&&r->width*r->height<800u*480u);
 ++submits;return fx_submit(c,f,r,n,o,token);
}
int main(void) {
 assert(app_module_init()==0);risc_display_output_api_v1 selected=fx_display;selected.get_info=info_clean;selected.submit=capture;display=&selected;assert(info_clean(NULL,&info));const paper_presentation *view=paper_presentation_get();assert(view);
 for(unsigned i=0;i<3;i++) {
  assert(portable_paper_frame_ready());view->begin();rect(100+(int)i*16,180,12,14,true);
  present(i!=1);assert(portable_paper_frame_drain());
 }
 assert(submits==3);app_module_fini();assert(!live&&!frames&&!subscriptions&&!retained);
 puts("Real adapter: first/full page and partial damage all use LOW_LATENCY without transition renderer");return 0;
}
