/* Production native-time adapter/controller/renderer driven only through raw
 * providers. Function-entry instrumentation observes discarded work without
 * adding counters or validation to the shipped application. */
#define TEST_NATIVE_SOURCE_NO_MAIN
#include "portable_native_time_source_test.c"

static unsigned fill_calls,background_copies,render_calls,poll_calls,next_calls,snapshot_calls,kv_reads;
static unsigned touch_phase,restore_depth,restore_fills,restore_pixels;
static bool cancelled,retain_surface;
static const char *cost_case;
static risc_touch_api_v1 cost_touch;
static risc_display_output_api_v1 cost_display;
static const char *capture_path;

void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *fn,void *caller) {
 (void)caller;
 if(fn==(void *)fill){++fill_calls;if(restore_depth)++restore_fills;}
 if(fn==(void *)native_point && restore_depth)++restore_pixels;
 if(fn==(void *)quick_copy_background){++background_copies;++restore_depth;}
 if(fn==(void *)quick_paper_render)++render_calls;
}
void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *fn,void *caller) {(void)caller;if(fn==(void *)quick_copy_background)--restore_depth;}
static bool cost_poll(void *c,size_t n){++poll_calls;++touch_phase;assert(touch_phase<200);return fx_touch_poll(c,n);}
static int32_t cost_next(void *c,uint64_t token,risc_touch_event_v1 *out){++next_calls;return fx_touch_next(c,token,out);}
static bool cost_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 ++snapshot_calls;fx_snapshot(c,out);
 unsigned n=touch_phase;
 if(n==2 || n==3 || (n>=5 && n<=35 && strcmp(cost_case,"tap")) || (n==5 && !strcmp(cost_case,"tap"))) {
  out->contact_count=1;out->contacts[0].id=1;
  out->contacts[0].x=200;out->contacts[0].y=n==2?20:n==3?100:500;
  if(n==5 && !strcmp(cost_case,"tap")){out->contacts[0].x=340;out->contacts[0].y=345;}
  if(n>=5 && !strcmp(cost_case,"move"))out->contacts[0].x=(uint16_t)(200+n);
 }
 if(n==40) {out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=675};}
 if(!strcmp(cost_case,"cancel") && n==35){out->contact_count=2;cancelled=true;}
 return true;
}
static int32_t cost_get(void *c,const char *key,void *out,uint32_t size,uint32_t *used) {
 ++kv_reads;
 if(!strcmp(key,PORTABLE_TIMEZONE_KEY))return source_get(c,key,out,size,used);
 return fx_kv_get(c,key,out,size,used);
}
static bool cost_frame(void *c,uint32_t format,risc_display_surface_v1 *out) {
 bool okay=fx_frame(c,format,out);
 if(okay && retain_surface){risc_display_surface_v1 delivered=*out;portable_adapter_retain();*out=delivered;}
 return okay;
}
static bool cost_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 bool okay=fx_submit(c,f,r,n,o,token);
 if(okay && capture_path){char path[512];snprintf(path,sizeof(path),"%s/frame-%u.bin",capture_path,presents);FILE *file=fopen(path,"wb");assert(file);assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));assert(!fclose(file));}
 return okay;
}
static bool cost_acquire(const char *name,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g) {
 bool ok=source_acquire(name,v,instance,g);
 if(ok && !strcmp(name,"input.touch.raw"))g->api=&cost_touch;
 if(ok && !strcmp(name,"display.output"))g->api=&cost_display;
 return ok;
}
static void cost_release_check(risc_runtime_capability_v1 *g) {
 if(g->api==&cost_touch)g->api=&fx_touch;
 if(g->api==&cost_display)g->api=&fx_display;
}
static bool cost_release(risc_runtime_capability_v1 *g){cost_release_check(g);return source_release(g);}
int main(int argc,char **argv) {
 assert(argc==2 || argc==3);cost_case=argv[1];capture_path=argc==3?argv[2]:NULL;scenario="valid";
 cost_touch=fx_touch;cost_touch.poll=cost_poll;cost_touch.next=cost_next;cost_touch.snapshot=cost_snapshot;
 cost_display=fx_display;cost_display.submit=cost_submit;cost_display.acquire=cost_frame;
 source_kv.get=cost_get;source_kv.put=fx_kv_put;fx_runtime.acquire=cost_acquire;fx_runtime.release=cost_release;
 assert(app_module_init()==0);
 /* A completed foreground image is the real modal restore source. */
 clear();fill(31,49,179,283,0);present(false);
 uint8_t before[sizeof(pixels)];memcpy(before,pixels,sizeof(before));
 fill_calls=background_copies=render_calls=0;kv_reads=native_reads=zone_reads=0;
 if(!strcmp(cost_case,"restore-retained")) {
  quick_background=malloc(sizeof(pixels));assert(quick_background);memcpy(quick_background,before,sizeof(pixels));
  memset(pixels,0xa5,sizeof(pixels));memcpy(before,pixels,sizeof(before));retain_surface=true;
  assert(!quick_copy_background()&&retained&&barriers==1&&frames==1);
  assert(!memcmp(before,pixels,sizeof(pixels))&&!restore_pixels);
  unsigned old_calls=calls,old_frees=free_calls;app_module_fini();assert(calls==old_calls&&free_calls==old_frees);
  printf("{\"case\":\"restore-retained\",\"background_copies\":%u,\"restore_fills\":%u,\"restore_pixel_visits\":%u}\n",background_copies,restore_fills,restore_pixels);
  return 0;
 }
 while(touch_phase<45){t5_app_input_t out={0};assert(poll(&out,8));assert(!out.tapped&&!out.exit_requested&&!out.buttons);}
 assert(!quick_modal&&!pqa_visible(&quick.ui)&&!quick_background);
 assert(presents==(!strcmp(cost_case,"tap")?4u:3u)&&!memcmp(before,pixels,sizeof(before)));
 assert(kv_writes==(!strcmp(cost_case,"tap")?1u:0u)&&refreshes==kv_writes);
 assert(!brightness_calls&&!reader_live&&!zone_live);
 assert(cancelled==!strcmp(cost_case,"cancel"));
 unsigned clears=fill_calls;
 app_module_fini();assert(!frames&&!live&&!subscriptions&&!retained);
 printf("{\"case\":\"%s\",\"poll\":%u,\"next\":%u,\"snapshot\":%u,\"kv_reads\":%u,\"native_reads\":%u,\"paper_renders\":%u,\"background_copies\":%u,\"fill_calls\":%u,\"presents\":%u,\"restore_fills\":%u,\"restore_pixel_visits\":%u}\n",cost_case,poll_calls,next_calls,snapshot_calls,kv_reads,native_reads,render_calls,background_copies,clears,presents,restore_fills,restore_pixels);
 return 0;
}
