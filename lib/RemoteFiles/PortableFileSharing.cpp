#include "PortableFileSharing.h"
#include "WebDavSharing.h"
#include "RiscEntropySourceV1.h"
#include <cstring>
#include <new>
using namespace RiscWebDav;
namespace {
void wipe(void *value,size_t size){auto*p=static_cast<volatile unsigned char*>(value);while(size--)*p++=0;}
enum class Step { Off,Files,Tcp,Entropy,Random,EntropyRelease,Prepare,Run,Stop,Release,Error,Retained };
constexpr int32_t BadConfig=-100,MissingCapability=-101,InvalidContract=-102,ClockChanged=-103;
}
struct portable_file_sharing {
 risc_runtime_api_v1 runtime{};portable_file_sharing_hooks hooks{};
 portable_file_sharing_config config{};portable_file_sharing_status view{};
 risc_runtime_capability_v1 grants[3]{};
 const risc_app_data_export_v1 *files=nullptr;
 const risc_tcp_connection_v1 *tcp=nullptr;
 const risc_entropy_source_v1 *entropy=nullptr;
 DigestSession auth{};Sharing sharing{};Step step=Step::Off;
 unsigned char random[32]{};char password[17]{};
 uint64_t now=0;
 bool owned(){if(step==Step::Retained)return false;if(!hooks.owner(hooks.context)){retain(ClockChanged);return false;}return true;}
 void clearSecrets(){wipe(random,sizeof(random));wipe(password,sizeof(password));wipe(view.password,sizeof(view.password));auth.clear();}
 void retain(int32_t error){clearSecrets();view.error=error;step=Step::Retained;view.state=PORTABLE_FILE_SHARING_RETAINED;view.active_work=true;}
 void stop(int32_t error=0){clearSecrets();if(error)view.error=error;if(step!=Step::Stop&&step!=Step::Release)step=Step::Stop;view.state=PORTABLE_FILE_SHARING_STOPPING;}
 void finish(){clearSecrets();view.active_work=false;view.state=view.error?PORTABLE_FILE_SHARING_ERROR:PORTABLE_FILE_SHARING_OFF;step=view.error?Step::Error:Step::Off;files=nullptr;tcp=nullptr;entropy=nullptr;}
 bool release(unsigned index){if(!grants[index].api)return true;bool ok=runtime.release(&grants[index]);if(!owned())return false;if(!ok){retain(InvalidContract);return false;}grants[index]={};return true;}
};
extern "C" portable_file_sharing *portable_file_sharing_create(const risc_runtime_api_v1 *runtime,const portable_file_sharing_hooks *hooks){
 if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!hooks||!hooks->owner||!hooks->allocate||!hooks->deallocate||!hooks->owner(hooks->context))return nullptr;
 void *memory=hooks->allocate(sizeof(portable_file_sharing));if(!memory)return nullptr;
 auto *p=new(memory) portable_file_sharing;std::memcpy(&p->runtime,runtime,RISC_RUNTIME_CAPABILITIES_V1_SIZE);p->hooks=*hooks;
 (void)p->owned();return p;
}
extern "C" bool portable_file_sharing_start(portable_file_sharing*p,const portable_file_sharing_config*config,uint64_t now){
 if(!p||!p->owned()||(p->step!=Step::Off&&p->step!=Step::Error))return false;
 if(!config||!config->authenticated_http||!config->port||config->address[0]>=224||!config->lifetime_ms||config->lifetime_ms>3600000||now>=UINT64_MAX-config->lifetime_ms){p->view.error=BadConfig;p->finish();return false;}
 p->clearSecrets();p->config=*config;p->now=now;p->view={};p->view.state=PORTABLE_FILE_SHARING_PREPARING;p->view.active_work=true;p->view.expires_ms=now+config->lifetime_ms;p->view.port=config->port;std::memcpy(p->view.username,"files",6);p->step=Step::Files;return true;
}
extern "C" void portable_file_sharing_stop(portable_file_sharing*p){
 if(!p||p->step==Step::Off||p->step==Step::Error||!p->owned())return;
 p->stop();
}
extern "C" void portable_file_sharing_tick(portable_file_sharing*p,uint64_t now){
 if(!p||p->step==Step::Off||p->step==Step::Error||!p->owned())return;
 if(p->step!=Step::Stop&&p->step!=Step::Release){if(now<p->now)p->stop(ClockChanged);else if(now>=p->view.expires_ms)p->stop();}p->now=now;
 if(p->step==Step::Files||p->step==Step::Tcp||p->step==Step::Entropy){
  unsigned index=p->step==Step::Files?0:p->step==Step::Tcp?1:2;
  const char* names[]={RISC_APP_DATA_EXPORT_CAPABILITY,RISC_TCP_CONNECTION_CAPABILITY,RISC_ENTROPY_SOURCE_CAPABILITY};
  const uint64_t instances[]={p->config.files_instance,p->config.tcp_instance,p->config.entropy_instance};
  auto &grant=p->grants[index];grant={};grant.struct_size=sizeof(grant);
  bool ok=p->runtime.acquire(names[index],1,instances[index],&grant);if(!p->owned())return;
  if(!ok){if(grant.api)p->retain(InvalidContract);else p->stop(MissingCapability);return;}
  if(!grant.api){p->retain(InvalidContract);return;}
  if(!index){p->files=risc_app_data_export_catalog(static_cast<const risc_storage_volume_api_v1*>(grant.api));if(!p->files){p->stop(InvalidContract);return;}p->step=Step::Tcp;}
  else if(index==1){p->tcp=static_cast<const risc_tcp_connection_v1*>(grant.api);auto*t=p->tcp;if(t->api_version!=1||t->struct_size<sizeof(*t)||!t->context||!t->listen||!t->accept||!t->read||!t->write||!t->close){p->stop(InvalidContract);return;}p->step=Step::Entropy;}
  else {p->entropy=static_cast<const risc_entropy_source_v1*>(grant.api);auto*e=p->entropy;if(e->api_version!=1||e->struct_size<sizeof(*e)||!e->context||!e->fill){p->stop(InvalidContract);return;}p->step=Step::Random;}
  return;
 }
 if(p->step==Step::Random){
  int32_t rc=p->entropy->fill(p->entropy->context,p->random,sizeof(p->random));if(!p->owned())return;
  if(rc==RISC_ENTROPY_SOURCE_BUSY||rc==RISC_ENTROPY_SOURCE_UNAVAILABLE){wipe(p->random,sizeof(p->random));return;}
  if(rc){if(rc==RISC_ENTROPY_SOURCE_INVALID)p->stop(rc);else p->retain(rc);return;}
  p->step=Step::EntropyRelease;return;
 }
 if(p->step==Step::EntropyRelease){if(p->release(2)){p->entropy=nullptr;p->step=Step::Prepare;}return;}
 if(p->step==Step::Prepare){
  static constexpr char alphabet[]="23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
  static_assert(sizeof(alphabet)==33,"32 password symbols");
  for(unsigned i=0;i<16;++i)p->password[i]=alphabet[p->random[i]&31];
  p->password[16]=0;
  char temporary[16];std::memcpy(temporary,p->password,sizeof(temporary));
  bool authenticated=p->auth.begin("files",temporary,sizeof(temporary),"RiscRTE files",p->random+16,16,now,p->view.expires_ms);wipe(p->random,sizeof(p->random));
  SharingHooks hooks{p,[](void*context){return static_cast<portable_file_sharing*>(context)->owned();},p->hooks.allocate,p->hooks.deallocate};
  risc_tcp_connection_listen_v1 bind{sizeof(bind),{},p->config.port,0};std::memcpy(bind.address,p->config.address,sizeof(bind.address));
  if(!authenticated||!p->sharing.begin(p->files,p->tcp,&p->auth,hooks,bind,now,p->view.expires_ms)){if(p->step!=Step::Retained)p->stop(InvalidContract);return;}
  p->step=Step::Run;return;
 }
 if(p->step==Step::Run){
  auto state=p->sharing.tick(now);if(!p->owned())return;auto status=p->sharing.status();p->view.last_http_status=status.lastHttpStatus;p->view.completed_requests=status.completedRequests;
  if(state==SharingState::Retained){p->retain(status.error);return;}
  if(state==SharingState::Off||state==SharingState::Error){p->stop(status.error);return;}
  if(state==SharingState::Stopping){p->stop(status.error);return;}
  p->view.state=state==SharingState::Serving?PORTABLE_FILE_SHARING_SERVING:state==SharingState::Listening?PORTABLE_FILE_SHARING_LISTENING:PORTABLE_FILE_SHARING_PREPARING;
  if(p->view.state==PORTABLE_FILE_SHARING_LISTENING||p->view.state==PORTABLE_FILE_SHARING_SERVING)std::memcpy(p->view.password,p->password,sizeof(p->password));
  return;
 }
 if(p->step==Step::Stop){
  p->sharing.requestStop();if(!p->owned())return;auto state=p->sharing.tick(now);if(!p->owned())return;
  if(state==SharingState::Retained){p->retain(p->sharing.status().error);return;}
  if(state==SharingState::Off||state==SharingState::Error)p->step=Step::Release;
  return;
 }
 if(p->step==Step::Release){for(unsigned i=3;i;--i)if(p->grants[i-1].api){p->release(i-1);return;}p->finish();}
}
extern "C" void portable_file_sharing_get_status(const portable_file_sharing*p,portable_file_sharing_status*out){if(out)*out=p?p->view:portable_file_sharing_status{};}
extern "C" bool portable_file_sharing_close(portable_file_sharing*p,uint64_t now){
 if(!p)return true;
 portable_file_sharing_stop(p);
 // At most client close, listener close, clear, three releases and final clear.
 for(unsigned i=0;i<8&&p->step!=Step::Off&&p->step!=Step::Error&&p->step!=Step::Retained;++i)portable_file_sharing_tick(p,now);
 return p->step==Step::Off||p->step==Step::Error;
}
extern "C" bool portable_file_sharing_destroy(portable_file_sharing**pointer){
 if(!pointer||!*pointer)return true;
 auto*p=*pointer;if((p->step!=Step::Off&&p->step!=Step::Error)||!p->owned())return false;
 auto release=p->hooks.deallocate;p->clearSecrets();p->~portable_file_sharing();wipe(p,sizeof(*p));*pointer=nullptr;release(p);return true;
}
