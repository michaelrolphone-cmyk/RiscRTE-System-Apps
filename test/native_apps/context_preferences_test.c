#include "PortableContextPreferences.h"
#include <assert.h>
#include <stdio.h>
static uint8_t stored[9][64];
static uint32_t sizes[9],reads,writes;
static int32_t read_error,write_result;
static bool commit_write=true,corrupt_write;
static unsigned index_for(const char *key) {
    if(!strcmp(key,PORTABLE_CONTEXT_ENABLED_KEY))return 8;
    assert(!strncmp(key,"ctx_p",5)&&key[5]>='0'&&key[5]<='7'&&!key[6]);return (unsigned)(key[5]-'0');
}
static int32_t get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *size) {
    (void)context;reads++;*size=0;if(read_error)return read_error;
    unsigned i=index_for(key);if(!sizes[i])return RISC_KEY_VALUE_NOT_FOUND;
    *size=sizes[i];if(capacity<sizes[i])return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(out,stored[i],sizes[i]);return RISC_KEY_VALUE_OK;
}
static int32_t put(void *context,const char *key,const void *in,uint32_t size) {
    (void)context;writes++;assert(size<=64);unsigned i=index_for(key);
    if(commit_write){memcpy(stored[i],in,size);sizes[i]=size;if(corrupt_write)stored[i][0]^=1;}
    return write_result;
}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static portable_context_preset sample(void) {
    portable_context_preset p;portable_context_preset_init(&p);
    p.enabled=true;p.source=PORTABLE_CONTEXT_AUDIO;p.slot=3;p.face=32;
    p.actions=PORTABLE_CONTEXT_ACTIONS;strcpy(p.name,"Living room");
    p.dnd=true;p.brightness=70;p.volume=30;p.alert=PORTABLE_ALERT_BOTH;
    p.idle_ms=45000;p.deep_ms=120000;return p;
}
static void checksum(uint8_t bytes[64]) {portable_context_u32_put(bytes+60,portable_context_crc(bytes,60));}
int main(void) {
    portable_context_preset p=sample(),out;uint8_t bytes[64],other[64];bool enabled=true;
    assert(portable_context_enabled_load(&kv,&enabled)&&!enabled&&!writes);
    for(unsigned i=0;i<8;i++)assert(portable_context_preset_load(&kv,i,&out)==RISC_KEY_VALUE_NOT_FOUND&&!out.enabled);
    assert(portable_context_preset_load(&kv,8,&out)==RISC_KEY_VALUE_INVALID);
    assert(portable_context_preset_encode(&p,bytes));
    assert(portable_context_preset_decode(&out,bytes,64));
    assert(portable_context_preset_encode(&out,other)&&!memcmp(bytes,other,64));
    assert(!portable_context_preset_decode(&out,bytes,63));
    for(unsigned i=0;i<64;i++)for(unsigned b=0;b<8;b++) {
        memcpy(other,bytes,64);other[i]^=(uint8_t)(1u<<b);out=sample();
        assert(!portable_context_preset_decode(&out,other,64));
        assert(!strcmp(out.name,"Living room"));
    }
    const unsigned positions[]={3,4,5,6,8,9,10,11,12,13,15,40,59};
    const unsigned values[]={2,2,3,8,9,101,2,0,3,1,1,1,1};
    for(unsigned i=0;i<sizeof(positions)/sizeof(*positions);i++) {
        memcpy(other,bytes,64);other[positions[i]]=(uint8_t)values[i];checksum(other);
        assert(!portable_context_preset_decode(&out,other,64));
    }
    memcpy(other,bytes,64);other[25]=0;checksum(other);assert(!portable_context_preset_decode(&out,other,64));
    strcpy(p.name,"1234567890123456");assert(portable_context_preset_encode(&p,bytes));
    assert(portable_context_preset_decode(&out,bytes,64)&&!strcmp(out.name,p.name));
    p.name[16]='x';assert(!portable_context_preset_encode(&p,bytes));p=sample();
    assert(portable_context_preset_save(&kv,0,&p)&&writes==1);
    assert(portable_context_preset_save(&kv,0,&p)&&writes==1);
    assert(portable_context_preset_load(&kv,0,&out)==RISC_KEY_VALUE_OK);
    assert(portable_context_preset_matches(&out,1,3,"Living room"));
    assert(!portable_context_preset_matches(&out,2,3,"Living room"));
    assert(!portable_context_preset_matches(&out,1,4,"Living room"));
    assert(!portable_context_preset_matches(&out,1,3,"Bedroom"));
    write_result=RISC_KEY_VALUE_IO;p.brightness=60;
    assert(portable_context_preset_save(&kv,0,&p)); /* committed despite IO */
    commit_write=false;p.brightness=80;assert(!portable_context_preset_save(&kv,0,&p));
    assert(portable_context_preset_load(&kv,0,&out)==0&&out.brightness==60);
    commit_write=true;corrupt_write=true;assert(!portable_context_preset_save(&kv,0,&p));
    assert(portable_context_preset_load(&kv,0,&out)==RISC_KEY_VALUE_INVALID&&!out.enabled);
    corrupt_write=false;write_result=0;
    assert(portable_context_enabled_save(&kv,true));assert(portable_context_enabled_load(&kv,&enabled)&&enabled);
    write_result=RISC_KEY_VALUE_IO;assert(portable_context_enabled_save(&kv,false));
    read_error=RISC_KEY_VALUE_IO;enabled=true;
    assert(!portable_context_enabled_load(&kv,&enabled)&&!enabled);
    assert(!portable_context_preset_save(&kv,1,&p));read_error=0;
    portable_context_preset_init(&p);assert(portable_context_preset_save(&kv,0,&p));
    assert(portable_context_preset_load(&kv,0,&out)==0&&!out.enabled&&!out.source&&!out.actions);
    assert(!portable_context_preset_matches(&out,0,0,""));
    assert(!portable_context_enabled_load(NULL,&enabled));
    assert(portable_context_preset_load(NULL,0,&out)==RISC_KEY_VALUE_CONTEXT);
    printf("Contexts preferences: verified identity, 512 corruption cases, readback/partial-save and defaults (%u reads, %u writes)\n",reads,writes);
    return 0;
}
