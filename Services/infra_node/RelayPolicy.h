#pragma once
#include "Policy.h"
namespace Infra {
inline bool relayContains(const char* text,const char* needle){for(;*text;++text)if(!strncmp(text,needle,strlen(needle)))return true;return false;}
inline bool relayChannel(const char* c){return !strcmp(c,"capabilities.events")||!strcmp(c,"bpmn.events")||!strcmp(c,"scheduler.events");}
inline bool relayHeader(const char* h){if(!h||!*h||strlen(h)>64)return false;for(const char* p=h;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return false;
 for(const char* reserved:{"host","content-length","content-type","connection","transfer-encoding","idempotency-key","x-relay-signature","x-relay-timestamp"}){const char* a=h;const char* b=reserved;while(*a&&*b&&((*a>='A'&&*a<='Z'?*a+32:*a)==*b)){++a;++b;}if(!*a&&!*b)return false;}return true;}
inline bool relayOrigin(JsonVariantConst url,JsonArrayConst allowed){if(!https(url))return false;const char* p=url;size_t n=8;while(p[n]&&p[n]!='/'&&p[n]!='?')++n;for(auto a:allowed)if(text(a,256)&&strlen(a)==n&&!strncmp(a,p,n))return true;return false;}
inline bool relaySubscription(JsonObjectConst s,JsonArrayConst allowed,JsonArrayConst credentialRefs){
 if(!keys(s,{"id","channel","match","url","auth","signing_credential_ref","enabled"})||!text(s["id"],64)||!relayChannel(s["channel"]|"")||!relayOrigin(s["url"],allowed)||!s["enabled"].is<bool>()||!text(s["signing_credential_ref"],128)||measureJson(s)>1536)return false;
 auto auth=s["auth"].as<JsonObjectConst>();if(!keys(auth,{"header","credential_ref"})||!relayHeader(auth["header"]|"")||!text(auth["credential_ref"],128))return false;
 bool authAllowed=false,signAllowed=false;for(auto ref:credentialRefs){authAllowed|=ref==auth["credential_ref"];signAllowed|=ref==s["signing_credential_ref"];}if(!authAllowed||!signAllowed)return false;
 auto m=s["match"].as<JsonObjectConst>();if(!keys(m,{"types","filters"})||!m["types"].is<JsonArrayConst>()||!m["types"].size()||m["types"].size()>8||!m["filters"].is<JsonArrayConst>()||m["filters"].size()>4)return false;
 for(auto t:m["types"].as<JsonArrayConst>())if(!text(t,128))return false;
 for(auto f:m["filters"].as<JsonArrayConst>()){if(!keys(f,{"path","op","value"})||!text(f["path"],128)||!text(f["value"],256,true)||(f["op"]!="prefix"&&f["op"]!="eq"))return false;const char* p=f["path"];bool first=true;for(;*p;++p){if(*p=='.'){if(first)return false;first=true;continue;}if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||*p=='_'||(!first&&*p>='0'&&*p<='9')))return false;first=false;}if(first||relayContains(f["path"].as<const char*>(),"__proto__")||relayContains(f["path"].as<const char*>(),"constructor")||relayContains(f["path"].as<const char*>(),"prototype"))return false;}
 return true;
}
inline bool relayPolicy(JsonObjectConst r){
 if(r.isNull())return true;
 if((!r["webhook_credential_refs"].isNull()&&!r["webhook_credential_refs"].is<JsonArrayConst>())||r["webhook_credential_refs"].size()>8)return false;for(auto ref:r["webhook_credential_refs"].as<JsonArrayConst>())if(!text(ref,128))return false;
 if(!keys(r,{"schema_version","relay_id","url","credential_ref","site_credential_ref"},{"allowed_origins","webhook_credential_refs","subscriptions"})||r["schema_version"]!=1||!text(r["relay_id"],128)||!https(r["url"])||!text(r["credential_ref"],128)||!text(r["site_credential_ref"],128,true)||(!r["allowed_origins"].isNull()&&!r["allowed_origins"].is<JsonArrayConst>())||r["allowed_origins"].size()>4||(!r["subscriptions"].isNull()&&!r["subscriptions"].is<JsonArrayConst>())||r["subscriptions"].size()>4||measureJson(r)>8192)return false;
 for(auto origin:r["allowed_origins"].as<JsonArrayConst>())if(!https(origin)||strlen(origin)>256||relayContains(origin.as<const char*>()+8,"/")||relayContains(origin.as<const char*>()+8,"?"))return false;
 const char* fields[8]{};unsigned count=0,i=0;for(auto s:r["subscriptions"].as<JsonArrayConst>()){if(!relaySubscription(s,r["allowed_origins"],r["webhook_credential_refs"]))return false;for(unsigned j=0;j<i;++j)if(r["subscriptions"][j]["id"]==s["id"])return false;++i;for(auto f:s["match"]["filters"].as<JsonArrayConst>()){bool seen=false;for(unsigned j=0;j<count;++j)seen|=!strcmp(fields[j],f["path"]);if(!seen){if(count==8)return false;fields[count++]=f["path"];}}}return true;
}
inline bool relayMatches(JsonObjectConst s,JsonObjectConst event){
 if(s["enabled"]!=true||s["channel"]!=event["channel"])return false;bool type=false;for(auto t:s["match"]["types"].as<JsonArrayConst>())type|=t=="*"||t==event["type"];if(!type)return false;
 for(auto f:s["match"]["filters"].as<JsonArrayConst>()){auto v=event["fields"][f["path"]|""];if(!text(v,512,true))return false;const char* a=v;const char* b=f["value"];if(f["op"]=="prefix"?strncmp(a,b,strlen(b))!=0:strcmp(a,b)!=0)return false;}return true;
}
}
