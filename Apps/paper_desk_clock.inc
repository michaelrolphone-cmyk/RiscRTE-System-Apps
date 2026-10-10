/* Explicit wall-time Clock deployment only. Included by paper_clock.c so the
 * normal clock retains its own RTC grant and no private hook becomes an export. */
#if !defined(PORTABLE_RTC_WALL_TIME) || defined(PORTABLE_RTC_UTC8_DENVER)
#error "Desk clock requires explicit unconverted RTC wall time"
#endif
#include "PortableDeskClockApp.h"
#include "PortableDeskClockFaces.h"
#include "PortableDeskClockSettings.h"
static portable_desk_record desk_boot_record;
static bool desk_boot_timer,desk_returned,desk_retained,desk_radios_off;
static const portable_desk_sleep_ops *desk_ops;
static unsigned desk_rectangles;
void portable_desk_clock_refused(void) { desk_returned=true;desk_radios_off=false; }
void portable_desk_clock_radios_off(void) { desk_radios_off=true; }
static const char *desk_notice(void) {
 return desk_radios_off?"DESK CLOCK STOPPED; RADIOS OFF":"DESK CLOCK DID NOT SLEEP";
}

static bool desk_uptime(uint32_t *out) {
 risc_runtime_health_v1 health={.struct_size=sizeof(health)};
 if(!rt->health(&health))return false;
 *out=health.uptime_ms;return true;
}
/* A bounded civil count in the RTC's supported 2000..2099 domain, expressed
 * as Unix epoch seconds without interpreting that civil value as UTC. */
static bool desk_seconds(const twatch_rtc_time_v1 *time,int64_t *out) {
 if(!tw_valid_time(time))return false;
 uint32_t days=10957;
 for(unsigned y=2000;y<time->year;y++)days+=y%4?365u:366u;
 for(unsigned m=1;m<time->month;m++)days+=watch_month_days(time->year,m);
 days+=time->day-1u;
 *out=(int64_t)days*86400+(uint32_t)time->hour*3600u+(uint32_t)time->minute*60u+time->second;
 return true;
}
static bool desk_calendar(int64_t seconds,twatch_rtc_time_v1 *out) {
 if(seconds<946684800 || seconds>4102444799LL)return false;
 uint32_t value=(uint32_t)seconds,days=value/86400u;
 twatch_rtc_time_v1 result={.year=2000,.month=1};
 result.weekday=(uint8_t)((days+4u)%7u);days-=10957u;
 while(days>=(result.year%4?365u:366u)){days-=result.year%4?365u:366u;++result.year;}
 while(days>=watch_month_days(result.year,result.month)){days-=watch_month_days(result.year,result.month);++result.month;}
 result.day=(uint8_t)(days+1u);value%=86400u;
 result.hour=(uint8_t)(value/3600u);result.minute=(uint8_t)(value/60u%60u);result.second=(uint8_t)(value%60u);
 *out=result;return true;
}
static bool desk_read(int64_t *out) {
 twatch_rtc_time_v1 time={0};return read_clock(&time)&&desk_seconds(&time,out);
}
static bool desk_cancelled(void) {
 if(desk_retained)return true;
 if(!portable_desk_adapter_ready() || !desk_ops)return true;
 int status=desk_ops->cancelled(desk_ops->context);
 if(status==-2)desk_retained=true;
 return status!=0;
}
/* rtc.clock@2 has whole seconds, not microseconds. Observe an adjacent second
 * transition in a <=100 ms bracket. The returned epoch origin is the bracket's
 * upper endpoint: conservatively late by 0..100 ms, never invented precision.
 * Reject a stalled/jumping RTC or slow/nonmonotonic sampling within 1250 ms.
 * Native timer-arm/entry latency remains outside this app-only bound. */
