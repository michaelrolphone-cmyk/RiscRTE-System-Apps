/* Deterministic CPU work cost in the actual portable raster. This preserves
 * provider edge queues and measures capture separately from model dispatch. */
#define FRAME_DELIVERY_NO_MAIN
#include "portable_frame_delivery_test.c"
#include <time.h>
static unsigned cpu_ms_per_batch,charged_pixels,captured_at;
static size_t copied_bytes,copy_remainder;
static double copy_host_ms;
static unsigned copy_cost_ms,edge_at;
static bool edge_at_copy,real_cpu_clock;
static clock_t observed_cpu;static double cpu_fraction;
static void *raster_test_memcpy(void *to,const void *from,size_t bytes) {
 bool copying=false;
#ifdef PORTABLE_RASTER_SNAPSHOT
 copying=raster_offscreen&&(uintptr_t)from>=(uintptr_t)raster_offscreen&&
  (uintptr_t)from<(uintptr_t)raster_offscreen+(size_t)info.height*(surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2);
#endif
 if(copying&&edge_at_copy&&edge_at==UINT32_MAX)edge_at=mock_ms+1;
 clock_t before=copying?clock():0;void *result=memcpy(to,from,bytes);
 if(copying){copy_host_ms+=1000.0*(clock()-before)/CLOCKS_PER_SEC;copied_bytes+=bytes;
  copy_remainder+=bytes;mock_ms+=(unsigned)(copy_remainder/4096)*copy_cost_ms;copy_remainder%=4096;}
 return result;
}
static void raster_test_clock(void) {
 if(real_cpu_clock){clock_t now=clock();cpu_fraction+=1000.0*(now-observed_cpu)/CLOCKS_PER_SEC;
  unsigned elapsed=(unsigned)cpu_fraction;cpu_fraction-=elapsed;mock_ms+=elapsed;observed_cpu=now;}

 unsigned batches=(raster_pixels-charged_pixels)/512u;
 if(cpu_ms_per_batch && batches){mock_ms+=batches*cpu_ms_per_batch;charged_pixels+=batches*512u;}
}
static bool raster_probe_poll(void *c,size_t budget) {
 mock_poll_touch(c,budget);
 for(;probe_scheduled<2;probe_scheduled++) {
  unsigned at=edge_at==UINT32_MAX?UINT32_MAX:edge_at+probe_scheduled*5;
  if(mock_ms<at)break;
  if(!captured_at)captured_at=mock_ms;
  probe_push(probe_scheduled?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,at);
 }
 return true;
}
static bool raster_nav_poll(void*c,risc_input_navigation_frame_v1*out) {
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(nav_edges<2 && edge_at!=UINT32_MAX && mock_ms>=edge_at+nav_edges*5) {
  if(nav_edges++)out->released=RISC_NAV_RIGHT;
  else out->pressed=out->buttons=RISC_NAV_RIGHT;
 }
 return true;
}
static void raster_case(unsigned format,unsigned cost) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 probe_head=probe_count=probe_scheduled=probe_begins=probe_taps=probe_model=0;
 nav_edges=0;captured_at=0;cpu_ms_per_batch=0;charged_pixels=0;edge_at=20;
 probe_snapshot=(risc_touch_snapshot_v1){.width=format==RISC_DISPLAY_FORMAT_MONO1?480:240,.height=format==RISC_DISPLAY_FORMAT_MONO1?800:240};
 assert(!app_module_init());
 risc_touch_api_v1 input=mock_touch;input.poll=raster_probe_poll;input.next=probe_next;input.snapshot=probe_snap;touch.api=&input;
 risc_input_navigation_api_v1 nav=probe_navigation;nav.poll=raster_nav_poll;navigation=&nav;
 if(format==RISC_DISPLAY_FORMAT_MONO1)assert(paper_presentation_get());else assert(springboard_presentation_get());handoff_finish();
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api->frame_ready());
 t5_app_input_t neutral;assert(api->poll(&neutral,0));
 mock_ms=0;raster_pixels=charged_pixels=0;cpu_ms_per_batch=cost;
 real_cpu_clock=getenv("RASTER_REAL_CPU_CLOCK")!=NULL;observed_cpu=clock();cpu_fraction=0;
 copied_bytes=copy_remainder=0;copy_host_ms=0;copy_cost_ms=getenv("RASTER_COPY_COST_MS")?(unsigned)atoi(getenv("RASTER_COPY_COST_MS")):0;
 mock_peak=mock_bytes;size_t initial_bytes=mock_bytes;
 edge_at_copy=getenv("RASTER_EDGE_AT_COPY")!=NULL;edge_at=edge_at_copy?UINT32_MAX:20;
 if(!cost&&getenv("RASTER_FAIL_ALLOCATION"))mock_fail_allocation=mock_allocations+(unsigned)atoi(getenv("RASTER_FAIL_ALLOCATION"));
 clock_t start=clock();api->clear();
 for(unsigned n=0;n<3;n++)api->fill_rect(0,0,width(),height(),n&1u);
 char borrowed[80]="COMPLETE API RASTER SNAPSHOT";
 api->draw_text(20,80,borrowed);memset(borrowed,'X',20);
 api->fill_rounded_rect_tone(12,30,180,48,7,2);
 api->draw_label(20,130,180,"LATEST MODEL");
 assert(api->draw_icon(30,170,"solid:f013",36,true));
 assert(!api->draw_icon(30,170,"does-not-exist",36,true));
