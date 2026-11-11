#pragma once
#include "Core.h"
namespace Infra {
// This is staged, public routing configuration only. The native provider owns
// its identity, private key, pairing credential and execution journal in NVS.
inline bool biomeSetupText(JsonVariantConst v,size_t maximum,bool empty=false){
 if(!v.is<const char*>())return false;const char* p=v.as<const char*>();
 if(strlen(p)>maximum||(!empty&&!*p))return false;
 for(;*p;++p)if(static_cast<unsigned char>(*p)<=32||static_cast<unsigned char>(*p)>=127)return false;
 return true;
}
inline bool biomeSetupId(JsonVariantConst v,bool empty=false){
 if(!biomeSetupText(v,128,empty))return false;
 for(const char* p=v.as<const char*>();*p;++p)
  if(!(*p>='A'&&*p<='Z')&&!(*p>='a'&&*p<='z')&&!(*p>='0'&&*p<='9')&&*p!='_'&&*p!='.'&&*p!=':'&&*p!='-')return false;
 return true;
}
inline bool biomeSetup(JsonVariantConst value){
 auto v=value.as<JsonObjectConst>();
 if(!keys(v,{"url"},{"site","realm","resource"})||measureJson(value)>2048||!biomeSetupText(v["url"],768))return false;
 const char* url=v["url"];if(strncmp(url,"https://",8))return false;
 const char* path=strchr(url+8,'/');
 if(!path||path==url+8||strcmp(path,"/api/provider/call"))return false;
 for(const char* p=url+8;p<path;++p)if(*p=='@'||*p=='?'||*p=='#'||*p=='\\')return false;
 if(v.containsKey("realm")&&v["realm"]!="site")return false;
 if(v.containsKey("resource")&&!biomeSetupId(v["resource"]))return false;
 if(v.containsKey("site")&&!biomeSetupId(v["site"],true))return false;
 return true;
}
inline bool sameBiomeSetup(JsonVariantConst a,JsonVariantConst b){
 if(!biomeSetup(a)||!biomeSetup(b))return false;
 for(const char* key:{"url","site","resource"})if(strcmp(a[key]|"",b[key]|""))return false;
 return !strcmp(a["realm"]|"site",b["realm"]|"site");
}
}
