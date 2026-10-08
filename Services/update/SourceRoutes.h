#pragma once
#include "Catalog.h"
/* Optional source-bound selection. This does not grant update authority: native
 * staging still validates the complete target against the active store. */
namespace WatchUpdate { namespace Routes {
constexpr unsigned Maximum=16;
struct Source {
 char product[65]{},version[32]{},source_repo[128]{},source_revision[41]{};
 char runtime_version[32]{},layout[40]{};uint32_t store_abi=0;uint8_t active_store_sha256[32]{};
};
inline bool same(const Source& a,const Source& b) {
 return a.store_abi==b.store_abi&&!std::memcmp(a.active_store_sha256,b.active_store_sha256,32)&&!std::strcmp(a.product,b.product)&&!std::strcmp(a.version,b.version)&&
  !std::strcmp(a.source_repo,b.source_repo)&&!std::strcmp(a.source_revision,b.source_revision)&&
  !std::strcmp(a.runtime_version,b.runtime_version)&&!std::strcmp(a.layout,b.layout);
}
inline bool valid(const Source& s) {
 bool digest=false;for(auto value:s.active_store_sha256)digest=digest||value;
 return digest&&safeId(s.product)&&!std::strcmp(s.source_repo,Repository)&&version(s.version)&&
  version(s.runtime_version)&&revision(s.source_revision)&&s.layout[0]&&s.store_abi;
}
inline bool exactFields(Slice s,const char* const* names,unsigned count,WorkBudget& b) {
 Cursor c(s.data,s.size,b);if(!c.take('{')||c.take('}'))return false;uint32_t found=0;
 for(;;){char key[64];Slice ignored;if(!c.key(key,sizeof(key))||!c.slice(ignored.data,ignored.size))return false;
  unsigned i=0;for(;i<count;++i)if(!std::strcmp(key,names[i]))break;
  if(i==count||(found&(1u<<i)))return false;found|=1u<<i;
  bool more=false;if(!c.next('}',more))return false;if(!more)return found==((1u<<count)-1)&&c.end();
 }
}
inline bool source(Slice json,Source& s,WorkBudget& b) {
 s={};static const char* const names[]={"product","version","source_repo","source_revision","runtime_version","layout","store_abi","active_store_sha256"};
 return exactFields(json,names,8,b)&&str(json,"product",s.product,sizeof(s.product),b)&&
  str(json,"version",s.version,sizeof(s.version),b)&&str(json,"source_repo",s.source_repo,sizeof(s.source_repo),b)&&
  str(json,"source_revision",s.source_revision,sizeof(s.source_revision),b)&&str(json,"runtime_version",s.runtime_version,sizeof(s.runtime_version),b)&&
  str(json,"layout",s.layout,sizeof(s.layout),b)&&number(json,"store_abi",s.store_abi,b)&&
  sha(json,"active_store_sha256",s.active_store_sha256,b)&&valid(s);
}
inline bool present(Slice json,bool (*yield)(void*),void* ctx) {
 WorkBudget b(yield,ctx);Slice ignored;return field(json,"firmware_routes",ignored,b);
}
inline bool select(Slice json,Catalog& out,const Source& have,Source& selected,bool& routed,
                   bool (*yield)(void*),void* ctx) {
 routed=false;selected={};
 /* Validate the complete JSON, product policy and legacy fallback first. */
 if(!parse(json,true,out,yield,ctx))return false;
 WorkBudget b(yield,ctx);Slice routes;if(!field(json,"firmware_routes",routes,b))return b.poll(true);
 out.count=0;out.cohort={};
 if(!valid(have))return false;
 Cursor c(routes.data,routes.size,b);if(!c.take('['))return false;if(c.take(']'))return c.end()&&b.poll(true);
 unsigned count=0;
 for(;;){Slice route,from,record;Source candidate{};risc_bank_cohort_v1 cohort{};
  static const char* const names[]={"from","firmware"};
  if(++count>Maximum||!c.slice(route.data,route.size)||!exactFields(route,names,2,b)||
     !field(route,"from",from,b)||!source(from,candidate,b)||!field(route,"firmware",record,b)||
     !firmwareRecord(record,out.rows[1],cohort,b)||!out.rows[1].pairedCohort||
     std::strcmp(candidate.product,cohort.product)||std::strcmp(candidate.layout,out.rows[1].layout)||candidate.store_abi!=out.rows[1].storeAbi||
     t5_package_version_compare(cohort.version,candidate.version)!=1||
     t5_package_version_compare(cohort.runtime_version,candidate.runtime_version)<0)return false;
  if(same(candidate,have)){
   if(routed)return false;out.rows[0]=out.rows[1];out.cohort=cohort;selected=candidate;routed=true;
  }
  bool more=false;if(!c.next(']',more))return false;
  if(!more){if(!c.end()||!b.poll(true))return false;out.count=routed?1:0;return true;}
 }
}
}}