#ifdef RASTER_ALL_GRAPHICS
 char *bounded=malloc(96);assert(bounded);memset(bounded,'A',96);
 if(format==RISC_DISPLAY_FORMAT_MONO1){
  const paper_presentation *p=&pp_view;
  p->circle(120,310,48,true);p->circle(120,310,22,false);
  p->text(16,350,460,bounded,1,false,true);
  p->text(16,390,220,"Complete fit & mixed case",PAPER_TEXT_LITERAL|PAPER_TEXT_CLOCK,false,true);
 } else {
  const springboard_presentation *p=&np_view;
  p->circle(50,55,40,0x39ffaa,173);assert(p->icon(100,55,48,"solid:f013",213));
  p->caption(160,bounded,false,0xffaa39);
 }
 portable_nova_text(1,8,8,224,bounded,NOVA_WHITE);free(bounded);
 portable_nova_fill(24,32,60,25,NOVA_DIM);portable_nova_round(100,32,110,46,12,NOVA_CAP);
 portable_nova_button(8,90,110,44,"Keep actions",true);
 portable_nova_row(8,136,224,54,"Label","Value",false);
 assert(portable_nova_wrap(2,14,192,200,12,2,"A complete wrapped label",NOVA_CYAN));
#endif
#ifdef RASTER_SETTINGS_GRAPHICS
 sv_begin();sv_heading("SETTINGS");sv_rect(14,60,212,118,0x061c29);
 sv_button(24,80,192,44,"Retain selected",true);
 char bounded_settings[96];memset(bounded_settings,'z',sizeof(bounded_settings));
 sv_text(2,18,150,bounded_settings,SV_CYAN,18);memset(bounded_settings,'Q',96);
 sv_fade(55,190);sv_back();
#endif
 unsigned extra=getenv("RASTER_EXTRA_COMMANDS")?(unsigned)atoi(getenv("RASTER_EXTRA_COMMANDS")):0;
 for(unsigned n=0;n<extra;n++)api->fill_rect((int)(n%(unsigned)width()),(int)((n/(unsigned)width())%(unsigned)height()),1,1,n&1u);
 unsigned drawn_at=mock_ms;double host_draw_ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
 api->present(true);unsigned first_dispatch=0,nav_dispatch=0;
 double max_poll_cpu_ms=0,total_poll_cpu_ms=0;unsigned poll_calls=0;
 for(unsigned i=0;i<(edge_at_copy?25000u:100u) && (probe_taps<1||nav_edges<2);i++) {
  clock_t poll_start=clock();
  t5_app_input_t e;assert(api->poll(&e,4));
  double poll_cpu_ms=1000.0*(clock()-poll_start)/CLOCKS_PER_SEC;
  if(poll_cpu_ms>max_poll_cpu_ms)max_poll_cpu_ms=poll_cpu_ms;
  total_poll_cpu_ms+=poll_cpu_ms;poll_calls++;
  if(e.buttons&T5_APP_BUTTON_RIGHT)nav_dispatch=mock_ms;
  springboard_contact c;np_contact(&c);
  if(c.began){probe_begins++;first_dispatch=mock_ms;}
  if(c.valid&&c.released&&c.tap_eligible&&!c.cancelled){probe_taps++;probe_model++;}
 }
 if(probe_begins!=1||probe_taps!=1||probe_model!=1||nav_edges!=2)fprintf(stderr,"fmt=%u cost=%u begins=%u taps=%u model=%u nav=%u time=%u\n",format,cost,probe_begins,probe_taps,probe_model,nav_edges,mock_ms);
 assert(probe_begins==1&&probe_taps==1&&probe_model==1&&nav_edges==2);
 if(!edge_at_copy)assert(captured_at<=20+cost*2+4);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(cost&&!edge_at_copy)assert(first_dispatch-captured_at<=8);
