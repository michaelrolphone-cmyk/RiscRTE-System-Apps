#pragma once
#include "ContextsServiceV1.h"
#include "PortableContextPreferences.h"
#include "PortableRadioPolicy.h"
#include "PortableLowBattery.h"
#include "RiscRuntimeV1.h"
typedef struct {
    risc_runtime_capability_v1 grant,storage,fingerprint_store;
    int32_t fingerprint_error;
    uint32_t fingerprint_sources,fingerprint_checkpoint_at,fingerprint_unread;
    const contexts_service_v1 *api;
    const risc_runtime_api_v1 *runtime;
    contexts_policy_v1 policy;
    uint32_t loaded_at,preset_checked_at;
    uint64_t rules_revision;
    bool rules_available;
    bool fingerprint_temporal_only;
    bool loaded,settings_valid,low_battery,save_failed,preset_checked,foreground_learning;
} portable_contexts_client;
#include "PortableContextFingerprints.h"
#include "PortableContextRules.h"
static inline bool portable_contexts_open(portable_contexts_client *c,const risc_runtime_api_v1 *rt) {
    memset(c,0,sizeof(*c));c->fingerprint_sources=CONTEXTS_ALL;c->runtime=rt;c->grant.struct_size=sizeof(c->grant);
    if(!rt->acquire(CONTEXTS_SERVICE_CAPABILITY,1,0,&c->grant))return false;
    const contexts_service_v1 *p=c->grant.api;
    if(!p||p->api_version!=1||p->struct_size<CONTEXTS_SERVICE_V1_SIZE||!p->step||!p->pause||!p->status||
       !p->request_export||!p->begin_export||!p->export_record||!p->finish_export||!p->label||
       !p->claim_preset||!p->preset_result||!p->capture_audio)return false;
    c->api=p;c->policy.struct_size=sizeof(c->policy);
    const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(p);
    if(fp){contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=CONTEXTS_ALL};
        if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_CONFIG,&cfg)||!portable_fp_checkpoint(c,true)||!portable_context_rules_load(c))return false;}
    return true;
}
static inline bool portable_contexts_pause(portable_contexts_client *c) {
    c->loaded=false;if(!c->api)return true;
    if(!c->api->pause(c->api->context))return false;
    return portable_fp_checkpoint(c,false);
}
/* A shared preference writer invalidates the cached background policy. */
static inline void portable_contexts_settings_changed(portable_contexts_client *c,bool confirmed){
    c->loaded=false;c->settings_valid=false;c->save_failed=!confirmed;c->policy.enabled=false;
}
static inline bool portable_contexts_capture(portable_contexts_client *c) {
    return !c->api||c->api->capture_audio(c->api->context);
}
/* Runtime0.1.54 appends the terminal invocation fence after boot confirmation.
 * A renderer may retain borrowed frame memory after capture cleanup fails, so
 * neither normal fini nor another display operation may follow this fence. */
