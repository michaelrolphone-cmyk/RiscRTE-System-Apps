#include "WebDavCore.h"
#include <cstring>
#include "WebDavBounds.h"
#include <cstdio>
#include <cinttypes>
#include <algorithm>
namespace RiscWebDav {
namespace {
bool same(const char*a,const char*b){while(*a&&*b){if(detail::asciiLower((unsigned char)*a++)!=detail::asciiLower((unsigned char)*b++))return false;}return !*a&&!*b;}
bool copy(char*out,size_t cap,const char*first,size_t size){if(size>=cap)return false;std::memcpy(out,first,size);out[size]=0;return true;}
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
bool path(const char*first,size_t size,char*out){
 if(!size||*first!='/')return false;
 size_t n=0;
 for(size_t i=0;i<size;++i){unsigned char c=first[i];if(c=='%'){if(i+2>=size||hex(first[i+1])<0||hex(first[i+2])<0)return false;c=(hex(first[i+1])<<4)|hex(first[i+2]);i+=2;}
  if(c<32||c>=127||c=='\\'||c=='?'||c=='#'||n>=RISC_APP_DATA_EXPORT_PATH_MAX)return false;
  out[n++]=c;}
 out[n]=0;if(n>1&&out[n-1]=='/'){if(out[n-2]=='/')return false;out[--n]=0;}
 for(const char*p=out+1;*p;){const char*end=std::strchr(p,'/');size_t k=end?size_t(end-p):std::strlen(p);
  if(!k||(k==1&&p[0]=='.')||(k==2&&p[0]=='.'&&p[1]=='.'))return false;
  if(!end)break;
  p=end+1;}
 return true;
}
bool token(const char*s){if(!*s)return false;for(;*s;++s)if(!detail::asciiAlphaNumeric((unsigned char)*s)&&!std::strchr("!#$%&'*+-.^_`|~",*s))return false;return true;}
bool decimal(const char*s,uint32_t&value){if(!*s)return false;value=0;for(;*s;++s){if(*s<'0'||*s>'9'||value>(UINT32_MAX-uint32_t(*s-'0'))/10)return false;value=value*10+(*s-'0');}return true;}
const char*reason(unsigned code){switch(code){case 200:return "OK";case 201:return "Created";case 204:return "No Content";case 207:return "Multi-Status";case 304:return "Not Modified";case 400:return "Bad Request";case 401:return "Unauthorized";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 409:return "Conflict";case 412:return "Precondition Failed";case 413:return "Content Too Large";case 414:return "URI Too Long";case 415:return "Unsupported Media Type";case 417:return "Expectation Failed";case 428:return "Precondition Required";case 431:return "Request Header Fields Too Large";case 503:return "Service Unavailable";case 507:return "Insufficient Storage";default:return "Internal Server Error";}}
void tag(char*out,size_t cap,uint64_t value){std::snprintf(out,cap,"\"risc-%016" PRIx64 "\"",value);}
bool tagList(const char* list,const char* etag=nullptr,bool present=false,bool weak=false,bool* matched=nullptr){
 if(matched)*matched=false;
 if(!std::strcmp(list,"*")){if(matched)*matched=present;return true;}
 const char* p=list;
 if(!*p)return false;
 for(;;){
  while(*p==' '||*p=='\t')++p;
  bool isWeak=false;
  if(p[0]=='W'&&p[1]=='/'){isWeak=true;p+=2;}
  const char* first=p;
  if(*p++!='"')return false;
  while(*p&&*p!='"'){unsigned char c=*p++;if(c<0x21||c==0x7f)return false;}
  if(*p++!='"')return false;
  if(present&&etag&&(!isWeak||weak)&&size_t(p-first)==std::strlen(etag)&&!std::memcmp(first,etag,p-first)&&matched)*matched=true;
  while(*p==' '||*p=='\t')++p;
  if(!*p)return true;
  if(*p++!=','||!*p)return false;
 }
}
bool match(const char* list,const char* etag,bool present,bool weak=false){bool yes=false;return tagList(list,etag,present,weak,&yes)&&yes;}

}
unsigned parseRequest(const char*data,size_t size,Request&out){
 out={};if(!data||size<4||size>HeaderMax)return size>HeaderMax?431:400;
 if(std::memcmp(data+size-4,"\r\n\r\n",4)||std::memchr(data,0,size))return 400;
 char work[HeaderMax+1];std::memcpy(work,data,size);work[size]=0;
 char*line=std::strstr(work,"\r\n");if(!line)return 400;*line=0;
 char*a=std::strchr(work,' ');if(!a)return 400;*a++=0;char*b=std::strchr(a,' ');if(!b)return 400;*b++=0;
 if(!token(work)||!copy(out.method,sizeof(out.method),work,std::strlen(work))||std::strcmp(b,"HTTP/1.1"))return 400;
 if(!copy(out.requestTarget,sizeof(out.requestTarget),a,std::strlen(a)))return 414;
 if(!std::strcmp(a,"*")&&!std::strcmp(work,"OPTIONS"))std::strcpy(out.path,"/");
 else if(!path(a,std::strlen(a),out.path))return std::strlen(a)>3*RISC_APP_DATA_EXPORT_PATH_MAX?414:400;
 unsigned seen=0;char*p=line+2;
 while(*p){char*end=std::strstr(p,"\r\n");if(!end)return 400;*end=0;if(!*p)break;if(*p==' '||*p=='\t')return 400;
  char*colon=std::strchr(p,':');if(!colon)return 400;*colon++=0;if(!token(p))return 400;while(*colon==' '||*colon=='\t')++colon;
  char*tail=colon+std::strlen(colon);while(tail>colon&&(tail[-1]==' '||tail[-1]=='\t'))*--tail=0;
  for(const char*q=colon;*q;++q)if(((unsigned char)*q<32&&*q!='\t')||(unsigned char)*q==127)return 400;
  unsigned bit=0;bool ok=true;
  if(same(p,"Host")){bit=1;ok=*colon&&copy(out.host,sizeof(out.host),colon,std::strlen(colon));for(const char*q=colon;*q;++q)if(*q==' '||*q=='\t'||(unsigned char)*q>=127)ok=false;}
  else if(same(p,"Content-Length")){bit=2;ok=decimal(colon,out.contentLength);}
  else if(same(p,"Authorization")){bit=4;ok=copy(out.authorization,sizeof(out.authorization),colon,std::strlen(colon));}
  else if(same(p,"If-Match")){bit=8;ok=copy(out.ifMatch,sizeof(out.ifMatch),colon,std::strlen(colon));}
  else if(same(p,"If-None-Match")){bit=16;ok=copy(out.ifNoneMatch,sizeof(out.ifNoneMatch),colon,std::strlen(colon));}
  else if(same(p,"Content-Type")){bit=128;ok=copy(out.contentType,sizeof(out.contentType),colon,std::strlen(colon));}
  else if(same(p,"Depth")){bit=32;if(!std::strcmp(colon,"0"))out.depth=0;else if(!std::strcmp(colon,"1"))out.depth=1;else if(std::strcmp(colon,"infinity"))ok=false;}
  else if(same(p,"Transfer-Encoding"))return 415;
  else if(same(p,"Expect")){bit=64;if(!same(colon,"100-continue"))return 417;out.expectContinue=true;}
  if(!ok||(bit&&(seen&bit)))return 400;
  seen|=bit;p=end+2;
 }
 if(!(seen&1))return 400;
 if((out.ifMatch[0]&&!tagList(out.ifMatch))||(out.ifNoneMatch[0]&&!tagList(out.ifNoneMatch)))return 400;
 if(out.contentLength>RISC_APP_DATA_FILE_MAX)return 413;
 if(!std::strcmp(out.method,"PUT")&&!(seen&2))return 400;
 if(!std::strcmp(out.method,"PROPFIND")&&out.contentLength>PropertyBodyMax)return 413;
 if(std::strcmp(out.method,"PUT")&&std::strcmp(out.method,"PROPFIND")&&out.contentLength)return 400;
 return 0;
}
bool Transaction::valid(){if(state_==State::Retained)return false;if(!hooks_.owner||!hooks_.owner(hooks_.context)){state_=State::Retained;return false;}return true;}
bool Transaction::reserve(size_t capacity){
 if(capacity<=outputCapacity_)return true;
 if(!valid())return false;
 auto* next=static_cast<unsigned char*>(hooks_.allocate(capacity));
 if(!valid())return false;
 if(!next)return false;
 if(outputSize_)std::memcpy(next,output_,outputSize_);
 if(output_)hooks_.deallocate(output_);
 if(!valid())return false;
 output_=next;outputCapacity_=capacity;return true;
}
bool Transaction::append(const char*s){size_t n=std::strlen(s);if(n>outputCapacity_-outputSize_)return false;std::memcpy(output_+outputSize_,s,n);outputSize_+=n;return true;}
bool Transaction::escaped(const char*s,bool uri){
 static const char digits[]="0123456789ABCDEF";
 for(;*s;++s){unsigned char c=*s;const char*escape=c=='&'?"&amp;":c=='<'?"&lt;":c=='>'?"&gt;":c=='\"'?"&quot;":c=='\''?"&apos;":nullptr;
  if(uri&&!detail::asciiAlphaNumeric(c)&&!std::strchr("/-._~",c)){char v[4]={'%',digits[c>>4],digits[c&15],0};if(!append(v))return false;}
  else if(escape){if(!append(escape))return false;}else {char v[2]={char(c),0};if(!append(v))return false;}}
 return true;
}
void Transaction::response(unsigned code,const char*type,size_t representationSize){
 if(!valid())return;
 status_=code;const size_t size=representationSize==SIZE_MAX?outputSize_:representationSize;
 char length[64]{};
 if(code!=204&&code!=304)std::snprintf(length,sizeof(length),"Content-Length: %zu\r\n",size);
 const int n=std::snprintf(header_,sizeof(header_),
  "HTTP/1.1 %u %s\r\nContent-Type: %s\r\n%sConnection: close\r\nCache-Control: no-store\r\n%s%s%s%s%s%s%s\r\n",
  code,reason(code),type,length,etag_[0]?"ETag: ":"",etag_,etag_[0]?"\r\n":"",
  (code==405||!std::strcmp(request_.method,"OPTIONS"))?"Allow: OPTIONS, GET, HEAD, PROPFIND, PUT\r\n":"",
  code==401?"WWW-Authenticate: ":"",code==401?challenge_:"",code==401?"\r\n":"");
 if(n<0||size_t(n)>=sizeof(header_)){state_=State::Retained;return;}headerSize_=n;state_=State::Ready;
}
void Transaction::error(unsigned code,const char*message){outputSize_=0;etag_[0]=0;if(!reserve(std::strlen(message)+1)){response(503);return;}append(message);response(code);}
bool Transaction::backend(int32_t rc){
 if(!valid())return false;
 if(rc==RISC_APP_DATA_RETAINED||rc==RISC_APP_DATA_CONTEXT){state_=State::Retained;return false;}
 if(!rc)return true;
 if(rc==RISC_APP_DATA_NOT_FOUND)error(404,"Not found\n");else if(rc==RISC_APP_DATA_STALE)error(412,"File changed; reload before writing\n");
 else if(rc==RISC_APP_DATA_NO_SPACE)error(507,"Insufficient storage\n");else if(rc==RISC_APP_DATA_UNAVAILABLE)error(503,"Storage unavailable\n");
 else if(rc==RISC_APP_DATA_COMMIT_UNKNOWN)error(500,"Commit outcome unknown; reload before retrying\n");else error(500,"Storage operation failed\n");
 return false;
}
bool Transaction::selected(const char*p,bool*writable)const{for(unsigned i=0;i<count_;++i)if(!std::strcmp(p,catalog_[i].path)){if(writable)*writable=catalog_[i].writable!=0;return true;}return false;}
bool Transaction::directory(const char*p)const{if(!std::strcmp(p,"/"))return true;size_t n=std::strlen(p);for(unsigned i=0;i<count_;++i)if(!std::strncmp(p,catalog_[i].path,n)&&catalog_[i].path[n]=='/')return true;return false;}
bool Transaction::begin(const risc_app_data_export_v1*files,const Hooks&hooks,const Request&request,const void*body,size_t size){
 if(state_!=State::Idle&&state_!=State::Done)return false;
 if(!files||!risc_app_data_export_catalog(&files->volume.terminal.power.volume.base)||!files->stat_revision||!files->read_revision||!files->replace_revision||!hooks.owner||!hooks.authorize||!hooks.authenticationChallenge||!hooks.allocate||!hooks.deallocate||(!body&&size)||size!=request.contentLength||size>RISC_APP_DATA_FILE_MAX)return false;
 const char* fields[]={request.method,request.path,request.host,request.authorization,request.ifMatch,request.ifNoneMatch,request.contentType,request.requestTarget};
 const size_t capacities[]={sizeof(request.method),sizeof(request.path),sizeof(request.host),sizeof(request.authorization),sizeof(request.ifMatch),sizeof(request.ifNoneMatch),sizeof(request.contentType),sizeof(request.requestTarget)};
 for(unsigned i=0;i<8;++i)if(!std::memchr(fields[i],0,capacities[i]))return false;
 char checked[sizeof(request.path)];
 if(!token(request.method)||!path(request.path,std::strlen(request.path),checked)||std::strcmp(checked,request.path)||request.depth>2)return false;
 if(std::strcmp(request.requestTarget,"*")||std::strcmp(request.method,"OPTIONS")){
  if(!path(request.requestTarget,std::strlen(request.requestTarget),checked)||std::strcmp(checked,request.path))return false;
 }else if(std::strcmp(request.path,"/"))return false;
 if((request.ifMatch[0]&&!tagList(request.ifMatch))||(request.ifNoneMatch[0]&&!tagList(request.ifNoneMatch)))return false;
 const size_t challengeSize=detail::boundedLength(hooks.authenticationChallenge,sizeof(challenge_));
 if(!challengeSize||challengeSize==sizeof(challenge_))return false;
 for(size_t i=0;i<challengeSize;++i)if((unsigned char)hooks.authenticationChallenge[i]<32||(unsigned char)hooks.authenticationChallenge[i]>=127)return false;
 std::memcpy(challenge_,hooks.authenticationChallenge,challengeSize+1);
 files_=files;hooks_=hooks;request_=request;input_=body;inputSize_=size;count_=row_=rows_=0;headerSize_=outputSize_=outputCapacity_=0;output_=nullptr;etag_[0]=0;operation_=Operation::None;status_=0;head_=!std::strcmp(request.method,"HEAD");properties_={};state_=State::Catalog;
 if(!valid())return false;
 if(!hooks_.authorize(hooks_.context,request_,AuthPhase::Complete,input_,inputSize_)){if(!valid())return false;error(401,"Authentication required\n");}return true;
}
bool Transaction::buildListing(){
 rows_=1;std::strcpy(listing_[0].path,request_.path);listing_[0].directory=directory(request_.path);
 if(!listing_[0].directory||request_.depth==0)return true;
 const size_t base=std::strlen(request_.path);for(unsigned i=0;i<count_;++i){const char*p=catalog_[i].path;
  if(base>1){if(std::strncmp(p,request_.path,base)||p[base]!='/')continue;p+=base;}const char*slash=std::strchr(p+1,'/');
  size_t n=slash?size_t(slash-catalog_[i].path):std::strlen(catalog_[i].path);bool exists=false;
  for(unsigned j=0;j<rows_;++j)exists=exists||(std::strlen(listing_[j].path)==n&&!std::memcmp(listing_[j].path,catalog_[i].path,n));
  if(exists)continue;
  if(rows_==CatalogMax+1||!copy(listing_[rows_].path,sizeof(listing_[rows_].path),catalog_[i].path,n))return false;
  listing_[rows_++].directory=slash!=nullptr;
 }return true;
}
bool Transaction::propertyRow(const Row&row,uint32_t size,uint64_t revision){
 if(!append("<D:response><D:href>")||!escaped(row.path,true)||(row.directory&&std::strcmp(row.path,"/")&&!append("/"))||!append("</D:href>"))return false;
 const char* names[]={"resourcetype","getcontentlength","getcontenttype","getetag","supportedlock","displayname"};
 const uint32_t available=row.directory?49u:63u;
 uint32_t wanted=properties_.mode==PropertyMode::Named?0u:available;
 for(unsigned n=0;n<properties_.count;++n)if(!std::strcmp(properties_.namespaceUri(n),"DAV:"))
  for(unsigned i=0;i<6;++i)if(!std::strcmp(properties_.localName(n),names[i]))wanted|=1u<<i;
 if(wanted&available){
  if(!append("<D:propstat><D:prop>"))return false;
  for(unsigned i=0;i<6;++i){
   if(!(wanted&available&(1u<<i)))continue;
   if(!append("<D:")||!append(names[i])||!append(">"))return false;
   if(properties_.mode!=PropertyMode::Names){char value[48]{};
    if(i==0&&row.directory&&!append("<D:collection/>"))return false;
    if(i==1){std::snprintf(value,sizeof(value),"%u",size);if(!append(value))return false;}
    if(i==2&&!append("application/octet-stream"))return false;
    if(i==3){tag(value,sizeof(value),revision);if(!escaped(value))return false;}
    if(i==5){const char* base=std::strrchr(row.path,'/');if(!escaped(base&&base[1]?base+1:row.path))return false;}
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
State Transaction::step(){
 if(state_==State::Idle||state_==State::Done||state_==State::Retained)return state_;
 if(!valid())return state_;
 void*context=files_->volume.terminal.power.volume.base.context;
 if(state_==State::Catalog){risc_app_data_export_entry_v1 value{};int32_t rc=files_->entry(context,count_,&value);if(!valid())return state_;
  if(rc==RISC_APP_DATA_NOT_FOUND){state_=State::Prepare;return state_;}if(!backend(rc))return state_;
  char checked[sizeof(value.path)];if(count_==CatalogMax||!std::memchr(value.path,0,sizeof(value.path))||!path(value.path,std::strlen(value.path),checked)||std::strcmp(value.path,checked)||value.writable>1){state_=State::Retained;return state_;}
  for(unsigned i=0;i<count_;++i)if(!std::strcmp(value.path,catalog_[i].path)){state_=State::Retained;return state_;}
  catalog_[count_++]=value;return state_;
 }
 if(state_==State::Prepare){
  if(operation_==Operation::Read){uint32_t count=0;uint64_t observed=0;int32_t rc=files_->read_revision(context,request_.path,revision_,output_,fileSize_,&count,&observed);
   if(!backend(rc))return state_;
   if(count!=fileSize_||observed!=revision_){state_=State::Retained;return state_;}outputSize_=count;response(200,"application/octet-stream");return state_;}
  if(operation_==Operation::Write){int32_t rc=files_->replace_revision(context,request_.path,revision_,input_,uint32_t(inputSize_));
   if(!backend(rc))return state_;
   etag_[0]=0;response(revision_?204:201);return state_;}
  bool writable=false;const bool file=selected(request_.path,&writable),dir=directory(request_.path);
  if(!file&&!dir){error(404,"Not found\n");return state_;}
  if(!std::strcmp(request_.method,"OPTIONS")){response(200);return state_;}
  if(!std::strcmp(request_.method,"PROPFIND")){
   if(dir&&request_.depth==2){error(403,"Finite Depth: 0 or 1 is required\n");return state_;}
   if(inputSize_&&request_.contentType[0]){
    char media[sizeof(request_.contentType)];std::strcpy(media,request_.contentType);
    if(char* semi=std::strchr(media,';'))*semi=0;
    size_t n=std::strlen(media);while(n&&(media[n-1]==' '||media[n-1]=='\t'))media[--n]=0;
    if(!same(media,"application/xml")&&!same(media,"text/xml")){error(415,"PROPFIND requires XML\n");return state_;}
   }
   const unsigned parsed=parseProperties(static_cast<const char*>(input_),inputSize_,properties_);
   if(parsed){error(parsed,"Invalid or unsupported PROPFIND XML\n");return state_;}
   if(!reserve(16384)||!buildListing()){error(503,"Response allocation failed\n");return state_;}
   append("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:multistatus xmlns:D=\"DAV:\">");state_=State::Properties;return state_;}
  const bool get=!std::strcmp(request_.method,"GET")||head_,put=!std::strcmp(request_.method,"PUT");
  if(!get&&!put){error(405,"Method unavailable for configured saved files\n");return state_;}
  if(dir){error(405,"Operation requires a file\n");return state_;}
  if(put&&!writable){error(403,"Read-only export\n");return state_;}
  int32_t rc=files_->stat_revision(context,request_.path,&fileSize_,&revision_);if(!valid())return state_;
  const bool present=rc==RISC_APP_DATA_OK;if(!present&&!(put&&rc==RISC_APP_DATA_NOT_FOUND)){backend(rc);return state_;}
  if(present&&(!revision_||fileSize_>RISC_APP_DATA_FILE_MAX)){state_=State::Retained;return state_;}tag(etag_,sizeof(etag_),revision_);
  if(request_.ifMatch[0]&&!match(request_.ifMatch,etag_,present)){error(412,"If-Match failed\n");return state_;}
  if(request_.ifNoneMatch[0]&&match(request_.ifNoneMatch,etag_,present,true)){if(get){outputSize_=0;response(304);}else error(412,"If-None-Match failed\n");return state_;}
  if(put){if(present&&(!request_.ifMatch[0]||!std::strcmp(request_.ifMatch,"*"))){error(428,"A specific If-Match revision is required\n");return state_;}
   if(!present&&std::strcmp(request_.ifNoneMatch,"*")){error(428,"Creation requires If-None-Match: *\n");return state_;}operation_=Operation::Write;return state_;}
  if(head_){response(200,"application/octet-stream",fileSize_);return state_;}
  if(!reserve(fileSize_?fileSize_:1)){error(503,"Snapshot allocation failed\n");return state_;}operation_=Operation::Read;return state_;
 }
 if(state_==State::Properties){if(row_==rows_){if(!append("</D:multistatus>")){error(507,"Property response too large\n");return state_;}response(207,"application/xml; charset=utf-8");return state_;}
  auto&entry=listing_[row_];uint32_t size=0;uint64_t revision=0;
  if(!entry.directory){int32_t rc=files_->stat_revision(context,entry.path,&size,&revision);if(!valid())return state_;if(rc==RISC_APP_DATA_NOT_FOUND&&row_){++row_;return state_;}if(!backend(rc))return state_;if(!revision||size>RISC_APP_DATA_FILE_MAX){state_=State::Retained;return state_;}}
  if(!propertyRow(entry,size,revision)){error(507,"Property response too large\n");return state_;}++row_;return state_;
 }
 return state_;
}
bool Transaction::finish(){
 if(state_==State::Idle||state_==State::Done)return true;
 if(!valid())return false;
 if(output_){auto* released=output_;output_=nullptr;hooks_.deallocate(released);if(!valid())return false;}
 outputSize_=outputCapacity_=0;files_=nullptr;input_=nullptr;state_=State::Done;return true;
}
}