#else
 if(cost)assert(first_dispatch>=drawn_at&&first_dispatch-captured_at>100);
#endif
 unsigned dispatched_pixels=raster_pixels;
#ifdef PORTABLE_RASTER_SNAPSHOT
 while(raster_sealed){
  clock_t poll_start=clock();t5_app_input_t e;assert(api->poll(&e,1));
  double poll_cpu_ms=1000.0*(clock()-poll_start)/CLOCKS_PER_SEC;
  if(poll_cpu_ms>max_poll_cpu_ms)max_poll_cpu_ms=poll_cpu_ms;
  total_poll_cpu_ms+=poll_cpu_ms;poll_calls++;
 }
#endif
 assert(api->frame_drain());
 uint32_t hash=2166136261u;unsigned bytes=format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 char destination[512];snprintf(destination,sizeof(destination),"%s/raster-%u-%u.bin",getenv("RASTER_OUTPUT"),format,cost);
 FILE*f=fopen(destination,"wb");assert(f);
 for(unsigned y=0;y<info.height;y++)assert(fwrite((uint8_t*)mock_pixels+y*surface.stride_bytes,1,bytes,f)==bytes);
 assert(!fclose(f));
 for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<bytes;x++){hash^=((uint8_t*)mock_pixels)[y*surface.stride_bytes+x];hash*=16777619u;}
 printf("{\"format\":%u,\"cost_ms_per_512_pixel_visits\":%u,\"draw_return_ms\":%u,\"touch_capture_ms\":%u,\"model_dispatch_ms\":%u,\"nav_dispatch_ms\":%u,\"host_cpu_draw_ms\":%.3f,\"pixel_visits_before_model\":%u,\"pixel_visits_total\":%u,\"completed_at_ms\":%u,\"raster_hash\":%u}\n",format,cost,drawn_at,captured_at,first_dispatch,nav_dispatch,host_draw_ms,dispatched_pixels,raster_pixels,mock_ms,hash);
 printf("{\"edge_at_ms\":%u,\"edge_to_model_ms\":%u,\"memory_format\":%u,\"width\":%u,\"height\":%u,\"initial_live_bytes\":%zu,\"peak_live_bytes\":%zu,\"incremental_peak_bytes\":%zu,\"copy_bytes\":%zu,\"copy_host_cpu_ms\":%.6f,\"copy_cost_ms_per_4096_bytes\":%u}\n",edge_at,first_dispatch-edge_at,format,info.width,info.height,initial_bytes,mock_peak,mock_peak-initial_bytes,copied_bytes,copy_host_ms,copy_cost_ms);
 printf("{\"poll_calls\":%u,\"maximum_poll_host_cpu_ms\":%.6f,\"total_poll_host_cpu_ms\":%.6f}\n",poll_calls,max_poll_cpu_ms,total_poll_cpu_ms);
 cpu_ms_per_batch=0;real_cpu_clock=false;assert(api->frame_drain());end_test();assert(!mock_bytes);
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);for(unsigned f=0;f<2;f++)for(unsigned cost=0;cost<(getenv("RASTER_ONLY_ZERO")?1u:3u);cost++)raster_case(f?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565,cost);return 0;}