static inline bool portable_contexts_retain(portable_contexts_client *c) {
    bool (*retain)(void)=NULL;
    const size_t offset=RISC_RUNTIME_CAPABILITIES_V1_SIZE+sizeof(bool (*)(void));
    if(!c->runtime||c->runtime->struct_size<offset+sizeof(retain))return false;
    memcpy(&retain,(const unsigned char*)c->runtime+offset,sizeof(retain));
    return retain&&retain();
}
static inline bool portable_contexts_release_storage(portable_contexts_client *c) {
    if(c->storage.api&&!c->runtime->release(&c->storage))return false;
    memset(&c->storage,0,sizeof(c->storage));return true;
}
static inline bool portable_contexts_step(portable_contexts_client *c,bool audio_allowed,bool radio_allowed) {
    if(!c->api||c->fingerprint_store.api)return false;
    if(c->storage.api) {
        if(!portable_contexts_pause(c)||!portable_contexts_release_storage(c))return false;
    }
    risc_runtime_health_v1 health={.struct_size=sizeof(health)};
    if(!c->runtime->health(&health))return portable_contexts_pause(c);
    if(!c->save_failed&&!c->loaded) {
        c->policy=(contexts_policy_v1){.struct_size=sizeof(c->policy),.sources=CONTEXTS_ALL};
        c->settings_valid=false;c->low_battery=true;
        c->storage=(risc_runtime_capability_v1){.struct_size=sizeof(c->storage)};
        if(c->runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&c->storage)) {
            const risc_key_value_v1 *kv=c->storage.api;uint8_t flags=0;bool enabled=false,low=true;
            c->settings_valid=portable_context_enabled_load(kv,&enabled)&&portable_radio_load(kv,&flags)&&
                portable_low_battery_load(kv,&low);
            c->low_battery=low;c->policy.enabled=c->settings_valid&&enabled;
            /* Background RF must not turn off user radios or contend with BLE.
             * Airplane mode and the low-battery edge remain authoritative. */
            c->policy.radio_allowed=(flags&7u)==PORTABLE_RADIO_WIFI&&!low;
            c->policy.audio_allowed=!low;
            if(contexts_fingerprint_api(c->api)){
                uint8_t sources=CONTEXTS_ALL;uint32_t size=0;
                int32_t source_result=kv->get(kv->context,"context_sources",&sources,1,&size);
                if(source_result==RISC_KEY_VALUE_NOT_FOUND)sources=CONTEXTS_ALL;
                else if(source_result!=RISC_KEY_VALUE_OK||size!=1||(sources&~CONTEXTS_ALL))sources=0;
                c->policy.sources=c->fingerprint_sources=sources;
                uint8_t timing=0;size=0;
                int32_t timing_result=kv->get(kv->context,"context_timing",&timing,1,&size);
                c->fingerprint_temporal_only=timing_result==RISC_KEY_VALUE_OK&&size==1&&timing==1;
            }
            if(!portable_contexts_release_storage(c)){(void)portable_contexts_pause(c);return false;}
        }
        c->loaded=true;c->loaded_at=health.uptime_ms;
    }
    contexts_policy_v1 policy=c->policy;policy.awake=true;
    policy.enabled=(policy.enabled||c->foreground_learning)&&c->settings_valid;
    policy.audio_allowed=policy.audio_allowed&&audio_allowed;
    policy.radio_allowed=policy.radio_allowed&&radio_allowed;
    const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(c->api);
    if(fp){
        contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only};
        if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_CONFIG,&cfg))return false;
        if((uint32_t)(health.uptime_ms-c->fingerprint_checkpoint_at)>=60000u){if(!portable_fp_checkpoint(c,false))return false;c->fingerprint_checkpoint_at=health.uptime_ms;}
    }
    return c->api->step(c->api->context,&policy);
}
static inline bool portable_contexts_set_enabled(portable_contexts_client *c,bool enabled) {
    if(!c->api||!portable_contexts_pause(c)||!portable_contexts_release_storage(c))return false;
    c->policy=(contexts_policy_v1){.struct_size=sizeof(c->policy)};c->save_failed=true;c->settings_valid=false;
    c->storage=(risc_runtime_capability_v1){.struct_size=sizeof(c->storage)};bool saved=false;
    if(c->runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&c->storage)) {
        saved=portable_context_enabled_save(c->storage.api,enabled);
        if(!portable_contexts_release_storage(c))return false;
    }
    c->save_failed=!saved;c->loaded=false;return saved;
}
static inline bool portable_contexts_close(portable_contexts_client *c) {
    if(!portable_contexts_pause(c))return false;
    if(!portable_contexts_release_storage(c))return false;
    if(c->grant.api&&!c->runtime->release(&c->grant))return false;
    memset(c,0,sizeof(*c));return true;
}
/* Caller claims a confirmed room edge in the service before this function.
 * Stop at the first unverified write. Partial success is reported and is never
 * automatically replayed, which preserves subsequent manual changes. */
