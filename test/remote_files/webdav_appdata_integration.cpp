#include "WebDavSession.h"
#include "runtime/storage/AppDataExport.h"
#include "runtime/storage/AppDataFiles.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>
using namespace RiscWebDav;
using RiscStorage::AppDataFiles;using RiscStorage::AppDataExport;
namespace {
bool owned=true,authorized=true,closeFault=false;
unsigned stats=0,reads=0,writes=0,closes=0,networkCalls=0;
std::string incoming,outgoing;size_t position=0;
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd){int rc=__real_close(fd);if(closeFault){closeFault=false;errno=EIO;return -1;}return rc;}
namespace {
const AppDataFiles::Hooks io{nullptr,[](void*){return 0u;},[](void*){return true;},malloc,free};
const AppDataExport::Hooks exportHooks{nullptr,[](void*){return owned;},[](void*){return true;},malloc,free};
const Hooks protocolHooks{nullptr,[](void*){return owned;},[](void*,const Request&,AuthPhase,const void*,size_t){return authorized;},"Bearer realm=\"Saved files\"",malloc,free};
const Connection socket{nullptr,1,
 [](void*,uint64_t,void*b,uint32_t cap,uint32_t*n)->int32_t{++networkCalls;*n=std::min<size_t>({cap,31,incoming.size()-position});if(!*n)return 1;std::memcpy(b,incoming.data()+position,*n);position+=*n;return 0;},
 [](void*,uint64_t,const void*b,uint32_t cap,uint32_t*n)->int32_t{++networkCalls;*n=std::min<uint32_t>(cap,7);outgoing.append(static_cast<const char*>(b),*n);return 0;},
 [](void*,uint64_t)->int32_t{++networkCalls;++closes;return 0;}};
std::string bytes(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::map<std::string,std::string> inventory(const std::string&root){std::map<std::string,std::string> out;for(auto&e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file())out[e.path().lexically_relative(root).string()]=bytes(e.path());return out;}
void wire(const char*method,const char*path,const std::string&condition="",const std::string&body=""){
 incoming=std::string(method)+" "+path+" HTTP/1.1\r\nHost: watch.local\r\n"+condition+"Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
 outgoing.clear();position=0;
}
void run(Session&s){for(unsigned i=0;i<10000&&s.state()!=SessionState::Done&&s.state()!=SessionState::Retained;++i){unsigned before=stats+reads+writes+networkCalls;s.tick(1);assert(stats+reads+writes+networkCalls-before<=1);}assert(s.state()==SessionState::Done||s.state()==SessionState::Retained);}
}
int main(int argc,char**argv){assert(argc==3);const std::string root=argv[1],mode=argv[2];std::filesystem::create_directory(root+"/appdata");std::filesystem::create_directory(root+"/bootfs");{std::ofstream f(root+"/bootfs/app.elf");f<<"installed";}{std::ofstream f(root+"/nvs-private");f<<"bonds and settings";}
 AppDataFiles files(io);assert(files.configure((root+"/appdata").c_str()));assert(files.replace(1,"state",0,"original",8)==0);assert(files.replace(2,"private",0,"unexported",10)==0);
 RiscBoot::AppDataBackend backend{&files,
 [](void*c,uint32_t ns,const char*n,uint32_t*z,uint64_t*r){++stats;return static_cast<AppDataFiles*>(c)->stat(ns,n,z,r);},
 [](void*c,uint32_t ns,const char*n,uint64_t r,void*b,uint32_t cap,uint32_t*z,uint64_t*v){++reads;return static_cast<AppDataFiles*>(c)->read(ns,n,r,b,cap,z,v);},
 [](void*c,uint32_t ns,const char*n,uint64_t r,const void*b,uint32_t z){++writes;return static_cast<AppDataFiles*>(c)->replace(ns,n,r,b,z);},
 [](void*c){return static_cast<AppDataFiles*>(c)->exitSafe();}};
 const AppDataExport::Entry entries[]={{"owner",1,"state","/saved/state",true},{"owner",1,"new","/saved/new",true}};
 AppDataExport exportVolume(exportHooks);risc_app_data_export_v1 api{};
 assert(exportVolume.configure(&backend,entries,2,"Saved files")&&exportVolume.begin(&api));
 const auto before=inventory(root);const auto initialStats=stats;Session session;
 uint32_t size=0;uint64_t revision=0;assert(files.stat(1,"state",&size,&revision)==0);char tag[80];std::snprintf(tag,sizeof(tag),"If-Match: \"risc-%016llx\"\r\n",static_cast<unsigned long long>(revision));
 if(mode=="read")wire("GET","/saved/state");
 else if(mode=="write"||mode=="retained"||mode=="race")wire("PUT","/saved/state",tag,"replacement");
 else if(mode=="create")wire("PUT","/saved/new","If-None-Match: *\r\n","created");
 else if(mode=="scope")wire("GET","/appdata/n00000002/private");
 else if(mode=="auth"){authorized=false;wire("PUT","/saved/state",tag,"replacement");}
 else if(mode=="propfind")wire("PROPFIND","/saved","Depth: 1\r\nContent-Type: application/xml\r\n","<propfind xmlns='DAV:'><prop><getetag/><missing/></prop></propfind>");
 else assert(false);
 assert(session.begin(&api,protocolHooks,socket,0,100));
 if(mode=="retained"||mode=="race"){
  while(stats==initialStats)session.tick(1);
  if(mode=="race"){uint32_t z=0;uint64_t r=0;assert(files.stat(1,"state",&z,&r)==0);assert(files.replace(1,"state",r,"winning owner",13)==0);}
  else closeFault=true;
 }
 run(session);
 if(mode=="retained"){
  assert(session.state()==SessionState::Retained&&exportVolume.retained()&&!session.stop()&&!exportVolume.end()&&closes==0&&outgoing.empty());
  unsigned total=stats+reads+writes+networkCalls;session.tick(2);assert(stats+reads+writes+networkCalls==total);
  std::puts("WebDAV real AppDataFiles/export: retained checked-close stops response/transport/cleanup PASS");std::fflush(stdout);std::_Exit(0);
 }
 assert(session.state()==SessionState::Done&&closes==1&&exportVolume.end());
 if(mode=="read")assert(session.status()==200&&outgoing.find("\r\n\r\noriginal")!=std::string::npos&&inventory(root)==before);
 if(mode=="write")assert(session.status()==204&&bytes(root+"/appdata/n00000001/state")=="replacement"&&writes==1);
 if(mode=="race")assert(session.status()==412&&bytes(root+"/appdata/n00000001/state")=="winning owner"&&writes==1);
 if(mode=="create")assert(session.status()==201&&bytes(root+"/appdata/n00000001/new")=="created");
 if(mode=="scope")assert(session.status()==404&&stats==initialStats&&reads==0&&writes==0&&inventory(root)==before);
 if(mode=="auth")assert(session.status()==401&&stats==initialStats&&reads==0&&writes==0&&inventory(root)==before);
 if(mode=="propfind")assert(session.status()==207&&outgoing.find("404 Not Found")!=std::string::npos&&outgoing.find("/saved/new")==std::string::npos&&inventory(root)==before);
 assert(bytes(root+"/bootfs/app.elf")=="installed"&&bytes(root+"/nvs-private")=="bonds and settings"&&bytes(root+"/appdata/n00000002/private")=="unexported");
 std::printf("WebDAV real AppDataFiles/export: %s PASS\n",mode.c_str());
}
