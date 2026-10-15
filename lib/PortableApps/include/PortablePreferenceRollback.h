#ifndef PORTABLE_PREFERENCE_ROLLBACK_H
#define PORTABLE_PREFERENCE_ROLLBACK_H
/* Explicit compensation for one confirmed quick_radio USER operation.
 * Ordinary PFI0001 USER records are unchanged. PFR0001 binds its expected
 * local revision/value to the originating committed receipt. No rebase or
 * automatic retry may overwrite a newer controller/automatic operation. */
#include "PortablePreferenceAtomic.h"
#define PORTABLE_PREFERENCE_ROLLBACK_BYTES 192u
typedef struct {
 portable_preference_atomic edit;
 uint8_t intent[PORTABLE_PREFERENCE_ROLLBACK_BYTES];
 uint32_t kind;
 uint64_t origin_operation,origin_revision;
} portable_preference_rollback;
static inline int portable_preference_rollback_begin(portable_preference_rollback *r,
 const portable_preference_atomic *origin,bool (*guard)(void *),void *context){
 if(!r||r->edit.token||!origin||origin->token||origin->status!=RISC_ATOMIC_OK||
    origin->receipt.status!=RISC_ATOMIC_OK||!origin->receipt.operation_id||
    origin->receipt.revision<=origin->revision||strcmp(origin->key,"quick_radio")||
    memcmp(origin->intent,"PFI0001",7)||origin->intent[8]!=PORTABLE_PREFERENCE_USER||
    origin->desired_size!=4)return RISC_ATOMIC_INVALID;
 memset(r,0,sizeof(*r));
 int rc=portable_preference_atomic_begin_at(&r->edit,origin->api,origin->key,PORTABLE_PREFERENCE_USER,guard,context,origin->receipt.revision);
 if(rc!=PORTABLE_PREFERENCE_READY)return rc;
 if(!r->edit.present||r->edit.expected_size!=origin->desired_size||memcmp(r->edit.expected,origin->desired,origin->desired_size))return r->edit.status=RISC_ATOMIC_STALE;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 rc=origin->api->state(origin->api->context,&state);
 if(portable_preference_atomic_safe(&r->edit))return r->edit.status=RISC_ATOMIC_RETAINED;
 if(rc)return r->edit.status=rc;
 if(state.revision!=r->edit.revision)return r->edit.status=RISC_ATOMIC_STALE;
 if(!portable_preference_atomic_state_valid(&state)||state.max_intent_bytes<sizeof(r->intent))return r->edit.status=RISC_ATOMIC_CAPACITY;
 r->kind=origin->present?RISC_ATOMIC_PUT:RISC_ATOMIC_DELETE;
 r->edit.desired_size=origin->expected_size;memcpy(r->edit.desired,origin->expected,origin->expected_size);
 r->origin_operation=origin->receipt.operation_id;r->origin_revision=origin->receipt.revision;
 memcpy(r->intent,"PFR0001",7);r->intent[8]=PORTABLE_PREFERENCE_ROLLBACK;r->intent[9]=(uint8_t)r->kind;r->intent[10]=1;
 memcpy(r->intent+12,r->edit.key,strlen(r->edit.key));
 portable_preference_word(r->intent+28,r->edit.expected_size);portable_preference_word(r->intent+32,r->edit.desired_size);
 portable_preference_word(r->intent+36,(uint32_t)r->edit.revision);portable_preference_word(r->intent+40,(uint32_t)(r->edit.revision>>32));
 memcpy(r->intent+44,r->edit.expected,r->edit.expected_size);memcpy(r->intent+108,r->edit.desired,r->edit.desired_size);
 portable_preference_word(r->intent+172,(uint32_t)r->origin_operation);portable_preference_word(r->intent+176,(uint32_t)(r->origin_operation>>32));
 portable_preference_word(r->intent+180,(uint32_t)r->origin_revision);portable_preference_word(r->intent+184,(uint32_t)(r->origin_revision>>32));
 portable_preference_word(r->intent+188,portable_preference_crc(r->intent,188));return PORTABLE_PREFERENCE_READY;
}
static inline int portable_preference_rollback_submit(portable_preference_rollback *r){
 if(!r||!r->origin_operation||!r->edit.valid||r->edit.submitted||r->edit.token||r->edit.status!=PORTABLE_PREFERENCE_READY)return RISC_ATOMIC_INVALID;
 portable_preference_atomic *s=&r->edit;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_mutation_v1 m={s->key,r->kind,r->kind==RISC_ATOMIC_PUT?s->desired:NULL,s->desired_size};
 uint64_t token=0;int rc=s->api->submit(s->api->context,s->revision,&m,1,r->intent,sizeof(r->intent),&token);
 s->token=token;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(rc==RISC_ATOMIC_BUSY&&!token)return s->status=PORTABLE_PREFERENCE_READY;
 s->submitted=true;
 if(rc!=RISC_ATOMIC_OK||!token)return s->status=token||rc==RISC_ATOMIC_OK?RISC_ATOMIC_RETAINED:
  portable_preference_atomic_terminal(rc)?rc:RISC_ATOMIC_RETAINED;
 return s->status=RISC_ATOMIC_PENDING;
}
#endif
