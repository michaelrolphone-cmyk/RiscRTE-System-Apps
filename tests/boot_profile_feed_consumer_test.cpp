// Compile against the selected boot-profile consumer checkout, without copying
// its implementation. Existing fixture supplies boundary doubles only.
#define INSTALLED_APPS_BOOT_PROFILES 1
#define INSTALLED_APPS_FEED_PRODUCT "test-watch"
#define INSTALLED_APPS_FEED_URL "https://example.invalid/native-packages-v1.json"
#include "installed_apps_fixture.h"
#include <fstream>
#include <iterator>
int main(int argc,char**argv){
 assert(argc==2);std::ifstream in(argv[1],std::ios::binary);assert(in);
 const std::string bytes{std::istreambuf_iterator<char>(in),{}};
 assert(bytes.size()<=FeedBytes);std::memcpy(feedInput,bytes.data(),bytes.size());feedSize=bytes.size();feedInput[feedSize]=0;
 assert(feedParse());assert(feedCount==0);
 JsonDocument d;assert(!deserializeJson(d,bytes));JsonObjectConst core=d["boot_profiles"]["core"];
 assert(bootFeedValid(d["boot_profiles"]) && core["profile"]["files"].size()==160 && core["mirrors"].size()==2);
 risc_provision_request_status_v1 status{};JsonObjectConst native=core["native"];
 std::strcpy(status.layout,native["layout"]);std::strcpy(status.runtime_version,native["runtime_version"]);
 status.store_abi=native["store_abi"];status.firmware_size=native["firmware_size"];
 assert(bootDigest(native["firmware_sha256"],status.firmware_sha256) && bootPair(native,status));
 status.firmware_size++;assert(!bootPair(native,status));
 JsonArrayConst mirrors=core["mirrors"];
 for(JsonVariantConst mirror:mirrors){Download::Record record;assert(bootMirror(mirror,record));}
 puts("Generated feed: existing Installed Apps parser, exact native binding and authorized app/driver mirror descriptors PASS");
}
