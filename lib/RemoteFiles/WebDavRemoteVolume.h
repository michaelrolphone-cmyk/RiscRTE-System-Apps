#pragma once
#include "WebDavClient.h"
#include <RiscStorageVolumeStateV1.h>
namespace RiscWebDav {
/* Asynchronous remote volume operations. Local storage keeps its existing ABI;
 * no network wait is hidden inside a synchronous storage callback. */
class RemoteVolume final {
public:
 static constexpr unsigned Rows=64,PreviewMax=2048;
 struct Entry {char name[128]{};char path[513]{};uint64_t size=0;bool directory=false;};
 enum class State { Idle, Network, Sync, Close, Publish, Done, Failed, Retained };
 struct Hooks {void*context;bool(*owner)(void*);};
 explicit RemoteVolume(Client&client,const Hooks&h):client_(client),hooks_(h){}
 bool list(const char*,uint64_t);
 bool preview(const char*,size_t limit,uint64_t now,uint64_t offset=0);
 bool previewGet(void*,size_t capacity,size_t*size)const;
 size_t previewSize()const{return mode_==Mode::Preview&&state_==State::Done&&success_&&!cancelled_?xmlSize_:0;}
 uint64_t previewTotal()const{return mode_==Mode::Preview&&state_==State::Done&&success_&&!cancelled_?client_.total():0;}
 bool previewTruncated()const{return previewTotal()>previewOffset_+previewSize();}
 bool download(const char*remote,const risc_storage_volume_api_v1*local,const char*destination,uint64_t);
 bool upload(const risc_storage_volume_api_v1*local,const char*source,const char*remote,uint64_t,bool replace=false);
 bool mutate(const char*method,const char*path,const char*destination,uint64_t);
 State tick(uint64_t);void cancel();bool close();
 State state()const{return state_;}unsigned count()const{return count_;}
 const Entry* entry(unsigned i)const{return i<count_?&rows_[i]:nullptr;}
 const char* error()const{return error_;}const char* directory()const{return path_;}
 const char* publishedPath()const{return published_?destination_:nullptr;}
 uint64_t progress()const{return mode_==Mode::Upload?client_.sent():client_.transferred();}
 bool uncertain()const{return uncertain_||client_.uncertain();}
private:
 enum class Mode { List,Preview,Download,Upload,ReplaceHead,Mutation }mode_=Mode::List;State state_=State::Idle;
 Client&client_;Hooks hooks_;const risc_storage_volume_api_v1*local_=nullptr;
 risc_storage_file_t file_=0;bool cancelled_=false,success_=false,published_=false,temporary_=false,uncertain_=false;
 uint64_t uploadSize_=0,previewOffset_=0,previewSkipped_=0;char uploadRemote_[513]{};
 char path_[513]{},destination_[513]{},stage_[513]{},error_[96]{};
 char xml_[32768]{};size_t xmlSize_=0;Entry rows_[Rows]{};unsigned count_=0;
 bool available()const;bool storageSafe();bool start(Mode);bool parseListing();void fail(const char*);bool reject(const char*);
 Client::Io io();static bool decode(const char*,size_t,char*,size_t);
};
}
