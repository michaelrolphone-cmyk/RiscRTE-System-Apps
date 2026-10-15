/* Optional observation only. No logging, allocation, provider polling or ABI
 * extension: bounded summaries use the existing runtime trace suffix. */
#ifdef PORTABLE_STAGE_PROFILE
enum {STAGE_RENDER,STAGE_POLL,STAGE_CATALOG,STAGE_DISPLAY,STAGE_INPUT,STAGE_COUNT};
static struct {
 bool enabled,drawing,first_draw,first_submit,first_complete;
 uint32_t epoch,render_started,frames;
 struct {uint32_t count,total,max;} stage[STAGE_COUNT];
} stage_profile;
static bool stage_profile_clock(uint32_t *out) {
 if(!rt || !stage_profile.enabled || failed || app_invocation_retained())return false;
 if(!rt->monotonic_ms(out)){stage_profile.enabled=false;return false;}
 return true;
}
static uint32_t stage_profile_start(void) {
 uint32_t now=0;(void)stage_profile_clock(&now);return now;
}
static void stage_profile_reset(void) {
 memset(&stage_profile,0,sizeof(stage_profile));
 stage_profile.enabled=rt->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE && rt->monotonic_ms &&
  rt->struct_size>=RISC_RUNTIME_TRACE_V1_SIZE && rt->trace;
 stage_profile.epoch=stage_profile_start();
}
static void stage_profile_trace(unsigned phase,uint32_t value) {
 if(stage_profile.enabled && !failed && !app_invocation_retained())(void)rt->trace(0,phase,value);
}
static uint32_t stage_profile_add(uint32_t old,uint32_t add) {return UINT32_MAX-old<add?UINT32_MAX:old+add;}
static void stage_profile_value(unsigned stage,uint32_t elapsed) {
 if(!stage_profile.enabled || failed || app_invocation_retained() || stage>=STAGE_COUNT)return;
 stage_profile.stage[stage].count=stage_profile_add(stage_profile.stage[stage].count,1);
 stage_profile.stage[stage].total=stage_profile_add(stage_profile.stage[stage].total,elapsed);
 if(elapsed>stage_profile.stage[stage].max)stage_profile.stage[stage].max=elapsed;
}
static void stage_profile_finish(unsigned stage,uint32_t started) {
 uint32_t now;if(stage_profile_clock(&now))stage_profile_value(stage,now-started);
}
static void stage_profile_counter(unsigned tag,uint32_t value) {
 if(value>0xffffffu)value=0xffffffu;
 stage_profile_trace(8u,((uint32_t)tag<<24)|value);
}
static void stage_profile_flush(uint32_t now,bool force) {
 if(!stage_profile.enabled || failed || app_invocation_retained() || (!force && (uint32_t)(now-stage_profile.epoch)<2000u))return;
 bool any=false;for(unsigned i=0;i<STAGE_COUNT;i++)any|=stage_profile.stage[i].count!=0;
 if(!any)return;
 /* Tags 0x40..0x4e contain count, total_ms and max_ms for each stage.
  * At most 15 ring records per two seconds, plus one final exit summary.
  * Slow/short gestures do not have to accumulate a minimum frame count. */
 for(unsigned i=0;i<STAGE_COUNT;i++) {
  stage_profile_counter(0x40u+i*3u,stage_profile.stage[i].count);
  stage_profile_counter(0x41u+i*3u,stage_profile.stage[i].total);
  stage_profile_counter(0x42u+i*3u,stage_profile.stage[i].max);
 }
 memset(stage_profile.stage,0,sizeof(stage_profile.stage));stage_profile.frames=0;stage_profile.epoch=now;
}
static void stage_profile_render_begin(void) {
 if(!stage_profile_clock(&stage_profile.render_started))return;
 stage_profile.drawing=true;
 if(!stage_profile.first_draw){stage_profile.first_draw=true;stage_profile_trace(4u,0);}
}
static void stage_profile_render_end(void) {
 uint32_t now;if(!stage_profile.drawing || !stage_profile_clock(&now))return;
 stage_profile.drawing=false;stage_profile_value(STAGE_RENDER,now-stage_profile.render_started);
 stage_profile.frames=stage_profile_add(stage_profile.frames,1);stage_profile_flush(now,false);
}
static void stage_profile_boundary(bool exiting) {
 uint32_t now;if(stage_profile_clock(&now))stage_profile_flush(now,exiting);
}
static inline void stage_profile_submit(void) {
 if(!stage_profile.first_submit){stage_profile.first_submit=true;stage_profile_trace(5u,0);}
}
static inline void stage_profile_complete(uint32_t started) {
 stage_profile_finish(STAGE_DISPLAY,started);
 if(!stage_profile.first_complete){stage_profile.first_complete=true;stage_profile_trace(6u,0);}
}
#else
#define stage_profile_reset() ((void)0)
#define stage_profile_start() 0u
#define stage_profile_finish(stage,started) ((void)(started))
#define stage_profile_value(stage,elapsed) ((void)0)
#define stage_profile_render_begin() ((void)0)
#define stage_profile_render_end() ((void)0)
#define stage_profile_boundary(exiting) ((void)0)
#define stage_profile_submit() ((void)0)
#define stage_profile_complete(started) ((void)0)
#endif
