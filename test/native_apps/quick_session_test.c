/* Preference corruption/uncertain-write matrix for the production session. */
#include "PortableQuickSession.h"
#include "PortableTimeFormat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {uint8_t bytes[8];uint32_t size;bool present;} record;
static record records[3];
static unsigned put_count[3],gets[3],grants,releases,hardware,calls;
static bool unavailable,invalid_api,release_fail,hardware_fail;
static int fail_get_key=-1,fail_after_put_key=-1,fail_put_key=-1,committed_io_key=-1,mismatch_key=-1;
static char write_order[16];static unsigned order_count;
static int index_for(const char *key){
 if(!strcmp(key,PQA_BRIGHTNESS_KEY))return 0;
 if(!strcmp(key,PQA_VOLUME_KEY))return 1;
 if(!strcmp(key,PQA_RESTORE_VOLUME_KEY))return 2;
 assert(!strcmp(key,PORTABLE_TIME_FORMAT_KEY));return -1;
}
static int32_t get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){
 (void)c;int i=index_for(key);*size=0;if(i<0)return RISC_KEY_VALUE_NOT_FOUND;
 gets[i]++;if(i==fail_get_key || (i==fail_after_put_key && put_count[i]))return RISC_KEY_VALUE_IO;
 if(!records[i].present)return RISC_KEY_VALUE_NOT_FOUND;
 *size=records[i].size;if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(out,records[i].bytes,*size);
 if(i==mismatch_key && put_count[i] && *size)((uint8_t*)out)[0]^=1;
 return RISC_KEY_VALUE_OK;
}
static int32_t put(void *c,const char *key,const void *data,uint32_t size){
 (void)c;int i=index_for(key);assert(i>=0 && size<=8);put_count[i]++;
 assert(order_count<sizeof(write_order));write_order[order_count++]=(char)('0'+i);
 if(i==fail_put_key)return RISC_KEY_VALUE_IO;
 records[i].present=true;records[i].size=size;memcpy(records[i].bytes,data,size);
 return i==committed_io_key?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out){
 assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY) && version==1 && id==1);
 if(unavailable)return false;
 kv.api_version=invalid_api?0:1;out->api=&kv;grants++;return true;
}
static bool release(risc_runtime_capability_v1 *out){
 assert(out->api && grants);releases++;
 if(release_fail)return false;
 out->api=NULL;grants--;return true;
}
static bool set_brightness(void *c,uint16_t level,uint16_t maximum){
 (void)c;assert(maximum==100 && level<=100);calls++;if(hardware_fail)return false;
 hardware=level;return true;
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.set_brightness=set_brightness};
static pqa_session fresh(void){
 memset(records,0,sizeof(records));memset(put_count,0,sizeof(put_count));memset(gets,0,sizeof(gets));
 grants=releases=calls=order_count=0;hardware=40;
 unavailable=invalid_api=release_fail=hardware_fail=false;
 fail_get_key=fail_after_put_key=fail_put_key=committed_io_key=mismatch_key=-1;
 pqa_session session;pqa_session_init(&session);return session;
}
static void saved(unsigned index,unsigned value,uint32_t size){
 records[index].present=true;records[index].size=size;records[index].bytes[0]=(uint8_t)value;
}
static void load(pqa_session *s){assert(pqa_session_load(s,&runtime));assert(!grants);}
static bool apply(pqa_session *s,uint32_t actions){bool changed=false;assert(pqa_session_apply(s,&runtime,&display_api,actions,&changed));assert(!grants);return changed;}
static void defaults(void){
 pqa_session s=fresh();load(&s);
 assert(s.brightness==40 && s.volume==50 && s.restore_volume==50);
 assert(s.ui.brightness_valid && s.ui.volume_valid && s.ui.last_nonzero_volume==50);
 assert(!put_count[0] && !put_count[1] && !put_count[2] && !calls && !s.hour_24);
 for(unsigned variant=0;variant<2;variant++){
  s=fresh();unavailable=variant==0;invalid_api=variant==1;load(&s);
  assert(s.brightness==40 && s.volume==50 && !s.ui.brightness_valid && !s.ui.volume_valid);
  assert(!put_count[0] && !put_count[1] && !put_count[2]);
 }
}
static void corruption(void){
 for(unsigned size=0;size<=2;size++)for(unsigned value=0;value<=101;value++){
  pqa_session s=fresh();saved(0,value,size);saved(1,value,size);saved(2,value,size);load(&s);
  bool bv=size==1 && value>=10 && value<=100,vv=size==1 && value<=100;
  assert(s.ui.brightness_valid==bv && s.ui.volume_valid==vv);
  assert(s.brightness==(bv?value:40) && s.volume==(vv?value:50));
  assert(!put_count[0] && !put_count[1] && !put_count[2]);
 }
 pqa_session s=fresh();saved(0,80,1);saved(1,70,1);load(&s);
 fail_get_key=0;load(&s);assert(!s.ui.brightness_valid && s.brightness==80);
 fail_get_key=1;load(&s);assert(!s.ui.volume_valid && s.volume==70);
 s=fresh();saved(1,0,1);saved(2,80,1);load(&s);
 assert(s.ui.volume_valid && s.volume==0 && s.restore_volume==80 && s.ui.last_nonzero_volume==80);
}
static void brightness_matrix(void){
 for(unsigned mode=0;mode<5;mode++){
  pqa_session s=fresh();load(&s);s.ui.brightness=90;s.ui.action_brightness=90;
  assert(!apply(&s,PQA_BRIGHTNESS_PREVIEW));assert(hardware==90 && !put_count[0] && s.brightness==40);
  if(mode==1)committed_io_key=0;
  if(mode==2)fail_put_key=0;
  if(mode==3)fail_after_put_key=0;
  if(mode==4)mismatch_key=0;
  assert(!apply(&s,PQA_BRIGHTNESS_COMMIT));
  bool confirmed=mode<=1;
  assert(s.brightness==(confirmed?90:40) && hardware==(confirmed?90:40));
  assert(!!(s.ui.error_flags&PQA_ERROR_SAVE)==!confirmed);
  assert(put_count[0]==1 && !put_count[1] && !put_count[2]);
 }
 pqa_session s=fresh();load(&s);s.ui.action_brightness=80;hardware_fail=true;bool changed=true;
 assert(!pqa_session_apply(&s,&runtime,&display_api,PQA_BRIGHTNESS_PREVIEW,&changed));
 assert(s.ui.error_flags&PQA_ERROR_BRIGHTNESS);assert(!put_count[0] && hardware==40);
}
static void silent_matrix(void){
 for(unsigned mode=0;mode<6;mode++){
  pqa_session s=fresh();saved(1,80,1);load(&s);s.ui.volume=0;s.ui.action_volume=0;
  if(mode==1)committed_io_key=1;
  if(mode==2)fail_put_key=2;
  if(mode==3)fail_after_put_key=2;
  if(mode==4)fail_after_put_key=1;
  if(mode==5)mismatch_key=1;
  bool confirmed=mode<=1,changed=apply(&s,PQA_SILENT);
  assert(changed==confirmed && s.volume==(confirmed?0:80));
  assert(s.ui.volume==(confirmed?0:80));
  assert(put_count[2]==1 && (mode==2 || mode==3 ? put_count[1]==0 : put_count[1]==1));
  assert(write_order[0]=='2');if(put_count[1])assert(write_order[1]=='1');
  assert(!put_count[0] && !calls);assert(!!(s.ui.error_flags&PQA_ERROR_SAVE)==!confirmed);
  if(confirmed){
   pqa_session r;pqa_session_init(&r);load(&r);assert(r.volume==0 && r.restore_volume==80);
   r.ui.volume=80;r.ui.action_volume=80;assert(apply(&r,PQA_SILENT));assert(r.volume==80 && records[1].bytes[0]==80);
  }
 }
}
static void transient_and_lifecycle(void){
 pqa_session s=fresh();load(&s);s.ui.torch=true;s.ui.action_torch=true;assert(!apply(&s,PQA_TORCH));assert(hardware==100);
 s.ui.torch=false;s.ui.action_torch=false;assert(!apply(&s,PQA_TORCH));assert(hardware==40);
 assert(!put_count[0] && !put_count[1] && !put_count[2]);
 s=fresh();release_fail=true;assert(!pqa_session_load(&s,&runtime));assert(releases==1 && grants==1);
}
int main(void){defaults();corruption();brightness_matrix();silent_matrix();transient_and_lifecycle();puts("quick session: corruption, defaults, uncertain writes, readback, silent restore and transient hardware passed");return 0;}
