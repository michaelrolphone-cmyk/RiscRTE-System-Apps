/* Strict session doubles: no hardware, and every callback rejects activity
 * after a terminal refusal. Exact-sized legacy allocations expose overreads. */
#include "PortableQuickSession.h"
#include "PortableTimeFormat.h"
#include "RiscDisplayOutputFrontlightV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BRIGHTNESS, RESTORE_BRIGHTNESS, VOLUME, RESTORE_VOLUME, DND, TONE, KEYS };
typedef struct { uint8_t bytes[4];uint32_t size;bool present; } record;
static record records[KEYS];
static unsigned gets[KEYS],put_count[KEYS],acquires,releases,live,activity;
static unsigned tone_gets,tone_sets,brightness_sets,hardware_brightness;
static uint16_t hardware_warm,hardware_maximum;
static bool dead,unavailable,invalid_kv,release_failure,brightness_failure;
static int fail_get,fail_put,fail_readback,mismatch,committed_io;
static int getter_status;
static unsigned fail_set_number;
static char order[4096];static size_t order_size;
static risc_display_output_api_v1_frontlight display;

static void called(char kind) {
 assert(!dead);activity++;assert(order_size+1<sizeof(order));
 order[order_size++]=kind;order[order_size]=0;
}
static int key_index(const char *key) {
 const char *keys[]={PQA_BRIGHTNESS_KEY,PQA_RESTORE_BRIGHTNESS_KEY,PQA_VOLUME_KEY,
                     PQA_RESTORE_VOLUME_KEY,PQA_DND_KEY,PQA_TONE_KEY};
 for(unsigned i=0;i<KEYS;i++)if(!strcmp(key,keys[i]))return (int)i;
 assert(!strcmp(key,PORTABLE_TIME_FORMAT_KEY));return -1;
}
static int32_t get(void *context,const char *key,void *data,uint32_t capacity,uint32_t *size) {
 (void)context;called('G');int i=key_index(key);*size=0;
 if(i<0)return RISC_KEY_VALUE_NOT_FOUND;
 gets[i]++;
 if(i==fail_get || (i==fail_readback && put_count[i]))return RISC_KEY_VALUE_IO;
 if(!records[i].present)return RISC_KEY_VALUE_NOT_FOUND;
 *size=records[i].size;if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(data,records[i].bytes,*size);
 if(i==mismatch && put_count[i] && *size)((uint8_t *)data)[0]^=1;
 return RISC_KEY_VALUE_OK;
}
static int32_t put(void *context,const char *key,const void *data,uint32_t size) {
 (void)context;called('P');int i=key_index(key);assert(i>=0 && size==1);put_count[i]++;
 if(i==fail_put)return RISC_KEY_VALUE_IO;
 records[i].present=true;records[i].size=size;memcpy(records[i].bytes,data,size);
 return i==committed_io?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
 called('A');assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY) && version==1 && instance==1);
 acquires++;if(unavailable)return false;
 kv.api_version=invalid_kv?0:1;out->api=&kv;live++;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
 called('R');assert(grant->api && live);releases++;
 if(release_failure){dead=true;return false;}
 live--;grant->api=NULL;return true;
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static bool set_brightness(void *context,uint16_t value,uint16_t maximum) {
 (void)context;called('B');assert(value<=100 && maximum==100);brightness_sets++;
 if(brightness_failure){dead=true;return false;}
 hardware_brightness=value;return true;
}
static bool set_tone(void *context,uint16_t value,uint16_t maximum) {
 (void)context;called('S');assert(value<=100 && maximum==100);tone_sets++;
 if(tone_sets==fail_set_number) {
  hardware_warm=17;hardware_maximum=100;dead=true;return false;
 }
 hardware_warm=value;hardware_maximum=maximum;return true;
}
static int32_t get_tone(void *context,uint16_t *warm,uint16_t *maximum) {
 (void)context;called('T');tone_gets++;
 if(getter_status!=RISC_DISPLAY_TONE_OK) {
  if(getter_status!=RISC_DISPLAY_TONE_UNAVAILABLE)dead=true;
  return getter_status;
 }
 *warm=hardware_warm;*maximum=hardware_maximum;return RISC_DISPLAY_TONE_OK;
}
/* Discovery must only inspect these prefix entries, never call them. */
static bool seed(void *context,risc_display_frame_v1 frame) {(void)context;(void)frame;assert(false);return false;}
static int32_t power(void *context,uint32_t timeout) {(void)context;(void)timeout;assert(false);return -1;}
static bool metrics(void *context,risc_display_present_metrics_v1 *out) {(void)context;(void)out;assert(false);return false;}
static bool copy(void *context,uint32_t format,void *pixels,size_t size,uint32_t stride) {
 (void)context;(void)format;(void)pixels;(void)size;(void)stride;assert(false);return false;
}
static risc_display_output_api_v1 *base(void) {return &display.snapshot.metrics.power.history.base;}
static pqa_session fresh(void) {
 memset(records,0,sizeof(records));memset(gets,0,sizeof(gets));memset(put_count,0,sizeof(put_count));
 acquires=releases=live=activity=tone_gets=tone_sets=brightness_sets=0;
 hardware_brightness=0;hardware_warm=33;hardware_maximum=100;
 dead=unavailable=invalid_kv=release_failure=brightness_failure=false;
 fail_get=fail_put=fail_readback=mismatch=committed_io=-1;
 getter_status=RISC_DISPLAY_TONE_OK;fail_set_number=0;order_size=0;order[0]=0;
 memset(&display,0,sizeof(display));
 *base()=(risc_display_output_api_v1){.api_version=1,.struct_size=sizeof(display),.set_brightness=set_brightness};
 display.snapshot.metrics.power.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;
 display.snapshot.metrics.power.history.extension_version=1;
 display.snapshot.metrics.power.history.seed_previous=seed;
 display.snapshot.metrics.power.power_tag=RISC_DISPLAY_POWER_TAG;
 display.snapshot.metrics.power.power_version=1;
 display.snapshot.metrics.power.prepare=power;display.snapshot.metrics.power.resume=power;
 display.snapshot.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;
 display.snapshot.metrics.metrics_version=RISC_DISPLAY_METRICS_VERSION;
 display.snapshot.metrics.snapshot=metrics;
 display.snapshot.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;
 display.snapshot.snapshot_version=RISC_DISPLAY_SNAPSHOT_VERSION;
 display.snapshot.copy_completed=copy;
 display.frontlight_tag=RISC_DISPLAY_FRONTLIGHT_TAG;display.frontlight_version=RISC_DISPLAY_FRONTLIGHT_VERSION;
 display.set_tone=set_tone;display.get_tone=get_tone;
 pqa_session s;pqa_session_init(&s);s.ui.paper=true;return s;
}
static void saved(unsigned key,unsigned value,uint32_t size) {
 records[key].present=true;records[key].size=size;records[key].bytes[0]=(uint8_t)value;
}
static void load(pqa_session *s) {assert(pqa_session_load(s,&runtime));assert(!live);}
static void apply(pqa_session *s,uint32_t actions) {
 bool changed=true;assert(pqa_session_apply(s,&runtime,base(),actions,&changed));assert(!live && !changed);
}
static void tone_action(pqa_session *s,unsigned value,uint32_t action) {
 s->ui.tone=s->ui.action_tone=(uint8_t)value;apply(s,action);
}
static void no_later_activity(pqa_session *s) {
 assert(s->retained);unsigned previous=activity;bool changed=true;
 assert(!pqa_session_sync_tone(s,base()));assert(!pqa_session_sync_brightness(s,base()));
 assert(!pqa_session_load(s,&runtime));assert(!pqa_session_restore(s,base()));
 assert(!pqa_session_apply(s,&runtime,base(),PQA_TONE_COMMIT|PQA_BRIGHTNESS_COMMIT|PQA_DND,&changed));
 assert(!changed && activity==previous);
}
static void defaults_and_storage(void) {
 pqa_session s=fresh();load(&s);
 assert(s.tone==50 && s.ui.tone==50 && s.ui.tone_valid && !put_count[TONE]);
 assert(pqa_session_sync_tone(&s,base()));
 assert(s.ui.tone_controls && s.ui.applied_tone_valid && s.ui.applied_tone==50);
 assert(hardware_warm==50 && hardware_brightness==0 && !brightness_sets && !put_count[TONE]);
 tone_action(&s,80,PQA_TONE_COMMIT);records[TONE].present=false;load(&s);
 assert(s.tone==50 && s.ui.tone_valid); /* Missing always means virtual 50. */
 for(unsigned size=0;size<=2;size++)for(unsigned value=0;value<=255;value++) {
  s=fresh();saved(TONE,value,size);load(&s);
  bool valid=size==1 && value<=100;
  assert(s.ui.tone_valid==valid && s.tone==(valid?value:50));
  assert(pqa_session_sync_tone(&s,base()));
  assert(s.ui.tone_controls && s.ui.applied_tone_valid && !put_count[TONE]);
  assert(tone_sets==(valid?1u:0u) && !brightness_sets && hardware_brightness==0);
  if(!valid)assert(s.ui.applied_tone==33);
 }
 for(unsigned mode=0;mode<3;mode++) {
  s=fresh();unavailable=mode==0;invalid_kv=mode==1;if(mode==2)fail_get=TONE;
  load(&s);assert(!s.ui.tone_valid && s.tone==50);
  assert(pqa_session_sync_tone(&s,base()));assert(!tone_sets && s.ui.applied_tone==33);
  unsigned before=acquires;tone_action(&s,70,PQA_TONE_COMMIT);
  assert(!tone_sets && !put_count[TONE] && acquires==before && (s.ui.error_flags&PQA_ERROR_SAVE));
 }
 s=fresh();saved(TONE,80,1);load(&s);fail_get=TONE;load(&s);
 assert(s.tone==80 && !s.ui.tone_valid);assert(pqa_session_sync_tone(&s,base()));
 assert(!tone_sets && s.ui.applied_tone==33);
}
static void legacy_and_malformed_tables(void) {
 const size_t sizes[]={sizeof(risc_display_output_api_v1),sizeof(risc_display_output_api_v1_history),
  sizeof(risc_display_output_api_v1_power),sizeof(risc_display_output_api_v1_metrics),
  sizeof(risc_display_output_api_v1_snapshot),sizeof(display)-1};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
  pqa_session s=fresh();load(&s);void *allocation=malloc(sizes[i]);assert(allocation);
  memcpy(allocation,&display,sizes[i]);risc_display_output_api_v1 *legacy=allocation;
  legacy->struct_size=(uint32_t)sizes[i];assert(pqa_session_sync_brightness(&s,legacy));
  assert(!tone_gets && !tone_sets && brightness_sets==1 && !s.ui.tone_controls);
  s.ui.action_tone=90;bool changed=true;
  assert(pqa_session_apply(&s,&runtime,legacy,PQA_TONE_COMMIT,&changed));assert(!put_count[TONE]);
  free(allocation);
 }
 for(unsigned fault=0;fault<18;fault++) {
  pqa_session s=fresh();load(&s);
  switch(fault) {
   case 0:base()->api_version=0;break;
   case 1:display.snapshot.metrics.power.history.extension_tag=0;break;
   case 2:display.snapshot.metrics.power.history.extension_version=2;break;
   case 3:display.snapshot.metrics.power.history.seed_previous=NULL;break;
   case 4:display.snapshot.metrics.power.power_tag=0;break;
   case 5:display.snapshot.metrics.power.power_version=2;break;
   case 6:display.snapshot.metrics.power.prepare=NULL;break;
   case 7:display.snapshot.metrics.power.resume=NULL;break;
   case 8:display.snapshot.metrics.metrics_tag=0;break;
   case 9:display.snapshot.metrics.metrics_version=2;break;
   case 10:display.snapshot.metrics.snapshot=NULL;break;
   case 11:display.snapshot.snapshot_tag=0;break;
   case 12:display.snapshot.snapshot_version=2;break;
   case 13:display.snapshot.copy_completed=NULL;break;
   case 14:display.frontlight_tag=0;break;
   case 15:display.frontlight_version=2;break;
   case 16:display.set_tone=NULL;break;
   case 17:display.get_tone=NULL;break;
  }
  assert(pqa_session_sync_brightness(&s,base()));
  assert(!s.ui.tone_controls && !s.ui.applied_tone_valid && !tone_gets && brightness_sets==1);
 }
 pqa_session s=fresh();load(&s);getter_status=RISC_DISPLAY_TONE_UNAVAILABLE;
 assert(pqa_session_sync_brightness(&s,base()));
 assert(!s.ui.tone_controls && tone_gets==1 && !tone_sets && brightness_sets==1);
 tone_action(&s,80,PQA_TONE_COMMIT);assert(!put_count[TONE] && !tone_sets);
 s=fresh();s.ui.paper=false;load(&s);unsigned before=activity;
 assert(pqa_session_sync_tone(&s,base()));tone_action(&s,80,PQA_TONE_COMMIT);
 assert(activity==before && !s.ui.tone_controls);
}
static void ratio_matrix(void) {
 const uint16_t ratios[][2]={{0,1},{1,1},{1,2},{1,3},{2,3},{65535,65535},{32768,65535},{1,65535}};
 for(unsigned i=0;i<sizeof(ratios)/sizeof(ratios[0]);i++) {
  pqa_session s=fresh();fail_get=TONE;load(&s);hardware_warm=ratios[i][0];hardware_maximum=ratios[i][1];
  assert(pqa_session_sync_tone(&s,base()));
  unsigned expected=((uint32_t)hardware_warm*100u+hardware_maximum/2u)/hardware_maximum;
  assert(s.ui.applied_tone_valid && s.ui.applied_tone==expected && !tone_sets);
 }
 for(unsigned mode=0;mode<4;mode++) {
  pqa_session s=fresh();load(&s);
  if(mode==0)hardware_maximum=0;
  if(mode==1){hardware_warm=101;hardware_maximum=100;}
  if(mode==2)getter_status=RISC_DISPLAY_TONE_FAILED;
  if(mode==3)getter_status=17;
  assert(!pqa_session_sync_brightness(&s,base()));
  assert(s.ui.tone_controls && !s.ui.applied_tone_valid && (s.ui.error_flags&PQA_ERROR_TONE));
  assert(!tone_sets && !brightness_sets && !put_count[TONE]);no_later_activity(&s);
 }
}
static void preview_commit_cancel(void) {
 pqa_session s=fresh();load(&s);assert(pqa_session_sync_brightness(&s,base()));
 tone_action(&s,80,PQA_TONE_PREVIEW);
 assert(s.tone==50 && hardware_warm==80 && s.ui.applied_tone==80 && !put_count[TONE]);
 s.ui.saved_tone=50;s.ui.gesture=PQA_TONE_DRAG;pqa_cancel_input(&s.ui);
 uint32_t action=pqa_take_action(&s.ui);assert(action&PQA_TONE_PREVIEW);apply(&s,action);
 assert(s.ui.tone==50 && hardware_warm==50 && !put_count[TONE]);
 tone_action(&s,90,PQA_TONE_PREVIEW|PQA_TONE_COMMIT);
 assert(s.tone==90 && records[TONE].bytes[0]==90 && put_count[TONE]==1);
 pqa_session next;pqa_session_init(&next);next.ui.paper=true;load(&next);
 assert(next.tone==90 && next.ui.tone_valid && !next.ui.applied_tone_valid);
 assert(pqa_session_sync_tone(&next,base()));assert(next.ui.applied_tone==90);
 assert(hardware_brightness==40 && !put_count[BRIGHTNESS] && !put_count[VOLUME] && !put_count[DND]);
}
static void off_restore_sequences(void) {
 pqa_session s=fresh();saved(BRIGHTNESS,70,1);load(&s);assert(pqa_session_sync_brightness(&s,base()));
 for(unsigned cycle=0;cycle<12;cycle++) {
  s.ui.action_brightness=0;apply(&s,PQA_BRIGHTNESS_COMMIT);
  assert(hardware_brightness==0 && s.brightness==0 && s.restore_brightness==70);
  unsigned expected=cycle%2?100:0;
  tone_action(&s,expected,PQA_TONE_COMMIT);assert(hardware_brightness==0 && hardware_warm==expected);
  tone_action(&s,50,PQA_TONE_PREVIEW);assert(hardware_brightness==0 && hardware_warm==50);
  tone_action(&s,expected,PQA_TONE_PREVIEW);assert(s.tone==expected);
  assert(pqa_session_restore(&s,base()) && hardware_brightness==0);
  assert(pqa_session_sync_tone(&s,base()) && hardware_brightness==0);
  s.ui.action_brightness=70;apply(&s,PQA_BRIGHTNESS_COMMIT);
  assert(hardware_brightness==70 && hardware_warm==expected && s.ui.applied_tone==expected);
 }
 assert(!put_count[VOLUME] && !put_count[DND]);
}
static void save_and_custody_failures(void) {
 for(unsigned mode=0;mode<7;mode++) {
  pqa_session s=fresh();load(&s);assert(pqa_session_sync_tone(&s,base()));
  if(mode==1)committed_io=TONE;
  if(mode==2)fail_put=TONE;
  if(mode==3)fail_readback=TONE;
  if(mode==4)mismatch=TONE;
  if(mode==5)unavailable=true;
  if(mode==6)invalid_kv=true;
  tone_action(&s,80,PQA_TONE_COMMIT);
  bool confirmed=mode<=1;
  assert(s.tone==(confirmed?80u:50u) && hardware_warm==s.tone && s.ui.tone==s.tone);
  assert(s.ui.applied_tone_valid && s.ui.applied_tone==s.tone);
  assert(!!(s.ui.error_flags&PQA_ERROR_SAVE)==!confirmed && !s.retained);
 }
 for(unsigned mode=0;mode<5;mode++) {
  pqa_session s=fresh();load(&s);assert(pqa_session_sync_tone(&s,base()));
  unsigned acquired=acquires,sets=tone_sets;bool changed=true;
  if(mode==0)getter_status=RISC_DISPLAY_TONE_FAILED;
  if(mode==1)fail_set_number=tone_sets+1;
  if(mode==2)release_failure=true;
  if(mode==3){fail_put=TONE;fail_set_number=tone_sets+2;}
  if(mode==4){hardware_warm=101;hardware_maximum=100;}
  s.ui.action_tone=80;s.ui.action_brightness=60;s.ui.action_dnd=true;
  assert(!pqa_session_apply(&s,&runtime,base(),PQA_TONE_COMMIT|PQA_BRIGHTNESS_COMMIT|PQA_DND,&changed));
  assert(!s.ui.applied_tone_valid && (s.ui.error_flags&PQA_ERROR_TONE) && s.tone==50);
  assert(!brightness_sets && !put_count[BRIGHTNESS] && !put_count[DND]);
  if(mode==0 || mode==1 || mode==4)assert(acquires==acquired && !put_count[TONE]);
  if(mode==0 || mode==4)assert(tone_sets==sets);
  if(mode==2)assert(live==1 && tone_sets==sets+1);
  if(mode==3)assert(!live && tone_sets==sets+2);
  no_later_activity(&s);
 }
 pqa_session s=fresh();release_failure=true;assert(!pqa_session_load(&s,&runtime));
 assert(live==1 && !tone_gets && !tone_sets);no_later_activity(&s);
 s=fresh();load(&s);fail_set_number=1;assert(!pqa_session_sync_brightness(&s,base()));
 assert(!brightness_sets && !put_count[TONE]);no_later_activity(&s);
 s=fresh();load(&s);brightness_failure=true;assert(!pqa_session_sync_brightness(&s,base()));
 assert(!s.ui.applied_brightness_valid);no_later_activity(&s);
}
int main(void) {
 defaults_and_storage();legacy_and_malformed_tables();ratio_matrix();preview_commit_cancel();
 off_restore_sequences();save_and_custody_failures();
 puts("Frontlight tone session: legacy-size bounds, optional discovery, ratios, preferences, preview/commit/cancel, OFF/restore and terminal custody passed");
 return 0;
}
