#include "../../Services/update/Catalog.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include "fixtures/paired_cohort_fixture.h"
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
 // Paired payloads are native firmware followed by one complete bootfs. The
 // outer USB record is never selected; all three SHA expectations pass intact.
 auto parse_cohort=[&](const std::string& text){return WatchUpdate::parse({text.data(),text.size()},true,*c,cooperate,nullptr);};
 const std::string cohort=cohortFixture();
 assert(parse_cohort(cohort)&&c->count==1&&c->rows[0].pairedCohort);
 assert(!strcmp(c->rows[0].view.version,"1.2.0")&&c->rows[0].view.size==513+0x4f0000);
 assert(!strcmp(c->cohort.product,"twatch-s3")&&!strcmp(c->cohort.runtime_version,"0.1.33")&&c->cohort.store_abi==1);
 for(unsigned i=0;i<32;++i)assert(c->cohort.sha256[i]==0xaa&&c->cohort.firmware_sha256[i]==0xbb&&c->cohort.store_sha256[i]==0xdd);
 assert(parse_cohort(cohortFixture(2,0x260000,0x510000))&&c->cohort.store_abi==2);
 for(const char *key:{"kind","product","version","runtime_version","source_repo","source_revision","layout","store_abi","asset","url","size","sha256","firmware_size","firmware_sha256","store_size","store_sha256"}){
  std::string bad=cohort;size_t start=bad.find(std::string("\"")+key+"\":",bad.find("\"ota\""));assert(start!=std::string::npos);
  size_t end=bad.find(',',start);if(end!=std::string::npos)bad.erase(start,end-start+1);else {size_t close=bad.find('}',start);bad.erase(start-1,close-start+1);}
  assert(!parse_cohort(bad)&&!c->count);
 }
 const std::pair<std::string,std::string> mutations[]={
  {"paired-cohort","merged-image"},{"\"product\":\"twatch-s3\"","\"product\":\"other-watch\""},
  {"\"source_repo\":\"michaelrolphone-cmyk/RiscRTE-T-Watch-S3\"","\"source_repo\":\"another-owner/RiscRTE-T-Watch-S3\""},
  {std::string(40,'c'),std::string(39,'c')},{std::string(40,'c'),std::string(40,'C')},
  {std::string(40,'c'),std::string(39,'c')+"g"},
  {"\"product\":\"twatch-s3\",\"version\":\"1.2.0\"","\"product\":\"twatch-s3\",\"version\":\"1.2.1\""},
  {"0.1.33","00.1.33"},{"0.1.33","0.1.33-dev"},{"riscrte-paired-16m-v1",std::string(40,'l')},
  {"\"store_abi\":1","\"store_abi\":0"},{"\"store_abi\":1","\"store_abi\":4294967296"},
  {"twatch-s3-cohort-1.2.0.bin","twatch-s3-launcher-1.2.0.bin"},
  {"firmware-v1.2.0/twatch-s3-cohort","firmware-v1.2.1/twatch-s3-cohort"},
  {"\"firmware_size\":513","\"firmware_size\":0"},{"\"firmware_size\":513","\"firmware_size\":31"},{"\"firmware_size\":513","\"firmware_size\":514"},
  {"\"firmware_size\":513","\"firmware_size\":4294967295"},
  {"\"store_size\":5177344","\"store_size\":0"},{"\"size\":5177857","\"size\":5177858"},
  {std::string(64,'a'),std::string(64,'A')},{std::string(64,'b'),std::string(63,'b')},
  {std::string(64,'d'),std::string(63,'d')+"g"},
  {"\"firmware_size\":513","\"firmware_size\":513,\"firmware_size\":513"},
  {"\"product\":\"twatch-s3\"","\"product\":\"twatch-\\u00733\""}
 };
 for(const auto& mutation:mutations){std::string bad=cohort;cohortReplace(bad,mutation.first,mutation.second);assert(!parse_cohort(bad)&&!c->count);}
 assert(!parse_cohort(cohortFixture(1,0x800000,0x800000))&&!c->count);
 for(size_t n=0;n<cohort.size();n+=11)assert(!WatchUpdate::parse({cohort.data(),n},true,*c,cooperate,nullptr));
 delete c;puts("Pinned Watch catalog, actual Spectrum API pairs, authority, paired cohort metadata, truncation, duplicates and cancellation passed");
}
