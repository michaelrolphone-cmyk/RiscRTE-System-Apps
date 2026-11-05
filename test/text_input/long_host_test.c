/* Share the same production-host doubles and old-client regression helpers. */
#define main legacy_host_main
#include "host_test.c"
#undef main
static const risc_text_entry_api_v1_long *long_api;
static risc_text_entry_state_v1_long long_tick(void){
 risc_text_entry_state_v1_long s={.struct_size=sizeof(s)};
 assert(long_api->poll_long(NULL,session,&s)==0&&s.struct_size==sizeof(s));return s;
}
int main(int argc,char **argv){
 assert(argc==2);const char *mode=argv[1];driver=t5_driver_get(2);api=driver->capability;
 long_api=risc_text_entry_long(api);assert(long_api);
 risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,&scenes},{"usb.hid.keyboard",1,&keyboard}};
 assert(driver->start(deps,2));
 unsigned cap=!strcmp(mode,"maximum")?256:241;
 risc_text_entry_request_v1_long q={.api_version=1,.struct_size=sizeof(q),.capacity=cap,.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON,.label="Message"};
 if(!strcmp(mode,"invalid")){
  for(unsigned i=0;i<10;i++){
   risc_text_entry_request_v1_long bad=q;
   switch(i){case 0:bad.capacity=0;break;case 1:bad.capacity=257;break;
   case 2:bad.struct_size--;break;case 3:bad.api_version++;break;
   case 4:bad.reserved=RISC_TEXT_ENTRY_REQUEST_MASKED;break;
   case 5:memset(bad.text,'X',cap);break;case 6:bad.text[0]='\n';break;
   case 7:bad.text[0]=(char)0x80;break;case 8:memset(bad.label,'X',sizeof(bad.label));break;
   case 9:bad.label[0]=(char)0x80;break;}
   assert(long_api->open_long(NULL,&bad,&session)==RISC_TEXT_ENTRY_INVALID&&!session&&!calls);
  }
  assert(long_api->open_long(NULL,NULL,&session)==RISC_TEXT_ENTRY_INVALID);
  assert(long_api->open_long(NULL,&q,NULL)==RISC_TEXT_ENTRY_INVALID);driver->stop();
 }else{
  if(!strcmp(mode,"hardware"))attached=true;
  unsigned initial=!strcmp(mode,"initial")?cap-1:cap-10;
  memset(q.text,'A',initial);assert(long_api->open_long(NULL,&q,&session)==0&&session);
  memset(&q,0xa5,sizeof(q));pending=true;
  risc_text_entry_state_v1_long s=long_tick();assert(strlen(s.text)==initial&&s.flags&RISC_TEXT_ENTRY_PRESENTING);
  /* Old layouts must be rejected without overwriting or consuming an input. */
  struct {risc_text_entry_state_v1 state;unsigned char guard[32];} base;
  struct {risc_text_entry_state_v1_extended state;unsigned char guard[32];} extended;
  memset(&base,0xa5,sizeof(base));base.state.struct_size=sizeof(base.state);
  memset(&extended,0xa5,sizeof(extended));extended.state.struct_size=sizeof(extended.state);
  unsigned before=calls;assert(api->poll(NULL,session,&base.state)==RISC_TEXT_ENTRY_INVALID);
  assert(long_api->base.poll_extended(NULL,session,&extended.state)==RISC_TEXT_ENTRY_INVALID&&calls==before);
  for(unsigned i=0;i<32;i++)assert(base.guard[i]==0xa5&&extended.guard[i]==0xa5);
  risc_text_entry_state_v1_long wrong={.struct_size=sizeof(wrong)-1};assert(long_api->poll_long(NULL,session,&wrong)==RISC_TEXT_ENTRY_INVALID&&calls==before);
  for(unsigned i=0;i<12;i++){
   if(attached){key(4,0);s=long_tick();flush();}else{scene_key('a');s=long_tick();}
   assert(strlen(s.text)==(initial+i+1<cap?initial+i+1:cap-1));
   assert(s.state==RISC_TEXT_ENTRY_EDITING&&s.flags&RISC_TEXT_ENTRY_PRESENTING);
   assert(doc.nodes[0].target==(((uint32_t)strlen(s.text)<<16)|(cap-1)));
   assert(strlen(doc.nodes[0].text)==71&&!strcmp(doc.nodes[0].text,s.text+strlen(s.text)-71));
  }
  assert(strlen(s.text)==cap-1);
  /* Each same key is acknowledged at the bound; backspace can then insert. */
  if(attached){key(0x2a,0);long_tick();flush();key(5,0);s=long_tick();flush();}
  else{scene_key(RISC_SCENE_KEY_BACKSPACE);long_tick();scene_key('b');s=long_tick();}
  assert(strlen(s.text)==cap-1&&s.text[cap-2]=='b');
  bool home=!strcmp(mode,"home"),back=!strcmp(mode,"back");
  if(home||back)event=(risc_scene_event_v1){.struct_size=sizeof(event),.kind=home?RISC_SCENE_SUSPEND_EVENT:RISC_SCENE_ACTION_EVENT,.document_revision=doc.revision,.action=2};
  else scene_key(RISC_SCENE_KEY_DONE);
  s=long_tick();assert(s.state==(home||back?RISC_TEXT_ENTRY_CANCELLED:RISC_TEXT_ENTRY_ACCEPTED));
  assert(!!(s.flags&RISC_TEXT_ENTRY_HOME_CANCEL)==home);scene_key('z');risc_text_entry_state_v1_long frozen=long_tick();assert(!memcmp(&s,&frozen,sizeof(s)));
  assert(api->close(NULL,session)==RISC_TEXT_ENTRY_AGAIN);pending=false;finish();
  /* Reopen the old base and extended clients after every larger session. */
  attached=false;assert(driver->start(deps,2));open_text("old",72);finish();
  assert(driver->start(deps,2));risc_text_entry_request_v1_extended old={.api_version=1,.struct_size=sizeof(old),.capacity=128,.label="File"};
  memset(old.text,'F',127);old.text[3]=(char)0xc3;old.text[4]=(char)0xa9;
  assert(long_api->base.open_extended(NULL,&old,&session)==0);risc_text_entry_state_v1_extended out={.struct_size=sizeof(out)};
  assert(long_api->base.poll_extended(NULL,session,&out)==0&&out.struct_size==sizeof(out)&&!memcmp(out.text,old.text,128));finish();
 }
 printf("long text host %s PASS\n",mode);return 0;
}
