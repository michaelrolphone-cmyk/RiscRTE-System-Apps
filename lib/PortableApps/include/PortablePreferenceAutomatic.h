#ifndef PORTABLE_PREFERENCE_AUTOMATIC_H
#define PORTABLE_PREFERENCE_AUTOMATIC_H
/* Explicit automatic-controller scope, not a legacy namespace writer. The
 * caller selects a consumed policy boundary before exposing this private view
 * to its existing encoders. Preserve their ordering, no-ops and partial errors.
 * PFA0001 is never a USER edit and must not be replayed as one. */
#include "PortablePreferenceAtomic.h"
#include "RiscKeyValueV1.h"
enum { PORTABLE_AUTOMATIC_LOW_BATTERY=1,PORTABLE_AUTOMATIC_HOME_RULES=2,
       PORTABLE_AUTOMATIC_HOME_PRESET=3,PORTABLE_AUTOMATIC_CONTEXTS_EDITOR=4 };
typedef struct {
 portable_preference_atomic operation;
 risc_key_value_v1 view;
 const risc_key_value_atomic_v1 *api;
 uint64_t revision;
 unsigned boundary;
 bool running,cancelled,retained;
 bool (*guard)(void *);bool (*before)(void *,bool);
 /* A wait services cancellation only; it must not dispatch another writer. */
 bool (*wait)(void *);void *context;
 int status;
} portable_preference_automatic;
static inline bool portable_preference_automatic_key(unsigned boundary,const char *key) {
 static const char *const low[]={"battery_edge","sleep_idle","sleep_deep","brightness","quick_radio"};
 static const char *const rules[]={"sleep_mode","sleep_idle","sleep_deep","brightness","quick_radio","alarm_volume","alert_dnd","alert_mode","watch_face"};
 static const char *const preset[]={"sleep_mode","sleep_idle","sleep_deep","brightness","alarm_volume","quick_volume","alert_dnd","alert_mode","watch_face"};
 /* The foreground controller applies its product editor mask before entering
  * this explicit automatic boundary. Keys cover the shared setting encoders. */
 static const char *const editor[]={"watch_face","sleep_mode","sleep_idle","sleep_deep","brightness","quick_radio","alarm_volume","alert_dnd","alert_mode"};
 if(!key)return false;
 const char *const *keys=boundary==PORTABLE_AUTOMATIC_LOW_BATTERY?low:boundary==PORTABLE_AUTOMATIC_HOME_RULES?rules:boundary==PORTABLE_AUTOMATIC_HOME_PRESET?preset:editor;
 unsigned count=boundary==PORTABLE_AUTOMATIC_LOW_BATTERY?sizeof(low)/sizeof(*low):boundary==PORTABLE_AUTOMATIC_HOME_RULES?sizeof(rules)/sizeof(*rules):boundary==PORTABLE_AUTOMATIC_HOME_PRESET?sizeof(preset)/sizeof(*preset):sizeof(editor)/sizeof(*editor);
 if(boundary<PORTABLE_AUTOMATIC_LOW_BATTERY||boundary>PORTABLE_AUTOMATIC_CONTEXTS_EDITOR)return false;
 for(unsigned i=0;i<count;++i)if(!strcmp(key,keys[i]))return true;
 return false;
}
static inline bool portable_preference_automatic_safe(portable_preference_automatic *s) {
 if(!s||s->retained)return false;
 if(!s->guard||!s->guard(s->context)){s->retained=true;s->status=RISC_ATOMIC_RETAINED;return false;}
 return true;
}
static inline int portable_preference_automatic_map(portable_preference_automatic *s,int rc) {
 s->status=rc;
 if(rc==RISC_ATOMIC_RETAINED||!portable_preference_automatic_safe(s)){s->retained=true;return RISC_KEY_VALUE_CONTEXT;}
 if(rc==RISC_ATOMIC_OK)return RISC_KEY_VALUE_OK;
 if(rc==RISC_ATOMIC_NOT_FOUND)return RISC_KEY_VALUE_NOT_FOUND;
 if(rc==RISC_ATOMIC_BUFFER_SMALL)return RISC_KEY_VALUE_BUFFER_SMALL;
 if(rc==RISC_ATOMIC_BUSY)return RISC_KEY_VALUE_BUSY;
 /* Uncertain, stale and conflicting operations cannot become successful from
  * a same-value legacy readback. Only a terminal IO preserves that old rule. */
 return rc==RISC_ATOMIC_IO?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_INVALID;
}
static inline bool portable_preference_automatic_before(portable_preference_automatic *s,bool write) {
 return portable_preference_automatic_safe(s)&&(!s->before||s->before(s->context,write))&&portable_preference_automatic_safe(s);
}
static inline int32_t portable_preference_automatic_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *size) {
 portable_preference_automatic *s=context;
 if(size)*size=0;
 if(!size||!portable_preference_automatic_key(s->boundary,key))return RISC_KEY_VALUE_INVALID;
 if(!portable_preference_automatic_before(s,false))return RISC_KEY_VALUE_CONTEXT;
 if(!s->revision||s->operation.token||s->running)return RISC_KEY_VALUE_INVALID;
 int rc=s->api->get(s->api->context,s->revision,key,out,capacity,size);
 return portable_preference_automatic_map(s,rc);
}
static inline int32_t portable_preference_automatic_put(void *context,const char *key,const void *value,uint32_t size) {
 portable_preference_automatic *s=context;
 if(!value||!size||size>64||!portable_preference_automatic_key(s->boundary,key)||s->cancelled)return RISC_KEY_VALUE_INVALID;
 if(!portable_preference_automatic_before(s,true))return RISC_KEY_VALUE_CONTEXT;
 if(!s->revision||s->operation.token||s->running)return RISC_KEY_VALUE_INVALID;
 portable_preference_atomic *op=&s->operation;
 *op=(portable_preference_atomic){.api=s->api,.guard=s->guard,.guard_context=s->context,.revision=s->revision};
 memcpy(op->key,key,strlen(key)+1);
 int rc=s->api->get(s->api->context,s->revision,key,op->expected,sizeof(op->expected),&op->expected_size);
 if(!portable_preference_automatic_safe(s))return RISC_KEY_VALUE_CONTEXT;
 if(rc!=RISC_ATOMIC_OK&&rc!=RISC_ATOMIC_NOT_FOUND)return portable_preference_automatic_map(s,rc);
 if(op->expected_size>64||(rc==RISC_ATOMIC_NOT_FOUND&&op->expected_size))return portable_preference_automatic_map(s,RISC_ATOMIC_INVALID);
 op->present=rc==RISC_ATOMIC_OK;op->valid=true;op->status=PORTABLE_PREFERENCE_READY;
 memcpy(op->desired,value,size);op->desired_size=size;
 /* Exact expected presence/value, desired bytes and fixed local revision;
  * byte 11 names the explicit controller. No USER route is invoked. */
 memcpy(op->intent,"PFA0001",7);op->intent[8]=PORTABLE_PREFERENCE_AUTOMATIC;
 op->intent[9]=RISC_ATOMIC_PUT;op->intent[10]=(uint8_t)op->present;op->intent[11]=(uint8_t)s->boundary;
 memcpy(op->intent+12,key,strlen(key));portable_preference_word(op->intent+28,op->expected_size);
 portable_preference_word(op->intent+32,size);portable_preference_word(op->intent+36,(uint32_t)op->revision);
 portable_preference_word(op->intent+40,(uint32_t)(op->revision>>32));
 memcpy(op->intent+44,op->expected,op->expected_size);memcpy(op->intent+108,value,size);
 portable_preference_word(op->intent+172,portable_preference_crc(op->intent,172));
 s->running=true;
 for(;;) {
  if(!portable_preference_automatic_safe(s)){rc=RISC_ATOMIC_RETAINED;break;}
  rc=s->cancelled?portable_preference_atomic_cancel(op):op->token?portable_preference_atomic_collect(op):portable_preference_atomic_submit(op);
  if(rc!=RISC_ATOMIC_PENDING&&rc!=PORTABLE_PREFERENCE_READY)break;
  if(s->wait&&s->wait(s->context))s->cancelled=true;
 }
 s->running=false;
 if(rc==RISC_ATOMIC_OK)s->revision=op->receipt.revision;
 return portable_preference_automatic_map(s,rc);
}
static inline int portable_preference_automatic_begin(portable_preference_automatic *s,
 const risc_key_value_atomic_v1 *api,unsigned boundary,bool (*guard)(void *),
 bool (*before)(void *,bool),bool (*wait)(void *),void *context) {
 if(!s||s->operation.token||s->running||s->retained)return RISC_ATOMIC_RETAINED;
 *s=(portable_preference_automatic){.api=api,.boundary=boundary,.guard=guard,.before=before,.wait=wait,.context=context};
 s->view=(risc_key_value_v1){1,sizeof(s->view),s,portable_preference_automatic_get,portable_preference_automatic_put};
 if(boundary<PORTABLE_AUTOMATIC_LOW_BATTERY||boundary>PORTABLE_AUTOMATIC_CONTEXTS_EDITOR||!portable_preference_atomic_api_valid(api))return s->status=RISC_ATOMIC_INVALID;
 if(!portable_preference_automatic_before(s,false))return s->status=RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=api->state(api->context,&state);
 if(!portable_preference_automatic_safe(s))return RISC_ATOMIC_RETAINED;
 if(rc)return s->status=rc;
 if(!portable_preference_atomic_state_valid(&state))return s->status=RISC_ATOMIC_INVALID;
 s->revision=state.revision;return s->status=PORTABLE_PREFERENCE_READY;
}
#endif
