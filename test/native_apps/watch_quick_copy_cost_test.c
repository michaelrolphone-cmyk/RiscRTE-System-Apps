/* Observe the unchanged Watch integration scenarios with the same production
 * input/controller/renderer. Instrumentation exists only in this host binary. */
#define PORTABLE_QUICK_FIXTURE_MAIN watch_fixture_main
#include "quick_adapter_test.c"
static unsigned restore_depth,restore_fills,restore_pixels,restore_calls;
static unsigned capture_sequence;
static bool cost_watch_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 bool okay=capture_submit(c,f,r,n,o,token);
 if(okay && capture_directory){char path[512];snprintf(path,sizeof(path),"%s/watch-frame-%03u.bin",capture_directory,++capture_sequence);FILE *file=fopen(path,"wb");assert(file);assert(fwrite(framebuffer,1,sizeof(framebuffer),file)==sizeof(framebuffer));assert(!fclose(file));}
 return okay;
}
void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *fn,void *caller) {
 (void)caller;
 if(fn==(void *)quick_copy_background){++restore_depth;++restore_calls;}
 if(fn==(void *)fill && restore_depth)++restore_fills;
 if(fn==(void *)native_point && restore_depth)++restore_pixels;
}
void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *fn,void *caller) {
 (void)caller;if(fn==(void *)quick_copy_background)--restore_depth;
 if(fn==(void *)setup)quick_display_api.submit=cost_watch_submit;
}
int main(int argc,char **argv) {
 int result=watch_fixture_main(argc,argv);
 printf("{\"case\":\"%s\",\"restore_fills\":%u,\"restore_pixel_visits\":%u,\"background_copies\":%u}\n",argv[1],restore_fills,restore_pixels,restore_calls);
 return result;
}
