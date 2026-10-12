#include "WebDavRemoteVolume.h"
#include <RiscProtectedSettingsPath.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
namespace RiscWebDav {
bool RemoteVolume::available()const{return state_==State::Idle||state_==State::Done||state_==State::Failed;
}
bool RemoteVolume::storageSafe(){if(!local_)return true;
if(atomic_){if(!hooks_.owner(hooks_.context)){state_=State::Retained;return false;}
const auto*ext=risc_storage_volume_extension(local_);
int32_t rc=mode_==Mode::Download?atomic_->write_status(local_->context,file_):int32_t(ext->handle_error(local_->context,file_,false));
if(!hooks_.owner(hooks_.context)||rc==RISC_APP_DATA_RETAINED||rc==RISC_APP_DATA_CONTEXT){state_=State::Retained;return false;}
if(rc!=(mode_==Mode::Download?RISC_APP_DATA_EXPORT_WRITE_PENDING:0))return false;
risc_app_data_export_entry_v1 entry{};rc=atomic_->entry(local_->context,atomicIndex_-1,&entry);
if(!hooks_.owner(hooks_.context)||rc==RISC_APP_DATA_RETAINED||rc==RISC_APP_DATA_CONTEXT){state_=State::Retained;return false;}
return rc==0;}
const auto*s=risc_storage_volume_state(local_);
if(!s||s->observe(local_->context)==RISC_STORAGE_STATE_RETAINED){state_=State::Retained;
return false;
}return s->observe(local_->context)==RISC_STORAGE_STATE_READY;
}
void RemoteVolume::fail(const char*s){if(state_==State::Retained)return;
snprintf(error_,sizeof(error_),"%s",s);
success_=false;
state_=file_?State::Close:State::Failed;
}
bool RemoteVolume::reject(const char*s){if(available()){published_=false;xmlSize_=0;fail(s);}return false;}
bool RemoteVolume::start(Mode m){if(!available()||file_||!client_.ready()||!hooks_.owner||!hooks_.owner(hooks_.context))return false;
mode_=m;
cancelled_=success_=published_=temporary_=uncertain_=false;
local_=nullptr;atomic_=nullptr;atomicIndex_=0;
error_[0]=0;
xmlSize_=0;
state_=State::Network;
return true;
}
Client::Io RemoteVolume::io(){return {this,[](void*c){auto&p=*static_cast<RemoteVolume*>(c);
return p.state_!=State::Retained&&p.hooks_.owner(p.hooks_.context);
},
 [](void*c,void*out,size_t cap,size_t*n){auto&p=*static_cast<RemoteVolume*>(c);
*n=0;
if(!p.storageSafe())return false;
*n=p.local_->file_read(p.local_->context,p.file_,out,cap);
return p.storageSafe();
},
 [](void*c,const void*in,size_t n){auto&p=*static_cast<RemoteVolume*>(c);
if(p.mode_==Mode::List||p.mode_==Mode::Preview){
if(p.mode_==Mode::Preview&&p.previewSkipped_<p.previewOffset_){size_t skip=size_t(std::min<uint64_t>(n,p.previewOffset_-p.previewSkipped_));p.previewSkipped_+=skip;in=static_cast<const unsigned char*>(in)+skip;n-=skip;}
if(n>=sizeof(p.xml_)-p.xmlSize_)return false;
memcpy(p.xml_+p.xmlSize_,in,n);
p.xmlSize_+=n;
p.xml_[p.xmlSize_]=0;
return true;
}if(p.mode_==Mode::Download){if(p.atomic_&&p.client_.total()>RISC_APP_DATA_FILE_MAX)return false;
if(!p.storageSafe())return false;
size_t wrote=p.local_->file_write(p.local_->context,p.file_,in,n);
return p.storageSafe()&&wrote==n;
}return true;
},atomic_?512u:0u};
}
bool RemoteVolume::list(const char*p,uint64_t now){char encoded[1537];
if(risc_protected_settings_path(p)||!Client::encodePath(p,encoded,sizeof(encoded))||!start(Mode::List))return reject("Invalid remote folder request");
strcpy(path_,p);
count_=0;
if(!client_.request("PROPFIND",p,nullptr,0,io(),now)){fail("Could not request remote folder");
return false;
}return true;
}
bool RemoteVolume::preview(const char*p,size_t limit,uint64_t now,uint64_t offset){char encoded[1537];
if(offset>64u*1024u*1024u||!limit||limit>PreviewMax||risc_protected_settings_path(p)||!Client::encodePath(p,encoded,sizeof(encoded))||!start(Mode::Preview))return reject("Invalid remote preview request");
previewOffset_=offset;previewSkipped_=0;
if(!client_.request("GET",p,nullptr,0,io(),now,nullptr,size_t(offset)+limit)){fail("Could not request remote preview");return false;}
return true;
}
bool RemoteVolume::previewGet(void*out,size_t capacity,size_t*size)const{
if(size)*size=0;
if(!size||!out||mode_!=Mode::Preview||state_!=State::Done||!success_||cancelled_||capacity<xmlSize_)return false;
memcpy(out,xml_,xmlSize_);*size=xmlSize_;return true;
}
bool RemoteVolume::download(const char*remote,const risc_storage_volume_api_v1*local,const char*destination,uint64_t now){char encoded[1537];
const auto*atomic=local?risc_app_data_export_catalog(local):nullptr;
if(risc_protected_settings_path(remote)||risc_protected_settings_path(destination)||!local||(!atomic&&(!risc_storage_volume_extension(local)||!risc_storage_volume_state(local)))||(atomic&&!atomic->write_status)||!local->file_open_write||!local->file_write||!local->file_close||!Client::encodePath(remote,encoded,sizeof(encoded))||!Client::encodePath(destination,encoded,sizeof(encoded))||!strcmp(destination,"/")||!start(Mode::Download))return reject("Invalid local download destination");
 local_=local;atomic_=atomic;strcpy(destination_,destination);
if(atomic_){strcpy(uploadRemote_,remote);state_=State::Prepare;return true;}
if(!storageSafe()){fail("Local storage unavailable or exported");
return false;
}strcpy(destination_,destination);
static uint32_t serial=0;
if(serial==UINT32_MAX){fail("Transfer generation exhausted");
return false;
}const char*slash=strrchr(destination,'/');
int n=snprintf(stage_,sizeof(stage_),"%.*s/.peer-%08x.tmp",int(slash-destination),destination,++serial);
if(n<0||size_t(n)>=sizeof(stage_)){fail("Destination path too long");
return false;
}
 file_=local_->file_open_write(local_->context,stage_);
if(!storageSafe()){fail("Local storage changed while opening");
return false;
}if(!file_){fail("Cannot create local temporary file");
return false;
}temporary_=true;
 if(!client_.request("GET",remote,nullptr,0,io(),now)){fail("Could not request remote file");
return false;
}return true;
}
bool RemoteVolume::upload(const risc_storage_volume_api_v1*local,const char*source,const char*remote,uint64_t now,bool replace){char encoded[1537];
const auto*atomic=local?risc_app_data_export_catalog(local):nullptr;const auto*ext=local?risc_storage_volume_extension(local):nullptr;
if(risc_protected_settings_path(source)||risc_protected_settings_path(remote)||!local||!local->file_open_read||!local->file_read||!local->file_close||(!atomic&&!risc_storage_volume_state(local))||(atomic&&(!ext||!ext->handle_error))||!Client::encodePath(source,encoded,sizeof(encoded))||!Client::encodePath(remote,encoded,sizeof(encoded))||!start(Mode::Upload))return reject("Invalid local upload source");
local_=local;atomic_=atomic;
if(atomic_){strcpy(stage_,source);strcpy(uploadRemote_,remote);if(replace)mode_=Mode::ReplaceHead;state_=State::Prepare;return true;}
if(!storageSafe()){fail("Local storage unavailable or exported");
return false;
}uint64_t size=0;
file_=local_->file_open_read(local_->context,source,&size);
if(!storageSafe()){fail("Local storage changed while opening");
return false;
}if(!file_){fail("Cannot read local file");
return false;
}uploadSize_=size;
strcpy(uploadRemote_,remote);
if(replace)mode_=Mode::ReplaceHead;
if(!client_.request(replace?"HEAD":"PUT",remote,nullptr,replace?0:size,io(),now)){fail("Could not start remote upload");
return false;
}return true;
}
bool RemoteVolume::mutate(const char*m,const char*p,const char*d,uint64_t now){if(risc_protected_settings_path(p)||risc_protected_settings_path(d)||!m||(strcmp(m,"MKCOL")&&strcmp(m,"DELETE")&&strcmp(m,"MOVE")&&strcmp(m,"COPY"))||!start(Mode::Mutation))return reject("Invalid remote operation");
if(!client_.request(m,p,d,0,io(),now)){fail("Could not start remote operation");
return false;
}return true;
}
bool RemoteVolume::decode(const char*p,size_t n,char*out,size_t cap){size_t used=0;
auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';
if(c>='A'&&c<='F')return c-'A'+10;
if(c>='a'&&c<='f')return c-'a'+10;
return -1;
};
for(size_t i=0;i<n;i++){unsigned char c=p[i];
if(c=='%'){if(n-i<3)return false;
int a=digit(p[++i]),b=digit(p[++i]);
if(a<0||b<0)return false;
c=uint8_t(a*16+b);
}if(c<32||c=='\\'||used+1>=cap)return false;
out[used++]=char(c);
}out[used]=0;
if(used>1&&out[used-1]=='/')out[--used]=0;
char encoded[1537];
return Client::encodePath(out,encoded,sizeof(encoded));
}
bool RemoteVolume::parseListing(){
 if(!strstr(xml_,"<D:multistatus xmlns:D=\"DAV:\">")||!strstr(xml_,"</D:multistatus>")||strstr(xml_,"<!"))return false;
 const char*p=xml_;
unsigned seen=0;
while((p=strstr(p,"<D:response>"))){const char*end=strstr(p,"</D:response>");
if(!end)return false;
const char*a=strstr(p,"<D:href>"),*b=a?strstr(a,"</D:href>"):nullptr;
if(!a||!b||b>end||++seen>Rows+1)return false;
a+=8;
char decoded[513];
if(!decode(a,size_t(b-a),decoded,sizeof(decoded)))return false;
const char*status=strstr(b,"<D:status>HTTP/1.1 200 OK</D:status>");
if(!status||status>end)return false;
 if(risc_protected_settings_path(decoded)){p=end+13;continue;}
 if(strcmp(decoded,path_)){size_t prefix=strlen(path_);
if(strncmp(decoded,path_,prefix))return false;
const char*name=decoded+prefix;
if(prefix>1){if(*name++!='/')return false;
}if(!*name||strchr(name,'/')||strlen(name)>=128||count_==Rows)return false;
for(unsigned i=0;i<count_;i++)if(!strcmp(rows_[i].path,decoded))return false;
auto&r=rows_[count_++];
r={};
strcpy(r.name,name);
strcpy(r.path,decoded);
const char*collection=strstr(b,"<D:collection/>");
r.directory=collection&&collection<end;
const char*length=strstr(b,"<D:getcontentlength>");
if(length&&length<end){length+=20;
const char*stop=strstr(length,"</D:getcontentlength>");
if(!stop||stop>end||length==stop)return false;
for(;length<stop;length++){if(*length<'0'||*length>'9'||r.size>(UINT64_MAX-9)/10)return false;
r.size=r.size*10+unsigned(*length-'0');
}}else if(!r.directory)return false;
}
 p=end+13;
}
 return seen>0;
}
RemoteVolume::State RemoteVolume::tick(uint64_t now){
 if(available()||state_==State::Retained)return state_;
if(!hooks_.owner(hooks_.context)){state_=State::Retained;
client_.cancel();
return state_;
}
 if(state_==State::Prepare){
 // Catalog admission is copied policy metadata; open_write itself exclusively
 // checks absence. No unconfigured path or staging alias is ever submitted.
 risc_app_data_export_entry_v1 entry{};int32_t rc=atomic_->entry(local_->context,atomicIndex_,&entry);
 if(!hooks_.owner(hooks_.context)||rc==RISC_APP_DATA_RETAINED||rc==RISC_APP_DATA_CONTEXT){state_=State::Retained;return state_;}
 if(rc){fail(rc==RISC_APP_DATA_NOT_FOUND?"Destination is not a configured saved file":"Saved files unavailable");return state_;}
 if(atomicIndex_++>=16||!memchr(entry.path,0,sizeof(entry.path))||entry.writable>1){state_=State::Retained;return state_;}
 const bool downloading=mode_==Mode::Download;const char*path=downloading?destination_:stage_;
 if(strcmp(entry.path,path))return state_;
 if(downloading&&!entry.writable){fail("Destination is read-only");return state_;}
 file_=downloading?local_->file_open_write(local_->context,path):local_->file_open_read(local_->context,path,&uploadSize_);
 if(!hooks_.owner(hooks_.context)){state_=State::Retained;return state_;}
 if(!file_){rc=atomic_->entry(local_->context,atomicIndex_-1,&entry);if(!hooks_.owner(hooks_.context)||rc==RISC_APP_DATA_RETAINED||rc==RISC_APP_DATA_CONTEXT){state_=State::Retained;return state_;}fail(downloading?"Cannot create saved file; it may already exist":"Cannot read saved file");return state_;}
 if(!storageSafe()){fail("Saved file unavailable");return state_;}
 if(!client_.request(downloading?"GET":mode_==Mode::ReplaceHead?"HEAD":"PUT",uploadRemote_,nullptr,downloading||mode_==Mode::ReplaceHead?0:uploadSize_,io(),now)){fail("Could not start remote transfer");return state_;}
 state_=State::Network;return state_;
 }
 if(state_==State::Network){auto s=client_.tick(now);
if(s==Client::State::Retained){state_=State::Retained;
return state_;
}if(s!=Client::State::Ready&&s!=Client::State::Error&&s!=Client::State::Off)return state_;
success_=!cancelled_&&s==Client::State::Ready&&client_.status()>=200&&client_.status()<300;
 if(success_&&mode_==Mode::ReplaceHead){char match[96];
snprintf(match,sizeof(match),"%s",client_.etag());
mode_=Mode::Upload;
 if(!match[0]||!client_.request("PUT",uploadRemote_,nullptr,uploadSize_,io(),now,match))fail("Remote file has no safe replacement revision");
return state_;
}
 if(success_&&(mode_==Mode::Mutation||mode_==Mode::Upload))count_=0;
 if(!success_){const char*reason=nullptr;
if(cancelled_)reason="Transfer cancelled";
else switch(client_.error()){case Client::Expired:reason="Peer session or request expired";break;case Client::Authentication:reason="Peer authentication failed";break;case Client::Protocol:reason="Invalid peer response";break;case Client::Transport:reason="Peer connection failed";break;case Client::Storage:reason="Transfer storage operation failed";break;case Client::Uncertain:reason="Operation outcome unknown; reload";break;default:break;}
if(reason)snprintf(error_,sizeof(error_),"%s",reason);else snprintf(error_,sizeof(error_),"Remote operation failed (%u)",client_.status());
}if(mode_==Mode::List){if(success_&&!parseListing()){success_=false;
count_=0;
snprintf(error_,sizeof(error_),"Invalid or oversized remote folder");
}state_=success_?State::Done:State::Failed;
return state_;
}state_=file_?(success_&&mode_==Mode::Download&&!atomic_?State::Sync:State::Close):(success_?State::Done:State::Failed);
return state_;
}
 const auto*ext=local_?risc_storage_volume_extension(local_):nullptr;
 if(state_==State::Sync){if(!storageSafe()){fail("Local storage unavailable while syncing");
return state_;
}if(!ext||!ext->file_sync||!ext->file_sync(local_->context,file_)){success_=false;
snprintf(error_,sizeof(error_),"Local sync failed");
}if(state_!=State::Retained)state_=State::Close;
return state_;
}
 if(state_==State::Close){if(!local_)return state_;
if(atomic_){
 const bool downloading=mode_==Mode::Download;
 int32_t status=downloading?atomic_->write_status(local_->context,file_):int32_t(ext->handle_error(local_->context,file_,false));
 if(!hooks_.owner(hooks_.context)||status==RISC_APP_DATA_RETAINED||status==RISC_APP_DATA_CONTEXT){state_=State::Retained;return state_;}
 // The exact export's catalog reports admission without filesystem access.
 // A temporarily unavailable gate holds custody until a later settled tick.
 risc_app_data_export_entry_v1 entry{};status=atomic_->entry(local_->context,atomicIndex_-1,&entry);
 if(!hooks_.owner(hooks_.context)||status==RISC_APP_DATA_RETAINED||status==RISC_APP_DATA_CONTEXT){state_=State::Retained;return state_;}
 if(status==RISC_APP_DATA_UNAVAILABLE||status==RISC_APP_DATA_BUSY)return state_;
 if(status){state_=State::Retained;return state_;}
 bool commit=downloading?success_:true;bool closed=local_->file_close(local_->context,file_,commit);
 if(!hooks_.owner(hooks_.context)){state_=State::Retained;return state_;}
 if(closed){file_=0;published_=downloading&&commit;state_=success_?State::Done:State::Failed;return state_;}
 if(!downloading||!commit){status=atomic_->entry(local_->context,atomicIndex_-1,&entry);if(hooks_.owner(hooks_.context)&&(status==RISC_APP_DATA_UNAVAILABLE||status==RISC_APP_DATA_BUSY))return state_;state_=State::Retained;return state_;}
 // A copied status query never repeats publication. Release only the in-memory
 // writer on a later tick; COMMIT_UNKNOWN must not delete a possible new file.
 status=atomic_->write_status(local_->context,file_);
 if(!hooks_.owner(hooks_.context)||status==RISC_APP_DATA_RETAINED||status==RISC_APP_DATA_CONTEXT){state_=State::Retained;return state_;}
 uncertain_=status==RISC_APP_DATA_COMMIT_UNKNOWN;success_=false;
 snprintf(error_,sizeof(error_),"%s",uncertain_?"Publication unknown; reload destination":"Saved file commit failed; no retry performed");return state_;
}
const auto*obs=risc_storage_volume_state(local_);
if(!obs||obs->observe(local_->context)==RISC_STORAGE_STATE_RETAINED){state_=State::Retained;
return state_;
}if(!local_->file_close(local_->context,file_,mode_==Mode::Download?success_:true)){state_=State::Retained;
return state_;
}file_=0;
if(mode_==Mode::Download&&success_)state_=State::Publish;
else {temporary_=false;
state_=success_?State::Done:State::Failed;
}return state_;
}
 if(state_==State::Publish){if(!storageSafe()){fail("Local storage unavailable at publication");
return state_;
}if(!ext||!ext->rename||!ext->rename(local_->context,stage_,destination_)){uncertain_=true;
fail("Publication not confirmed; reload destination");
return state_;
}temporary_=false;
published_=true;
state_=State::Done;
return state_;
}return state_;
}
void RemoteVolume::cancel(){if(state_==State::Retained)return;
cancelled_=true;
success_=false;
if(state_==State::Network)client_.cancelRequest();
else if(file_)state_=State::Close;
else if(!available()){state_=State::Failed;snprintf(error_,sizeof(error_),"Transfer cancelled");}
}
bool RemoteVolume::close(){cancel();
if(file_||state_==State::Network||state_==State::Retained)return false;
return client_.end();
}
}
