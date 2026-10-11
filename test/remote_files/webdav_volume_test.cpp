#include "WebDavSession.h"
#include <RiscStorageVolumeFsV1.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
using namespace RiscWebDav;
struct Fixture {
 struct Node {std::string data;bool directory=false;};
 struct Handle {std::string path;size_t offset=0;bool writer=false;std::vector<std::string> names;};
 std::map<std::string,Node> nodes{{"/",{"",true}},{"/folder",{"",true}},{"/file.txt",{"old",false}}};
 std::map<uint32_t,Handle> handles;uint32_t serial=0;unsigned calls=0,trigger=0;int state=0,triggerState=1;bool owned=true,closeFail=false,authorized=true,bodyAuthorized=true;
 risc_storage_volume_api_v1_fs fs{};std::string wire,response;size_t networkOffset=0;bool networkClosed=false;
 std::map<void*,size_t> allocations;size_t live=0,peak=0;static size_t maximumPeak;
 ~Fixture(){maximumPeak=std::max(maximumPeak,peak);for(auto&a:allocations)std::free(a.first);}
 static Fixture*active;
 void op(){assert(owned&&state==0);++calls;if(trigger==calls)state=triggerState;}
 Fixture(){active=this;auto&b=base();b.api_version=1;b.struct_size=sizeof(fs);b.context=this;fs.state.state_tag=RISC_STORAGE_STATE_TAG;fs.state.state_version=1;fs.state.observe=[](void*c){return static_cast<Fixture*>(c)->state;};fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
 b.stat=[](void*c,const char*p,uint64_t*n,bool*d){auto&f=*static_cast<Fixture*>(c);f.op();auto i=f.nodes.find(p);if(i==f.nodes.end())return false;*n=i->second.data.size();*d=i->second.directory;return true;};
 b.file_open_read=[](void*c,const char*p,uint64_t*n)->uint32_t{auto&f=*static_cast<Fixture*>(c);f.op();auto i=f.nodes.find(p);if(i==f.nodes.end()||i->second.directory)return 0;*n=i->second.data.size();uint32_t h=++f.serial;f.handles[h]={p,0,false,{}};return h;};
 b.file_read=[](void*c,uint32_t h,void*out,size_t n){auto&f=*static_cast<Fixture*>(c);f.op();auto&i=f.handles.at(h);auto&data=f.nodes.at(i.path).data;n=std::min(n,data.size()-i.offset);memcpy(out,data.data()+i.offset,n);i.offset+=n;return n;};
 b.file_open_write=[](void*c,const char*p)->uint32_t{auto&f=*static_cast<Fixture*>(c);f.op();if(f.nodes.count(p))return 0;f.nodes[p]={};uint32_t h=++f.serial;f.handles[h]={p,0,true,{}};return h;};
 b.file_write=[](void*c,uint32_t h,const void*in,size_t n){auto&f=*static_cast<Fixture*>(c);f.op();auto&i=f.handles.at(h);f.nodes.at(i.path).data.append(static_cast<const char*>(in),n);return n;};
 b.file_close=[](void*c,uint32_t h,bool commit){auto&f=*static_cast<Fixture*>(c);assert(f.owned&&f.state!=-2);++f.calls;if(f.closeFail)return false;auto i=f.handles.find(h);if(i==f.handles.end())return false;if(i->second.writer&&!commit)f.nodes.erase(i->second.path);f.handles.erase(i);return true;};
 ext().file_sync=[](void*c,uint32_t){static_cast<Fixture*>(c)->op();return true;};
 ext().rename=[](void*c,const char*a,const char*b){auto&f=*static_cast<Fixture*>(c);f.op();if(!f.nodes.count(a)||f.nodes.count(b))return false;f.nodes[b]=f.nodes[a];f.nodes.erase(a);return true;};
 fs.replace_file=[](void*c,const char*a,const char*b){auto&f=*static_cast<Fixture*>(c);f.op();if(!f.nodes.count(a))return false;f.nodes[b]=f.nodes[a];f.nodes.erase(a);return true;};
 fs.recover_replace=[](void*,const char*){return true;};fs.file_truncate=[](void*,uint32_t,uint64_t){return false;};fs.metadata=[](void*c,const char*p,risc_storage_metadata_v1*out)->int32_t{auto&f=*static_cast<Fixture*>(c);f.op();auto i=f.nodes.find(p);if(i==f.nodes.end())return RISC_STORAGE_FS_NOT_FOUND;out->flags=i->second.directory?RISC_STORAGE_DIRECTORY:0;out->size=i->second.data.size();return 0;};fs.dir_tell=[](void*c,uint32_t h,uint64_t*out){auto&f=*static_cast<Fixture*>(c);f.op();*out=f.handles.at(h).offset;return true;};fs.dir_seek=[](void*c,uint32_t h,uint64_t pos){auto&f=*static_cast<Fixture*>(c);f.op();auto&i=f.handles.at(h);if(pos>i.names.size())return false;i.offset=pos;return true;};
 b.remove=[](void*c,const char*p){auto&f=*static_cast<Fixture*>(c);f.op();std::string prefix=std::string(p)+"/";for(auto&i:f.nodes)if(i.first.find(prefix)==0)return false;return f.nodes.erase(p)>0;};
 ext().mkdir=[](void*c,const char*p){auto&f=*static_cast<Fixture*>(c);f.op();return f.nodes.emplace(p,Node{"",true}).second;};
 b.dir_open=[](void*c,const char*p)->uint32_t{auto&f=*static_cast<Fixture*>(c);f.op();auto i=f.nodes.find(p);if(i==f.nodes.end()||!i->second.directory)return 0;Handle h{p,0,false,{}};std::string pre=std::string(p)=="/"?"/":std::string(p)+"/";for(auto&node:f.nodes)if(node.first.find(pre)==0){auto name=node.first.substr(pre.size());if(!name.empty()&&name.find('/')==std::string::npos)h.names.push_back(name);}f.handles[++f.serial]=h;return f.serial;};
 b.dir_next=[](void*c,uint32_t h,risc_storage_dirent_v1*out){auto&f=*static_cast<Fixture*>(c);f.op();auto&i=f.handles.at(h);if(i.offset==i.names.size())return false;auto&name=i.names[i.offset++];strcpy(out->name,name.c_str());auto&node=f.nodes.at(i.path+(i.path=="/"?"":"/")+name);out->size=node.data.size();out->is_directory=node.directory;return true;};
 ext().dir_close_checked=[](void*c,uint32_t h){auto&f=*static_cast<Fixture*>(c);assert(f.owned&&f.state!=-2);++f.calls;if(f.closeFail)return false;return f.handles.erase(h)>0;};ext().handle_error=[](void*,uint32_t,bool){return 0u;};
 }
 risc_storage_volume_api_v1&base(){return fs.state.prepared.base.sleep.terminal.power.volume.base;}
 risc_storage_volume_api_v1_ext&ext(){return fs.state.prepared.base.sleep.terminal.power.volume;}
 Hooks hooks(){return {this,[](void*c){return static_cast<Fixture*>(c)->owned;},[](void*c,const Request&,AuthPhase phase,const void*,size_t){auto&f=*static_cast<Fixture*>(c);return f.authorized&&(phase!=AuthPhase::BodyHash||f.bodyAuthorized);},"Digest realm=\"files\"",[](size_t n){auto&f=*active;void*p=std::malloc(n);if(p){f.allocations[p]=n;f.live+=n;f.peak=std::max(f.peak,f.live);}return p;},[](void*p){auto&f=*active;assert(f.allocations.count(p));f.live-=f.allocations.at(p);f.allocations.erase(p);std::free(p);}};}
 Connection connection(){return {this,1,[](void*c,uint64_t,void*out,uint32_t n,uint32_t*got)->int32_t{auto&f=*static_cast<Fixture*>(c);assert(f.owned&&f.state!=-2);n=std::min<size_t>(n,f.wire.size()-f.networkOffset);*got=n;if(!n)return 2;memcpy(out,f.wire.data()+f.networkOffset,n);f.networkOffset+=n;return 0;},[](void*c,uint64_t,const void*p,uint32_t n,uint32_t*sent)->int32_t{auto&f=*static_cast<Fixture*>(c);assert(f.owned&&f.state!=-2);*sent=std::min<uint32_t>(n,71);f.response.append(static_cast<const char*>(p),*sent);return 0;},[](void*c,uint64_t)->int32_t{auto&f=*static_cast<Fixture*>(c);assert(f.owned&&f.state!=-2);f.networkClosed=true;return 0;}};}
 unsigned run(const char*method,const char*path,const char*headers="",const std::string&body="") {wire=std::string(method)+" "+path+" HTTP/1.1\r\nHost: watch.local\r\n"+headers+"Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;networkOffset=0;response.clear();networkClosed=false;Session s;if(!s.begin(&base(),hooks(),connection(),0,10000000)){assert(s.state()==SessionState::Retained);return 999;}for(unsigned n=0;n<100000&&s.state()!=SessionState::Done&&s.state()!=SessionState::Retained;n++)s.tick(n);assert(s.state()==SessionState::Done||s.state()==SessionState::Retained);if(s.state()==SessionState::Retained)return 999;assert(networkClosed);assert(handles.empty()||state==1);return s.status();}
};
Fixture*Fixture::active=nullptr;size_t Fixture::maximumPeak=0;
int main(){
 {Fixture f;assert(f.run("GET","/file.txt")==200);assert(f.response.substr(f.response.find("\r\n\r\n")+4)=="old");assert(f.run("HEAD","/file.txt")==200);assert(f.response.find("Content-Length: 3")!=std::string::npos);}
 {Fixture f;assert(f.run("PUT","/large-upload","",std::string(1024*1024,'u'))==201);assert(f.nodes["/large-upload"].data==std::string(1024*1024,'u'));}
 {Fixture f;f.nodes["/big"]={std::string(2*1024*1024,'x'),false};assert(f.run("GET","/big")==200);assert(f.response.substr(f.response.find("\r\n\r\n")+4)==f.nodes["/big"].data);assert(f.run("COPY","/big","Destination: http://watch.local/copy\r\nOverwrite: F\r\n")==201);assert(f.nodes["/copy"].data==f.nodes["/big"].data);}
 {Fixture f;assert(f.run("PUT","/new","If-None-Match: *\r\n","hello")==201);assert(f.nodes["/new"].data=="hello");assert(f.run("PUT","/new","If-None-Match: *\r\n","lost")==412);assert(f.nodes["/new"].data=="hello");assert(f.run("PUT","/new","","replacement")==204);assert(f.nodes["/new"].data=="replacement");assert(f.run("MOVE","/new","Destination: /moved\r\nOverwrite: F\r\n")==201);assert(!f.nodes.count("/new"));assert(f.run("DELETE","/moved")==204);assert(f.run("MKCOL","/created")==201);assert(f.run("DELETE","/created")==204);}
 {Fixture f;for(unsigned i=0;i<513;i++)f.nodes["/folder/file"+std::to_string(i)]={"v",false};assert(f.run("PROPFIND","/folder","Depth: 1\r\n")==207);assert(f.response.find("file512")!=std::string::npos);assert(f.run("PROPFIND","/folder","Depth: 0\r\n","<D:propfind xmlns:D=\"DAV:\"><D:prop><D:displayname/><D:unknown/></D:prop></D:propfind>")==207);assert(f.response.find("404 Not Found")!=std::string::npos);}
 {Fixture f;f.nodes["/folder/nested"]={"",true};f.nodes["/folder/nested/file"]={"nested data",false};f.nodes["/folder/root"]={"root data",false};assert(f.run("COPY","/folder","Destination: /tree-copy\r\nOverwrite: F\r\n")==201);assert(f.nodes["/tree-copy/nested/file"].data=="nested data");assert(f.nodes["/tree-copy/root"].data=="root data");assert(f.run("DELETE","/tree-copy")==204);assert(!f.nodes.count("/tree-copy"));assert(f.nodes.count("/folder/nested/file"));assert(f.run("COPY","/folder","Destination: /folder/cycle\r\n")==403);}
 {Fixture f;f.bodyAuthorized=false;assert(f.run("PUT","/file.txt","",std::string(90000,'u'))==401);assert(f.nodes["/file.txt"].data=="old");for(auto&n:f.nodes)assert(n.first.find(".risc-webdav-")==std::string::npos);}
 {Fixture f;f.nodes["/folder/target"]={"previous",false};assert(f.run("MOVE","/file.txt","Destination: /folder/target\r\n")==204);assert(!f.nodes.count("/file.txt")&&f.nodes["/folder/target"].data=="old");}
 {Fixture f;f.nodes["/caf\xc3\xa9"]={"unicode",false};assert(f.run("GET","/caf%C3%A9")==200);}
 {Fixture f;f.trigger=3;f.triggerState=-2;assert(f.run("GET","/file.txt")==999);assert(!f.networkClosed&&f.calls==3);}
 // Cancel at every phase around body staging, sync, close and publication.
 for(unsigned cut=1;cut<20;cut++){Fixture f;f.wire="PUT /file.txt HTTP/1.1\r\nHost: local\r\nContent-Length: 12000\r\n\r\n"+std::string(12000,'u');Session session;assert(session.begin(&f.base(),f.hooks(),f.connection(),0,100000));for(unsigned i=0;i<cut&&session.state()!=SessionState::Done;i++)session.tick(i);assert(session.stop());assert(f.handles.empty());assert(f.nodes["/file.txt"].data=="old"||f.nodes["/file.txt"].data==std::string(12000,'u'));for(auto&n:f.nodes)assert(n.first.find(".risc-webdav-")==std::string::npos);}
 for(int state:{1,-2}){Fixture f;f.state=state;assert(f.run("GET","/file.txt")==unsigned(state==1?503:999));assert(f.calls==0);}
 for(unsigned point=1;point<=8;point++){Fixture f;f.trigger=point;f.triggerState=1;f.run("PUT","/file.txt","",std::string(12000,'q'));assert(f.calls<=point+2);if(f.state==1)assert(f.nodes["/file.txt"].data=="old");}
 {Fixture f;f.closeFail=true;assert(f.run("GET","/file.txt")==999);assert(!f.networkClosed);}
 {Fixture f;f.authorized=false;assert(f.run("PUT","/file.txt","","no")==401);assert(f.calls==0);}
 {Request q;std::string h="MOVE /file HTTP/1.1\r\nHost: watch.local\r\nDestination: http://other.local/file\r\n\r\n";assert(parseRequest(h.data(),h.size(),q)==403);h="GET /%2e%2e/secret HTTP/1.1\r\nHost: watch.local\r\n\r\n";assert(parseRequest(h.data(),h.size(),q)==400);}
 std::printf("SD request allocator peak: %zu bytes; Session object: %zu bytes; transfer chunk: %u bytes\n",Fixture::maximumPeak,sizeof(Session),RISC_STORAGE_VOLUME_IO_MAX);
 puts("WebDAV volume: streamed 2 MiB GET/COPY, 513 rows, property selection, PUT/MOVE/DELETE/MKCOL, conditions, absent/exported/removal and retained close PASS");
}
