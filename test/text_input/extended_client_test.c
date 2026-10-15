#include "PortableTextInputExtendedClient.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static const risc_text_entry_api_v1 *provider;
static bool provider_active,retained;
static unsigned acquired,released,suspended,resumed,opened,polled,closed;
static risc_text_entry_state_v1_extended result;
int portable_text_adapter_suspend(void){assert(!suspended&&!retained);suspended=1;return 0;}
int portable_text_adapter_resume(void){assert(suspended&&!provider_active&&!retained);suspended=0;resumed++;return 0;}
int portable_text_adapter_attention(void){assert(suspended&&!retained);return 0;}
void portable_text_adapter_retain(void){retained=true;}
bool portable_text_adapter_retained(void){return retained;}
static int32_t old_open(void*c,const risc_text_entry_request_v1*q,uint64_t*s){(void)c;(void)q;(void)s;assert(false);return -1;}
static int32_t old_poll(void*c,uint64_t s,risc_text_entry_state_v1*v){(void)c;(void)s;(void)v;assert(false);return -1;}
static int32_t extended_open(void*c,const risc_text_entry_request_v1_extended*q,uint64_t*s){(void)c;assert(suspended&&!provider_active);assert(q->capacity==128&&q->reserved==RISC_TEXT_ENTRY_REQUEST_HOME_REASON&&strlen(q->text)==127);opened++;provider_active=true;*s=123;return 0;}
static int32_t extended_poll(void*c,uint64_t s,risc_text_entry_state_v1_extended*v){(void)c;assert(s==123&&provider_active);polled++;*v=result;return 0;}
static int32_t close_text(void*c,uint64_t s){(void)c;assert(s==123&&provider_active);closed++;provider_active=false;return 0;}
static const risc_text_entry_api_v1_extended extended={{{{1,sizeof(extended),NULL,old_open,old_poll,close_text},RISC_TEXT_ENTRY_HOME_REASON_TAG,1},RISC_TEXT_ENTRY_MASKED_TAG,1},RISC_TEXT_ENTRY_EXTENDED_TAG,1,extended_open,extended_poll};
static bool acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){assert(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)&&v==1&&!instance);g->api=provider;g->slot=1;g->generation=2;acquired++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api==provider&&!provider_active);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};released++;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!retained?&runtime:NULL;}
int main(int argc,char**argv){assert(argc==2);const char*mode=argv[1];size_t size=sizeof(extended);
 if(!strcmp(mode,"prefix"))size=sizeof(risc_text_entry_api_v1);
 if(!strcmp(mode,"home"))size=sizeof(risc_text_entry_api_v1_home_reason);
 if(!strcmp(mode,"masked"))size=sizeof(risc_text_entry_api_v1_masked);
 if(!strcmp(mode,"short"))size--;
 unsigned char *allocation=malloc(size);assert(allocation);memcpy(allocation,&extended,size);((risc_text_entry_api_v1*)allocation)->struct_size=(uint32_t)size;provider=(const risc_text_entry_api_v1*)allocation;
 if(!strcmp(mode,"tag"))((risc_text_entry_api_v1_extended*)allocation)->extended_tag^=1;
 if(!strcmp(mode,"version"))((risc_text_entry_api_v1_extended*)allocation)->extended_version++;
 if(!strcmp(mode,"open-null"))((risc_text_entry_api_v1_extended*)allocation)->open_extended=NULL;
 if(!strcmp(mode,"poll-null"))((risc_text_entry_api_v1_extended*)allocation)->poll_extended=NULL;
 char initial[128];memset(initial,'Q',127);initial[127]=0;portable_text_client client={0};
 int rc=portable_text_client_begin_extended(&client,&runtime,"Rename",initial,128);
 bool unsupported=size<sizeof(extended)||!strcmp(mode,"tag")||!strcmp(mode,"version")||strstr(mode,"null");
 if(unsupported){assert(rc==RISC_TEXT_ENTRY_UNAVAILABLE&&!opened&&!suspended&&acquired==1&&released==1&&!retained);}
 else{
  assert(rc==0&&opened==1&&suspended&&!released);
  result=(risc_text_entry_state_v1_extended){.struct_size=sizeof(result),.revision=1,.state=RISC_TEXT_ENTRY_ACCEPTED};memcpy(result.text,initial,128);
  bool bad=!strncmp(mode,"bad-",4);
  if(!strcmp(mode,"bad-text"))memset(result.text,'X',128);
  if(!strcmp(mode,"bad-ascii"))result.text[1]='\n';
  if(!strcmp(mode,"bad-size"))result.struct_size--;
  if(!strcmp(mode,"bad-revision"))result.revision=0;
  if(!strcmp(mode,"bad-flags"))result.flags=8;
  if(!strcmp(mode,"bad-home"))result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;
  if(!strcmp(mode,"home-cancel")){result.state=RISC_TEXT_ENTRY_CANCELLED;result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;}
  risc_text_entry_state_v1_extended out={0};rc=portable_text_client_poll_extended(&client,&out);
  if(bad){assert(rc==RISC_TEXT_ENTRY_RETAINED&&retained&&client.retained&&!closed&&!released);assert(portable_text_client_close(&client)==RISC_TEXT_ENTRY_RETAINED&&!closed);}
  else{assert(rc==0&&!strcmp(out.text,initial));assert(portable_text_client_close(&client)==0&&closed==1&&released==1&&resumed==1&&!suspended&&!retained);}
 }
 free(allocation);printf("extended text client %s PASS\n",mode);return 0;}
