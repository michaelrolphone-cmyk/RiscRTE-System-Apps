/* Full-feature native controller with actual cooperative panel-provider code.
 * Host CPU costs and calibrated fake-clock transfer/scheduler costs are separate. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
uint64_t cadence_calls[32];
unsigned cadence_capture_us,cadence_raster_us;
#define main healthy_fixture_main
#include PROFILE_FIXTURE
#undef main
#include "provider_model.h"
static uint64_t stamp(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
int main(int argc,char **argv) {
 assert(argc==4);fw=410;fh=502;pitch=fw+3;use_async=true;contact_count=1;
 unsigned pair_us=(unsigned)atoi(argv[1]);cadence_capture_us=(unsigned)atoi(argv[2]);cadence_raster_us=(unsigned)atoi(argv[3]);
 model_reset(fw,fh,pair_us);refresh_millihz=present_latency_us=0;
 extended_contexts=(contexts_fingerprint_service_v1){.base=ctx_api,.fingerprint_abi=0x31504643u,.fingerprint=healthy_fingerprint};extended_contexts.base.struct_size=sizeof(extended_contexts);
 guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 assert(app_module_init()==0);api=t5_app_get_api(T5_APP_ABI_VERSION);assert(has_required_app_api());
 assert(api->screen_width()==410&&api->screen_height()==502);
 count=api->installed_apps_count();assert(count==24);selected=0;layout();
 production=springboard_presentation_get();assert(production);expect_transition=false;
 nova_state s={.view=production,.pending=-1,.initial=true};nova_layout(&s);s.time_known=true;s.hour=10;s.minute=21;
 uint64_t catalog_snap=cadence_calls[6],catalog_get=cadence_calls[7];
 uint64_t poll_host_ns=0,render_host_ns=0,poll_model_us=0,render_model_us=0,max_poll_us=0;
 unsigned previous=api->millis(),native_contacts=0;
 for(unsigned i=0;i<120;i++) {
  if(i==60){s.first=19;nova_layout(&s);}
  contact_down=i%60<30;contact_x=220+i%30;contact_y=247;
  t5_app_input_t in={0};uint64_t before_model=model_now_us(),before_host=stamp();assert(api->poll(&in,20));
  poll_host_ns+=stamp()-before_host;uint64_t spent=model_now_us()-before_model;poll_model_us+=spent;if(spent>max_poll_us)max_poll_us=spent;
  springboard_contact c={0};production->contact(&c);
  if(c.valid&&c.down){assert(c.x==(int)contact_x&&c.y==(int)contact_y);native_contacts++;}
  unsigned now=api->millis(),dt=now-previous;previous=now;
  nova_contact_input(&s,&c,dt);nova_tick(&s,dt);
  before_host=stamp();before_model=model_now_us();nova_render(&s);render_host_ns+=stamp()-before_host;render_model_us+=model_now_us()-before_model;guards();
 }
 assert(native_contacts>40);assert(cadence_calls[6]==catalog_snap&&cadence_calls[7]==catalog_get);
 app_module_fini();assert(!frames&&!grants&&!subscriptions&&!model_pending());
 printf("CADENCE {\"width\":410,\"height\":502,\"frames\":120,\"pair_us\":%u,\"capture_us\":%u,\"raster_checkpoint_us\":%u,\"poll_host_us_per_frame\":%.3f,\"render_host_us_per_frame\":%.3f,\"poll_model_us_per_frame\":%.3f,\"render_model_us_per_frame\":%.3f,\"max_poll_model_us\":%llu,\"submitted\":%u,\"provider_rows\":%u,\"provider_polls\":%u,\"native_contacts\":%u,\"catalog_refresh_during_gestures\":0,\"calls\":[",pair_us,cadence_capture_us,cadence_raster_us,poll_host_ns/120000.0,render_host_ns/120000.0,poll_model_us/120.0,render_model_us/120.0,(unsigned long long)max_poll_us,presents,model_row_count(),model_poll_count(),native_contacts);
 for(unsigned i=0;i<16;i++)printf("%s%llu",i?",":"",(unsigned long long)cadence_calls[i]);
 puts("]}");
 model_teardown();free(guarded);assert(!allocation_count);return 0;
}
