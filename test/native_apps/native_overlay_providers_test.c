/* Bounded host-only namespace-1 preferences and ordered raw-input providers.
 * Included by both real-app overlay fixtures; namespace 6 remains independent. */
#include "RiscKeyValueAtomicV1.h"
#include "RiscKeyValueAvailabilityV1.h"
#include "PortablePreferenceAtomic.h"
#include "PortableQuickLayout.h"
#include "PortableAlarmLayout.h"
static unsigned ov_width,ov_height,ov_stride,ov_bytes,ov_latency,ov_submitted;
static unsigned ov_puts,ov_submits,ov_collects,ov_pairs,ov_retains,ov_injections;
static bool ov_pending,ov_inject,ov_torch;
static uint8_t *ov_storage,*ov_frame,*ov_frozen,*ov_underlay;
static uint64_t ov_revision=1,ov_token;
static unsigned ov_collect_phase;
static uint64_t ov_clock(void);
static struct {char key[16];uint8_t value[64];uint32_t size;} ov_cells[16],ov_copied[2];
static unsigned ov_copy_count,ov_quick_frames,ov_alarm_frames,ov_torch_frames,ov_pending_edges;
static int ov_index(const char *key,bool create){
 for(unsigned i=0;i<16;i++)if(!strcmp(ov_cells[i].key,key))return (int)i;
 if(create)for(unsigned i=0;i<16;i++)if(!ov_cells[i].key[0]){assert(strlen(key)<16);strcpy(ov_cells[i].key,key);return (int)i;}
 return -1;
}
static int32_t ov_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){
 (void)c;int i=ov_index(k,false);*n=i<0?0:ov_cells[i].size;if(!*n)return RISC_KEY_VALUE_NOT_FOUND;
 if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(out,ov_cells[i].value,*n);return RISC_KEY_VALUE_OK;
}
static int32_t ov_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;++ov_puts;return RISC_KEY_VALUE_INVALID;}
static const risc_key_value_v1 ov_kv={1,sizeof(ov_kv),NULL,ov_get,ov_put};
static int32_t ov_state(void*c,risc_key_value_atomic_state_v1*s){
 (void)c;*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,4096,1024,0,ov_revision};return RISC_ATOMIC_OK;
}
static int32_t ov_atomic_get(void*c,uint64_t rev,const char*k,void*out,uint32_t cap,uint32_t*n){
 if(rev!=ov_revision)return RISC_ATOMIC_STALE;
 int r=ov_get(c,k,out,cap,n);return r==RISC_KEY_VALUE_OK?RISC_ATOMIC_OK:r==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:RISC_ATOMIC_BUFFER_SMALL;
}
static int32_t ov_submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t n,uint64_t*t){
 (void)c;assert(!surface.frame&&!ov_token&&rev==ov_revision&&count>=1&&count<=2&&n==PORTABLE_PREFERENCE_INTENT_BYTES);
 const uint8_t*p=intent;assert(!memcmp(p,"PFI0001",8)&&p[8]==PORTABLE_PREFERENCE_USER&&!strcmp((const char*)p+12,m[0].key));
 assert(portable_preference_crc(p,172)==((uint32_t)p[172]|(uint32_t)p[173]<<8|(uint32_t)p[174]<<16|(uint32_t)p[175]<<24));
 ov_copy_count=count;for(unsigned i=0;i<count;i++){assert(m[i].kind==RISC_ATOMIC_PUT&&m[i].size<=64&&strlen(m[i].key)<16);strcpy(ov_copied[i].key,m[i].key);memcpy(ov_copied[i].value,m[i].value,m[i].size);ov_copied[i].size=m[i].size;}
 if(count==2){assert(!strcmp(m[0].key,PQA_VOLUME_KEY)&&!strcmp(m[1].key,PQA_RESTORE_VOLUME_KEY));assert(m[0].size==1&&*(const uint8_t*)m[0].value==0&&m[1].size==1);++ov_pairs;}
 ov_collect_phase=0;*t=ov_token=++ov_submits;return RISC_ATOMIC_OK;
}
static int32_t ov_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){
 (void)c;assert(t==ov_token&&t);++ov_collects;if(++ov_collect_phase<3)return RISC_ATOMIC_PENDING;
 for(unsigned i=0;i<ov_copy_count;i++){int j=ov_index(ov_copied[i].key,true);assert(j>=0);ov_cells[j].size=ov_copied[i].size;memcpy(ov_cells[j].value,ov_copied[i].value,ov_copied[i].size);}
 ++ov_revision;*r=(risc_key_value_atomic_receipt_v1){sizeof(*r),RISC_ATOMIC_OK,ov_revision,ov_submits};ov_token=0;return RISC_ATOMIC_OK;
}
static int32_t ov_cancel(void*c,uint64_t t){(void)c;assert(t==ov_token);return RISC_ATOMIC_PENDING;}
static const risc_key_value_atomic_v1 ov_atomic={1,sizeof(ov_atomic),NULL,ov_state,ov_atomic_get,ov_submit,ov_collect,ov_cancel};
static unsigned ov_availability_reads;
static int32_t ov_availability_snapshot(void*c,risc_key_value_availability_state_v1*s){
 (void)c;++ov_availability_reads;*s=(risc_key_value_availability_state_v1){.struct_size=sizeof(*s),.namespace_id=1,.availability=RISC_KEY_VALUE_AVAILABLE,.generation=1};return RISC_KEY_VALUE_OK;
}
static const risc_key_value_availability_v1 ov_availability={1,sizeof(ov_availability),NULL,ov_availability_snapshot};
static bool ov_battery_read(void*c,risc_battery_sample_v1*s){(void)c;*s=(risc_battery_sample_v1){.millivolts=4000,.percent=80};return true;}
static const risc_battery_gauge_api_v1 ov_battery={.api_version=1,.struct_size=sizeof(ov_battery),.read=ov_battery_read};
static bool ov_retain(void){++ov_retains;return true;}
static struct {uint64_t at;bool down;unsigned x,y;} ov_script[64];
static unsigned ov_script_count,ov_script_next,ov_raw_head,ov_raw_tail,ov_subscriptions;
static risc_touch_event_v1 ov_events[256];
static risc_touch_snapshot_v1 ov_raw;
static void ov_event(uint64_t at,bool down,unsigned x,unsigned y){assert(ov_script_count<64);ov_script[ov_script_count].at=at;ov_script[ov_script_count].down=down;ov_script[ov_script_count].x=x;ov_script[ov_script_count++].y=y;}
static void ov_tap(uint64_t at,unsigned x,unsigned y){ov_event(at,true,x,y);ov_event(at+60,false,x,y);}
static void ov_tap_rect(uint64_t at,pqa_rect r){ov_tap(at,(unsigned)(r.x+r.w/2),(unsigned)(r.y+r.h/2));}
static void ov_open(uint64_t at){ov_event(at,true,ov_width/2,0);ov_event(at+60,true,ov_width/2,ov_height-1);ov_event(at+120,false,ov_width/2,ov_height-1);}
static uint64_t ov_subscribe(void*c){(void)c;++ov_subscriptions;ov_raw_head=ov_raw_tail;return 19;}
static bool ov_unsubscribe(void*c,uint64_t t){(void)c;assert(t==19&&ov_subscriptions);--ov_subscriptions;return true;}
static bool ov_poll(void*c,size_t n){
 (void)c;assert(n==1);while(ov_script_next<ov_script_count&&ov_script[ov_script_next].at<=ov_clock()){
  unsigned i=ov_script_next++,kind=0;bool before=ov_raw.contact_count!=0;
  if(before&&!ov_script[i].down)kind=RISC_TOUCH_EVENT_UP;else if(!before&&ov_script[i].down)kind=RISC_TOUCH_EVENT_DOWN;
  else if(before&&(ov_raw.contacts[0].x!=ov_script[i].x||ov_raw.contacts[0].y!=ov_script[i].y))kind=RISC_TOUCH_EVENT_MOVE;
  if(kind){if(ov_pending)++ov_pending_edges;assert(ov_raw_tail<256);ov_events[ov_raw_tail++]=(risc_touch_event_v1){.sequence=++ov_raw.sequence,.timestamp_ms=ov_script[i].at,.kind=kind,.id=1,.x=ov_script[i].x,.y=ov_script[i].y};}
  ov_raw.contact_count=ov_script[i].down?1:0;ov_raw.contacts[0]=(risc_touch_contact_v1){.id=1,.x=ov_script[i].x,.y=ov_script[i].y};ov_raw.timestamp_ms=ov_script[i].at;
 }return true;
}
static int32_t ov_next(void*c,uint64_t id,risc_touch_event_v1*e){(void)c;assert(id==19);if(ov_raw_head==ov_raw_tail)return 0;*e=ov_events[ov_raw_head++];return 1;}
static bool ov_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=ov_raw;s->width=ov_width;s->height=ov_height;return true;}
static const risc_touch_api_v1 ov_touch={1,sizeof(ov_touch),NULL,ov_subscribe,ov_unsubscribe,ov_poll,ov_next,ov_snapshot};
static void ov_allocate(unsigned w,unsigned h){
 ov_width=w;ov_height=h;ov_stride=w*2+6;ov_bytes=ov_stride*h;ov_storage=malloc(ov_bytes+64);ov_frozen=malloc(ov_bytes);ov_underlay=malloc(ov_bytes);assert(ov_storage&&ov_frozen&&ov_underlay);memset(ov_storage,0xa5,ov_bytes+64);ov_frame=ov_storage+32;
}
static void ov_guards(void){
 for(unsigned i=0;i<32;i++)assert(ov_storage[i]==0xa5&&ov_storage[32+ov_bytes+i]==0xa5);
 for(unsigned y=0;y<ov_height;y++)for(unsigned x=ov_width*2;x<ov_stride;x++)assert(ov_frame[y*ov_stride+x]==0xa5);
}
static void ov_free(void){ov_guards();free(ov_storage);free(ov_frozen);free(ov_underlay);}
static void ov_copy_underlay(void){memcpy(ov_underlay,ov_frame,ov_bytes);}
static void ov_check_underlay(void){assert(!memcmp(ov_underlay,ov_frame,ov_bytes));ov_guards();}
static bool ov_display_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){
 (void)c;assert(t&&ov_pending&&!memcmp(ov_frozen,ov_frame,ov_bytes));s->state=ov_clock()-ov_submitted<ov_latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE)ov_pending=false;return true;
}
static void ov_overlay_script(unsigned which){
 uint64_t at=ov_clock()+100;ov_open(at);pqa_layout l=pqa_layout_for(&quick.ui);
 if(which==0)ov_tap_rect(at+1200,l.handle);
 if(which==1){ov_tap_rect(at+900,l.tiles[0]);ov_tap_rect(at+1700,l.handle);}
 if(which==2||which==3){ov_inject=true;ov_torch=which==3;if(ov_torch)ov_tap_rect(at+900,l.tiles[5]);}
}
static void ov_schedule_dismiss(void){
 portable_alarm_layout l;assert(portable_alarm_layout_for(ov_width,ov_height,false,&l));ov_tap(ov_clock()+600,(unsigned)(l.dismiss.x+l.dismiss.w/2),(unsigned)(l.dismiss.y+l.dismiss.h/2));++ov_injections;
}
static void ov_drain(void){assert(portable_paper_frame_drain());for(unsigned n=0;quick_restore_pending;n++){assert(n<1000);assert(quick_restore_step());assert(portable_paper_frame_drain());}assert(!failed&&!ov_pending);}
static void ov_run(uint64_t until){while(ov_clock()<until){t5_app_input_t in;assert(poll(&in,20));assert(!in.tapped&&!in.buttons&&!in.exit_requested);assert(ov_clock()<120000);}ov_drain();}
static void ov_saved_pair(void){
 assert(ov_pairs==1&&ov_submits==1&&!ov_puts&&!ov_token);int a=ov_index(PQA_VOLUME_KEY,false),b=ov_index(PQA_RESTORE_VOLUME_KEY,false);assert(a>=0&&b>=0&&ov_cells[a].size==1&&ov_cells[a].value[0]==0&&ov_cells[b].size==1&&ov_cells[b].value[0]==50);
 pqa_session loaded;pqa_session_init(&loaded);assert(pqa_session_load(&loaded,rt));assert(loaded.volume==0&&loaded.restore_volume==50);
}
