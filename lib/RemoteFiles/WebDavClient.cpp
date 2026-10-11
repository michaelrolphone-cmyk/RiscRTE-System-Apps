#include "WebDavClient.h"
#include "vendor/tinydtls_sha2/sha2.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
namespace RiscWebDav { namespace {
void erase(void*p,size_t n){volatile uint8_t*b=static_cast<volatile uint8_t*>(p);
while(n--)*b++=0;
}
void hex(const uint8_t*b,size_t n,char*out){const char*d="0123456789abcdef";
for(size_t i=0;i<n;i++){out[i*2]=d[b[i]>>4];
out[i*2+1]=d[b[i]&15];
}out[n*2]=0;
}
void hash(const char*a,const char*b,const char*c,char*out){dtls_sha256_ctx h;
dtls_sha256_init(&h);
const char*parts[]={a,":",b,":",c};
for(auto p:parts)dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(p),std::strlen(p));
uint8_t d[32];
dtls_sha256_final(d,&h);
hex(d,32,out);
erase(d,32);
erase(&h,sizeof(h));
}
bool text(const char*s,size_t cap){if(!s||!memchr(s,0,cap)||!*s)return false;
for(;*s;s++)if(*s<32||*s>126||*s=='"'||*s=='\\'||*s==':')return false;
return true;
}
bool equal(const char*a,const char*b){while(*a&&*b){char x=*a++,y=*b++;
if(x>='A'&&x<='Z')x+=32;
if(y>='A'&&y<='Z')y+=32;
if(x!=y)return false;
}return !*a&&!*b;
}
bool number(const char*s,uint64_t&n){n=0;
if(!*s)return false;
for(;*s;s++){if(*s<'0'||*s>'9'||n>(UINT64_MAX-9)/10)return false;
n=n*10+unsigned(*s-'0');
}return true;
}
}
void Client::wipe(){erase(password_,sizeof(password_));
erase(ha1_,sizeof(ha1_));
erase(nonce_,sizeof(nonce_));
erase(cnonce_,sizeof(cnonce_));
erase(header_,sizeof(header_));
authenticated_=false;
}
bool Client::encodePath(const char*s,char*out,size_t cap){
 if(!s||s[0]!='/'||!memchr(s,0,513))return false;
if(!strcmp(s,"/")){if(cap<2)return false;
strcpy(out,"/");
return true;
}size_t used=0;
const char*start=s;
 for(const char*p=s;;p++)if(*p=='/'||!*p){size_t n=size_t(p-start);
if((n==1&&start[0]=='.')||(n==2&&start[0]=='.'&&start[1]=='.')||(p!=s&&n==0))return false;
start=p+1;
if(!*p)break;
}
 for(;*s;s++){unsigned char c=*s;
if(c<32||c=='\\'||c==127)return false;
bool plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("/-._~",c);
size_t n=plain?1:3;
if(used+n>=cap)return false;
if(plain)out[used++]=char(c);
else{const char*d="0123456789ABCDEF";
out[used++]='%';
out[used++]=d[c>>4];
out[used++]=d[c&15];
}}
 out[used]=0;
return true;
}
bool Client::begin(const risc_tcp_connection_v1*t,const risc_bluetooth_session_descriptor_v1&d,const uint8_t entropy[16],const Io&io,uint64_t now){
 if(handle_||state_==State::Retained)return false;
wipe();
state_=State::Off;
 if(!t||t->api_version!=1||t->struct_size<sizeof(*t)||t->connect_tag!=RISC_TCP_CONNECTION_CONNECT_TAG||!t->context||!t->connect||!t->connected||!t->read||!t->write||!t->close||!io.owner||!io.owner(io.context)||!entropy||d.struct_size<sizeof(d)||d.transport!=RISC_SESSION_HTTP||!d.remaining_lifetime_ms||d.remaining_lifetime_ms>300000||now>UINT64_MAX-d.remaining_lifetime_ms||!text(d.username,sizeof(d.username))||!text(d.password,sizeof(d.password))||!memchr(d.url,0,sizeof(d.url))||strncmp(d.url,"http://",7))return false;
 const char*p=d.url+7;
endpoint_={};
endpoint_.struct_size=sizeof(endpoint_);
 for(unsigned i=0;i<4;i++){unsigned n=0,digits=0;
while(*p>='0'&&*p<='9'){n=n*10+unsigned(*p++-'0');
if(++digits>3||n>255)return false;
}if(!digits||*p++!=(i==3?':':'.'))return false;
endpoint_.address[i]=uint8_t(n);
}
 unsigned port=0;
while(*p>='0'&&*p<='9'){port=port*10+unsigned(*p++-'0');
if(port>65535)return false;
}if(!port||strcmp(p,"/")||!endpoint_.address[0]||endpoint_.address[0]>=224)return false;
endpoint_.port=uint16_t(port);
 std::snprintf(host_,sizeof(host_),"%u.%u.%u.%u:%u",endpoint_.address[0],endpoint_.address[1],endpoint_.address[2],endpoint_.address[3],port);
 tcp_=*t;
io_=io;
strcpy(username_,d.username);
strcpy(password_,d.password);
hex(entropy,16,cnonce_);
nonceCount_=0;
expiry_=now+d.remaining_lifetime_ms;
lastNow_=now;
ending_=false;
retryConnect_=false;
handshake_=true;
authenticated_=false;
state_=State::Ready;
 return request("OPTIONS","/",nullptr,0,io,now);
}
bool Client::live(uint64_t now){if(!io_.owner||!io_.owner(io_.context)){state_=State::Retained;
error_=Transport;
wipe();
return false;
}if(now<lastNow_||now>=expiry_){fail(Expired);
wipe();
return false;
}lastNow_=now;
return true;
}
bool Client::request(const char*m,const char*p,const char*d,uint64_t size,const Io&io,uint64_t now,const char*match){
 if(state_!=State::Ready||(!authenticated_&&!handshake_)||!live(now)||!m||!io.owner||!io.owner(io.context)||!encodePath(p,path_,sizeof(path_)))return false;
 if(strcmp(m,"OPTIONS")&&strcmp(m,"PROPFIND")&&strcmp(m,"GET")&&strcmp(m,"HEAD")&&strcmp(m,"PUT")&&strcmp(m,"DELETE")&&strcmp(m,"MKCOL")&&strcmp(m,"MOVE")&&strcmp(m,"COPY"))return false;
 if(size>64u*1024u*1024u||(size&&!io.source)||(!strcmp(m,"PUT")?false:size!=0))return false;
 destination_[0]=0;
if(d&&!encodePath(d,destination_,sizeof(destination_)))return false;
if((!strcmp(m,"MOVE")||!strcmp(m,"COPY"))&&!d)return false;
 match_[0]=etag_[0]=0;
 if(match){size_t n=strlen(match);
if(n<2||n>=sizeof(match_)||match[0]!='"'||match[n-1]!='"')return false;
for(size_t i=1;i+1<n;i++)if(match[i]<33||match[i]>126||match[i]=='"')return false;
strcpy(match_,match);
}
 strcpy(method_,m);
io_=io;
mutating_=!strcmp(m,"PUT")||!strcmp(m,"DELETE")||!strcmp(m,"MKCOL")||!strcmp(m,"MOVE")||!strcmp(m,"COPY");
uncertain_=false;
error_=None;
status_=0;
uploadSize_=size;
sent_=received_=responseSize_=0;
replySize_=bufferSize_=bufferAt_=headerAt_=0;
deadline_=expiry_-now>30000?now+30000:expiry_;
 if(!buildHeader()){fail(Invalid);
return false;
}state_=State::Connect;
return true;
}
bool Client::buildHeader(){
 char auth[1536]{};
if(!handshake_){if(nonceCount_==UINT32_MAX)return false;
char nc[9],ha2[65],response[65],joined[512];
std::snprintf(nc,sizeof(nc),"%08x",++nonceCount_);
 // HA2 is method:uri, without an extra separator.
 dtls_sha256_ctx h;
dtls_sha256_init(&h);
dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(method_),strlen(method_));
dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(":"),1);
dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(path_),strlen(path_));
uint8_t raw[32];
dtls_sha256_final(raw,&h);
hex(raw,32,ha2);
erase(raw,32);
erase(&h,sizeof(h));
 int z=snprintf(joined,sizeof(joined),"%s:%s:%s:%s:auth:%s",ha1_,nonce_,nc,cnonce_,ha2);
