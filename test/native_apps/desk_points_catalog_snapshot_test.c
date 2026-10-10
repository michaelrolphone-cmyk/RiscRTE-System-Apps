#include "PortableDeskClock.h"
#include <assert.h>
#include <stdio.h>
#include "fixtures/desk_points_schema2.h"
static portable_points_catalog_event event(unsigned id,unsigned deadline) {
 portable_points_catalog_event e={.event_id=id,.type_id=70000+id,.revision=11,.deadline=deadline,
  .parent_day=9500,.color=0x1234abcd,.mode=3};
 strcpy(e.label,"1234567890123456789012345678901");return e;
}
int main(void) {
 /* The schema2 payload remains a separate compatibility fixture. These
  * offsets are the frozen 0.3.16 wire map, not the new catalog format. */
 legacy_desk_points_snapshot legacy={0};points_config old=points_default_config(),old_out;
 points_meta meta=points_default_meta(),meta_out;
 assert(legacy_desk_points_pack(&legacy,&old,&meta));
 assert(legacy.bytes[0]==1 && legacy.bytes[8]==63 && legacy.bytes[9]==30);
 assert(legacy.bytes[56]==1 && legacy.bytes[60]==6 && !memcmp(legacy.bytes+61,"Drive to Work",14));
 assert(legacy.bytes[75]==0 && !memcmp(legacy.bytes+76,"Wakeup",7));
 assert(legacy_desk_points_unpack(&legacy,&old_out,&meta_out));
 assert(old_out.points[6].kind==POINTS_WORK_END && !strcmp(meta_out.custom[0].name,"Drive to Work"));
 uint8_t legacy_wire[128]={0};memcpy(legacy_wire,"DCLK",4);legacy_wire[4]=2;legacy_wire[5]=1;
 legacy_wire[6]=6;legacy_wire[25]=1;legacy_wire[28]=0x60;legacy_wire[29]=0xc8;legacy_wire[30]=0xd4;legacy_wire[31]=0x29;
 memcpy(legacy_wire+32,legacy.bytes,90);
 portable_desk_record no_reinterpretation;
 assert(!portable_desk_decode(legacy_wire,sizeof(legacy_wire),&no_reinterpretation));
 portable_desk_record record={.config={.face=PORTABLE_DESK_POINTS,.time_format=1,.rtc_stores_utc=1},
  .displayed_minute=1791331140,.has_image=true};strcpy(record.config.time_zone,"America/Denver");
 portable_points_catalog_view *v=&record.config.points.view;
 record.config.points.status=PORTABLE_DESK_POINTS_READY;
 *v=(portable_points_catalog_view){.struct_size=sizeof(*v),.catalog_revision=0xfedcba98,.snapshot=77,
  .seconds=1000,.count=4,.has_previous=1,.valid_until=1400,.flags=3,.previous=event(50001,900)};
 for(unsigned i=0;i<4;i++)v->next[i]=event(50002+i,1100+i*100);
 v->previous.symbol=7;for(unsigned i=0;i<4;i++)v->next[i].symbol=(uint8_t)(i+1);
 assert(portable_points_catalog_view_valid(v));
 v->previous.parent_day=1;assert(portable_points_catalog_view_valid(v));
 v->previous.parent_day=0;assert(!portable_points_catalog_view_valid(v));v->previous.parent_day=9500;
 portable_points_catalog_view advanced={0};
 assert(portable_points_catalog_view_at(v,1200,&advanced));
 assert(advanced.count==2 && advanced.previous.event_id==50003 && advanced.next[0].event_id==50004);
 assert(!strcmp(advanced.next[0].label,"1234567890123456789012345678901"));
 advanced.catalog_revision=42;
 assert(!portable_points_catalog_view_at(v,999,&advanced) && advanced.catalog_revision==42);
 assert(!portable_points_catalog_view_at(v,1400,&advanced) && advanced.catalog_revision==42);
 uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];assert(sizeof(bytes)==408);
 assert(portable_desk_encode(&record,bytes,sizeof(bytes)));
 assert(bytes[4]==3 && bytes[25]==1 && bytes[80]==0x98 && bytes[83]==0xfe);
 /* Header80 + metadata28 + previous60: the first future row starts at168. */
 assert(portable_desk_read32(bytes+168)==50002);
 assert(!memcmp(bytes+168+28,"1234567890123456789012345678901",32));
 portable_desk_record decoded={0};assert(portable_desk_decode(bytes,sizeof(bytes),&decoded));
 assert(portable_desk_same_config(&decoded.config,&record.config));
 assert(decoded.config.points.view.next[3].type_id==120005);
 assert(decoded.config.points.view.previous.symbol==7 && decoded.config.points.view.next[3].symbol==4);
 uint8_t bad[sizeof(bytes)];
 memcpy(bad,bytes,sizeof(bad));bad[4]=2;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));bad[26]=1;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));bad[80+12]=5;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));bad[168+26]=8;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));bad[168+27]=1;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));memset(bad+168+28,'x',32);assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 memcpy(bad,bytes,sizeof(bad));bad[168+28]=0xff;assert(!portable_desk_decode(bad,sizeof(bad),&decoded));
 v->count=5;assert(!portable_desk_encode(&record,bytes,sizeof(bytes)));v->count=4;
 record.config.points.status=PORTABLE_DESK_POINTS_UNAVAILABLE;
 assert(portable_desk_encode(&record,bytes,sizeof(bytes)));
 for(unsigned i=80;i<sizeof(bytes);i++)assert(!bytes[i]);
 assert(portable_desk_decode(bytes,sizeof(bytes),&decoded));
 puts("Schema3 full copied catalog:408 bytes, five full labels/IDs, expiry/rewind and malformed data passed");
 return 0;
}
