#include "PortableContextsClient.h"
#include <assert.h>
#include <stdio.h>
typedef struct {char name[16];uint8_t bytes[64];uint32_t size;} record;
static record records[32];
static uint32_t tick,reads,writes,steps,pauses,claims,results,releases,acquires,grants;
static bool health_ok=true,pause_ok=true,release_ok=true,write_commit=true;
static const char *bad_read,*bad_write;
static int32_t write_result;
static contexts_policy_v1 last_policy;
static contexts_status_v1 state;
static record *find(const char *key,bool create) {
    for(unsigned i=0;i<32;i++)if(!strcmp(records[i].name,key))return records+i;
    if(create)for(unsigned i=0;i<32;i++)if(!records[i].name[0]){assert(strlen(key)<16);strcpy(records[i].name,key);return records+i;}
    return NULL;
}
static int32_t get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size) {
    (void)c;reads++;*size=0;if(bad_read&&!strcmp(key,bad_read))return RISC_KEY_VALUE_IO;
    record *r=find(key,false);if(!r||!r->size)return RISC_KEY_VALUE_NOT_FOUND;
    *size=r->size;if(capacity<r->size)return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(out,r->bytes,r->size);return 0;
}
static int32_t put(void *c,const char *key,const void *bytes,uint32_t size) {
    (void)c;writes++;assert(size<=64);
    if(bad_write&&!strcmp(key,bad_write))return RISC_KEY_VALUE_IO;
    if(write_commit){record *r=find(key,true);assert(r);memcpy(r->bytes,bytes,size);r->size=size;}
    return write_result;
}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool pause_service(void *c){(void)c;pauses++;if(!pause_ok)return false;state.cleanup_pending=false;state.state=CONTEXTS_PAUSED;return true;}
static bool step_service(void *c,const contexts_policy_v1 *p){(void)c;assert(!state.cleanup_pending);steps++;last_policy=*p;state.state=p->enabled?CONTEXTS_LIVE:CONTEXTS_OFF;return true;}
static bool status_service(void *c,contexts_status_v1 *out){(void)c;assert(out->struct_size==sizeof(*out));*out=state;return true;}
static bool request(void *c,uint32_t mask){(void)c;(void)mask;return true;}
static bool begin(void *c,uint32_t source){(void)c;(void)source;return true;}
static bool export(void *c,uint32_t s,uint32_t k,uint32_t i,const void *b,uint32_t n){(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool finish(void *c,uint32_t s,uint32_t result){(void)c;(void)s;(void)result;return true;}
static int32_t label(void *c,uint32_t source,uint32_t slot,contexts_label_v1 *out){(void)c;(void)source;(void)slot;(void)out;return 0;}
static bool claim(void *c,uint32_t source,uint32_t slot,const char *name,uint32_t generation) {
    (void)c;assert(!strcmp(name,"Study"));
    if(state.audio.room_entry==state.audio.preset_entry)return false;
    state.audio.preset_entry=state.audio.room_entry;
    claims++;state.preset_source=source;state.preset_slot=(int32_t)slot;state.preset_generation=generation;state.preset_result=CONTEXTS_PRESET_CLAIMED;return true;
}
static bool result(void *c,uint32_t source,uint32_t generation,uint32_t value){(void)c;assert(source==state.preset_source&&generation==state.preset_generation);results++;state.preset_result=value;return true;}
static const contexts_service_v1 service={1,sizeof(service),NULL,step_service,pause_service,status_service,request,begin,export,finish,label,claim,result};
static bool acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *g) {
    assert(api==1);acquires++;grants++;
    if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);g->api=&kv;}
    else {assert(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)&&!instance);g->api=&service;}
    return true;
}
static bool release(risc_runtime_capability_v1 *g){releases++;if(g->api==&kv&&!release_ok)return false;assert(grants);grants--;g->api=NULL;return true;}
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=tick;return health_ok;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.acquire=acquire,.release=release};
static bool face(const risc_key_value_v1 *k,unsigned id){assert(id==32);uint8_t b=(uint8_t)id;return k->put(k->context,"test_face",&b,1)==0;}
static portable_context_preset preset(void) {
    portable_context_preset p;portable_context_preset_init(&p);p.enabled=true;p.source=1;p.slot=2;p.actions=PORTABLE_CONTEXT_ACTIONS;
    strcpy(p.name,"Study");p.brightness=65;p.volume=0;p.dnd=true;p.face=32;p.sleep=0;p.idle_ms=30000;p.deep_ms=90000;return p;
}
static void confirmed(void) {
    state.audio=(contexts_source_status_v1){.source=1,.model_generation=1,.room_slot=2,.room_valid=true,.current=true,.room_entry=1};
    strcpy(state.audio.room_name,"Study");state.preset_result=CONTEXTS_PRESET_NONE;
}
int main(void) {
    portable_contexts_client c;assert(portable_contexts_open(&c,&runtime)&&grants==1);
    assert(portable_contexts_step(&c,true,true)&&!last_policy.enabled);
    assert(portable_contexts_set_enabled(&c,true));assert(portable_contexts_step(&c,true,true));
    assert(last_policy.enabled&&last_policy.audio_allowed&&last_policy.radio_allowed);
    unsigned before=reads;assert(portable_contexts_step(&c,false,false)&&reads==before&&!last_policy.audio_allowed&&!last_policy.radio_allowed);
    assert(portable_radio_save(&kv,3));tick+=1000;assert(portable_contexts_step(&c,true,true)&&!last_policy.radio_allowed);
    assert(portable_radio_save(&kv,4));tick+=1000;assert(portable_contexts_step(&c,true,true)&&!last_policy.radio_allowed);
    assert(portable_radio_save(&kv,1));assert(portable_low_battery_save(&kv,true));
    tick+=1000;assert(portable_contexts_step(&c,true,true)&&!last_policy.audio_allowed&&!last_policy.radio_allowed);
    assert(portable_low_battery_save(&kv,false));tick+=1000;assert(portable_contexts_step(&c,true,true));
    portable_context_preset p=preset();assert(portable_context_preset_save(&kv,0,&p));confirmed();uint16_t applied=0;
    tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&applied==255&&claims==1&&results==1&&state.preset_result==CONTEXTS_PRESET_APPLIED);
    unsigned value=0;assert(pqa_preference_load(&kv,PQA_RESTORE_VOLUME_KEY,0,0,&value)&&value==50);
    assert(pqa_preference_save(&kv,PQA_BRIGHTNESS_KEY,85,10));before=writes;
    tick+=1000;assert(portable_contexts_step(&c,true,true));assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&writes==before&&claims==1);
    /* Unknown, pause and ordinary app switch preserve service claim custody. */
    state.audio.current=false;assert(portable_contexts_pause(&c));
    assert(portable_contexts_close(&c)&&!grants);assert(portable_contexts_open(&c,&runtime));
    state.audio.current=true;tick+=1000;assert(portable_contexts_step(&c,true,true));before=writes;
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&writes==before&&claims==1);
    /* Different generation permits exactly one new edge; partial write stops. */
    state.audio.model_generation++;state.audio.room_entry++;bad_write=PQA_BRIGHTNESS_KEY;tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&applied==7&&claims==2&&state.preset_result==CONTEXTS_PRESET_PARTIAL);
    bad_write=NULL;before=writes;tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&writes==before);
    /* Known low battery suppresses only the selected power preferences. */
    state.audio.model_generation++;state.audio.room_entry++;assert(portable_low_battery_save(&kv,true));tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&applied==240&&state.preset_result==CONTEXTS_PRESET_PARTIAL);
    assert(portable_low_battery_save(&kv,false));
    /* Unread/duplicate/ambiguous/conflicting rooms cannot write or claim. */
    state.audio.model_generation++;state.audio.room_entry++;bad_read="ctx_p5";tick+=1000;assert(portable_contexts_step(&c,true,true));before=claims;
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&claims==before);bad_read=NULL;
    assert(portable_context_preset_save(&kv,1,&p));tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&claims==before);
    portable_context_preset_init(&p);assert(portable_context_preset_save(&kv,1,&p));
    state.audio.room_ambiguous=true;tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&claims==before);state.audio.room_ambiguous=false;
    state.radio=state.audio;state.radio.source=2;strcpy(state.radio.room_name,"Hall");tick+=1000;assert(portable_contexts_step(&c,true,true));
    assert(portable_contexts_apply_room(&c,&applied,face)&&!applied&&claims==before);state.radio.current=false;
    /* Refused storage release admits cleanup retry only, never a normal step. */
    tick+=1000;release_ok=false;before=steps;assert(!portable_contexts_step(&c,true,true)&&c.storage.api&&steps==before);
    pause_ok=false;state.cleanup_pending=true;unsigned old_reads=reads,old_release=releases;
    assert(!portable_contexts_step(&c,true,true)&&reads==old_reads&&releases==old_release&&steps==before);
    pause_ok=release_ok=true;assert(portable_contexts_step(&c,true,true)&&!c.storage.api&&steps==before+1);
    health_ok=false;before=steps;assert(portable_contexts_step(&c,true,true)&&steps==before);health_ok=true;
    /* An uncertain commit is accepted only after exact readback. */
    write_result=RISC_KEY_VALUE_IO;assert(portable_contexts_set_enabled(&c,false));
    write_commit=false;assert(!portable_contexts_set_enabled(&c,true));before=steps;
    assert(portable_contexts_step(&c,true,true)&&steps==before+1&&!last_policy.enabled);
    assert(portable_contexts_close(&c)&&!grants);
    printf("Contexts client: policy/coexistence, edge custody, partial saves, manual override, ambiguity and cleanup (%u grants, %u writes) PASS\n",acquires,writes);
    return 0;
}
