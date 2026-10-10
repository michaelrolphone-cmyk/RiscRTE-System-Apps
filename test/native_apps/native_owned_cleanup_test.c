/* Native finalization uses the same ordered app-owned RF/audio shutdown as
 * legacy finalization; a refusing or retaining hook stops all later work. */
#define PORTABLE_RADIO_SESSION
#define PORTABLE_AUDIO_SESSION
#define TEST_NATIVE_TOOLBAR_QUICK
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
static unsigned radio_closes,audio_closes;
static const char *cleanup_case;
bool portable_radio_services_safe(void){return true;}
bool portable_audio_services_safe(void){return true;}
bool portable_radio_suspend(void){
 io();assert(!audio_closes);++radio_closes;
 if(!strcmp(cleanup_case,"radio-retained")){portable_adapter_retain();return true;}
 return strcmp(cleanup_case,"radio-refused")!=0;
}
bool portable_audio_suspend(void){
 io();assert(radio_closes==1);++audio_closes;
 if(!strcmp(cleanup_case,"audio-retained")){portable_adapter_retain();return true;}
 return strcmp(cleanup_case,"audio-refused")!=0;
}
int main(int argc,char **argv){
 assert(argc==2);cleanup_case=argv[1];assert(app_module_init()==0);
 const paper_presentation *view=paper_presentation_get();assert(view);
 app_module_fini();assert(radio_closes==1);
 if(!strcmp(cleanup_case,"closed")){assert(audio_closes==1&&!live&&!frames&&!subscriptions&&!retained);}
 else {assert(audio_closes==(!strncmp(cleanup_case,"audio",5)?1u:0u));check_retained(view);}
 printf("native owned cleanup %s PASS\n",cleanup_case);return 0;
}
