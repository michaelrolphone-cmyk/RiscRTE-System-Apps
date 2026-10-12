/* New optional client tested against synthetic ABI providers and real short
 * allocations; no production hooks or copied implementations. */
#define main legacy_client_fixture_main
#include "client_test.c"
#undef main
#include "PortableTextInputCheckpointClient.h"
static risc_text_entry_api_v1_checkpoint cp_api;
static unsigned cp_calls;
static int enable_rc,snapshot_rc,pause_rc,resume_rc;
static const char *lose_at;
static risc_text_entry_checkpoint_state_v1 cp_state={sizeof(cp_state),0};
static int cp_boundary(const char*site,int rc){assert(alive&&!adapter_retained);++cp_calls;if(lose_at&&!strcmp(site,lose_at))alive=false;return rc;}
static int cp_enable(void*c,uint64_t s){(void)c;assert(s==123);return cp_boundary("enable",enable_rc);}
static int cp_snapshot(void*c,uint64_t s,risc_text_entry_checkpoint_state_v1*out){(void)c;assert(s==123&&out->struct_size==sizeof(*out));*out=cp_state;return cp_boundary("snapshot",snapshot_rc);}
static int cp_pause(void*c,uint64_t s){(void)c;assert(s==123);return cp_boundary("pause",pause_rc);}
static int cp_resume(void*c,uint64_t s,uint32_t flags){(void)c;assert(s==123&&!(flags&~(RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)));return cp_boundary("resume",resume_rc);}
static int unused_extended_open(void*c,const risc_text_entry_request_v1_extended*q,uint64_t*s){(void)c;(void)q;(void)s;assert(0);return -1;}
static int unused_extended_poll(void*c,uint64_t s,risc_text_entry_state_v1_extended*out){(void)c;(void)s;(void)out;assert(0);return -1;}
static int unused_long_open(void*c,const risc_text_entry_request_v1_long*q,uint64_t*s){(void)c;(void)q;(void)s;assert(0);return -1;}
static int unused_long_poll(void*c,uint64_t s,risc_text_entry_state_v1_long*out){(void)c;(void)s;(void)out;assert(0);return -1;}
static void setup(void){
 cp_api.base.base.base.base.base=(risc_text_entry_api_v1){1,sizeof(cp_api),NULL,open_text,poll_text,close_text};
 cp_api.base.base.base.base.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG;cp_api.base.base.base.base.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION;
 cp_api.base.base.base.masked_tag=RISC_TEXT_ENTRY_MASKED_TAG;cp_api.base.base.base.masked_version=RISC_TEXT_ENTRY_MASKED_VERSION;
 cp_api.base.base.extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG;cp_api.base.base.extended_version=RISC_TEXT_ENTRY_EXTENDED_VERSION;cp_api.base.base.open_extended=unused_extended_open;cp_api.base.base.poll_extended=unused_extended_poll;
 cp_api.base.long_tag=RISC_TEXT_ENTRY_LONG_TAG;cp_api.base.long_version=RISC_TEXT_ENTRY_LONG_VERSION;cp_api.base.open_long=unused_long_open;cp_api.base.poll_long=unused_long_poll;
 cp_api.checkpoint_tag=RISC_TEXT_ENTRY_CHECKPOINT_TAG;cp_api.checkpoint_version=RISC_TEXT_ENTRY_CHECKPOINT_VERSION;cp_api.checkpoint_enable=cp_enable;cp_api.checkpoint_snapshot=cp_snapshot;cp_api.pause=cp_pause;cp_api.resume=cp_resume;cp_api.pause_for_focus_change=cp_pause;offered=(const risc_text_entry_api_v1*)&cp_api;
}
static void fence(portable_text_client*c,portable_text_checkpoint_client*p){unsigned before=cp_calls+closes+releases+resumes;risc_text_entry_checkpoint_state_v1 out={0};assert(c->retained||adapter_retained);assert(portable_text_checkpoint_enable(c,p)==RISC_TEXT_ENTRY_RETAINED);assert(portable_text_checkpoint_snapshot(c,p,&out)==RISC_TEXT_ENTRY_RETAINED);assert(portable_text_checkpoint_pause(c,p)==RISC_TEXT_ENTRY_RETAINED);assert(portable_text_checkpoint_resume(c,p,0)==RISC_TEXT_ENTRY_RETAINED);assert(portable_text_client_close(c)==RISC_TEXT_ENTRY_RETAINED);assert(before==cp_calls+closes+releases+resumes);}
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];setup();portable_text_client c={0};portable_text_checkpoint_client p={0};risc_text_entry_checkpoint_state_v1 out={0};
 if(!strcmp(mode,"prefix")){
  for(size_t size=sizeof(risc_text_entry_api_v1);size<sizeof(cp_api);size++){void*short_api=malloc(size);assert(short_api);memcpy(short_api,&cp_api,size);((risc_text_entry_api_v1*)short_api)->struct_size=(uint32_t)size;assert(!risc_text_entry_checkpoint(short_api));free(short_api);}
  puts("review checkpoint client bounded old allocations PASS");return 0;
 }
 assert(!portable_text_client_begin(&c,&runtime,"NAME","draft",25));
 if(!strncmp(mode,"enable-",7)){
  const char *what=mode+7;if(!strcmp(what,"busy"))enable_rc=RISC_TEXT_ENTRY_BUSY;else if(!strcmp(what,"unavailable"))enable_rc=RISC_TEXT_ENTRY_UNAVAILABLE;else if(!strcmp(what,"loss"))lose_at="enable";else enable_rc=!strcmp(what,"retained")?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_INVALID;
  int rc=portable_text_checkpoint_enable(&c,&p);
  if(enable_rc==RISC_TEXT_ENTRY_BUSY||enable_rc==RISC_TEXT_ENTRY_UNAVAILABLE){assert(rc==enable_rc&&!p.enabled&&!adapter_retained);assert(!portable_text_client_close(&c));}else{assert(rc==RISC_TEXT_ENTRY_RETAINED);fence(&c,&p);}
 }else{
  assert(!portable_text_checkpoint_enable(&c,&p)&&p.enabled);unsigned before=cp_calls;assert(portable_text_checkpoint_enable(&c,&p)==RISC_TEXT_ENTRY_BUSY&&cp_calls==before);
  if(!strncmp(mode,"snapshot-",9)){
   const char *what=mode+9;if(!strcmp(what,"loss"))lose_at="snapshot";else if(!strcmp(what,"retained"))snapshot_rc=RISC_TEXT_ENTRY_RETAINED;else if(!strcmp(what,"busy"))snapshot_rc=RISC_TEXT_ENTRY_BUSY;else if(!strcmp(what,"size"))cp_state.struct_size--;else if(!strcmp(what,"flags"))cp_state.flags=16;else if(!strcmp(what,"phase"))cp_state.flags=RISC_TEXT_ENTRY_CHECKPOINT_PAUSED;
   else if(!strcmp(what,"paused-presenting")||!strcmp(what,"paused-busy")){assert(!portable_text_checkpoint_pause(&c,&p));cp_state.flags=RISC_TEXT_ENTRY_CHECKPOINT_PAUSED|(!strcmp(what,"paused-presenting")?RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING:RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY);}
   else assert(0);
   int rc=portable_text_checkpoint_snapshot(&c,&p,&out);if(snapshot_rc==RISC_TEXT_ENTRY_BUSY){assert(rc==snapshot_rc);assert(!portable_text_client_close(&c));}else{assert(rc==RISC_TEXT_ENTRY_RETAINED);fence(&c,&p);}
  }else if(!strncmp(mode,"pause-",6)){
   const char *what=mode+6;if(!strcmp(what,"loss"))lose_at="pause";else if(!strcmp(what,"retained"))pause_rc=RISC_TEXT_ENTRY_RETAINED;else if(!strcmp(what,"busy"))pause_rc=RISC_TEXT_ENTRY_BUSY;else if(!strcmp(what,"busy-after-again")){pause_rc=RISC_TEXT_ENTRY_AGAIN;assert(portable_text_checkpoint_pause(&c,&p)==RISC_TEXT_ENTRY_AGAIN&&p.pausing);pause_rc=RISC_TEXT_ENTRY_BUSY;}else pause_rc=RISC_TEXT_ENTRY_INVALID;
   int rc=portable_text_checkpoint_pause(&c,&p);if(pause_rc==RISC_TEXT_ENTRY_BUSY&&!p.pausing){assert(rc==RISC_TEXT_ENTRY_BUSY);assert(!portable_text_client_close(&c));}else{assert(rc==RISC_TEXT_ENTRY_RETAINED);fence(&c,&p);}
  }else if(!strncmp(mode,"resume-",7)){
   assert(!portable_text_checkpoint_pause(&c,&p));const char *what=mode+7;if(!strcmp(what,"loss"))lose_at="resume";else resume_rc=!strcmp(what,"retained")?RISC_TEXT_ENTRY_RETAINED:!strcmp(what,"busy")?RISC_TEXT_ENTRY_BUSY:RISC_TEXT_ENTRY_INVALID;
   assert(portable_text_checkpoint_resume(&c,&p,RISC_TEXT_ENTRY_RESUME_REDRAW)==RISC_TEXT_ENTRY_RETAINED);fence(&c,&p);
  }else if(!strcmp(mode,"lifecycle")){
   cp_state.flags=RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY;assert(!portable_text_checkpoint_snapshot(&c,&p,&out)&&out.flags==cp_state.flags);pause_rc=RISC_TEXT_ENTRY_AGAIN;assert(portable_text_checkpoint_pause(&c,&p)==RISC_TEXT_ENTRY_AGAIN&&p.pausing&&!p.paused);assert(portable_text_checkpoint_pause(&c,&p)==RISC_TEXT_ENTRY_AGAIN);pause_rc=0;assert(!portable_text_checkpoint_pause(&c,&p)&&p.paused&&!p.pausing);before=cp_calls;assert(!portable_text_checkpoint_pause(&c,&p)&&cp_calls==before);cp_state.flags=RISC_TEXT_ENTRY_CHECKPOINT_PAUSED;assert(!portable_text_checkpoint_snapshot(&c,&p,&out));assert(!portable_text_checkpoint_resume(&c,&p,RISC_TEXT_ENTRY_RESUME_REDRAW)&&!p.paused);assert(!portable_text_client_close(&c));before=cp_calls;assert(portable_text_checkpoint_snapshot(&c,&p,&out)==RISC_TEXT_ENTRY_BUSY&&cp_calls==before);
  }else if(!strcmp(mode,"close-paused")){assert(!portable_text_checkpoint_pause(&c,&p));before=cp_calls;assert(!portable_text_client_close(&c)&&cp_calls==before&&closes==1&&releases==1&&resumes==1);}
  else assert(0);
 }
 printf("review checkpoint client %s PASS\n",mode);return 0;
}