if(z<0||size_t(z)>=sizeof(joined))return false;
dtls_sha256_init(&h);
dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(joined),size_t(z));
dtls_sha256_final(raw,&h);
hex(raw,32,response);
erase(joined,sizeof(joined));
erase(raw,32);
erase(&h,sizeof(h));
 z=snprintf(auth,sizeof(auth),"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=SHA-256, qop=auth, nc=%s, cnonce=\"%s\", response=\"%s\"\r\n",username_,realm_,nonce_,path_,nc,cnonce_,response);
if(z<0||size_t(z)>=sizeof(auth))return false;
}
 char dest[1664]{};
if(*destination_){int z=snprintf(dest,sizeof(dest),"Destination: http://%s%s\r\nOverwrite: F\r\n",host_,destination_);
if(z<0||size_t(z)>=sizeof(dest))return false;
}
 char condition[128]{};
 if(!strcmp(method_,"PUT")){if(match_[0])snprintf(condition,sizeof(condition),"If-Match: %s\r\n",match_);
else strcpy(condition,"If-None-Match: *\r\n");
}
 int n=snprintf(header_,sizeof(header_),"%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length: %llu\r\n%s%s%s%s\r\n",method_,path_,host_,static_cast<unsigned long long>(uploadSize_),auth,!strcmp(method_,"PROPFIND")?"Depth: 1\r\n":"",condition,dest);
