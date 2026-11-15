#define PORTABLE_DESK_POINTS_SNAPSHOT
#define PORTABLE_DESK_POINTS_FACE
#include "PortableDeskClock.h"
#include "PortableTimeZone.h"
#include <assert.h>
#include <stdio.h>
static const char token[]="@2007:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0";
static int64_t epoch(int year,int month,int day,int hour,int minute) {
 portable_timezone_civil c={year,month,day,hour,minute,0,0};int64_t e;
 assert(!portable_timezone_civil_to_epoch(&c,&e));return e;
}
static void clock_at(const portable_timezone_rule *r,int y,int m,int d,int h,int min,int expected_h,int expected_min) {
 portable_timezone_civil c;assert(!portable_timezone_utc_to_local(r,epoch(y,m,d,h,min),&c,NULL));
 assert(c.hour==expected_h&&c.minute==expected_min);
}
static portable_points_catalog_event event(unsigned id,int64_t e) {
 portable_points_catalog_event v={.event_id=id,.type_id=id+70000,.revision=5,
  .deadline=(uint32_t)(e-INT64_C(946684800)),.parent_day=2557,.color=0x123456,.mode=3,.symbol=7};
 strcpy(v.label,"Saved full catalog event label");return v;
}
int main(void) {
 assert(sizeof(token)==40);portable_timezone_rule rule;
 assert(!portable_timezone_resolve(token,sizeof(token),&rule));
 clock_at(&rule,2000,3,20,18,0,11,0);clock_at(&rule,2007,3,20,18,0,12,0);
 clock_at(&rule,2000,4,2,8,59,1,59);clock_at(&rule,2000,4,2,9,0,3,0);
 clock_at(&rule,2000,10,29,7,59,1,59);clock_at(&rule,2000,10,29,8,0,1,0);
 clock_at(&rule,2006,12,31,23,59,16,59);clock_at(&rule,2007,1,1,0,0,17,0);
 clock_at(&rule,2007,3,11,8,59,1,59);clock_at(&rule,2007,3,11,9,0,3,0);
 clock_at(&rule,2007,11,4,7,59,1,59);clock_at(&rule,2007,11,4,8,0,1,0);
 portable_timezone_civil civil={2000,4,2,2,30,0,0};portable_timezone_inverse candidates;
 assert(portable_timezone_local_to_utc(&rule,&civil,&candidates)==PORTABLE_TIMEZONE_GAP&&candidates.count==0);
 civil=(portable_timezone_civil){2000,10,29,1,30,0,0};
 assert(portable_timezone_local_to_utc(&rule,&civil,&candidates)==PORTABLE_TIMEZONE_FOLD&&candidates.count==2);
 for(unsigned i=0;i<2;i++) {
  portable_timezone_civil back;assert(!portable_timezone_utc_to_local(&rule,candidates.candidate[i].epoch,&back,NULL));
  assert(back.hour==1&&back.minute==30);
 }
 const char generic[]="@2040:0,-1;M1.1.0,M2.1.0;M6.1.0,M7.1.0";
 portable_timezone_rule other;assert(!portable_timezone_resolve(generic,sizeof(generic),&other));
 clock_at(&other,2039,1,15,12,0,13,0);clock_at(&other,2040,1,15,12,0,12,0);
 clock_at(&other,2040,6,15,12,0,13,0);
 portable_timezone_rule unchanged=rule;
 const char *bad[]={"@200:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0",
  "@2007:25,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0","@2007:7,6;M4.1.0,M10.5.0;M13.2.0,M11.1.0",
  "@2007:7,6;M4.1.0,M10.5.0","@0000:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
  assert(portable_timezone_resolve(bad[i],strlen(bad[i])+1,&rule)==PORTABLE_TIMEZONE_BAD_RULE);
  assert(!memcmp(&rule,&unchanged,sizeof(rule)));
 }
 char missing_nul[40];memset(missing_nul,'x',40);missing_nul[0]='@';
 assert(portable_timezone_resolve(missing_nul,40,&rule)==PORTABLE_TIMEZONE_BAD_RULE);
 assert(portable_timezone_resolve(token,39,&rule)==PORTABLE_TIMEZONE_BAD_RULE);
 portable_desk_record record={.config={.face=PORTABLE_DESK_POINTS,.time_format=1,.rtc_stores_utc=1},
  .displayed_minute=0,.has_image=true};
 record.displayed_minute=epoch(2006,12,31,23,58);memcpy(record.config.time_zone,token,40);
 portable_points_catalog_view *v=&record.config.points.view;
 record.config.points.status=PORTABLE_DESK_POINTS_READY;
 *v=(portable_points_catalog_view){.struct_size=sizeof(*v),.catalog_revision=12,.snapshot=91,
  .seconds=(uint32_t)(record.displayed_minute-INT64_C(946684800)),.count=2};
 v->valid_until=v->seconds+600;v->next[0]=event(5,record.displayed_minute+60);v->next[1]=event(6,record.displayed_minute+180);
 assert(portable_points_catalog_view_valid(v));
 uint8_t bytes[408],roundtrip[408];assert(PORTABLE_DESK_CLOCK_RECORD_BYTES==408);
 assert(portable_desk_encode(&record,bytes,sizeof(bytes))&&!memcmp(bytes+32,token,40));
 portable_desk_record decoded;assert(portable_desk_decode(bytes,sizeof(bytes),&decoded));
 assert(portable_desk_encode(&decoded,roundtrip,sizeof(roundtrip))&&!memcmp(bytes,roundtrip,408));
 assert(!portable_timezone_resolve(decoded.config.time_zone,40,&rule));
 for(unsigned i=0;i<v->count;i++) {
  portable_timezone_civil before,after;int64_t e=INT64_C(946684800)+v->next[i].deadline;
  assert(!portable_timezone_utc_to_local(&unchanged,e,&before,NULL));
  assert(!portable_timezone_utc_to_local(&rule,e,&after,NULL)&&before.hour==after.hour&&before.day==after.day);
 }
 portable_points_catalog_view advanced;assert(portable_points_catalog_view_at(v,v->seconds+180,&advanced));
 assert(advanced.has_previous&&advanced.previous.event_id==6&&advanced.count==0);
 assert(!memcmp(bytes,roundtrip,408)); /* advancing copied window never rewrites the saved record */
 strcpy(record.config.time_zone,"America/Denver");
 assert(portable_desk_encode(&record,bytes,408)&&portable_desk_decode(bytes,408,&decoded));
 assert(portable_desk_encode(&decoded,roundtrip,408)&&!memcmp(bytes,roundtrip,408));
 assert(!portable_timezone_resolve(record.config.time_zone,40,&rule));
 clock_at(&rule,2000,3,20,18,0,12,0); /* existing native-UTC IANA behavior unchanged */
 assert(!portable_timezone_parse("UTC0",5,&rule)&&!rule.era_year);
 puts("Retained civil eras: generic transitions, cutover/fold/gap/refusal and unchanged408-byte catalog records PASS");
}
