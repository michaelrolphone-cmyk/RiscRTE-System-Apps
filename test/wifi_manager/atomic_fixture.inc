/* Copied asynchronous namespaces for the real shared-scene app fixture.
 * This is not proof of NativeReplicaCore durability or device storage. */
typedef struct {
    uint64_t revision,serial,token,submitted_revision;
    unsigned ns,count,collect_ticks,collect_delay,submissions,collections,cancellations;
    uint64_t availability_generation;
    uint32_t availability;
    bool cancelled,commit_wins,malformed_receipt;
    portable_wifi_atomic_cell copied[8];
} fixture_namespace;
static fixture_namespace fixture_profile={.revision=1,.ns=6,.collect_delay=1,.availability_generation=1};
static fixture_namespace fixture_policy={.revision=1,.ns=1,.collect_delay=1,.availability_generation=1};
static int32_t fixture_atomic_state(void *context,risc_key_value_atomic_state_v1 *out){
    fixture_namespace *s=context;
    if(io_fail)return RISC_ATOMIC_IO;
    if(db_busy){--db_busy;return RISC_ATOMIC_BUSY;}
    if(s->token)return RISC_ATOMIC_BUSY;
    *out=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*out),
        .flags=RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC,
        .namespace_id=s->ns,.max_mutations=8,.max_value_bytes=64,
        .max_group_bytes=4096,.max_intent_bytes=1024,.revision=s->revision};
    return RISC_ATOMIC_OK;
}
static int32_t fixture_atomic_get(void *context,uint64_t revision,const char *key,
        void *out,uint32_t capacity,uint32_t *size){
    fixture_namespace *s=context;
    if(revision!=s->revision)return RISC_ATOMIC_STALE;
    int rc=kv_get((void*)(uintptr_t)s->ns,key,out,capacity,size);
    return rc==RISC_KEY_VALUE_OK?RISC_ATOMIC_OK:
        rc==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:
        rc==RISC_KEY_VALUE_BUSY?RISC_ATOMIC_BUSY:
        rc==RISC_KEY_VALUE_BUFFER_SMALL?RISC_ATOMIC_BUFFER_SMALL:RISC_ATOMIC_IO;
}
static int32_t fixture_atomic_submit(void *context,uint64_t revision,
        const risc_key_value_atomic_mutation_v1 *mutations,uint32_t count,
        const void *intent,uint32_t size,uint64_t *token){
    fixture_namespace *s=context;*token=0;
    if(io_fail)return RISC_ATOMIC_IO;
    if(s->token)return RISC_ATOMIC_BUSY;
    if(revision!=s->revision)return RISC_ATOMIC_STALE;
    assert(count<=8&&intent&&size<=1024);
    for(unsigned i=0;i<count;i++){
        assert(mutations[i].kind==RISC_ATOMIC_PUT&&mutations[i].size<=64);
        assert(strlen(mutations[i].key)<sizeof(s->copied[i].key));
        strcpy(s->copied[i].key,mutations[i].key);
        s->copied[i].size=mutations[i].size;
        memcpy(s->copied[i].value,mutations[i].value,mutations[i].size);
    }
    s->count=count;s->submitted_revision=revision;s->collect_ticks=0;
    s->cancelled=false;++s->submissions;s->token=++s->serial;*token=s->token;return RISC_ATOMIC_OK;
}
static int32_t fixture_atomic_collect(void *context,uint64_t token,
        risc_key_value_atomic_receipt_v1 *out){
    fixture_namespace *s=context;assert(s->token==token&&token);
    if(s->collect_ticks++<s->collect_delay)return RISC_ATOMIC_PENDING;
    int rc=s->cancelled&&!s->commit_wins?RISC_ATOMIC_CANCELLED:
        s->submitted_revision!=s->revision?RISC_ATOMIC_STALE:
        io_fail?RISC_ATOMIC_IO:RISC_ATOMIC_OK;
    if(rc==RISC_ATOMIC_OK){
        for(unsigned i=0;i<s->count;i++)
            assert(!kv_put((void*)(uintptr_t)s->ns,s->copied[i].key,s->copied[i].value,s->copied[i].size));
        ++s->revision;
    }
    *out=(risc_key_value_atomic_receipt_v1){.struct_size=sizeof(*out),.status=rc,
        .revision=s->revision,.operation_id=rc==RISC_ATOMIC_OK&&!s->malformed_receipt?s->token:0};
    ++s->collections;wipe(s->copied,sizeof(s->copied));s->count=0;s->token=0;return RISC_ATOMIC_OK;
}
static int32_t fixture_atomic_cancel(void *context,uint64_t token){
    fixture_namespace *s=context;assert(s->token==token&&token);
    s->cancelled=true;++s->cancellations;return RISC_ATOMIC_PENDING;
}
static int32_t fixture_availability(void *context,risc_key_value_availability_state_v1 *out){
    fixture_namespace *s=context;
    *out=(risc_key_value_availability_state_v1){.struct_size=sizeof(*out),
        .namespace_id=s->ns,.availability=io_fail?RISC_KEY_VALUE_UNAVAILABLE:s->availability?s->availability:RISC_KEY_VALUE_AVAILABLE,
        .generation=s->availability_generation};
    return RISC_KEY_VALUE_AVAILABILITY_OK;
}
static const risc_key_value_atomic_v1 fixture_profile_atomic={1,sizeof(fixture_profile_atomic),
    &fixture_profile,fixture_atomic_state,fixture_atomic_get,fixture_atomic_submit,fixture_atomic_collect,fixture_atomic_cancel};
static const risc_key_value_atomic_v1 fixture_policy_atomic={1,sizeof(fixture_policy_atomic),
    &fixture_policy,fixture_atomic_state,fixture_atomic_get,fixture_atomic_submit,fixture_atomic_collect,fixture_atomic_cancel};
static const risc_key_value_availability_v1 fixture_profile_availability={1,sizeof(fixture_profile_availability),
    &fixture_profile,fixture_availability};
static const risc_key_value_availability_v1 fixture_policy_availability={1,sizeof(fixture_policy_availability),
    &fixture_policy,fixture_availability};