static inline bool portable_contexts_apply_preferences(const risc_key_value_v1 *kv,
        const portable_context_preset *p,bool low,uint16_t *applied,
        bool (*face_save)(const risc_key_value_v1 *,unsigned)) {
    if(!applied)return false;
    *applied=0;if(!pqa_preferences_valid(kv)||!portable_context_preset_valid(p)||!p->enabled)return false;
    uint16_t mask=p->actions;
    if(low)mask&=~(PORTABLE_CONTEXT_SLEEP|PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_DEEP|PORTABLE_CONTEXT_BRIGHTNESS);
#define CONTEXT_WRITE(bit,operation) do { if(mask&(bit)) {if(!(operation))return false;*applied|=(bit);} } while(0)
    CONTEXT_WRITE(PORTABLE_CONTEXT_SLEEP,portable_sleep_save(kv,p->sleep));
    CONTEXT_WRITE(PORTABLE_CONTEXT_IDLE,portable_sleep_timer_save(kv,false,p->idle_ms));
    CONTEXT_WRITE(PORTABLE_CONTEXT_DEEP,portable_sleep_timer_save(kv,true,p->deep_ms));
    CONTEXT_WRITE(PORTABLE_CONTEXT_BRIGHTNESS,pqa_preference_save(kv,PQA_BRIGHTNESS_KEY,p->brightness,10));
    if(mask&PORTABLE_CONTEXT_VOLUME) {
        unsigned previous=0;
        if(!pqa_preference_load(kv,PQA_VOLUME_KEY,PQA_VOLUME_DEFAULT,0,&previous))return false;
        if(!p->volume&&previous&&!pqa_preference_save(kv,PQA_RESTORE_VOLUME_KEY,previous,1))return false;
        if(!pqa_preference_save(kv,PQA_VOLUME_KEY,p->volume,0))return false;
        *applied|=PORTABLE_CONTEXT_VOLUME;
    }
    CONTEXT_WRITE(PORTABLE_CONTEXT_DND,pqa_dnd_save(kv,p->dnd));
    CONTEXT_WRITE(PORTABLE_CONTEXT_ALERT,portable_alert_save(kv,p->alert));
    CONTEXT_WRITE(PORTABLE_CONTEXT_FACE,face_save&&face_save(kv,p->face));
#undef CONTEXT_WRITE
    return true;
}
static inline bool portable_contexts_apply_room(portable_contexts_client *c,uint16_t *applied,
        bool (*face_save)(const risc_key_value_v1 *,unsigned)) {
    if(!applied)return false;
    if(c->storage.api)return false;
    *applied=0;if(!c->api||!c->settings_valid||!c->policy.enabled)return true;
    if(c->preset_checked&&c->preset_checked_at==c->loaded_at)return true;
    c->preset_checked=true;c->preset_checked_at=c->loaded_at;
    contexts_status_v1 status={.struct_size=sizeof(status)};
    if(!c->api->status(c->api->context,&status))return false;
    if(status.cleanup_pending)return false;
    const contexts_source_status_v1 *room=NULL;
    if(status.audio.current&&status.audio.room_valid&&!status.audio.room_ambiguous)room=&status.audio;
    if(status.radio.current&&status.radio.room_valid&&!status.radio.room_ambiguous) {
        if(room&&strncmp(room->room_name,status.radio.room_name,CONTEXTS_NAME_SIZE))return true;
        if(!room)room=&status.radio;
    }
    if(!room||room->room_slot<0||room->room_slot>=8||!memchr(room->room_name,0,CONTEXTS_NAME_SIZE))return true;
    if(room->room_entry&&room->room_entry==room->preset_entry)return true;
    portable_context_preset selected;portable_context_preset_init(&selected);unsigned matches=0;bool readable=true;
    c->storage=(risc_runtime_capability_v1){.struct_size=sizeof(c->storage)};
    if(!c->runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&c->storage))return true;
    for(unsigned i=0;i<PORTABLE_CONTEXT_PRESETS;i++) {
        portable_context_preset p;int32_t result=portable_context_preset_load(c->storage.api,i,&p);
        if(result!=RISC_KEY_VALUE_OK&&result!=RISC_KEY_VALUE_NOT_FOUND){readable=false;break;}
        if(result==RISC_KEY_VALUE_OK&&portable_context_preset_matches(&p,room->source,(unsigned)room->room_slot,room->room_name)) {
            selected=p;matches++;
        }
    }
    if(!portable_contexts_release_storage(c)){(void)portable_contexts_pause(c);return false;}
    if(!readable||matches!=1)return true;
    if(!c->api->claim_preset(c->api->context,room->source,(uint32_t)room->room_slot,room->room_name,room->model_generation))return true;
    if(!portable_contexts_pause(c))return false;
    c->storage=(risc_runtime_capability_v1){.struct_size=sizeof(c->storage)};bool ok=false;
    if(c->runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&c->storage))
        ok=portable_contexts_apply_preferences(c->storage.api,&selected,c->low_battery,applied,face_save);
    if(!portable_contexts_release_storage(c))return false;
    return c->api->preset_result(c->api->context,room->source,room->model_generation,
        ok&&*applied==selected.actions?CONTEXTS_PRESET_APPLIED:CONTEXTS_PRESET_PARTIAL);
}
/* Synchronous app-local borrowed access. Call only at settled foreground
 * boundaries; the common adapter retains the owning capability grant. */
const contexts_service_v1 *portable_contexts_service(void);
bool portable_contexts_stop(void);
bool portable_contexts_enable(bool enabled);
/* Foreground training is temporary and never writes the background toggle. */
bool portable_contexts_training(bool enabled);
unsigned portable_contexts_face_count(void);
const char *portable_contexts_face_name(unsigned id);

/* All returned data is copied; the adapter owns persistence and grants. */
bool portable_contexts_rules_read(cr_store *out);
bool portable_contexts_rules_save(const cr_store *value);
bool portable_contexts_models_save(void);
