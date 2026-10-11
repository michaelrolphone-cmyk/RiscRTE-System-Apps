#include "PortableTimeZone.h"
#include "PortableTimeZonePreference.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static portable_timezone_civil civil(int year,unsigned month,unsigned day,unsigned hour,unsigned minute,unsigned second) {
    portable_timezone_civil c={year,(uint8_t)month,(uint8_t)day,(uint8_t)hour,(uint8_t)minute,(uint8_t)second,255}; return c;
}
static int64_t epoch(int year,unsigned month,unsigned day,unsigned hour,unsigned minute,unsigned second) {
    portable_timezone_civil c=civil(year,month,day,hour,minute,second); int64_t e;
    assert(portable_timezone_civil_to_epoch(&c,&e)==0); return e;
}
static portable_timezone_rule zone(const char *id) {
    portable_timezone_rule r; assert(portable_timezone_resolve(id,strlen(id)+1,&r)==0); return r;
}
static void check_local(const char *id,int64_t utc,int year,unsigned month,unsigned day,unsigned hour,unsigned minute,int offset,int daylight) {
    portable_timezone_rule r=zone(id); portable_timezone_civil c; portable_timezone_candidate d;
    assert(portable_timezone_utc_to_local(&r,utc,&c,&d)==0);
    assert(c.year==year && c.month==month && c.day==day && c.hour==hour && c.minute==minute);
    assert(d.offset==offset && d.daylight==daylight && d.epoch==utc);
}
static void catalog(void) {
    assert(portable_timezone_count()==419 && !portable_timezone_get(419));
    unsigned sum=0,seen[419]={0};
    for (unsigned region=0;region<PORTABLE_TIMEZONE_REGION_COUNT;++region) {
        assert(portable_timezone_region_name(region)); unsigned count=portable_timezone_region_count(region); sum+=count;
        for (unsigned city=0;city<count;++city) {
            int i=portable_timezone_city_index(region,city); assert(i>=0 && i<419); ++seen[i];
            assert(portable_timezone_get((unsigned)i)->region==(portable_timezone_region)region);
        }
        assert(portable_timezone_city_index(region,count)==-1);
    }
    assert(sum==419 && portable_timezone_region_count(PORTABLE_TIMEZONE_AMERICA)>96);
    assert(!portable_timezone_region_name(99) && !portable_timezone_region_count(99));
    assert(portable_timezone_city_index(99,0)==-1);
    char name[40],short_name[3];
    assert(portable_timezone_display_name((unsigned)portable_timezone_find("America/Argentina/Buenos_Aires",40),name,sizeof(name))==0);
    assert(!strcmp(name,"Argentina/Buenos Aires"));
    assert(portable_timezone_display_name(0,short_name,sizeof(short_name))==PORTABLE_TIMEZONE_RANGE && !short_name[0]);
    assert(portable_timezone_find("Etc/UTC",8)==0 && portable_timezone_find("Etc/GMT",8)==0);
    const int years[]={1601,1900,1999,2000,2024,2026,2038,2099,2100,2400,9998};
    for (unsigned i=0;i<419;++i) {
        assert(seen[i]==1); const portable_timezone_entry *e=portable_timezone_get(i);
        assert(strlen(e->id)<40 && strlen(e->posix)<96 && portable_timezone_find(e->id,40)==(int)i);
        portable_timezone_rule r=zone(e->id),parsed;
        assert(portable_timezone_parse(e->posix,96,&parsed)==0);
        for (size_t cap=0;cap<=strlen(e->posix);++cap)
            assert(portable_timezone_parse(e->posix,cap,&parsed)==PORTABLE_TIMEZONE_BAD_RULE);
        for (size_t cap=0;cap<=strlen(e->id);++cap) assert(portable_timezone_find(e->id,cap)==-1);
        for (unsigned y=0;y<sizeof(years)/sizeof(years[0]);++y) for (unsigned m=1;m<=12;++m) {
            int64_t utc=epoch(years[y],m,15,12,34,56); portable_timezone_civil local; portable_timezone_inverse inverse;
            assert(portable_timezone_utc_to_local(&r,utc,&local,0)==0);
            int rc=portable_timezone_local_to_utc(&r,&local,&inverse); assert(rc==0 || rc==PORTABLE_TIMEZONE_FOLD);
            assert(inverse.count>=1 && inverse.count<=2);
            assert(inverse.candidate[0].epoch==utc || (inverse.count==2 && inverse.candidate[1].epoch==utc));
        }
    }
}
static void conversion(void) {
    check_local("America/New_York",epoch(2026,3,8,6,59,59),2026,3,8,1,59,-18000,0);
    check_local("America/New_York",epoch(2026,3,8,7,0,0),2026,3,8,3,0,-14400,1);
    check_local("America/New_York",epoch(2026,11,1,5,59,59),2026,11,1,1,59,-14400,1);
    check_local("America/New_York",epoch(2026,11,1,6,0,0),2026,11,1,1,0,-18000,0);
    check_local("Australia/Sydney",epoch(2026,1,1,0,0,0),2026,1,1,11,0,39600,1);
    check_local("Australia/Sydney",epoch(2026,7,1,0,0,0),2026,7,1,10,0,36000,0);
    check_local("Australia/Lord_Howe",epoch(2026,1,1,0,0,0),2026,1,1,11,0,39600,1);
    check_local("Australia/Lord_Howe",epoch(2026,7,1,0,0,0),2026,7,1,10,30,37800,0);
    check_local("Pacific/Chatham",epoch(2026,1,1,0,0,0),2026,1,1,13,45,49500,1);
    check_local("Pacific/Chatham",epoch(2026,7,1,0,0,0),2026,7,1,12,45,45900,0);
    check_local("Asia/Kathmandu",epoch(2025,12,31,23,0,0),2026,1,1,4,45,20700,0);
    check_local("Pacific/Marquesas",epoch(2026,1,1,0,0,0),2025,12,31,14,30,-34200,0);
    check_local("Asia/Kolkata",epoch(2024,2,29,23,0,0),2024,3,1,4,30,19800,0);
    portable_timezone_civil c=civil(2026,3,8,2,30,0); portable_timezone_inverse inverse;
    portable_timezone_rule r=zone("America/New_York");
    assert(portable_timezone_local_to_utc(&r,&c,&inverse)==PORTABLE_TIMEZONE_GAP && !inverse.count);
    c=civil(2026,11,1,1,30,0);
    assert(portable_timezone_local_to_utc(&r,&c,&inverse)==PORTABLE_TIMEZONE_FOLD && inverse.count==2);
    assert(inverse.candidate[0].epoch==epoch(2026,11,1,5,30,0) && inverse.candidate[1].epoch==epoch(2026,11,1,6,30,0));
    r=zone("Australia/Lord_Howe"); c=civil(2026,4,5,1,45,0);
    assert(portable_timezone_local_to_utc(&r,&c,&inverse)==PORTABLE_TIMEZONE_FOLD && inverse.count==2);
    assert(inverse.candidate[1].epoch-inverse.candidate[0].epoch==1800);
    c=civil(2026,10,4,2,15,0); assert(portable_timezone_local_to_utc(&r,&c,&inverse)==PORTABLE_TIMEZONE_GAP);
    for (int year=1600;year<=9999;++year) {
        int64_t e=epoch(year,3,1,0,0,0); portable_timezone_civil back;
        assert(portable_timezone_epoch_to_civil(e-1,&back)==0 && back.year==year && back.month==2);
        unsigned days=(year%4==0 && (year%100!=0 || year%400==0))?29:28;
        assert(back.day==days && back.hour==23 && back.minute==59 && back.second==59);
        int64_t recovered; assert(portable_timezone_civil_to_epoch(&back,&recovered)==0 && recovered==e-1);
    }
    int64_t e; c=civil(2100,2,29,0,0,0); assert(portable_timezone_civil_to_epoch(&c,&e)==PORTABLE_TIMEZONE_INVALID);
    c=civil(2400,2,29,0,0,0); assert(portable_timezone_civil_to_epoch(&c,&e)==0);
    assert(portable_timezone_epoch_to_civil(INT64_MAX,&c)==PORTABLE_TIMEZONE_RANGE);
    assert(portable_timezone_epoch_to_civil(INT64_MIN,&c)==PORTABLE_TIMEZONE_RANGE);
    assert(portable_timezone_epoch_to_civil(epoch(1600,1,1,0,0,0)-1,&c)==PORTABLE_TIMEZONE_RANGE);
    assert(portable_timezone_epoch_to_civil(epoch(9999,12,31,23,59,59)+1,&c)==PORTABLE_TIMEZONE_RANGE);
    r=zone("Asia/Tokyo"); assert(portable_timezone_utc_to_local(&r,epoch(9999,12,31,23,0,0),&c,0)==PORTABLE_TIMEZONE_RANGE);
    r=zone("America/Denver"); c=civil(9999,12,31,23,0,0);
    assert(portable_timezone_local_to_utc(&r,&c,&inverse)==PORTABLE_TIMEZONE_RANGE);
}
static void invalid(void) {
    portable_timezone_rule r=zone("UTC"),before=r; char bad_id[40],bad_rule[96]; memset(bad_id,'A',sizeof(bad_id)); memset(bad_rule,'A',sizeof(bad_rule));
    assert(portable_timezone_find(bad_id,sizeof(bad_id))==-1 && portable_timezone_find("UTC",3)==-1);
    assert(portable_timezone_resolve(bad_id,40,&r)==PORTABLE_TIMEZONE_FALLBACK && !r.standard_offset);
    assert(portable_timezone_resolve(0,0,&r)==PORTABLE_TIMEZONE_FALLBACK);
    assert(portable_timezone_resolve("Unknown/Place",14,&r)==PORTABLE_TIMEZONE_FALLBACK);
    assert(portable_timezone_parse(bad_rule,96,&r)==PORTABLE_TIMEZONE_BAD_RULE);
    const char *bad[]={"","UT0","UTC","UTC25","UTC24:01","UTC0junk","UTC0DST","UTC0DST,J1,J2",
        "UTC0DST,M0.1.0,M2.1.0","UTC0DST,M1.0.0,M2.1.0","UTC0DST,M1.1.7,M2.1.0",
        "UTC0DST,M1.1.0/168,M2.1.0","UTC0DST,M1.1.0/2:60,M2.1.0","<+12:45-12:45","UTC0,","UTC0DST,M1.1.0,M2.1.0junk"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        assert(portable_timezone_parse(bad[i],strlen(bad[i])+1,&r)==PORTABLE_TIMEZONE_BAD_RULE);
        assert(!memcmp(&before,&r,sizeof(r)));
    }
    assert(portable_timezone_parse("UTC0",4,&r)==PORTABLE_TIMEZONE_BAD_RULE);
    /* Extended signed rule times/explicit second offsets accepted by parser. */
    const char *valid[]={"UTC0","ABC+4:30:15","ABC0DEF-0:30,M1.1.0/-1,M12.5.0/26:30:59","<+0>0<+2>-2,M3.5.0/1,M10.5.0/3"};
    for (unsigned i=0;i<sizeof(valid)/sizeof(valid[0]);++i) assert(portable_timezone_parse(valid[i],strlen(valid[i])+1,&r)==0);
    r.standard_offset=INT32_MAX; portable_timezone_civil c; assert(portable_timezone_utc_to_local(&r,0,&c,0)==PORTABLE_TIMEZONE_BAD_RULE);
    assert(portable_timezone_parse(0,0,&r)==PORTABLE_TIMEZONE_BAD_RULE);
    assert(portable_timezone_parse("UTC0",5,0)==PORTABLE_TIMEZONE_INVALID);
    assert(portable_timezone_civil_to_epoch(0,0)==PORTABLE_TIMEZONE_INVALID);
}
static void migration(void) {
    portable_timezone_rule r=zone("Africa/Lagos"); portable_timezone_civil c=civil(2026,1,1,12,0,0);
    portable_timezone_rtc_result result; uint32_t utc=(uint32_t)epoch(2026,1,1,12,0,0);
    assert(portable_timezone_interpret_rtc(&r,&c,0,utc-900,-1,&result)==0 && !result.stores_utc);
    assert(portable_timezone_interpret_rtc(&r,&c,0,utc-899,-1,&result)==0 && result.stores_utc && result.mode_changed);
    assert(portable_timezone_interpret_rtc(&r,&c,1,utc-2700,-1,&result)==0 && result.stores_utc);
    assert(portable_timezone_interpret_rtc(&r,&c,1,utc-2701,-1,&result)==0 && !result.stores_utc && result.mode_changed);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,-1,&result)==0 && !result.stores_utc);
    assert(result.epoch==(int64_t)utc-3600);
    c=civil(2000,1,1,0,30,0);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,-1,&result)==0 && result.stores_utc && !result.local_usable);
    r=zone("Atlantic/Cape_Verde"); c=civil(1999,12,31,23,30,0);
    assert(portable_timezone_interpret_rtc(&r,&c,1,0,-1,&result)==0 && !result.stores_utc && !result.utc_usable);
    c=civil(1998,1,1,0,0,0);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,-1,&result)==PORTABLE_TIMEZONE_UNUSABLE);
    r=zone("America/New_York"); c=civil(2026,11,1,1,30,0);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,-1,&result)==0 && result.stores_utc && result.local_status==PORTABLE_TIMEZONE_FOLD);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,0,&result)==0 && !result.stores_utc && result.epoch==epoch(2026,11,1,5,30,0));
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,1,&result)==0 && !result.stores_utc && result.epoch==epoch(2026,11,1,6,30,0));
    c=civil(2026,3,8,2,30,0);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,-1,&result)==0 && result.stores_utc && result.local_status==PORTABLE_TIMEZONE_GAP);
    assert(portable_timezone_interpret_rtc(&r,&c,2,0,-1,&result)==PORTABLE_TIMEZONE_INVALID);
    assert(portable_timezone_interpret_rtc(&r,&c,0,0,2,&result)==PORTABLE_TIMEZONE_INVALID);
}
typedef struct { uint8_t bytes[64]; uint32_t size; int read_rc,write_rc,put_calls,corrupt,fail_after,suppress_write; } store;
static int32_t get(void *ctx,const char *key,void *buf,uint32_t cap,uint32_t *size) {
    store *s=ctx; assert(!strcmp(key,PORTABLE_TIMEZONE_KEY)); *size=0;
    if (s->fail_after && s->put_calls) return RISC_KEY_VALUE_IO;
    if (s->read_rc) return s->read_rc;
    if (!s->size) return RISC_KEY_VALUE_NOT_FOUND;
    *size=s->size; if (s->size>cap) return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(buf,s->bytes,s->size); return RISC_KEY_VALUE_OK;
}
static int32_t put(void *ctx,const char *key,const void *data,uint32_t size) {
    store *s=ctx; assert(!strcmp(key,PORTABLE_TIMEZONE_KEY)); assert(size==44); ++s->put_calls;
    if (!s->suppress_write) { memcpy(s->bytes,data,size); s->size=size; }
    if (s->corrupt) s->bytes[3]^=1;
    return s->write_rc;
}
static void preferences(void) {
    store s={0}; risc_key_value_v1 kv={1,sizeof(kv),&s,get,put}; char id[40];
    assert(PORTABLE_TIMEZONE_STORE_INSTANCE==1 && strlen(PORTABLE_TIMEZONE_KEY)<=15 && PORTABLE_TIMEZONE_RECORD_BYTES<=64);
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_MISSING && !strcmp(id,"UTC") && !s.put_calls);
    assert(portable_timezone_preference_save(&kv,"Etc/UTC",8)==PORTABLE_TIMEZONE_VIRTUAL_DEFAULT && !s.put_calls);
    assert(portable_timezone_preference_save(&kv,"Unknown/Place",14)==PORTABLE_TIMEZONE_SAVE_INVALID && !s.put_calls);
    assert(portable_timezone_preference_save(&kv,"Asia/Kathmandu",15)==PORTABLE_TIMEZONE_SAVED && s.put_calls==1);
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_LOADED && !strcmp(id,"Asia/Kathmandu"));
    assert(portable_timezone_preference_save(&kv,id,sizeof(id))==PORTABLE_TIMEZONE_UNCHANGED && s.put_calls==1);
    assert(portable_timezone_preference_save(&kv,"Etc/GMT",8)==PORTABLE_TIMEZONE_SAVED);
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_LOADED && !strcmp(id,"UTC"));
    uint8_t valid[44]; memcpy(valid,s.bytes,44);
    for (unsigned i=0;i<44;++i) {
        memcpy(s.bytes,valid,44); s.bytes[i]^=1; int writes=s.put_calls;
        assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD && !strcmp(id,"UTC"));
        assert(s.put_calls==writes);
        assert(portable_timezone_preference_save(&kv,"UTC",4)==PORTABLE_TIMEZONE_SAVED && s.put_calls==writes+1);
    }
    /* Structurally checksummed corruption must not pass as persisted UTC. */
    const char *malformed[]={"Unknown/Place","Etc/UTC","Etc/GMT",""};
    for (unsigned i=0;i<4;++i) {
        memcpy(s.bytes,valid,44); memset(s.bytes+4,0,40); memcpy(s.bytes+4,malformed[i],strlen(malformed[i]));
        s.bytes[3]=0xa5; for (unsigned j=0;j<44;++j) if (j!=3) s.bytes[3]^=s.bytes[j];
        assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD);
    }
    memcpy(s.bytes,valid,44); memset(s.bytes+4,'A',40);
    s.bytes[3]=0xa5; for (unsigned j=0;j<44;++j) if (j!=3) s.bytes[3]^=s.bytes[j];
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD);
    memcpy(s.bytes,valid,44); s.bytes[43]=1;
    s.bytes[3]=0xa5; for (unsigned j=0;j<44;++j) if (j!=3) s.bytes[3]^=s.bytes[j];
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD);
    s.size=64; assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD);
    s.size=43; assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_INVALID_RECORD);
    s.size=44; s.read_rc=RISC_KEY_VALUE_IO;
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_UNAVAILABLE);
    int writes=s.put_calls; assert(portable_timezone_preference_save(&kv,"Asia/Tokyo",11)==PORTABLE_TIMEZONE_SAVE_UNAVAILABLE && s.put_calls==writes);
    s.read_rc=0; s.size=0; s.put_calls=0; s.write_rc=RISC_KEY_VALUE_IO;
    assert(portable_timezone_preference_save(&kv,"Asia/Tokyo",11)==PORTABLE_TIMEZONE_WRITE_FAILED && s.put_calls==1);
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_LOADED && !strcmp(id,"Asia/Tokyo")); /* IO after commit. */
    s.suppress_write=1; s.write_rc=RISC_KEY_VALUE_IO;
    assert(portable_timezone_preference_save(&kv,"UTC",4)==PORTABLE_TIMEZONE_WRITE_FAILED);
    assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_LOADED && !strcmp(id,"Asia/Tokyo"));
    s.write_rc=0;
    assert(portable_timezone_preference_save(&kv,"UTC",4)==PORTABLE_TIMEZONE_VERIFY_FAILED); /* stale readback */
    s.suppress_write=0; s.corrupt=1;
    assert(portable_timezone_preference_save(&kv,"UTC",4)==PORTABLE_TIMEZONE_VERIFY_FAILED);
    s.corrupt=0; s.fail_after=1; s.put_calls=0;
    assert(portable_timezone_preference_save(&kv,"Asia/Tokyo",11)==PORTABLE_TIMEZONE_VERIFY_FAILED);
    kv.api_version=2; assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_UNAVAILABLE);
    kv.api_version=1; kv.struct_size=0; assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_UNAVAILABLE);
    kv.struct_size=sizeof(kv); kv.get=0; assert(portable_timezone_preference_load(&kv,id)==PORTABLE_TIMEZONE_UNAVAILABLE);
    kv.get=get; kv.put=0; assert(portable_timezone_preference_save(&kv,"UTC",4)==PORTABLE_TIMEZONE_SAVE_UNAVAILABLE);
    assert(portable_timezone_preference_load(0,id)==PORTABLE_TIMEZONE_UNAVAILABLE);
    assert(portable_timezone_preference_load(&kv,0)==PORTABLE_TIMEZONE_INVALID_RECORD);
}
int main(void) {
    catalog(); conversion(); invalid(); migration(); preferences();
    puts("Portable timezone: 419 entries, 55308 round trips, 8400 leap edges, DST/folds/gaps, migration and preference failures passed");
    return 0;
}
