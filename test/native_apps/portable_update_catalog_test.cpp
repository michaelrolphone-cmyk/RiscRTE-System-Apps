#include "../../Services/update/Catalog.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
static unsigned work;
static bool cooperate(void*){++work;return true;}
static bool cancel(void*){return false;}
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1]);assert(f);std::stringstream b;b<<f.rdbuf();std::string json=b.str();auto *c=new WatchUpdate::Catalog;
 assert(WatchUpdate::parse({json.data(),json.size()},false,*c,cooperate,nullptr)&&c->count==7&&work>20);
 assert(!strcmp(c->rows[2].native_id,"twatch-clock-return"));assert(!strcmp(c->rows[3].native_id,"twatch-clock"));
 assert(WatchUpdate::parse({json.data(),json.size()},true,*c,cooperate,nullptr)&&c->count==1&&c->rows[0].view.availability==SOFTWARE_UPDATE_USB_ONLY&&!c->rows[0].url[0]);
 assert(!WatchUpdate::parse({json.data(),json.size()},false,*c,cancel,nullptr)&&!c->count);
 for(size_t n=0;n<json.size();n+=31)assert(!WatchUpdate::parse({json.data(),n},false,*c,cooperate,nullptr));
 for(const char*bad:{"{\"schema\":1,\"schema\":1,\"apps\":[]}","{\"schema\":1,\"apps\":[],\"metadata\":{\"x\":1,\"x\":2}}","{\"schema\":01,\"apps\":[]}","{\"schema\":1,\"apps\":[]}x"})assert(!WatchUpdate::parse({bad,strlen(bad)},false,*c,cooperate,nullptr));
 assert(!WatchUpdate::version("01.0.0")&&!WatchUpdate::version("1.2.4294967296")&&!WatchUpdate::version("1.0.1-dev"));
 std::string changed=json;size_t at=changed.find("twatch-clock-return");assert(at!=std::string::npos);changed.replace(at,19,"unauthorized-alias");assert(!WatchUpdate::parse({changed.data(),changed.size()},false,*c,cooperate,nullptr));
 delete c;puts("Pinned Watch catalog, identity mapping, truncation, duplicates and cancellation passed");
}
