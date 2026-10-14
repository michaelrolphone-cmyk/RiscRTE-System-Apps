// Generated feed -> actual Runtime profile parser. Nonexecutable byte fixtures
// only; this does not claim board/graph/ELF admission or hardware qualification.
#include "bootstrap/Json.h"
#include "runtime/provisioning/Profile.h"
#include "runtime/provisioning/TimeInput.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
static std::string encode(JsonVariantConst value){std::string out;serializeJson(value,out);return out;}
int main(int argc,char**argv){
 assert(argc==2);std::ifstream in(argv[1],std::ios::binary);assert(in);
 const std::string bytes{std::istreambuf_iterator<char>(in),{}};
 JsonDocument feed;assert(RiscBoot::parse(bytes.data(),bytes.size(),feed,160));
 assert(feed["schema"]=="riscrte.native-packages" && feed["version"]==1);
 assert(feed["apps"].size()==0 && feed["profiles"]["core"].size()==0);
 JsonObject core=feed["boot_profiles"]["core"];JsonObject json=core["profile"];
 assert(json.size()==4 && json["schema_version"]==4 && json["wifi"].isNull());
 assert(measureJson(json)<=15872 && json["files"].size()==160);
 auto profile=std::make_unique<RiscProvision::Profile>();
 auto text=encode(json);assert(!RiscProvision::parseProfile(text.data(),text.size(),*profile));
 // Maximum lengths and escaped characters: 32 backslash-byte SSID and 63
 // printable quote/backslash password bytes. Public output remains untouched.
 const std::string ssid(32,'\\'), password(63,'"');
 json["wifi"]["ssid"]=ssid;json["wifi"]["password"]=password;
 text=encode(json);assert(text.size()<=16384);
 assert(RiscProvision::parseProfile(text.data(),text.size(),*profile));
 assert(profile->count==160 && profile->imageMode() && profile->downloads()==1);
 assert(profile->image.bytes==0x550000 && std::string(profile->ssid)==ssid && std::string(profile->password)==password);
 for(size_t i=0;i<160;++i){auto row=json["files"][i];assert(row.size()==3);
  assert(std::string(profile->files[i].path)==row[0].as<const char*>() && profile->files[i].bytes==row[1].as<uint32_t>());
  const char* digest=row[2];const char hex[]="0123456789abcdef";
  for(size_t n=0;n<32;++n){assert(digest[n*2]==hex[profile->files[i].sha256[n]>>4]);assert(digest[n*2+1]==hex[profile->files[i].sha256[n]&15]);}}
 JsonObjectConst time=core["time"];assert(time.size()==3 && time["schema"]=="riscrte.sntp" && time["schema_version"]==1);
 assert(measureJson(time)<=384 && time["servers"].size()>=1 && time["servers"].size()<=3);
 JsonArrayConst servers=time["servers"];
 for(JsonVariantConst server:servers)assert(RiscProvision::timeServer(server.as<const char*>()));
 puts("Generated 160-file feed: actual native schema-4 parser, full inventory, explicit time and maximum escaped Wi-Fi injection PASS");
}
