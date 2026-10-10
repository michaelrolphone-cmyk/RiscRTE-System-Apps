#pragma once
#include "JsonCursor.h"
#include "Policy.h"
#include "SoftwareUpdateV1.h"
#include "RiscBankStoreV1.h"
#include "T5PackageVersion.h"
#include <cstdio>
namespace WatchUpdate {
constexpr size_t CatalogMax=512u*1024u, ManifestMax=4096u;
struct Slice { const char *data=nullptr;size_t size=0; };
struct Requirement {char name[96]{};uint32_t api=0;};
struct Manifest {char id[65]{},version[32]{},file[80]{},entry[64]{};Requirement requirements[32]{};unsigned count=0;};
struct Row {
 software_update_row_v1 view{};
 char url[512]{},native_id[65]{};uint8_t digest[32]{};
 Slice manifest{};uint32_t storeAbi=0;char layout[40]{};bool pairedCohort=false;
};
struct Catalog { Row rows[SOFTWARE_UPDATE_ROWS_MAX]{};uint32_t count=0;Manifest scratch[2]{};risc_bank_cohort_v1 cohort{}; };
inline bool field(Slice object,const char *name,Slice& out,WorkBudget& b) {
 Cursor c(object.data,object.size,b);if(!c.take('{'))return false;
 if(c.take('}'))return false;
 for(;;){char key[64]{};Slice s;if(!c.key(key,sizeof(key)) || !c.slice(s.data,s.size))return false;
  if(!std::strcmp(key,name)){out=s;return true;}bool more=false;if(!c.next('}',more)||!more)return false;
 }
}
inline bool text(Slice s,char *out,size_t cap,WorkBudget& b) {Cursor c(s.data,s.size,b);return c.text(out,cap)&&c.end();}
inline bool str(Slice s,const char *key,char *out,size_t cap,WorkBudget& b) {Slice v;return field(s,key,v,b)&&text(v,out,cap,b);}
inline bool number(Slice s,const char *key,uint32_t& out,WorkBudget& b) {
 Slice v;uint64_t n=0;if(!field(s,key,v,b))return false;Cursor c(v.data,v.size,b);
 if(!c.integer(n)||!c.end()||n>UINT32_MAX)return false;out=(uint32_t)n;return true;
}
inline bool equals(Slice s,const char *key,const char *expected,WorkBudget& b) {char t[160]{};return str(s,key,t,sizeof(t),b)&&!std::strcmp(t,expected);}
inline bool version(const char *s) {
 uint32_t p[3];if(!t5_package_version_parts(s,p))return false;
 for(unsigned i=0;i<3;++i){if(*s=='0'&&s[1]&&s[1]!='.')return false;while(*s&&*s!='.')++s;if(*s)++s;}return true;
}
inline bool safeId(const char *s) {
 size_t n=std::strlen(s);if(!n||n>64)return false;
 for(size_t i=0;i<n;++i){char c=s[i];bool a=(c>='a'&&c<='z')||(c>='0'&&c<='9');
  if(!a&&(!i||(c!='-'&&c!='_'&&c!='.')))return false;if(c=='.'&&i&&s[i-1]=='.')return false;}return true;
}
inline bool revision(const char *s) {
 if(std::strlen(s)!=40)return false;
 for(unsigned i=0;i<40;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;
}
inline bool sha(Slice s,const char *key,uint8_t out[32],WorkBudget& b) {
 char t[65]{};if(!str(s,key,t,sizeof(t),b)||std::strlen(t)!=64)return false;
 for(unsigned i=0;i<64;++i){char c=t[i];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:16;
  if(d==16)return false;if(!(i&1))out[i/2]=(uint8_t)(d<<4);else out[i/2]|=(uint8_t)d;}return true;
}
inline bool source(Slice s,WorkBudget& b) {Slice v;return !field(s,"source_repo",v,b)||equals(s,"source_repo",Repository,b);}
inline bool payload(Slice s,const char *tag,const char *asset,Row& out,WorkBudget& b) {
 char url[512];std::snprintf(url,sizeof(url),"https://github.com/%s/releases/download/%s/%s",Repository,tag,asset);
 return equals(s,"asset",asset,b)&&str(s,"url",out.url,sizeof(out.url),b)&&!std::strcmp(out.url,url)&&
  number(s,"size",out.view.size,b)&&out.view.size&&sha(s,"sha256",out.digest,b)&&source(s,b);
}
inline bool manifest(Slice s,const char *id,Manifest& out,WorkBudget& b) {
 out=Manifest{};if(!s.data||s.size>ManifestMax)return false;Cursor valid(s.data,s.size,b);if(!valid.objectOnly())return false;
 if(!equals(s,"type","application",b)||!equals(s,"architecture","xtensa-esp32s3",b)||
 !str(s,"version",out.version,sizeof(out.version),b)||!version(out.version)||
 !str(s,"file_name",out.file,sizeof(out.file),b)||!str(s,"entry",out.entry,sizeof(out.entry),b))return false;
 std::snprintf(out.id,sizeof(out.id),"%s",id);Slice optional;if(field(s,"id",optional,b)&&!equals(s,"id",id,b))return false;
 const char *fileId=(!std::strcmp(id,"twatch-clock")|| (RequireProduct&&!std::strcmp(id,"paper_clock")))?"default":!std::strcmp(id,"twatch-clock-return")?"clock":id;
 char file[80];std::snprintf(file,sizeof(file),"%s.elf",fileId);if(std::strcmp(file,out.file)||std::strcmp(out.entry,"app_main"))return false;
 Slice req;if(!field(s,"requires",req,b))return false;Cursor c(req.data,req.size,b);if(!c.take('['))return false;if(c.take(']'))return c.end();
 for(;;){Slice row;if(out.count==32||!c.slice(row.data,row.size))return false;Requirement& r=out.requirements[out.count];
  if(!str(row,"capability",r.name,sizeof(r.name),b)||!number(row,"api",r.api,b)||!r.api)return false;
  for(unsigned i=0;i<out.count;++i)if(out.requirements[i].api==r.api&&!std::strcmp(out.requirements[i].name,r.name))return false;
  ++out.count;bool more=false;if(!c.next(']',more))return false;if(!more)return c.end();}
}
inline bool authority(const Manifest& a,const Manifest& b) {
 if(a.count!=b.count||std::strcmp(a.id,b.id)||std::strcmp(a.file,b.file)||std::strcmp(a.entry,b.entry))return false;
 for(unsigned i=0;i<a.count;++i){bool found=false;for(unsigned j=0;j<b.count;++j)if(a.requirements[i].api==b.requirements[j].api&&!std::strcmp(a.requirements[i].name,b.requirements[j].name))found=true;if(!found)return false;}return true;
}
inline bool appRecord(Slice s,Row& out,Manifest& scratch,WorkBudget& b) {
 out=Row{};out.view.struct_size=sizeof(out.view);out.view.availability=SOFTWARE_UPDATE_UNSUPPORTED;
 if(!str(s,"id",out.view.id,sizeof(out.view.id),b)||!safeId(out.view.id)||!str(s,"version",out.view.version,sizeof(out.view.version),b)||!version(out.view.version))return false;
 char tag[128],asset[80];std::snprintf(tag,sizeof(tag),"app-%s-v%s",out.view.id,out.view.version);std::snprintf(asset,sizeof(asset),"%s.elf",out.view.id);
 Slice kind;if(field(s,"kind",kind,b)&&!equals(s,"kind","app",b))return false;
 if(!equals(s,"tag",tag,b)||!payload(s,tag,asset,out,b)||out.view.size>2097152u||!field(s,"manifest",out.manifest,b))return false;
 if(!str(out.manifest,"id",out.native_id,sizeof(out.native_id),b)||!safeId(out.native_id))return false;
 bool identity=!std::strcmp(out.native_id,out.view.id)||(!std::strcmp(out.view.id,"default")&&(!std::strcmp(out.native_id,"twatch-clock")||(RequireProduct&&!std::strcmp(out.native_id,"paper_clock"))))||(!std::strcmp(out.view.id,"clock")&&!std::strcmp(out.native_id,"twatch-clock-return"));
 if(!identity)return false;
 if(!manifest(out.manifest,out.native_id,scratch,b)||std::strcmp(scratch.version,out.view.version))return false;
 std::snprintf(out.view.reason,sizeof(out.view.reason),"Not an installed authorized application");return true;
}
inline bool firmwareRecord(Slice s,Row& out,risc_bank_cohort_v1& cohort,WorkBudget& b) {
 out=Row{};out.view.struct_size=sizeof(out.view);std::strcpy(out.view.id,"Runtime");
 if(!str(s,"version",out.view.version,sizeof(out.view.version),b)||!version(out.view.version))return false;
 char tag[96],asset[128];std::snprintf(tag,sizeof(tag),"firmware-v%s",out.view.version);std::snprintf(asset,sizeof(asset),"%s-launcher-%s.bin",AssetPrefix,out.view.version);
 Slice kind;if(field(s,"kind",kind,b)&&!equals(s,"kind","firmware",b))return false;
 if(!equals(s,"tag",tag,b)||!payload(s,tag,asset,out,b))return false;
 Slice ota;if(!field(s,"ota",ota,b)){out.view.availability=SOFTWARE_UPDATE_USB_ONLY;out.url[0]=0;std::strcpy(out.view.reason,"USB install only; merged image is not OTA");return true;}
 if(!str(ota,"layout",out.layout,sizeof(out.layout),b)||!number(ota,"store_abi",out.storeAbi,b))return false;
 if(equals(ota,"kind","paired-cohort",b)){
  cohort={};cohort.struct_size=sizeof(cohort);cohort.store_abi=out.storeAbi;
  if(!out.storeAbi||!str(ota,"product",cohort.product,sizeof(cohort.product),b)||std::strcmp(cohort.product,Product)||
     !str(ota,"version",cohort.version,sizeof(cohort.version),b)||std::strcmp(cohort.version,out.view.version)||
     !str(ota,"runtime_version",cohort.runtime_version,sizeof(cohort.runtime_version),b)||!version(cohort.runtime_version)||
     !str(ota,"source_repo",cohort.source_repo,sizeof(cohort.source_repo),b)||std::strcmp(cohort.source_repo,Repository)||
     !str(ota,"source_revision",cohort.source_revision,sizeof(cohort.source_revision),b)||!revision(cohort.source_revision)||
     !number(ota,"firmware_size",cohort.firmware_size,b)||cohort.firmware_size<32||
     !number(ota,"store_size",cohort.store_size,b)||!cohort.store_size||
     cohort.firmware_size>8u*1024u*1024u||cohort.store_size>8u*1024u*1024u||
     !sha(ota,"firmware_sha256",cohort.firmware_sha256,b)||!sha(ota,"store_sha256",cohort.store_sha256,b))return false;
  std::snprintf(asset,sizeof(asset),"%s-cohort-%s.bin",AssetPrefix,out.view.version);
  if(!payload(ota,tag,asset,out,b)||out.view.size>8u*1024u*1024u||out.view.size!=cohort.firmware_size+cohort.store_size)return false;
  std::memcpy(cohort.sha256,out.digest,sizeof(cohort.sha256));out.pairedCohort=true;std::strcpy(out.view.id,Product);
  std::strcpy(out.view.reason,"Paired cohort compatibility not established");
 }else{
  if(!AllowRuntimeOnly||!equals(ota,"kind","runtime-image",b)||!str(ota,"runtime_version",out.view.version,sizeof(out.view.version),b)||!version(out.view.version))return false;
  std::snprintf(asset,sizeof(asset),"riscrte-runtime-%s.bin",out.view.version);
  if(!payload(ota,tag,asset,out,b)||out.view.size>8u*1024u*1024u)return false;
  std::strcpy(out.view.reason,"Runtime compatibility not established");
 }
 out.view.availability=SOFTWARE_UPDATE_UNSUPPORTED;return true;
}
inline bool parseImpl(Slice json,bool firmware,Catalog& out,bool (*yield)(void*),void *ctx) {
 out.count=0;out.cohort={};if(!json.data||json.size<2||json.size>CatalogMax)return false;WorkBudget b(yield,ctx);Cursor valid(json.data,json.size,b);
 if(!b.poll(true)||!valid.objectOnly())return false;uint32_t schema=0;if(!number(json,"schema",schema,b)||schema!=1)return false;
 if(RequireProduct&&(!equals(json,"product",Product,b)||!equals(json,"source_repo",Repository,b)))return false;
 Slice section;if(!field(json,firmware?"firmware":"apps",section,b))return false;Cursor c(section.data,section.size,b);
 if(firmware){if(c.nullValue()&&c.end())return true;if(!firmwareRecord(section,out.rows[0],out.cohort,b))return false;out.count=1;return b.poll(true);}
 if(!c.take('['))return false;if(c.take(']'))return c.end();
 for(;;){Slice s;if(out.count==SOFTWARE_UPDATE_ROWS_MAX||!c.slice(s.data,s.size)||!appRecord(s,out.rows[out.count],out.scratch[0],b)) {out.count=0;return false;}
  for(unsigned i=0;i<out.count;++i)if((!std::strcmp(out.rows[i].view.id,out.rows[out.count].view.id)||!std::strcmp(out.rows[i].native_id,out.rows[out.count].native_id))){out.count=0;return false;}
  ++out.count;bool more=false;if(!c.next(']',more)){out.count=0;return false;}if(!more)return c.end()&&b.poll(true);}
}
inline bool parse(Slice json,bool firmware,Catalog& out,bool (*yield)(void*),void *ctx) {
 bool ok=parseImpl(json,firmware,out,yield,ctx);if(!ok)out.count=0;return ok;
}
}
