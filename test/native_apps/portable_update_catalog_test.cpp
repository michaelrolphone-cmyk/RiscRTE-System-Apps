#include "../../Services/update/Catalog.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
static unsigned work;
static bool cooperate(void*){++work;return true;}
static bool cancel(void*){return false;}
int main(int argc,char**argv){assert(argc==3);std::ifstream f(argv[1]);assert(f);std::stringstream b;b<<f.rdbuf();std::string json=b.str();auto *c=new WatchUpdate::Catalog;
 assert(WatchUpdate::parse({json.data(),json.size()},false,*c,cooperate,nullptr)&&c->count==7&&work>20);
 assert(!strcmp(c->rows[2].native_id,"twatch-clock-return"));assert(!strcmp(c->rows[3].native_id,"twatch-clock"));
 assert(WatchUpdate::parse({json.data(),json.size()},true,*c,cooperate,nullptr)&&c->count==1&&c->rows[0].view.availability==SOFTWARE_UPDATE_USB_ONLY&&!c->rows[0].url[0]);
 assert(!WatchUpdate::parse({json.data(),json.size()},false,*c,cancel,nullptr)&&!c->count);
 for(size_t n=0;n<json.size();n+=31)assert(!WatchUpdate::parse({json.data(),n},false,*c,cooperate,nullptr));
 for(const char*bad:{"{\"schema\":1,\"schema\":1,\"apps\":[]}","{\"schema\":1,\"apps\":[],\"metadata\":{\"x\":1,\"x\":2}}","{\"schema\":01,\"apps\":[]}","{\"schema\":1,\"apps\":[]}x"})assert(!WatchUpdate::parse({bad,strlen(bad)},false,*c,cooperate,nullptr));
 assert(!WatchUpdate::version("01.0.0")&&!WatchUpdate::version("1.2.4294967296")&&!WatchUpdate::version("1.0.1-dev"));
 std::string changed=json;size_t at=changed.find("twatch-clock-return");assert(at!=std::string::npos);changed.replace(at,19,"unauthorized-alias");assert(!WatchUpdate::parse({changed.data(),changed.size()},false,*c,cooperate,nullptr));
 std::ifstream spectrum_file(argv[2]);assert(spectrum_file);std::stringstream spectrum_stream;spectrum_stream<<spectrum_file.rdbuf();
 std::string installed=spectrum_stream.str(),updated=installed;size_t version_at=updated.find("0.4.0");assert(version_at!=std::string::npos);updated.replace(version_at,5,"0.4.2");
 auto parse_manifest=[](const std::string& text,WatchUpdate::Manifest& result){WatchUpdate::WorkBudget budget(cooperate,nullptr);return WatchUpdate::manifest({text.data(),text.size()},"audio_spectrum",result,budget);};
 WatchUpdate::Manifest before,after,rejected;
 assert(parse_manifest(installed,before)&&parse_manifest(updated,after)&&before.count==12&&after.count==12);
 assert(WatchUpdate::authority(before,after));
 unsigned key_value_apis=0;for(unsigned i=0;i<after.count;++i)if(!strcmp(after.requirements[i].name,"storage.key-value"))key_value_apis|=1u<<after.requirements[i].api;
 assert(key_value_apis==((1u<<1)|(1u<<2)));
 std::string duplicate=updated;duplicate.insert(duplicate.rfind(']'),",{\"capability\":\"storage.key-value\",\"api\":1}");assert(!parse_manifest(duplicate,rejected));
 auto authority_changed=after;for(unsigned i=0;i<authority_changed.count;++i)if(!strcmp(authority_changed.requirements[i].name,"storage.key-value")&&authority_changed.requirements[i].api==1)authority_changed.requirements[i].api=3;
 assert(!WatchUpdate::authority(before,authority_changed));
 auto missing=after;--missing.count;assert(!WatchUpdate::authority(before,missing));
 delete c;puts("Pinned Watch catalog, actual Spectrum API pairs, authority, truncation, duplicates and cancellation passed");
}
