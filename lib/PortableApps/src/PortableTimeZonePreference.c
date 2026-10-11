#include "PortableTimeZonePreference.h"

static int readable(const risc_key_value_v1 *kv) {
    return kv && kv->api_version==RISC_KEY_VALUE_API_V1 && kv->struct_size>=sizeof(*kv) && kv->get;
}
static void copy_id(char *out,const char *id) {
    unsigned i=0;
    for (;i<PORTABLE_TIMEZONE_ID_BYTES && id[i];++i) out[i]=id[i];
    for (;i<PORTABLE_TIMEZONE_ID_BYTES;++i) out[i]=0;
}
static uint8_t checksum(const uint8_t *bytes) {
    uint8_t sum=0xa5u;
    for (unsigned i=0;i<PORTABLE_TIMEZONE_RECORD_BYTES;++i) if (i!=3) sum^=bytes[i];
    return sum;
}
static void encode(unsigned index,uint8_t bytes[PORTABLE_TIMEZONE_RECORD_BYTES]) {
    bytes[0]='T'; bytes[1]='Z'; bytes[2]=1; bytes[3]=0;
    copy_id((char *)bytes+4,portable_timezone_get(index)->id);
    bytes[3]=checksum(bytes);
}
static int bytes_equal(const uint8_t *a,const uint8_t *b) {
    for (unsigned i=0;i<PORTABLE_TIMEZONE_RECORD_BYTES;++i) if (a[i]!=b[i]) return 0;
    return 1;
}
int portable_timezone_preference_load(const risc_key_value_v1 *kv,char out[PORTABLE_TIMEZONE_ID_BYTES]) {
    if (!out) return PORTABLE_TIMEZONE_INVALID_RECORD;
    copy_id(out,"UTC");
    if (!readable(kv)) return PORTABLE_TIMEZONE_UNAVAILABLE;
    uint8_t bytes[PORTABLE_TIMEZONE_RECORD_BYTES]={0},canonical[PORTABLE_TIMEZONE_RECORD_BYTES]; uint32_t size=0;
    int32_t rc=kv->get(kv->context,PORTABLE_TIMEZONE_KEY,bytes,sizeof(bytes),&size);
    if (rc==RISC_KEY_VALUE_NOT_FOUND) return PORTABLE_TIMEZONE_MISSING;
    if (rc==RISC_KEY_VALUE_BUFFER_SMALL) return PORTABLE_TIMEZONE_INVALID_RECORD;
    if (rc!=RISC_KEY_VALUE_OK) return PORTABLE_TIMEZONE_UNAVAILABLE;
    if (size!=sizeof(bytes) || bytes[0]!='T' || bytes[1]!='Z' || bytes[2]!=1 || bytes[3]!=checksum(bytes))
        return PORTABLE_TIMEZONE_INVALID_RECORD;
    int index=portable_timezone_find((const char *)bytes+4,PORTABLE_TIMEZONE_ID_BYTES);
    if (index<0) return PORTABLE_TIMEZONE_INVALID_RECORD;
    encode((unsigned)index,canonical);
    /* Stored IDs are canonical and padding is zero. No silent alias rewrite. */
    if (!bytes_equal(bytes,canonical)) return PORTABLE_TIMEZONE_INVALID_RECORD;
    copy_id(out,portable_timezone_get((unsigned)index)->id);
    return PORTABLE_TIMEZONE_LOADED;
}
int portable_timezone_preference_save(const risc_key_value_v1 *kv,const char *id,size_t capacity) {
    int index=portable_timezone_find(id,capacity);
    if (index<0) return PORTABLE_TIMEZONE_SAVE_INVALID;
    if (!readable(kv) || !kv->put) return PORTABLE_TIMEZONE_SAVE_UNAVAILABLE;
    char current[PORTABLE_TIMEZONE_ID_BYTES];
    int status=portable_timezone_preference_load(kv,current);
    if (status==PORTABLE_TIMEZONE_UNAVAILABLE) return PORTABLE_TIMEZONE_SAVE_UNAVAILABLE;
    int current_index=portable_timezone_find(current,sizeof(current));
    if (index==current_index && status!=PORTABLE_TIMEZONE_INVALID_RECORD) {
        if (status==PORTABLE_TIMEZONE_LOADED) return PORTABLE_TIMEZONE_UNCHANGED;
        return PORTABLE_TIMEZONE_VIRTUAL_DEFAULT;
    }
    uint8_t bytes[PORTABLE_TIMEZONE_RECORD_BYTES],actual[PORTABLE_TIMEZONE_RECORD_BYTES]={0}; uint32_t size=0;
    encode((unsigned)index,bytes);
    if (kv->put(kv->context,PORTABLE_TIMEZONE_KEY,bytes,sizeof(bytes))!=RISC_KEY_VALUE_OK)
        return PORTABLE_TIMEZONE_WRITE_FAILED;
    if (kv->get(kv->context,PORTABLE_TIMEZONE_KEY,actual,sizeof(actual),&size)!=RISC_KEY_VALUE_OK ||
        size!=sizeof(actual) || !bytes_equal(bytes,actual)) return PORTABLE_TIMEZONE_VERIFY_FAILED;
    return PORTABLE_TIMEZONE_SAVED;
}
