/* Link the production controller, preference codecs and timezone/time planner.
 * The in-memory capability implements transport/faults, not model behavior. */
#include "PortableSettingsModel.h"
#include "PortableTimeFormat.h"
#include "PortableAlarmSettings.h"
#include "PortableSleepPolicy.h"
#include "PortableReaderPreferences.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {char key[16];unsigned n;uint8_t data[64];} Record;
static Record records[20],pending;static unsigned reads,writes,submits,collects,cancels;
static bool alive=true,fail_read,io_write,lose_after_write,corrupt,cancelled,cancel_loses,collect_fault;
static unsigned pending_steps;static uint64_t revision=1,token;
static int atomic_terminal=RISC_ATOMIC_OK;
static bool guard(void*c){(void)c;return alive;}
static Record *record(const char*k){for(unsigned i=0;i<20;i++)if(!strcmp(k,records[i].key))return &records[i];for(unsigned i=0;i<20;i++)if(!records[i].key[0]){strcpy(records[i].key,k);return &records[i];}assert(0);return NULL;}
static int32_t get(void*c,const char*k,void*p,uint32_t cap,uint32_t*n){(void)c;assert(alive);++reads;*n=0;if(fail_read)return RISC_KEY_VALUE_IO;Record*r=record(k);if(!r->n)return RISC_KEY_VALUE_NOT_FOUND;*n=r->n;if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(p,r->data,*n);if(corrupt)((uint8_t*)p)[0]^=1;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*p,uint32_t n){(void)c;assert(alive&&n<=64);++writes;Record*r=record(k);r->n=n;memcpy(r->data,p,n);if(lose_after_write)alive=false;return io_write?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static int32_t astate(void*c,risc_key_value_atomic_state_v1*s){(void)c;assert(alive);*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,512,176,0,revision};return 0;}
static int32_t aget(void*c,uint64_t rev,const char*k,void*p,uint32_t cap,uint32_t*n){(void)c;assert(alive);if(rev!=revision)return RISC_ATOMIC_STALE;int rc=get(NULL,k,p,cap,n);return rc==RISC_KEY_VALUE_OK?0:rc==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:RISC_ATOMIC_IO;}
static int32_t submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t n,uint64_t*t){(void)c;assert(alive&&!token&&count==1&&n==176);*t=0;if(rev!=revision)return RISC_ATOMIC_STALE;assert(!memcmp(intent,"PFI0001",7));const uint8_t*i=intent;assert(i[8]==PORTABLE_PREFERENCE_USER&&!strcmp((const char*)i+12,m->key));assert(portable_preference_crc(i,172)==portable_rtc_basis_word(i+172));pending=*record(m->key);pending.n=m->size;memcpy(pending.data,m->value,m->size);++submits;*t=token=submits;return 0;}
static int32_t collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){(void)c;assert(alive&&t==token);++collects;if(collect_fault)return -100;if(pending_steps){--pending_steps;return RISC_ATOMIC_PENDING;}int status=cancelled?RISC_ATOMIC_CANCELLED:atomic_terminal;if(status==0){*record(pending.key)=pending;++revision;}*r=(risc_key_value_atomic_receipt_v1){sizeof(*r),status,revision,status==0?submits:0};token=0;return 0;}
static int32_t cancel(void*c,uint64_t t){(void)c;assert(alive&&t==token);++cancels;cancelled=!cancel_loses;return 0;}
static risc_key_value_atomic_v1 atomic={1,sizeof(atomic),NULL,astate,aget,submit,collect,cancel};
static void reset(void){memset(records,0,sizeof(records));memset(&pending,0,sizeof(pending));reads=writes=submits=collects=cancels=0;alive=true;fail_read=io_write=lose_after_write=corrupt=cancelled=cancel_loses=collect_fault=false;pending_steps=0;revision=1;token=0;atomic_terminal=0;}
static void init(PortableSettingsModel*m,bool a){psm_policy p=psm_default_policy();psm_services s={.live=guard};assert(psm_init(m,&p,&s,&kv,a?&atomic:NULL));assert(!writes&&!submits);}
static void commit(PortableSettingsModel*m){assert(psm_save(m));unsigned before=writes+submits;psm_tick(m);assert(writes+submits==before);assert(!psm_presented(m,m->save_revision-1));assert(psm_presented(m,m->save_revision));psm_tick(m);while(m->phase==PSM_COMMITTING)psm_tick(m);assert(m->phase==PSM_IDLE);}
static void preferences(void){
 static const psm_feature fs[]={PSM_FORMAT,PSM_SLEEP,PSM_ALERT,PSM_FACE,PSM_FLIP,PSM_LANGUAGE,PSM_DIRECTION,PSM_IDLE_TIMER,PSM_DEEP_TIMER};
 static const unsigned vs[]={1,1,3,6,1,21,1,75000,420000};
 for(unsigned a=0;a<2;a++)for(unsigned i=0;i<sizeof(fs)/sizeof(*fs);i++){
  reset();PortableSettingsModel m;init(&m,a);assert(psm_open(&m,fs[i]));assert(psm_set_number(&m,vs[i]));assert(!writes&&!submits);commit(&m);assert(m.route==PSM_ROOT&&m.status[fs[i]]==PSM_LOADED);Record*r=record(psm_key(fs[i]));assert(r->n==(fs[i]==PSM_ALERT?1u:fs[i]>=PSM_IDLE_TIMER?5u:4u));PortableSettingsModel reopened;assert(psm_init(&reopened,&m.policy,&m.services,&kv,a?&atomic:NULL));assert(reopened.confirmed[fs[i]].number==vs[i]);assert(a?submits==1:writes==1);
 }
 /* Atomic encoded values decode through existing production codecs. */
 reset();PortableSettingsModel m;init(&m,true);assert(psm_open(&m,PSM_ALERT)&&psm_set_number(&m,2));commit(&m);unsigned v;assert(portable_alert_load(&kv,&v)==0&&v==2);
}
static void zones(void){
 for(unsigned a=0;a<2;a++)for(unsigned i=0;i<portable_timezone_count();i++){
  reset();PortableSettingsModel m;init(&m,a);assert(psm_open(&m,PSM_ZONE));const char*id=portable_timezone_get(i)->id;assert(psm_set_zone(&m,id,strlen(id)+1));commit(&m);char out[40];int rc=portable_timezone_preference_load(&kv,out);assert((rc==0||(i==0&&!a&&rc==1))&&!strcmp(out,id));
 }
}
static void faults(void){
 PortableSettingsModel m;reset();init(&m,false);assert(psm_open(&m,PSM_FORMAT));commit(&m);assert(!writes&&m.status[PSM_FORMAT]==PSM_MISSING);
 reset();init(&m,false);m.policy.sleep_modes=1;m.policy.sleep_default=0;assert(portable_sleep_save(&kv,2));assert(psm_refresh(&m)&&m.status[PSM_SLEEP]==PSM_UNSUPPORTED&&m.confirmed[PSM_SLEEP].number==0);assert(psm_open(&m,PSM_SLEEP)&&!psm_set_number(&m,2));psm_cancel(&m);assert(record("sleep_mode")->data[2]==2);
reset();init(&m,false);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));psm_cancel(&m);assert(!writes&&m.route==PSM_ROOT);
 assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));io_write=true;commit(&m);assert(m.uncertain&&m.status[PSM_FORMAT]==PSM_UNCONFIRMED&&m.draft.number==1);psm_cancel(&m);assert(m.route==PSM_EDITOR&&m.uncertain);io_write=false;commit(&m);assert(!m.uncertain&&m.route==PSM_ROOT);
 fail_read=true;assert(psm_refresh(&m));assert(m.status[PSM_FORMAT]==PSM_UNAVAILABLE&&m.confirmed[PSM_FORMAT].number==1);
 reset();init(&m,false);Record*r=record("alert_mode");r->n=1;r->data[0]=9;assert(psm_refresh(&m));assert(m.status[PSM_ALERT]==PSM_INVALID);assert(psm_open(&m,PSM_ALERT));assert(!psm_save(&m));assert(psm_set_number(&m,1));io_write=true;commit(&m);assert(m.status[PSM_ALERT]==PSM_LOADED);/* IO verified */
 reset();init(&m,false);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));lose_after_write=true;assert(psm_save(&m)&&psm_presented(&m,m.save_revision));unsigned before=reads;psm_tick(&m);assert(m.phase==PSM_RETAINED&&reads==before+1);before=reads+writes;psm_tick(&m);psm_cancel(&m);assert(!psm_refresh(&m)&&reads+writes==before&&!psm_can_handoff(&m));
 reset();init(&m,true);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));pending_steps=2;assert(psm_save(&m)&&psm_presented(&m,m.save_revision));psm_tick(&m);assert(token&&!psm_can_handoff(&m));psm_cancel(&m);while(m.phase==PSM_CANCELLING)psm_tick(&m);assert(!token&&submits==1&&cancels==1&&m.uncertain&&!record("time_format")->n);assert(psm_discard(&m)&&m.route==PSM_ROOT);
 reset();init(&m,true);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));++revision;commit(&m);assert(m.uncertain&&!submits&&!writes);assert(!psm_save(&m));assert(psm_reload_draft(&m)&&m.draft.number==1);commit(&m);assert(submits==1&&m.status[PSM_FORMAT]==PSM_LOADED);
 reset();init(&m,true);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));assert(psm_save(&m)&&psm_presented(&m,m.save_revision));psm_tick(&m);alive=false;unsigned calls=collects;psm_tick(&m);assert(m.phase==PSM_RETAINED&&m.atomic.token&&collects==calls);
}
static unsigned storage_state;
static int storage(void*c,unsigned*s){(void)c;*s=storage_state;return PSM_OK;}
static void policy_and_custody(void){
 PortableSettingsModel m;reset();psm_policy p=psm_default_policy();p.features=PSM_FEATURE(PSM_TIME);p.writable=p.features;psm_services services={.live=guard};assert(psm_init(&m,&p,&services,NULL,&atomic));assert(m.status[PSM_BASIS]==PSM_MISSING&&m.status[PSM_ZONE]==PSM_MISSING);
 reset();p=psm_default_policy();p.desk_lock=p.visual_alerts=true;assert(psm_init(&m,&p,&services,&kv,NULL));assert(psm_open(&m,PSM_SLEEP));assert(!psm_set_number(&m,1)&&!psm_save(&m));psm_cancel(&m);assert(psm_open(&m,PSM_ALERT)&&!psm_set_number(&m,2)&&!psm_save(&m));
 reset();init(&m,true);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));assert(psm_save(&m)&&psm_presented(&m,m.save_revision));psm_tick(&m);cancel_loses=true;psm_cancel(&m);psm_tick(&m);assert(m.route==PSM_ROOT&&m.status[PSM_FORMAT]==PSM_LOADED&&record("time_format")->n==4);
 reset();init(&m,true);assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1));assert(psm_save(&m)&&psm_presented(&m,m.save_revision));psm_tick(&m);collect_fault=true;psm_tick(&m);assert(m.phase==PSM_RETAINED&&m.atomic.token);unsigned before=collects;psm_tick(&m);assert(collects==before);
 reset();services.storage_state=storage;storage_state=PSM_STORAGE_UNAVAILABLE;assert(psm_init(&m,&p,&services,&kv,NULL));assert(psm_open(&m,PSM_FORMAT)&&psm_set_number(&m,1)&&!psm_save(&m)&&m.draft.number==1&&!writes);storage_state=PSM_STORAGE_AVAILABLE;assert(psm_reload_draft(&m));commit(&m);assert(writes==1);
}
static unsigned clocks;static bool clock_failure;static int clock_begin(void*c){(void)c;return 0;}
static int clock_read(void*c,portable_timezone_civil*v){(void)c;*v=(portable_timezone_civil){2026,11,4,12,0,0,0};return 0;}
static int clock_start(void*c,const portable_set_time_request*r,portable_set_time_result*out){(void)c;++clocks;portable_set_time_plan p;assert(portable_set_time_prepare(r,&p)==0);out->planned=true;out->plan=p;return PSM_PENDING;}
static int clock_poll(void*c,portable_set_time_result*out){(void)c;out->native.verified=!clock_failure;return clock_failure?PSM_ERROR:PSM_OK;}
static int clock_cancel(void*c,portable_set_time_result*out){(void)c;(void)out;return PSM_CANCELLED;}
static void time_planning(void){
 reset();PortableSettingsModel m;psm_policy p=psm_default_policy();psm_services s={.live=guard,.clock_begin=clock_begin,.clock_read=clock_read,.clock_start=clock_start,.clock_poll=clock_poll,.clock_cancel=clock_cancel};assert(portable_timezone_preference_save(&kv,"America/New_York",17)>=0);assert(psm_init(&m,&p,&s,&kv,NULL));assert(psm_open(&m,PSM_TIME));portable_timezone_civil c={2026,3,8,2,30,0,0};assert(psm_set_civil(&m,&c)&&!psm_save(&m)&&!clocks);c=(portable_timezone_civil){2026,11,1,1,30,0,0};assert(psm_set_civil(&m,&c)&&!psm_save(&m)&&m.route==PSM_FOLD);assert(psm_choose_fold(&m,1));commit(&m);assert(clocks==1&&m.clock_result.native.verified);assert(psm_open(&m,PSM_TIME));psm_cancel(&m);psm_tick(&m);assert(m.phase==PSM_IDLE&&m.route==PSM_ROOT&&!m.uncertain&&clocks==1);
 assert(psm_open(&m,PSM_TIME));clock_failure=true;commit(&m);assert(m.uncertain&&!m.clock_active&&!psm_save(&m));portable_timezone_civil kept=m.draft.civil;assert(psm_reload_draft(&m)&&!memcmp(&kept,&m.draft.civil,sizeof(kept)));clock_failure=false;commit(&m);assert(m.route==PSM_ROOT&&clocks==3);
}
int main(void){preferences();zones();faults();policy_and_custody();time_planning();puts("Settings model: all preference codecs, full timezone catalog, save barrier, cancellation, faults, retention and DST planning PASS");return 0;}
