#include "WebDavVolume.h"
#include <RiscProtectedSettingsPath.h>
#include <RiscStorageVolumeFsV1.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cinttypes>
namespace RiscWebDav {
namespace {const char* reason(unsigned n){switch(n){case 200:return "OK";case 201:return "Created";case 204:return "No Content";case 207:return "Multi-Status";case 304:return "Not Modified";case 400:return "Bad Request";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 409:return "Conflict";case 412:return "Precondition Failed";case 413:return "Content Too Large";case 415:return "Unsupported Media Type";case 501:return "Not Implemented";case 503:return "Service Unavailable";case 507:return "Insufficient Storage";default:return "Internal Server Error";}}
bool eq(const char*a,const char*b){return !std::strcmp(a,b);}
bool reserved(const char*p){if(risc_protected_settings_path(p))return true;for(;*p;++p){for(const char*needle:{".risc-replace",".risc-webdav-"}){size_t n=0;while(needle[n]&&p[n]){char c=p[n];if(c>='A'&&c<='Z')c+=32;if(c!=needle[n])break;++n;}if(!needle[n])return true;}}return false;}
}
bool VolumeTransaction::owner(){if(state_==State::Retained)return false;if(!hooks_.owner(hooks_.context)){state_=State::Retained;return false;}return true;}
int VolumeTransaction::observed(){if(!owner())return -2;const int n=observation_->observe(api_->context);if(!owner()||n==RISC_STORAGE_STATE_RETAINED||(n!=0&&n!=1)){state_=State::Retained;return -2;}return n;}
bool VolumeTransaction::safe(){const int n=observed();if(n==1){streaming_=false;fail(503);}return n==0;}
bool VolumeTransaction::after(){return safe();}
bool VolumeTransaction::reserve(size_t n){if(n<=outputCapacity_)return true;if(n>262144||!owner())return false;auto*p=static_cast<unsigned char*>(hooks_.allocate(n));if(!owner())return false;if(!p)return false;if(outputSize_)std::memcpy(p,output_,outputSize_);if(output_)hooks_.deallocate(output_);output_=p;outputCapacity_=n;return owner();}
bool VolumeTransaction::append(const char*s){size_t n=std::strlen(s);if(outputSize_+n>outputCapacity_&&!reserve(std::max(outputSize_+n,outputCapacity_?2*outputCapacity_:size_t(4096))))return false;std::memcpy(output_+outputSize_,s,n);outputSize_+=n;return true;}
bool VolumeTransaction::escaped(const char*s,bool uri){const char*digits="0123456789ABCDEF";for(;*s;++s){unsigned char c=*s;char text[4]={char(c),0,0,0};const char*p=text;if(uri&&!(c>='a'&&c<='z')&&!(c>='A'&&c<='Z')&&!(c>='0'&&c<='9')&&!std::strchr("/-._~",c)){text[0]='%';text[1]=digits[c>>4];text[2]=digits[c&15];}else if(c=='&')p="&amp;";else if(c=='<')p="&lt;";else if(c=='>')p="&gt;";else if(c=='\"')p="&quot;";if(!append(p))return false;}return true;}
bool VolumeTransaction::row(const char*path,bool directory,uint64_t size){
 if(!append("<D:response><D:href>")||!escaped(path,true)||(directory&&std::strcmp(path,"/")&&!append("/"))||!append("</D:href>"))return false;
 const char* names[]={"resourcetype","getcontentlength","getcontenttype","getetag","supportedlock","displayname"};
 const uint32_t available=directory?49u:55u;
 uint32_t wanted=properties_.mode==PropertyMode::Named?0u:available;
 for(unsigned n=0;n<properties_.count;++n)if(!std::strcmp(properties_.namespaceUri(n),"DAV:"))
  for(unsigned i=0;i<6;++i)if(!std::strcmp(properties_.localName(n),names[i]))wanted|=1u<<i;
 if(wanted&available){
  if(!append("<D:propstat><D:prop>"))return false;
  for(unsigned i=0;i<6;++i){
   if(!(wanted&available&(1u<<i)))continue;
   if(!append("<D:")||!append(names[i])||!append(">"))return false;
   if(properties_.mode!=PropertyMode::Names){char value[48]{};
    if(i==0&&directory&&!append("<D:collection/>"))return false;
    if(i==1){std::snprintf(value,sizeof(value),"%" PRIu64,size);if(!append(value))return false;}
    if(i==2&&!append("application/octet-stream"))return false;
    
    if(i==5){const char* base=std::strrchr(path,'/');if(!escaped(base&&base[1]?base+1:path))return false;}
   }
   if(!append("</D:")||!append(names[i])||!append(">"))return false;
  }
  if(!append("</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>"))return false;
 }
 bool unknown=false;
 for(unsigned n=0;n<properties_.count;++n){
  bool found=false;
  if(!std::strcmp(properties_.namespaceUri(n),"DAV:"))for(unsigned i=0;i<6;++i)
   if((available&(1u<<i))&&!std::strcmp(properties_.localName(n),names[i]))found=true;
  if(found)continue;
  if(!unknown){if(!append("<D:propstat><D:prop>"))return false;unknown=true;}
  const char* uri=properties_.namespaceUri(n);
  if(!std::strcmp(uri,"http://www.w3.org/XML/1998/namespace")){if(!append("<xml:")||!append(properties_.localName(n))||!append("/>"))return false;}
  else if(*uri){if(!append("<U:")||!append(properties_.localName(n))||!append(" xmlns:U=\"")||!escaped(uri)||!append("\"/>"))return false;}
  else if(!append("<")||!append(properties_.localName(n))||!append(" xmlns=\"\"/>"))return false;
 }
 if(unknown&&!append("</D:prop><D:status>HTTP/1.1 404 Not Found</D:status></D:propstat>"))return false;
 // An empty prop selection still returns a successful empty propstat.
 if(!unknown&&!(wanted&available)&&!append("<D:propstat><D:prop/><D:status>HTTP/1.1 200 OK</D:status></D:propstat>"))return false;
 return append("</D:response>");
}
void VolumeTransaction::response(unsigned n,size_t length,const char*type){if(!owner())return;status_=n;char lengthHeader[64]{};if(n!=204&&n!=304)std::snprintf(lengthHeader,sizeof(lengthHeader),"Content-Length: %zu\r\n",length);int count=std::snprintf(header_,sizeof(header_),"HTTP/1.1 %u %s\r\n%sContent-Type: %s\r\nConnection: close\r\nCache-Control: no-store\r\nAllow: OPTIONS, GET, HEAD, PROPFIND, PUT, DELETE, MKCOL, MOVE, COPY\r\n%s%s%s\r\n",n,reason(n),lengthHeader,type,n==401?"WWW-Authenticate: ":"",n==401?hooks_.authenticationChallenge:"",n==401?"\r\n":"");if(count<0||size_t(count)>=sizeof(header_)){state_=State::Retained;return;}headerSize_=count;state_=State::Ready;}
void VolumeTransaction::fail(unsigned n){if(!owner())return;outputSize_=0;streaming_=false;response(n);}
bool VolumeTransaction::writablePath(const char*p)const{return !eq(p,"/")&&!reserved(p);}
bool VolumeTransaction::begin(const risc_storage_volume_api_v1* api,const Hooks&h,const Request&q,const void*body,size_t n,bool upload){
 if(state_!=State::Idle&&state_!=State::Done)return false;
 if(!api||!(ext_=risc_storage_volume_extension(api))||!(observation_=risc_storage_volume_state(api))||!api->stat||!api->file_open_read||!api->file_read||!api->file_close||!api->dir_open||!api->dir_next||!ext_->dir_close_checked||!ext_->handle_error||!h.owner||!h.authorize||!h.allocate||!h.deallocate||(!upload&&n!=q.contentLength)||(!body&&n))return false;
 upload_=upload;if(upload_)dtls_sha256_init(&entity_);
 api_=api;hooks_=h;request_=q;input_=body;inputSize_=n;head_=eq(q.method,"HEAD");state_=State::Prepare;phase_=Phase::Stat;
 if(!owner())return false;
 if(!upload_&&!h.authorize(h.context,q,AuthPhase::Complete,body,n)){if(owner())fail(401);return true;}
 if(!safe())return true;
 if(reserved(q.path)){fail(403);return true;}
 if(q.ifMatch[0]&&!eq(q.ifMatch,"*")){fail(412);return true;} // No strong revision primitive on generic SD.
 if(q.ifNoneMatch[0]&&!eq(q.ifNoneMatch,"*")){fail(412);return true;}
 return true;
}
bool VolumeTransaction::closeFile(risc_storage_file_t&f,bool commit){if(!f)return true;const int n=observed();if(n<0)return false;bool ok=api_->file_close(api_->context,f,commit);const int afterState=observed();if(afterState<0)return false;if(!ok){state_=State::Retained;return false;}f=0;return true;}
bool VolumeTransaction::closeDirectory(){if(!directory_)return true;int n=observed();if(n<0)return false;bool ok=ext_->dir_close_checked(api_->context,directory_);n=observed();if(n<0)return false;if(!ok){state_=State::Retained;return false;}directory_=0;return true;}
State VolumeTransaction::step(){
 if(state_!=State::Prepare||!safe())return state_;
 void*c=api_->context;const auto&m=request_.method;
 if(phase_==Phase::Stat){
  const auto*fs=risc_storage_volume_fs(api_);
  if(fs){risc_storage_metadata_v1 metadata{sizeof(metadata),0,0,0};int rc=fs->metadata(c,request_.path,&metadata);if(!after())return state_;if(rc!=RISC_STORAGE_FS_OK&&rc!=RISC_STORAGE_FS_NOT_FOUND){fail(503);return state_;}exists_=rc==RISC_STORAGE_FS_OK;size_=metadata.size;isDirectory_=(metadata.flags&RISC_STORAGE_DIRECTORY)!=0;}
  else {exists_=api_->stat(c,request_.path,&size_,&isDirectory_);if(!after())return state_;}
  if(request_.ifMatch[0]&&!exists_){fail(412);return state_;}if(request_.ifNoneMatch[0]&&exists_){fail(eq(m,"GET")||head_?304:412);return state_;}
  if(eq(m,"OPTIONS")){response(200);return state_;}
  if(eq(m,"PROPFIND")){
   if(!exists_){fail(404);return state_;}if(request_.depth==2&&isDirectory_){fail(403);return state_;}
   if(request_.contentType[0]){char media[sizeof(request_.contentType)];std::strcpy(media,request_.contentType);if(char*semi=std::strchr(media,';'))*semi=0;size_t n=std::strlen(media);while(n&&(media[n-1]==' '||media[n-1]=='\t'))media[--n]=0;for(char*p=media;*p;++p)if(*p>='A'&&*p<='Z')*p+=32;if(!eq(media,"text/xml")&&!eq(media,"application/xml")){fail(415);return state_;}}
   unsigned result=parseProperties(static_cast<const char*>(input_),inputSize_,properties_);if(result){fail(result);return state_;}
   if(!append("<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">")||!row(request_.path,isDirectory_,size_)){fail(507);return state_;}
   phase_=isDirectory_&&request_.depth==1?Phase::ListOpen:Phase::ListClose;return state_;
  }
  if(eq(m,"GET")||head_){if(!exists_){fail(404);return state_;}if(isDirectory_){fail(405);return state_;}if(head_){response(200,size_);return state_;}phase_=Phase::ReadOpen;return state_;}
  if(eq(m,"PUT")){if(!writablePath(request_.path)){fail(403);return state_;}if(isDirectory_&&exists_){fail(405);return state_;}if(exists_&&!risc_storage_volume_fs(api_)){fail(501);return state_;}phase_=Phase::WriteOpen;return state_;}
  if(eq(m,"DELETE")){if(!exists_){fail(404);return state_;}if(isDirectory_){startWalk(true);return state_;}phase_=Phase::Remove;return state_;}
  if(eq(m,"MKCOL")){if(exists_){fail(405);return state_;}phase_=Phase::Mkdir;return state_;}
  if(eq(m,"MOVE")||eq(m,"COPY")){if(!exists_){fail(404);return state_;}if(!request_.destination[0]||!writablePath(request_.destination)||eq(request_.path,request_.destination)){fail(403);return state_;}copy_=eq(m,"COPY");phase_=Phase::Destination;return state_;}
  fail(405);return state_;
 }
 if(phase_==Phase::Destination){uint64_t bytes=0;bool directory=false;destinationExists_=api_->stat(c,request_.destination,&bytes,&directory);if(!after())return state_;if(destinationExists_&&(!request_.overwrite||directory||isDirectory_)){fail(request_.overwrite?409:412);return state_;}if(destinationExists_&&!risc_storage_volume_fs(api_)){fail(501);return state_;}if(isDirectory_){walkInspect_=!copy_;startWalk(false);return state_;}phase_=copy_?Phase::CopyOpen:Phase::Move;return state_;}
 if(phase_==Phase::RemoveOriginal){bool ok=api_->remove&&api_->remove(c,request_.path);if(after()){if(ok)response(destinationExists_?204:201);else fail(500);}return state_;}
 if(phase_==Phase::ReadOpen){uint64_t n=0;file_=api_->file_open_read(c,request_.path,&n);if(!after())return state_;if(!file_){fail(503);return state_;}size_=remaining_=n;streaming_=true;outputSize_=0;response(200,n);return state_;}
 if(phase_==Phase::ListOpen){directory_=api_->dir_open(c,request_.path);if(!after())return state_;if(!directory_){fail(503);return state_;}phase_=Phase::ListNext;return state_;}
 if(phase_==Phase::ListNext){risc_storage_dirent_v1 e{};bool got=api_->dir_next(c,directory_,&e);if(!after())return state_;if(!got){uint32_t error=ext_->handle_error(c,directory_,true);if(!after())return state_;if(error){fail(503);return state_;}phase_=Phase::ListClose;return state_;}if(!std::memchr(e.name,0,sizeof(e.name))||std::strchr(e.name,'/')||std::strchr(e.name,'\\')){state_=State::Retained;return state_;}if(eq(e.name,".")||eq(e.name,"..")||!e.name[0]||reserved(e.name))return state_;char p[RISC_STORAGE_VOLUME_PATH_MAX+1];int n=std::snprintf(p,sizeof(p),"%s%s%s",request_.path,eq(request_.path,"/")?"":"/",e.name);if(n<0||size_t(n)>=sizeof(p)||!row(p,e.is_directory,e.size)){fail(507);return state_;}return state_;}
 if(phase_==Phase::ListClose){if(!closeDirectory())return state_;if(!append("</D:multistatus>")){fail(507);return state_;}response(207,outputSize_,"application/xml; charset=utf-8");return state_;}
 if(phase_==Phase::WriteOpen){
  if(!api_->file_open_write||!api_->file_write||!ext_->file_sync){fail(405);return state_;}
  const char*dst=copy_?destinationPath():request_.path;const char*slash=std::strrchr(dst,'/');static uint32_t sequence=0;
  int n=std::snprintf(temporary_,sizeof(temporary_),"%.*s/.risc-webdav-%08x.tmp",int(slash-dst),dst,++sequence);
  if(n<0||size_t(n)>=sizeof(temporary_)){fail(400);return state_;}writer_=api_->file_open_write(c,temporary_);if(!after())return state_;if(!writer_){fail(507);return state_;}temporaryOwned_=true;offset_=0;phase_=copy_?Phase::CopyRead:Phase::WriteData;return state_;
 }
 if(phase_==Phase::WriteData){if(upload_)return state_;if(offset_==inputSize_){phase_=Phase::WriteSync;return state_;}size_t n=std::min<size_t>(RISC_STORAGE_VOLUME_IO_MAX,inputSize_-offset_);size_t wrote=api_->file_write(c,writer_,static_cast<const unsigned char*>(input_)+offset_,n);if(!after())return state_;if(wrote!=n){fail(507);return state_;}offset_+=n;return state_;}
 if(phase_==Phase::WriteSync){if(file_){closeFile(file_,true);return state_;}bool ok=ext_->file_sync(c,writer_);if(!after())return state_;if(!ok){fail(507);return state_;}phase_=Phase::WriteClose;return state_;}
 if(phase_==Phase::WriteClose){if(!closeFile(writer_,true))return state_;phase_=Phase::Commit;return state_;}
 if(phase_==Phase::Commit){const char*dst=copy_?destinationPath():request_.path;bool ok=false;const auto*fs=risc_storage_volume_fs(api_);bool replacing=(copy_?(!walk_&&destinationExists_):exists_)&&fs;if(replacing)ok=fs->replace_file(c,temporary_,dst);else if(ext_->rename)ok=ext_->rename(c,temporary_,dst);if(!after())return state_;if(!ok){if(replacing)temporaryOwned_=false;fail(replacing?500:409);return state_;}temporaryOwned_=false;if(walk_)phase_=Phase::WalkOpen;else if(moveCopy_)phase_=Phase::RemoveOriginal;else response((copy_?destinationExists_:exists_)?204:201);return state_;}
 if(walk_){
  auto&frame=walk_[depth_];const auto*fs=risc_storage_volume_fs(api_);
  if(phase_==Phase::WalkMkdir){bool ok=ext_->mkdir&&ext_->mkdir(c,frame.destination);if(!after())return state_;if(!ok){fail(409);return state_;}phase_=Phase::WalkOpen;return state_;}
  if(phase_==Phase::WalkOpen){directory_=api_->dir_open(c,frame.source);if(!after())return state_;if(!directory_){fail(503);return state_;}phase_=!walkDelete_&&frame.position?Phase::WalkSeek:Phase::WalkNext;return state_;}
  if(phase_==Phase::WalkSeek){bool ok=fs&&fs->dir_seek(c,directory_,frame.position);if(!after())return state_;if(!ok){fail(409);return state_;}phase_=Phase::WalkNext;return state_;}
  if(phase_==Phase::WalkNext){risc_storage_dirent_v1 e{};bool got=api_->dir_next(c,directory_,&e);if(!after())return state_;walkEof_=!got;if(!got){uint32_t error=ext_->handle_error(c,directory_,true);if(!after())return state_;if(error){fail(503);return state_;}phase_=Phase::WalkClose;return state_;}
   if(!std::memchr(e.name,0,sizeof(e.name))||std::strchr(e.name,'/')||std::strchr(e.name,'\\')){state_=State::Retained;return state_;}if(eq(e.name,".")||eq(e.name,".."))return state_;
   if(!e.name[0]||reserved(e.name)){fail(409);return state_;}
   int n=std::snprintf(workSource_,sizeof(workSource_),"%s/%s",frame.source,e.name),m=std::snprintf(workDestination_,sizeof(workDestination_),"%s/%s",frame.destination,e.name);
   if(n<0||m<0||size_t(n)>=sizeof(workSource_)||size_t(m)>=sizeof(workDestination_)){fail(409);return state_;}walkDirectory_=e.is_directory;phase_=walkDelete_?Phase::WalkClose:Phase::WalkTell;return state_;
  }
  if(phase_==Phase::WalkTell){bool ok=fs&&fs->dir_tell(c,directory_,&frame.position);if(!after())return state_;if(!ok){fail(503);return state_;}phase_=Phase::WalkClose;return state_;}
  if(phase_==Phase::WalkClose){if(!closeDirectory())return state_;if(walkEof_){if(walkDelete_)phase_=Phase::WalkRemove;else popWalk();}else phase_=Phase::WalkEntry;return state_;}
  if(phase_==Phase::WalkEntry){if(walkDirectory_){if(depth_==31){fail(507);return state_;}auto&next=walk_[++depth_];std::strcpy(next.source,workSource_);std::strcpy(next.destination,workDestination_);next.position=0;phase_=walkDelete_||walkInspect_?Phase::WalkOpen:Phase::WalkMkdir;return state_;}
   if(walkDelete_){bool ok=api_->remove&&api_->remove(c,workSource_);if(!after())return state_;if(!ok){fail(409);return state_;}phase_=Phase::WalkOpen;}else phase_=walkInspect_?Phase::WalkOpen:Phase::CopyOpen;return state_;
  }
  if(phase_==Phase::WalkRemove){bool ok=api_->remove&&api_->remove(c,frame.source);if(!after())return state_;if(!ok){fail(409);return state_;}popWalk();return state_;}
 }
 if(!writablePath(request_.path)){fail(403);return state_;}
 if(phase_==Phase::Remove){bool ok=api_->remove&&api_->remove(c,request_.path);if(after()){if(ok)response(204);else fail(409);}return state_;}
 if(phase_==Phase::Mkdir){bool ok=ext_->mkdir&&ext_->mkdir(c,request_.path);if(after()){if(ok)response(201);else fail(409);}return state_;}
 if(phase_==Phase::Move){const char*a=std::strrchr(request_.path,'/'),*b=std::strrchr(request_.destination,'/');if(destinationExists_&&(a-request_.path!=b-request_.destination||std::memcmp(request_.path,request_.destination,a-request_.path))){copy_=moveCopy_=true;phase_=Phase::CopyOpen;return state_;}const auto*fs=risc_storage_volume_fs(api_);bool ok=destinationExists_&&fs?fs->replace_file(c,request_.path,request_.destination):ext_->rename&&ext_->rename(c,request_.path,request_.destination);if(after()){if(ok)response(destinationExists_?204:201);else fail(412);}return state_;}
 if(phase_==Phase::CopyOpen){file_=api_->file_open_read(c,sourcePath(),&remaining_);if(!after())return state_;if(!file_){fail(503);return state_;}phase_=Phase::WriteOpen;return state_;}
 if(phase_==Phase::CopyRead){if(!remaining_){phase_=Phase::WriteSync;return state_;}size_t n=std::min<uint64_t>(remaining_,sizeof(chunk_));chunkSize_=api_->file_read(c,file_,chunk_,n);if(!after())return state_;if(chunkSize_!=n){fail(503);return state_;}remaining_-=n;phase_=Phase::CopyWrite;return state_;}
 if(phase_==Phase::CopyWrite){size_t n=api_->file_write(c,writer_,chunk_,chunkSize_);if(!after())return state_;if(n!=chunkSize_){fail(507);return state_;}phase_=Phase::CopyRead;return state_;}
 return state_;
}
bool VolumeTransaction::nextBody(){if(!streaming_)return false;if(!safe())return false;outputSize_=0;if(!remaining_){if(!closeFile(file_,true))return false;streaming_=false;return true;}size_t n=std::min<uint64_t>(remaining_,RISC_STORAGE_VOLUME_IO_MAX);if(!reserve(n)){fail(503);return false;}size_t got=api_->file_read(api_->context,file_,output_,n);if(!after())return false;if(got!=n){fail(503);return false;}remaining_-=n;outputSize_=n;return true;}
bool VolumeTransaction::finish(){if(state_==State::Done||state_==State::Idle)return true;if(!owner()||!closeDirectory()||!closeFile(file_,true))return false;bool aborted=writer_!=0;if(!closeFile(writer_,false))return false;if(aborted)temporaryOwned_=false;if(temporaryOwned_){int n=observed();if(n<0)return false;if(n==0){bool removed=api_->remove&&api_->remove(api_->context,temporary_);if(observed()<0)return false;if(!removed){state_=State::Retained;return false;}}temporaryOwned_=false;}if(walk_){hooks_.deallocate(walk_);walk_=nullptr;}if(output_){hooks_.deallocate(output_);output_=nullptr;}streaming_=false;state_=State::Done;return owner();}
bool VolumeTransaction::feedUpload(const void*data,size_t n){
 if(!uploadReady()||!data||!n||n>RISC_STORAGE_VOLUME_IO_MAX||offset_>request_.contentLength||n>request_.contentLength-offset_||!safe())return false;
 size_t wrote=api_->file_write(api_->context,writer_,data,n);if(!after())return false;if(wrote!=n){fail(507);return false;}
 dtls_sha256_update(&entity_,static_cast<const uint8_t*>(data),n);offset_+=n;return true;
}
bool VolumeTransaction::endUpload(){
 if(!uploadReady()||offset_!=request_.contentLength||!safe())return false;
 unsigned char hash[32];dtls_sha256_final(hash,&entity_);
 bool authorized=hooks_.authorize(hooks_.context,request_,AuthPhase::BodyHash,hash,sizeof(hash));
 std::memset(hash,0,sizeof(hash));if(!owner())return false;
 if(!authorized){fail(401);return false;}upload_=false;phase_=Phase::WriteSync;return true;
}

bool VolumeTransaction::startWalk(bool removing){
 if(!writablePath(request_.path)||(!removing&&!risc_storage_volume_fs(api_))){fail(405);return false;}
 size_t sourceLength=std::strlen(request_.path);
 if(!removing&&!std::strncmp(request_.path,request_.destination,sourceLength)&&request_.destination[sourceLength]=='/'){fail(403);return false;}
 walk_=static_cast<WalkFrame*>(hooks_.allocate(32*sizeof(WalkFrame)));if(!owner())return false;if(!walk_){fail(503);return false;}
 std::memset(walk_,0,32*sizeof(WalkFrame));std::strcpy(walk_[0].source,request_.path);std::strcpy(walk_[0].destination,request_.destination);depth_=0;walkDelete_=removing;phase_=removing||walkInspect_?Phase::WalkOpen:Phase::WalkMkdir;return true;
}
void VolumeTransaction::popWalk(){if(depth_){--depth_;phase_=Phase::WalkOpen;}else if(walkInspect_){hooks_.deallocate(walk_);walk_=nullptr;walkInspect_=false;phase_=Phase::Move;}else response(walkDelete_?204:201);}

}