static bool desk_sample_edge(int64_t *seconds,uint32_t *sampled_at) {
 uint32_t start,before,after,previous_before;int64_t previous,current;
 if(!desk_uptime(&start) || !desk_read(&previous) || !desk_uptime(&after) ||
    (uint32_t)(after-start)>100u)return false;
 previous_before=start;
 for(unsigned n=0;n<126;n++) {
  if(desk_cancelled())return false;
  rt->yield_ms(10);
  if(!desk_uptime(&before) || (uint32_t)(before-start)>1250u ||
     !desk_read(&current) || !desk_uptime(&after) ||
     (uint32_t)(after-before)>100u || (uint32_t)(after-start)>1250u)return false;
  if(current!=previous) {
   if(current!=previous+1 || (uint32_t)(after-previous_before)>100u)return false;
   *seconds=current;*sampled_at=after;return true;
  }
  previous=current;previous_before=before;
 }
 return false;
}
/* No cooperative yield while prepared GPIO holds are live: Runtime yield
 * polls the full provider graph, whose storage clients must remain fenced.
 * Advance the pre-preparation edge bracket with monotonic uptime, and verify
 * one immediate RTC read. A clock jump or >100 ms read fails closed. */
static bool desk_after_prepare(int64_t anchor,uint32_t anchored_at,int64_t *seconds,
                               int32_t *microseconds,uint32_t *sampled_at) {
 uint32_t before,after;int64_t actual;
 if(desk_cancelled() || !desk_uptime(&before) || !desk_read(&actual) ||
    !desk_uptime(&after) || (uint32_t)(after-before)>100u ||
    (uint32_t)(after-anchored_at)>65000u)return false;
 uint32_t elapsed=after-anchored_at;
 int64_t estimated=anchor+elapsed/1000u;
 if(actual==estimated){*seconds=estimated;*microseconds=(int32_t)(elapsed%1000u)*1000;}
 else if(actual==estimated+1 && elapsed%1000u>=900u){*seconds=actual;*microseconds=0;}
 else return false;
 *sampled_at=after;return true;
}
static bool desk_config(portable_desk_config *out,unsigned *mode) {
 portable_desk_config config={0}; /* Legacy chip/UTC-versus-local metadata is unavailable. */
 strcpy(config.time_zone,"RTC wall time");
 risc_runtime_capability_v1 prefs={.struct_size=sizeof(prefs)};
 unsigned face=PORTABLE_DESK_SEGMENTS,time_format=PORTABLE_TIME_FORMAT_12;
 *mode=PORTABLE_SLEEP_LIGHT;
 if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&prefs)) {
  (void)portable_time_format_load(prefs.api,&time_format);
  (void)portable_desk_face_load(prefs.api,&face);
  unsigned language,flip;
  (void)portable_reader_preference_load(prefs.api,true,&language);
  (void)portable_reader_preference_load(prefs.api,false,&flip);
  config.language=(uint8_t)language;config.flip_ui=(uint8_t)flip;
  (void)portable_sleep_load_profile(prefs.api,3u,PORTABLE_SLEEP_LIGHT,mode);
  if(!rt->release(&prefs))return false;
 }
 config.face=(uint8_t)face;config.time_format=(uint8_t)time_format;
 *out=config;return true;
}
int portable_desk_clock_mode(void) {
 portable_desk_config config;unsigned mode;
 return desk_config(&config,&mode)?(int)mode:-2;
}
static bool desk_rect(void *unused,int x,int y,int w,int h,bool black) {
 (void)unused;
 if((desk_rectangles++&63u)==0 && desk_cancelled())return false;
 app->fill_rect(x,y,w,h,black);return portable_desk_adapter_ready();
}
static bool desk_scene(const portable_desk_config *config,int64_t minute) {
 twatch_rtc_time_v1 time={0};
 bool valid=minute>=1704067200LL && desk_calendar(minute,&time);
 if(!portable_desk_adapter_flip(config->flip_ui))return false;
 /* All variable scene content comes from this retained config/minute pair.
  * Battery, transient notices, ambient state and current preferences cannot
  * contaminate exact old-image reconstruction after RAM loss. */
 portable_desk_adapter_begin();desk_rectangles=0;
 portable_desk_canvas canvas={NULL,app->screen_width(),app->screen_height(),desk_rect};
 if(!portable_desk_draw_face(&canvas,config->face,time.hour,time.minute,
     config->time_format==PORTABLE_TIME_FORMAT_12,valid))return false;
 char date[32];
 if(valid)snprintf(date,sizeof(date),"%04u-%02u-%02u",time.year,time.month,time.day);
 portable_desk_adapter_caption(32,valid?date:portable_reader_clock_set_time(config->language));
 if(valid && config->time_format==PORTABLE_TIME_FORMAT_12)
  portable_desk_adapter_caption(canvas.height-78,time.hour<12?"AM":"PM");
 return !desk_cancelled();
}
static int desk_restore(void) {
 if(desk_retained)return -2;
 int status=desk_ops->resume(desk_ops->context);
 if(status!=1)return -2;
 portable_desk_adapter_invalidate();return 0;
}
int portable_desk_clock_run(const risc_runtime_api_v1 *runtime,const portable_desk_sleep_ops *ops) {
 if(runtime!=rt || !ops || !ops->cancelled || !ops->stage || !ops->prepare || !ops->resume || !ops->stage_enter)return 0;
 desk_ops=ops;desk_returned=true;desk_retained=false;desk_radios_off=false;
 portable_desk_config config;unsigned mode;
 if(!desk_config(&config,&mode))return -2;
 if(mode!=PORTABLE_SLEEP_DEEP)return 0;
 portable_desk_cycle cycle;
 if(!portable_desk_begin(&cycle,&config,&desk_boot_record,desk_boot_timer))return 0;
 desk_boot_timer=false;
 for(unsigned attempt=0;attempt<PORTABLE_DESK_CLOCK_MAX_PRESENTS;attempt++) {
  int64_t now;portable_desk_frame frame;
  if(desk_cancelled() || !desk_read(&now) || !portable_desk_plan_frame(&cycle,now,&frame))break;
  if(frame.has_previous) {
   if(!desk_scene(&cycle.current.config,frame.previous_minute))break;
   int seeded=portable_desk_adapter_seed();
   if(seeded<0)return -2;
   if(!seeded)frame.full=true;
  }
  if(!desk_scene(&config,frame.minute))break;
  bool shown=portable_desk_adapter_present(frame.full);
  if(!portable_desk_presented(&cycle,shown))return -2;
  if(desk_cancelled())break;
  /* Sampling/yield and RAM staging both precede peripheral holds. */
  uint32_t anchored_at=0,duration=0;int64_t anchor;
  portable_desk_record proposal;
  if(!desk_sample_edge(&anchor,&anchored_at))break;
  int plan=portable_desk_prepare_record(&cycle,anchor,0,&proposal,&duration);
  if(plan==PORTABLE_DESK_REPAINT)continue;
  if(plan!=PORTABLE_DESK_READY)break;
  int staged_status=ops->stage(ops->context,&proposal);
  if(staged_status==-2)return -2;
  if(staged_status!=1)break;
  int prepared=ops->prepare(ops->context);
  if(prepared==-2)return -2;
  if(prepared!=1)break;
  uint32_t sampled_at=0;int32_t microseconds=0;
  if(!desk_after_prepare(anchor,anchored_at,&now,&microseconds,&sampled_at))return desk_restore();
  plan=portable_desk_prepare_record(&cycle,now,microseconds,&proposal,&duration);
  if(plan==PORTABLE_DESK_READY && !desk_cancelled()) {
   int entered=ops->stage_enter(ops->context,&proposal,duration,sampled_at);
   if(entered!=0)return -2;
   return desk_restore();
  }
  if(desk_restore()==-2)return -2;
  if(plan!=PORTABLE_DESK_REPAINT)break;
  /* Resume invalidates panel history, so a catch-up cannot use diff seeding. */
  cycle.full=true;
 }
 if(desk_retained || !portable_desk_adapter_ready())return -2;
 portable_desk_adapter_invalidate();return 0;
}
static int desk_boot(void) {
 desk_boot_timer=false;desk_returned=false;
 int status=portable_desk_clock_boot_read(rt,&desk_boot_record);
 if(status==-2)return -2;
 if(status!=1)return 0;
 portable_desk_config config;unsigned mode;
 if(!desk_config(&config,&mode))return -2;
 if(mode!=PORTABLE_SLEEP_DEEP)return 0;
 desk_boot_timer=true;
 bool resumed=portable_desk_adapter_sleep();
 desk_boot_timer=false; /* Normal Clock drawing invalidates every boot checkpoint. */
 if(!resumed)return portable_app_sleep_retained()?-2:-1;
 return 0;
}