erase(auth,sizeof(auth));
if(n<0||size_t(n)>=sizeof(header_))return false;
headerSize_=size_t(n);
return true;
}
bool Client::challenge(const char*s){
 if(strncmp(s,"Digest ",7))return false;
s+=7;
unsigned seen=0;
char algorithm[24]{},qop[32]{};
 while(*s){while(*s==' '||*s==',')s++;
char key[32]{},value[128]{};
size_t n=0;
while(*s&&*s!='='&&n+1<sizeof(key))key[n++]=*s++;
if(*s++!='=')return false;
n=0;
 if(*s=='"'){s++;
while(*s&&*s!='"'&&n+1<sizeof(value)){if(*s=='\\'||*s<32)return false;
value[n++]=*s++;
}if(*s++!='"')return false;
}else{while(*s&&*s!=','&&*s!=' '&&n+1<sizeof(value))value[n++]=*s++;
}
 char*out=nullptr;
size_t cap=0;
unsigned bit=0;
if(equal(key,"realm")){out=realm_;
cap=sizeof(realm_);
bit=1;
}else if(equal(key,"nonce")){out=nonce_;
cap=sizeof(nonce_);
bit=2;
}else if(equal(key,"algorithm")){out=algorithm;
cap=sizeof(algorithm);
bit=4;
}else if(equal(key,"qop")){out=qop;
cap=sizeof(qop);
bit=8;
}else return false;
 if((seen&bit)||!n||n>=cap)return false;
memcpy(out,value,n+1);
seen|=bit;
if(*s&&*s!=','&&*s!=' ')return false;
}
 if(seen!=15||strcmp(algorithm,"SHA-256")||(!strstr(qop,"auth,")&&strcmp(qop,"auth")&&strcmp(qop,"auth,auth-int"))||!text(realm_,sizeof(realm_))||!text(nonce_,sizeof(nonce_)))return false;
 hash(username_,realm_,password_,ha1_);
erase(password_,sizeof(password_));
return true;
}
bool Client::parseReply(){
 reply_[replySize_]=0;
char*line=strstr(reply_,"\r\n");
if(!line)return false;
*line=0;
 if(strncmp(reply_,"HTTP/1.1 ",9)||strlen(reply_)<12||reply_[9]<'1'||reply_[9]>'5'||reply_[10]<'0'||reply_[10]>'9'||reply_[11]<'0'||reply_[11]>'9')return false;
status_=unsigned(reply_[9]-'0')*100+unsigned(reply_[10]-'0')*10+unsigned(reply_[11]-'0');
bool length=false,auth=false;
 for(char*p=line+2;*p;){char*end=strstr(p,"\r\n");
if(!end)return false;
if(end==p)break;
*end=0;
char*colon=strchr(p,':');
if(!colon)return false;
*colon=0;
char*v=colon+1;
while(*v==' ')v++;
 if(equal(p,"Content-Length")){if(length||!number(v,responseSize_)||responseSize_>64u*1024u*1024u)return false;
length=true;
}
 else if(equal(p,"Transfer-Encoding"))return false;
 else if(equal(p,"ETag")){if(etag_[0]||strlen(v)>=sizeof(etag_))return false;
strcpy(etag_,v);
}
 else if(equal(p,"WWW-Authenticate")){if(auth||status_!=401||(handshake_&&!challenge(v)))return false;
auth=true;
}
 p=end+2;
}
 if(handshake_&&(status_!=401||!auth))return false;
 if(!handshake_&&(status_==401||status_==403)){error_=Authentication;
wipe();
}
 if(!length&&status_!=204&&status_!=304)return false;
if(!strcmp(method_,"HEAD")||status_==204||status_==304)responseSize_=0;
 return true;
}
void Client::fail(Error e){
// A private AP can become addressable just before its listener is ready.
// Retry only the initial connect, before any HTTP byte; never retry a write.
retryConnect_=e==Transport&&handshake_&&!headerAt_&&!ending_&&
 (state_==State::Connect||state_==State::Connecting)&&lastNow_+250<deadline_&&lastNow_+250<expiry_;
retryAt_=lastNow_+250;
error_=e;
if(mutating_&&headerAt_){uncertain_=true;
if(e==Transport)e=Uncertain;
error_=e;
}state_=handle_?State::Closing:retryConnect_?State::ConnectWait:State::Error;
}
bool Client::result(int32_t rc){if(rc==RISC_TCP_CONNECTION_CONTEXT||rc==RISC_TCP_CONNECTION_RETAINED){state_=State::Retained;
error_=Transport;
wipe();
return false;
}if(rc<0){fail(Transport);
return false;
}return true;
}
bool Client::consume(const void*p,size_t n){if(n>responseSize_-received_){fail(Protocol);
return false;
}if(!handshake_&&status_>=200&&status_<300&&io_.sink&&!io_.sink(io_.context,p,n)){fail(Storage);
return false;
}received_+=n;
if(received_==responseSize_)state_=State::Closing;
return true;
}
Client::State Client::tick(uint64_t now){
 if(state_==State::Off||state_==State::Retained)return state_;
if(!io_.owner||!io_.owner(io_.context)){state_=State::Retained;
error_=Transport;
wipe();
return state_;
}
 if(state_!=State::Closing&&!live(now))return state_;
if(state_==State::Ready||state_==State::Error)return state_;
if(state_!=State::Closing&&now>=deadline_){fail(Expired);
return state_;
}
 int32_t rc=0;
uint32_t n=0;
 switch(state_){
 case State::ConnectWait:if(now>=retryAt_){retryConnect_=false;error_=None;state_=State::Connect;}break;
 case State::Connect:rc=tcp_.connect(tcp_.context,&endpoint_,&handle_);
if(result(rc)){if(rc||!handle_){fail(Transport);
break;
}state_=State::Connecting;
}break;
 case State::Connecting:rc=tcp_.connected(tcp_.context,handle_);
if(result(rc)&&rc==0)state_=State::Header;
else if(rc!=0&&rc!=RISC_TCP_CONNECTION_WOULD_BLOCK&&state_==State::Connecting)fail(Protocol);
break;
 case State::Header:rc=tcp_.write(tcp_.context,handle_,header_+headerAt_,uint32_t(headerSize_-headerAt_>2048?2048:headerSize_-headerAt_),&n);
if(!result(rc))break;
if(rc==0&&n&&n<=headerSize_-headerAt_){headerAt_+=n;
if(headerAt_==headerSize_)state_=uploadSize_?State::UploadRead:State::Reply;
}else if(rc!=1||n)fail(Protocol);
break;
 case State::UploadRead:{size_t count=0,cap=uploadSize_-sent_>sizeof(buffer_)?sizeof(buffer_):size_t(uploadSize_-sent_);
if(!io_.source||!io_.source(io_.context,buffer_,cap,&count)||!count||count>cap){fail(Storage);
break;
}bufferSize_=count;
bufferAt_=0;
state_=State::UploadSend;
break;
}
 case State::UploadSend:rc=tcp_.write(tcp_.context,handle_,buffer_+bufferAt_,uint32_t(bufferSize_-bufferAt_),&n);
if(!result(rc))break;
if(rc==0&&n&&n<=bufferSize_-bufferAt_){bufferAt_+=n;
sent_+=n;
if(bufferAt_==bufferSize_)state_=sent_==uploadSize_?State::Reply:State::UploadRead;
}else if(rc!=1||n)fail(Protocol);
break;
 case State::Reply:case State::Body:{rc=tcp_.read(tcp_.context,handle_,buffer_,sizeof(buffer_),&n);
if(!result(rc))break;
if(rc==1&&!n)break;
if(rc||!n||n>sizeof(buffer_)){fail(Protocol);
break;
}size_t i=0;
if(state_==State::Reply){for(;i<n;i++){if(replySize_+1>=sizeof(reply_)){fail(Protocol);
break;
}reply_[replySize_++]=char(buffer_[i]);
if(replySize_>=4&&!memcmp(reply_+replySize_-4,"\r\n\r\n",4)){i++;
if(!parseReply()){fail(Protocol);
break;
}state_=responseSize_?State::Body:State::Closing;
break;
}}}if((state_==State::Body||state_==State::Closing)&&i<n)consume(buffer_+i,n-i);
break;
}
 case State::Closing:if(handle_){rc=tcp_.close(tcp_.context,handle_);
if(rc){state_=State::Retained;
error_=Transport;
wipe();
break;
}handle_=0;
}if(retryConnect_&&!ending_){state_=State::ConnectWait;break;}
erase(header_,sizeof(header_));
erase(buffer_,sizeof(buffer_));
if(ending_){wipe();
state_=State::Off;
}else if(error_){state_=State::Error;
}else{if(handshake_){handshake_=false;
authenticated_=true;
}state_=State::Ready;
}break;
 default:break;
}
 return state_;
}
void Client::cancel(){if(state_==State::Retained||state_==State::Off)return;
ending_=true;
fail(Cancelled);
if(!handle_){wipe();
state_=State::Off;
}}
bool Client::end(){cancel();
return state_==State::Off;
}
}
