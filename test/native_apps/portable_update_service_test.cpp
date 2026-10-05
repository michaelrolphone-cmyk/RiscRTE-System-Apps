#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "../../Services/update/service.cpp"
namespace {
uint64_t now_ms=0;
unsigned opened_http,closed_http,began_app,began_firmware,written,activated,restarted,aborted;
bool fail_close,fail_abort,fail_write,fail_finish,fail_activate,fail_restart,wrong_status,truncated,oversize,unknown_app,changed_grants,current_release,usb_only,unknown_activation;
uint32_t bank_state=RISC_BANK_IDLE,firmware_size=4;
std::string response_body,provided_catalog;
size_t at;
std::string manifest_json(const char *v,bool changed=false) {
 return std::string("{\"type\":\"application\",\"id\":\"clock\",\"version\":\"")+v+"\",\"architecture\":\"xtensa-esp32s3\",\"file_name\":\"clock.elf\",\"entry\":\"app_main\",\"requires\":[{\"capability\":\""+(changed?"software.update.firmware":"display.output")+"\",\"api\":1}]}";
}
std::string index_json() {
 const char *v=current_release?"1.0.0":"1.1.0";std::string digest(64,'a');
 std::string apps="[{\"kind\":\"app\",\"id\":\"clock\",\"version\":\""+std::string(v)+"\",\"tag\":\"app-clock-v"+v+"\",\"asset\":\"clock.elf\",\"url\":\"https://github.com/"+WatchUpdate::Repository+"/releases/download/app-clock-v"+v+"/clock.elf\",\"size\":4,\"sha256\":\""+digest+"\",\"manifest\":"+manifest_json(v,changed_grants)+"}]";
 std::string fw="{\"kind\":\"firmware\",\"version\":\"1.0.0\",\"tag\":\"firmware-v1.0.0\",\"asset\":\"twatch-s3-launcher-1.0.0.bin\",\"url\":\"https://github.com/"+std::string(WatchUpdate::Repository)+"/releases/download/firmware-v1.0.0/twatch-s3-launcher-1.0.0.bin\",\"size\":8388608,\"sha256\":\""+digest+"\"";
 if(!usb_only)fw+=",\"ota\":{\"kind\":\"runtime-image\",\"runtime_version\":\""+std::string(v)+"\",\"layout\":\"riscrte-paired-16m-v1\",\"store_abi\":1,\"asset\":\"riscrte-runtime-"+v+".bin\",\"url\":\"https://github.com/"+WatchUpdate::Repository+"/releases/download/firmware-v1.0.0/riscrte-runtime-"+v+".bin\",\"size\":"+std::to_string(firmware_size)+",\"sha256\":\""+digest+"\"}";
 return "{\"schema\":1,\"firmware\":"+fw+"},\"apps\":"+apps+",\"drivers\":[]}";
}
int32_t h_open(void*,const risc_http_request_v1 *r,uint64_t *h){assert(!http_handle);assert(r->utc_seconds==1800000000ULL);++opened_http;*h=opened_http;at=0;response_body=std::string(r->url)==WatchUpdate::CatalogUrl?provided_catalog:(truncated?"ELF":"ELF!");return 0;}
int32_t h_read(void*,uint64_t,void *buf,uint32_t cap,uint32_t *n){assert(cap<=512);if(oversize){*n=cap+1;return 0;}*n=0;if(at==response_body.size())return RISC_HTTP_EOF;size_t amount=response_body.size()-at;if(amount>17)amount=17;memcpy(buf,response_body.data()+at,amount);at+=amount;*n=(uint32_t)amount;return 0;}
int32_t h_info(void*,uint64_t,risc_http_response_v1 *o){o->status_code=wrong_status?500:200;return 0;}
int32_t h_close(void*,uint64_t){if(fail_close)return RISC_HTTP_RETAINED;++closed_http;return 0;}
bool b_status(void*,risc_bank_status_v1 *o){o->state=bank_state;o->firmware_capacity=3u*1024u*1024u;o->store_abi=1;o->app_count=unknown_app?0:1;strcpy(o->runtime_version,"1.0.0");strcpy(o->layout,"riscrte-paired-16m-v1");memset(o->active_store_sha256,42,32);return true;}
int32_t b_firmware(void*,const risc_bank_image_v1 *im,uint64_t *h){assert(UPDATE_FIRMWARE);assert(im->size==4&&im->active_store_sha256[0]==42);++began_firmware;*h=1;bank_state=RISC_BANK_COPY_STORE;return 0;}
int32_t b_app(void*,const char *id,const void*,uint32_t n,const risc_bank_image_v1 *im,uint64_t *h){assert(!UPDATE_FIRMWARE);assert(!strcmp(id,"clock")&&n&&im->size==4&&im->active_store_sha256[0]==42);++began_app;*h=1;bank_state=RISC_BANK_COPY_STORE;return 0;}
int32_t b_step(void*,uint64_t,risc_bank_status_v1 *o){bank_state=bank_state==RISC_BANK_COPY_STORE?RISC_BANK_RECEIVING:bank_state==RISC_BANK_VERIFY_STORE?RISC_BANK_READY:bank_state;o->state=bank_state;return 0;}
int32_t b_write(void*,uint64_t,const void*,uint32_t n){if(fail_write)return RISC_BANK_IO;written+=n;return 0;}
int32_t b_finish(void*,uint64_t){if(fail_finish)return RISC_BANK_INTEGRITY;assert(written==4);bank_state=RISC_BANK_VERIFY_STORE;return 0;}
int32_t b_activate(void*,uint64_t){assert(!http_handle&&bank_state==RISC_BANK_READY);if(fail_activate)return RISC_BANK_IO;if(unknown_activation){bank_state=RISC_BANK_ACTIVATION_UNKNOWN;return RISC_BANK_RETAINED;}++activated;bank_state=RISC_BANK_ACTIVATED;return 0;}
int32_t b_abort(void*,uint64_t){if(fail_abort)return RISC_BANK_RETAINED;++aborted;bank_state=RISC_BANK_IDLE;return 0;}
bool b_restart(void*,uint64_t){assert(activated||bank_state==RISC_BANK_ACTIVATION_UNKNOWN);if(fail_restart)return false;++restarted;return true;}
int32_t b_get(void*,uint32_t i,void *out,uint32_t cap,uint32_t *n){assert(!i);std::string s=manifest_json("1.0.0");assert(cap>=s.size());memcpy(out,s.data(),s.size());*n=(uint32_t)s.size();return 0;}
uint64_t time_ms(void*){return now_ms;}
void sleep_ms(void*,uint32_t n){now_ms+=n;}
const risc_http_client_v1 http_api={1,sizeof(http_api),nullptr,h_open,h_read,h_info,h_close};
const risc_bank_store_v1 bank_api={1,sizeof(bank_api),nullptr,b_status,b_firmware,b_app,b_step,b_write,b_finish,b_activate,b_abort,b_restart,b_get};
const risc_platform_clock_api_v1 time_api={1,sizeof(time_api),nullptr,time_ms,sleep_ms};
void run_until(uint32_t target){for(unsigned i=0;i<10000&&view.state!=target&&view.state!=SOFTWARE_UPDATE_ERROR&&view.state!=SOFTWARE_UPDATE_RETAINED;++i)step(nullptr);}
}
int main(int argc,char**argv){assert(argc==2);int scenario=atoi(argv[1]);
 provided_catalog=index_json();
 if(scenario==1)truncated=true;if(scenario==2)fail_write=true;if(scenario==3)fail_finish=true;if(scenario==4)fail_close=true;if(scenario==5)wrong_status=true;
 if(scenario==6){current_release=true;provided_catalog=index_json();}if(scenario==7){changed_grants=true;provided_catalog=index_json();}
 if(scenario==8){unknown_app=true;}if(scenario==9){usb_only=true;provided_catalog=index_json();}
 if(scenario==10)provided_catalog="{\"schema\":1,\"schema\":1,\"apps\":[]}";
 if(scenario==11){auto at=provided_catalog.find("github.com");provided_catalog.replace(at,10,"evil.test/");}
 if(scenario==12)oversize=true;
 if(scenario==18||scenario==19){firmware_size=scenario==18?2621440u:3145729u;provided_catalog=index_json();}
 risc_provider_dependency_v1 deps[]={{RISC_HTTP_CLIENT_CAPABILITY,1,&http_api},{RISC_BANK_STORE_CAPABILITY,1,&bank_api},{"platform.clock",1,&time_api}};
 const risc_driver_v2 *d=t5_driver_get(2);assert(d&&d->start(deps,3));assert(!strcmp(d->capability_id,UPDATE_FIRMWARE?SOFTWARE_UPDATE_FIRMWARE_CAPABILITY:SOFTWARE_UPDATE_APPS_CAPABILITY));
 assert(!refresh(nullptr,0));assert(refresh(nullptr,1800000000ULL));assert(!begin(nullptr,0,1800000000ULL));run_until(SOFTWARE_UPDATE_LIST);
 bool catalog_failure=scenario==4||scenario==5||scenario==10||scenario==12||(scenario==11&&UPDATE_FIRMWARE);
 if(catalog_failure){assert(view.state==SOFTWARE_UPDATE_ERROR||view.state==SOFTWARE_UPDATE_RETAINED);assert(!activated&&!began_app&&!began_firmware);}
 else {assert(view.state==SOFTWARE_UPDATE_LIST&&catalog->count==1);bool unavailable=scenario==6||(!UPDATE_FIRMWARE&&(scenario==7||scenario==8))||(UPDATE_FIRMWARE&&(scenario==9||scenario==19));
  if(unavailable){assert(catalog->rows[0].view.availability!=SOFTWARE_UPDATE_AVAILABLE);assert(!begin(nullptr,0,1800000000ULL));}
  else if(UPDATE_FIRMWARE&&scenario==18){assert(catalog->rows[0].view.size==2621440u&&catalog->rows[0].view.availability==SOFTWARE_UPDATE_AVAILABLE);}
  else {assert(begin(nullptr,0,1800000000ULL));assert(!activate(nullptr));if(scenario==13){assert(cancel(nullptr));assert(!activated);}
   else {if(scenario==14)now_ms=deadline;run_until(SOFTWARE_UPDATE_READY);
    if(scenario==1||scenario==2||scenario==3||scenario==14){assert(view.state==SOFTWARE_UPDATE_ERROR);assert(aborted&&!activated);}
    else {assert(view.state==SOFTWARE_UPDATE_READY&&written==4&&!activated);if(scenario==15){fail_abort=true;assert(!cancel(nullptr));assert(view.state==SOFTWARE_UPDATE_RETAINED);}
     else {if(scenario==16)fail_activate=true;if(scenario==17)unknown_activation=true;bool ok=activate(nullptr);assert(ok==(scenario!=16&&scenario!=17));
      if(scenario==17){assert(view.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN&&!activated&&!aborted);assert(!cancel(nullptr)&&!d->quiesce());assert(restart(nullptr)&&restarted==1);view.state=SOFTWARE_UPDATE_READY;}if(ok){assert(activated==1&&view.state==SOFTWARE_UPDATE_ACTIVATED);assert(!cancel(nullptr));fail_restart=true;assert(!restart(nullptr));fail_restart=false;assert(restart(nullptr)&&restarted==1);assert(!d->quiesce());/* model device reboot for fixture teardown */view.state=SOFTWARE_UPDATE_READY;}}
   }}
  }
 }
 fail_abort=fail_close=false;assert(d->quiesce());assert(!http_handle&&!bank_handle&&!catalog&&!json);printf("update service kind %d scenario %d passed\n",UPDATE_FIRMWARE,scenario);
}
