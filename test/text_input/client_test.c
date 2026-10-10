/* Negotiation and custody checks against a real old-sized prefix allocation. */
#include "PortableTextInputClient.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static bool alive=true,adapter_retained,provider_active,pending,release_fault;
static unsigned acquires,releases,suspends,resumes,polls,closes,attention_calls;
static int attention_result;
static const risc_text_entry_api_v1 *offered;
static risc_text_entry_state_v1 result;
static uint32_t request_flags;
int portable_text_adapter_suspend(void){assert(!provider_active);++suspends;return 0;}
int portable_text_adapter_resume(void){assert(!provider_active&&releases);++resumes;return 0;}
int portable_text_adapter_attention(void){++attention_calls;return attention_result;}
void portable_text_adapter_retain(void){adapter_retained=true;}
bool portable_text_adapter_retained(void){return adapter_retained;}
static int32_t open_text(void*c,const risc_text_entry_request_v1*q,uint64_t*s){(void)c;assert(suspends&&!provider_active);provider_active=true;request_flags=q->reserved;*s=123;return 0;}
static int32_t poll_text(void*c,uint64_t s,risc_text_entry_state_v1*out){(void)c;assert(s==123&&provider_active);++polls;*out=result;return 0;}
static int32_t close_text(void*c,uint64_t s){(void)c;assert(s==123&&provider_active);++closes;if(pending)return RISC_TEXT_ENTRY_AGAIN;provider_active=false;return 0;}
static bool acquire(const char*cap,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){assert(!strcmp(cap,RISC_TEXT_ENTRY_CAPABILITY)&&v==1&&!instance);++acquires;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=offered,.slot=1,.generation=7};return true;}
static bool release(risc_runtime_capability_v1*g){assert(!provider_active&&g->api==offered);++releases;if(release_fault)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return alive&&v==1?&runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];
 const risc_text_entry_api_v1 prefix={1,sizeof(prefix),NULL,open_text,poll_text,close_text};
 if(!strncmp(mode,"mask-",5)){
  risc_text_entry_api_v1_masked mask={{prefix,RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION},RISC_TEXT_ENTRY_MASKED_TAG,RISC_TEXT_ENTRY_MASKED_VERSION};
  mask.base.base.struct_size=sizeof(mask);
  bool supported=!strcmp(mode,"mask-ok");size_t bytes=!strcmp(mode,"mask-prefix")?sizeof(prefix):!strcmp(mode,"mask-home")?sizeof(mask.base):sizeof(mask);
  void*memory=malloc(bytes);assert(memory);memcpy(memory,&mask,bytes);((risc_text_entry_api_v1*)memory)->struct_size=bytes;
  if(!strcmp(mode,"mask-bad-tag"))((risc_text_entry_api_v1_masked*)memory)->masked_tag^=1;
  if(!strcmp(mode,"mask-bad-version"))((risc_text_entry_api_v1_masked*)memory)->masked_version++;
  offered=memory;portable_text_client client={0};int rc=portable_text_client_begin_mode(&client,&runtime,"Password","synthetic",64,true);
  if(supported){assert(rc==0&&suspends==1&&request_flags==(RISC_TEXT_ENTRY_REQUEST_MASKED|RISC_TEXT_ENTRY_REQUEST_HOME_REASON));assert(portable_text_client_close(&client)==0&&releases==1&&resumes==1);}
  else assert(rc==RISC_TEXT_ENTRY_UNAVAILABLE&&!suspends&&!provider_active&&releases==1&&!resumes);
  free(memory);puts("masked request uses only negotiated capability PASS");return 0;
 }

 risc_text_entry_api_v1_home_reason ext={prefix,RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION};ext.base.struct_size=sizeof(ext);
 bool old=!strcmp(mode,"legacy")||!strcmp(mode,"unsolicited");
 void*allocation=malloc(old?sizeof(prefix):sizeof(ext));assert(allocation);
 if(old)memcpy(allocation,&prefix,sizeof(prefix));else memcpy(allocation,&ext,sizeof(ext));
 offered=allocation;
 if(!strcmp(mode,"short"))((risc_text_entry_api_v1*)allocation)->struct_size=sizeof(ext)-1;
 if(!strcmp(mode,"bad-tag"))((risc_text_entry_api_v1_home_reason*)allocation)->home_reason_tag^=1;
 if(!strcmp(mode,"bad-version"))((risc_text_entry_api_v1_home_reason*)allocation)->home_reason_version=0;
 if(!strcmp(mode,"future-version"))((risc_text_entry_api_v1_home_reason*)allocation)->home_reason_version=2;
 bool negotiated=!old&&strcmp(mode,"short")&&strcmp(mode,"bad-tag")&&strcmp(mode,"bad-version")&&strcmp(mode,"future-version");
 assert(!risc_text_entry_home_reason(NULL));assert(!!risc_text_entry_home_reason(offered)==negotiated);
 portable_text_client client={0};assert(portable_text_client_begin(&client,&runtime,"Name","draft",16)==0);
 assert(request_flags==(negotiated?RISC_TEXT_ENTRY_REQUEST_HOME_REASON:0));
 result=(risc_text_entry_state_v1){.struct_size=sizeof(result),.state=RISC_TEXT_ENTRY_CANCELLED,.revision=2,.text="draft"};
 if(strstr(mode,"home")||!strcmp(mode,"unsolicited"))result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;
 if(!strcmp(mode,"home-editing"))result.state=RISC_TEXT_ENTRY_EDITING;
 if(!strcmp(mode,"home-accepted"))result.state=RISC_TEXT_ENTRY_ACCEPTED;
 pending=strstr(mode,"pending")!=NULL;if(pending)result.flags|=RISC_TEXT_ENTRY_PRESENTING;
 if(!strcmp(mode,"attention-pending")){result.state=RISC_TEXT_ENTRY_EDITING;attention_result=1;}
 risc_text_entry_state_v1 out={0};int rc=portable_text_client_poll(&client,&out);
 bool invalid=!strcmp(mode,"unsolicited")||!strcmp(mode,"home-editing")||!strcmp(mode,"home-accepted");
 if(invalid){assert(rc==RISC_TEXT_ENTRY_RETAINED&&adapter_retained&&client.retained&&!attention_calls);assert(portable_text_client_poll(&client,&out)==RISC_TEXT_ENTRY_RETAINED&&portable_text_client_close(&client)==RISC_TEXT_ENTRY_RETAINED&&polls==1&&!closes&&!releases&&!resumes);}
 else{
  assert(!rc&&!strcmp(out.text,"draft"));
  if(!strcmp(mode,"attention-pending")){/* Baseline intentionally defers attention; the combined client must cancel now. */
#ifdef EXPECT_DECOUPLED_ATTENTION
   assert(attention_calls==1&&out.state==RISC_TEXT_ENTRY_CANCELLED);
#else
   assert(!attention_calls&&out.state==RISC_TEXT_ENTRY_EDITING);
#endif
  }
  else assert(out.state==RISC_TEXT_ENTRY_CANCELLED&&out.flags==result.flags);
  if(pending){assert(portable_text_client_close(&client)==RISC_TEXT_ENTRY_AGAIN&&client.active&&client.acquired&&client.suspended&&client.closing&&!releases&&!resumes);assert(portable_text_client_poll(&client,&out)==RISC_TEXT_ENTRY_AGAIN&&polls==1);pending=false;}
  release_fault=!strcmp(mode,"release-fault");rc=portable_text_client_close(&client);
  if(release_fault){assert(rc==RISC_TEXT_ENTRY_RETAINED&&client.retained&&adapter_retained&&!resumes);}
  else {assert(!rc&&!client.active&&!client.acquired&&!client.suspended&&!client.session&&releases==1&&resumes==1);assert(portable_text_client_poll(&client,&out)==RISC_TEXT_ENTRY_STALE);assert(portable_text_client_close(&client)==0&&releases==1&&resumes==1);}
 }
 free(allocation);printf("text client %s PASS\n",mode);return 0;
}
