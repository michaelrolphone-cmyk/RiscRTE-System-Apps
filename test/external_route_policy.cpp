// Host-only harness of unchanged production Runtime preserving policy.
// Inventory is generated from independently decoded, hash-verified stores.
#include "runtime/provisioning/PreservingPolicy.h"
#include "runtime/provisioning/Profile.h"
#include "bootstrap/Json.h"
#include "RuntimeFileFixture.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>

static bool profile(const char* path,RiscProvision::Profile& out) {
  std::ifstream stream(path);std::string raw((std::istreambuf_iterator<char>(stream)),{});
  JsonDocument doc;if(deserializeJson(doc,raw))return false;
  const auto rows=doc["files"].as<JsonArrayConst>();
  if(rows.isNull() || !rows.size() || rows.size()>RiscProvision::MaxFiles)return false;
  for(JsonVariantConst row:rows) {
    auto& file=out.files[out.count++];const char* name=row["path"],*hash=row["sha256"];
    if(!name || !hash || strlen(name)>=sizeof(file.path) || strlen(hash)!=64 ||
       !row["bytes"].is<uint32_t>() || !row["bytes"].as<uint32_t>() || row["bytes"].as<uint32_t>()>RiscProvision::MaxFileBytes)return false;
    strcpy(file.path,name);file.bytes=row["bytes"].as<uint32_t>();
    for(size_t i=0;i<64;++i) {
      const char c=hash[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
      const uint8_t n=uint8_t(c<='9'?c-'0':c-'a'+10);
      if(i%2)file.sha256[i/2]|=n;else file.sha256[i/2]=uint8_t(n<<4);
    }
  }
  return true;
}

int main(int argc,char** argv) {
  if(argc!=6)return 2;
  auto p=std::make_unique<RiscProvision::Profile>();if(!profile(argv[3],*p))return 3;
  RiscTest::FileSources files({std::string(argv[1]),std::string(argv[2])});
  bool retained=false;
  const bool policy=RiscProvision::preservingPolicy(files.sources(),argv[1],argv[2],retained);
  const bool images=RiscProvision::preservingImageSources(files.sources(),argv[2],*p,retained);
  std::cout<<"{\"policy\":"<<(policy?"true":"false")<<",\"image_sources\":"<<(images?"true":"false")
           <<",\"retained\":"<<(retained?"true":"false")<<",\"files\":"<<p->count<<"}\n";
  return retained || policy!=(std::string(argv[4])=="1") || images!=(std::string(argv[5])=="1");
}
