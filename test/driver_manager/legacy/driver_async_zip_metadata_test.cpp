#include "DriverAsyncZipMetadata.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
using DriverManager::AsyncZipMetadata;
using namespace RuntimePackages;
using State=AsyncZipMetadata::State;
using Request=AsyncZipMetadata::Request;
static std::vector<uint8_t> archive(const std::string& json) {
 uint8_t payload[52]{};
 RteZipSourceFile files[]={{kOrdinaryManifestName,reinterpret_cast<const uint8_t*>(json.data()),uint32_t(json.size())},{"driver.elf",payload,sizeof(payload)}};
 std::vector<uint8_t> bytes;
 auto write=[&](const uint8_t*p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;};
 uint32_t written=0;
 assert(packRteZip(files,2,write,&written)==RteZipResult::Ready);
 assert(written==bytes.size());return bytes;
}
static std::string manifest(){
 return std::string("{\"schema\":1,\"kind\":\"driver\",\"id\":\"sample-module\",\"version\":\"1.2.3\",\"artifact\":\"driver.elf\",\"architecture\":\"xtensa-esp32s3\",\"min_runtime_api\":2,\"entries\":[{\"name\":\"driver.elf\",\"size_bytes\":52,\"sha256\":\"")+
 std::string(64,'a')+"\",\"executable\":true}],\"requires\":[]}";
}
static State complete(AsyncZipMetadata&m,const std::vector<uint8_t>&bytes){
 unsigned requests=0;
 for(;;){
  Request q{};State state=m.next(&q);
  if(state!=State::NeedRead)return state;
  assert(++requests<=70);
  assert(q.size&&q.size<=4096&&q.offset<=bytes.size()&&q.size<=bytes.size()-q.offset);
  // Asking for progress must never submit another worker read or consume bytes.
  for(unsigned i=0;i<1000;++i){Request same{};assert(m.next(&same)==State::NeedRead);assert(same.offset==q.offset&&same.size==q.size&&same.attempt==q.attempt);}
  Request wrong=q;++wrong.offset;
  assert(!m.accept(wrong,bytes.data()+q.offset,q.size,true));
  assert(m.state()==State::NeedRead);
  std::vector<uint8_t> copied(bytes.begin()+q.offset,bytes.begin()+q.offset+q.size);
  assert(m.accept(q,copied.data(),copied.size(),true));
  // A replayed completion before the next request is rejected.
  assert(!m.accept(q,copied.data(),copied.size(),true));
  std::memset(copied.data(),0,copied.size()); // accepted bytes must be owned copies
 }
}
int main(){
 AsyncZipMetadata m;
 assert(m.state()==State::Idle);
 Request q{};
 assert(m.next(&q)==State::Idle);
 auto bytes=archive(manifest());const auto original=bytes;
 assert(m.begin(bytes.size()));assert(m.next(&q)==State::NeedRead);
 const Request old=q;
 m.reset();assert(m.begin(bytes.size()));assert(m.next(&q)==State::NeedRead);
 assert(old.offset==q.offset&&old.size==q.size&&old.attempt!=q.attempt);
 assert(!m.accept(old,bytes.data()+old.offset,old.size,true));
 assert(m.state()==State::NeedRead);
 assert(m.accept(q,bytes.data()+q.offset,q.size,true));
 assert(m.begin(bytes.size()));assert(complete(m,bytes)==State::MetadataReady);
 assert(m.policyResult()==RteZipResult::Ready);
 assert(m.zip().entryCount==2&&m.plan().entryCount==1);
 assert(m.plan().identity.kind==Kind::Driver);
 assert(!std::strcmp(m.plan().identity.id,"sample-module"));
 assert(bytes==original);
 assert(!m.accept(q,bytes.data(),1,true));
 assert(m.next(nullptr)==State::MetadataReady);
 m.reset();assert(m.state()==State::Idle);
 assert(m.next(nullptr)==State::Idle);
 assert(!m.begin(0));assert(m.state()==State::Invalid);
 assert(!m.begin(kRteZipEocdBytes-1));assert(m.state()==State::Invalid);
 assert(!m.begin(kRteZipMaxTotalBytes+65536u+1u));assert(m.state()==State::Invalid);
 // A failed or truncated copied read terminates metadata inspection.
 assert(m.begin(bytes.size()));assert(m.next(&q)==State::NeedRead);
 assert(!m.accept(q,bytes.data()+q.offset,q.size,false));assert(m.state()==State::Invalid);
 assert(m.begin(bytes.size()));assert(m.next(&q)==State::NeedRead);
 assert(!m.accept(q,bytes.data()+q.offset,q.size-1,true));assert(m.state()==State::Invalid);
 assert(m.begin(bytes.size()));assert(m.next(&q)==State::NeedRead);
 assert(!m.accept(q,nullptr,q.size,true));assert(m.state()==State::Invalid);
 auto bad=bytes;bad[bad.size()-kRteZipEocdBytes]=0;
 assert(m.begin(bad.size()));assert(complete(m,bad)==State::Invalid);
 assert(m.policyResult()==RteZipResult::InvalidHeader);
 auto malformed=archive("{bad}");
 assert(m.begin(malformed.size()));assert(complete(m,malformed)==State::Invalid);
 // Shared Runtime JSON guard allows larger documents; ordinary manifests do not.
 auto oversized=archive(manifest()+std::string(4097,' '));
 assert(m.begin(oversized.size()));assert(complete(m,oversized)==State::Invalid);
 // MetadataReady intentionally does NOT establish payload CRC/SHA or ELF validity.
 // The fixture's SHA is synthetic and payload is not an ELF; metadata still parses.
 assert(m.begin(bytes.size()));assert(complete(m,bytes)==State::MetadataReady);
 auto corrupt=bytes;corrupt[m.zip().entries[1].dataOffset]^=0x80;
 assert(m.begin(corrupt.size()));assert(complete(m,corrupt)==State::MetadataReady);
 return 0;
}
