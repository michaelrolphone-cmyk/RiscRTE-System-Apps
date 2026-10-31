#pragma once
#include "Policy.h"
namespace Infra {
constexpr unsigned RelaySubscriptionsMax=8,RelayAttachmentsMax=38;
inline bool relayContains(const char* text,const char* needle){for(;*text;++text)if(!strncmp(text,needle,strlen(needle)))return true;return false;}
inline bool relayChannel(const char* c){return !strcmp(c,"capabilities.events")||!strcmp(c,"bpmn.events")||!strcmp(c,"scheduler.events")||!strcmp(c,"rhizome.events");}
inline bool relayWatches(JsonObjectConst s,JsonVariantConst channel){if(s["channels"].is<JsonArrayConst>()){for(auto c:s["channels"].as<JsonArrayConst>())if(c==channel)return true;return false;}return s["channel"]==channel;}
inline bool relayMcp(JsonObjectConst s){return s["destination"]["kind"]=="mcp_events";}
inline bool relayEnabled(JsonObjectConst s,uint64_t now){return s["enabled"]==true&&s["revoked"]!=true&&(!s["expires_at"].as<uint64_t>()||now<s["expires_at"].as<uint64_t>());}
inline bool relayHeader(const char* h){if(!h||!*h||strlen(h)>64)return false;for(const char* p=h;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return false;
 for(const char* reserved:{"host","content-length","content-type","connection","transfer-encoding","idempotency-key","x-relay-signature","x-relay-timestamp","webhook-id","webhook-timestamp","webhook-signature","x-mcp-subscription-id"}){const char* a=h;const char* b=reserved;while(*a&&*b&&((*a>='A'&&*a<='Z'?*a+32:*a)==*b)){++a;++b;}if(!*a&&!*b)return false;}return true;}
inline bool relayOrigin(JsonVariantConst url,JsonArrayConst allowed){if(!https(url))return false;const char* p=url;size_t n=8;while(p[n]&&p[n]!='/'&&p[n]!='?')++n;for(auto a:allowed)if(text(a,256)&&strlen(a)==n&&!strncmp(a,p,n))return true;return false;}
inline bool relaySubscription(JsonObjectConst s,JsonArrayConst allowed,JsonArrayConst credentialRefs){
 if(!keys(s,{"id","match","url","enabled"},{"channel","channels","auth","signing_credential_ref","previous_signing_credential_ref","rotation_until","destination","principal","generation","expires_at","revoked","fanout_group","invocation"})||!text(s["id"],64)||!relayOrigin(s["url"],allowed)||!s["enabled"].is<bool>()||measureJson(s)>2048)return false;
 if(s["channels"].isNull()){if(!relayChannel(s["channel"]|""))return false;}else{if(!s["channels"].is<JsonArrayConst>()||!s["channels"].size()||s["channels"].size()>4)return false;unsigned i=0;for(auto c:s["channels"].as<JsonArrayConst>()){if(!relayChannel(c|""))return false;for(unsigned j=0;j<i;++j)if(s["channels"][j]==c)return false;++i;}}
 if(!s["destination"].isNull()){auto d=s["destination"].as<JsonObjectConst>();if(!keys(d,{"kind","origin"},{"event_name"})||(d["kind"]!="webhook"&&d["kind"]!="mcp_events")||!relayOrigin(s["url"],allowed))return false;const char* url=s["url"];const char* origin=d["origin"]|"";size_t n=strlen(origin);if(!n||strncmp(url,origin,n)||(url[n]&&url[n]!='/'&&url[n]!='?'))return false;if(relayMcp(s)&&!text(d["event_name"],128))return false;}
 for(const char* k:{"expires_at","rotation_until","generation"})if(!s[k].isNull()&&!integer(s[k],0,9007199254740991ULL))return false;
 if(!s["revoked"].isNull()&&!s["revoked"].is<bool>())return false;
 auto auth=s["auth"].as<JsonObjectConst>();if(!auth.isNull()&&(!keys(auth,{"header","credential_ref"})||!relayHeader(auth["header"]|"")||!text(auth["credential_ref"],128)))return false;
 if(!relayMcp(s)&&auth.isNull())return false;
 for(const char* ref:{auth["credential_ref"]|"",s["signing_credential_ref"]|"",s["previous_signing_credential_ref"]|""}){if(!*ref)continue;bool found=false;for(auto allowed:credentialRefs)found|=allowed==ref;if(!found)return false;}
 if(relayMcp(s)&&!text(s["signing_credential_ref"],128))return false;
 if(!s["invocation"].isNull()){auto i=s["invocation"].as<JsonObjectConst>();if(!keys(i,{"routine_id","routine_version","assignment_ref","deadline","effect_key"})||!text(i["routine_id"],128)||!text(i["routine_version"],128)||!text(i["assignment_ref"],200,true)||!integer(i["deadline"],0,9007199254740991ULL)||!text(i["effect_key"],200,true))return false;}
 auto m=s["match"].as<JsonObjectConst>();if(!keys(m,{"types","filters"})||!m["types"].is<JsonArrayConst>()||!m["types"].size()||m["types"].size()>8||!m["filters"].is<JsonArrayConst>()||m["filters"].size()>4)return false;
 for(auto t:m["types"].as<JsonArrayConst>())if(!text(t,128))return false;
 for(auto f:m["filters"].as<JsonArrayConst>()){if(!keys(f,{"path","op","value"})||!text(f["path"],128)||!text(f["value"],256,true)||(f["op"]!="prefix"&&f["op"]!="eq"))return false;const char* p=f["path"];bool first=true;for(;*p;++p){if(*p=='.'){if(first)return false;first=true;continue;}if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||*p=='_'||(!first&&*p>='0'&&*p<='9')))return false;first=false;}if(first||relayContains(f["path"].as<const char*>(),"__proto__")||relayContains(f["path"].as<const char*>(),"constructor")||relayContains(f["path"].as<const char*>(),"prototype"))return false;}
 return true;
}
inline bool relayPolicy(JsonObjectConst r){
 if(r.isNull())return true;
 if((!r["webhook_credential_refs"].isNull()&&!r["webhook_credential_refs"].is<JsonArrayConst>())||r["webhook_credential_refs"].size()>24)return false;for(auto ref:r["webhook_credential_refs"].as<JsonArrayConst>())if(!text(ref,128))return false;
 if(!keys(r,{"schema_version","relay_id","url","credential_ref","site_credential_ref"},{"allowed_origins","webhook_credential_refs","subscriptions"})||r["schema_version"]!=1||!text(r["relay_id"],128)||!https(r["url"])||!text(r["credential_ref"],128)||!text(r["site_credential_ref"],128,true)||(!r["allowed_origins"].isNull()&&!r["allowed_origins"].is<JsonArrayConst>())||r["allowed_origins"].size()>RelaySubscriptionsMax||(!r["subscriptions"].isNull()&&!r["subscriptions"].is<JsonArrayConst>())||r["subscriptions"].size()>RelaySubscriptionsMax||measureJson(r)>24576)return false;
 for(auto origin:r["allowed_origins"].as<JsonArrayConst>())if(!https(origin)||strlen(origin)>256||relayContains(origin.as<const char*>()+8,"/")||relayContains(origin.as<const char*>()+8,"?"))return false;
 const char* fields[8]{};unsigned count=0,i=0;for(auto s:r["subscriptions"].as<JsonArrayConst>()){if(!relaySubscription(s,r["allowed_origins"],r["webhook_credential_refs"]))return false;for(unsigned j=0;j<i;++j)if(r["subscriptions"][j]["id"]==s["id"])return false;++i;for(auto f:s["match"]["filters"].as<JsonArrayConst>()){bool seen=false;for(unsigned j=0;j<count;++j)seen|=!strcmp(fields[j],f["path"]);if(!seen){if(count==8)return false;fields[count++]=f["path"];}}}return true;
}
// Only Capabilities activation events may supply per-assignment invocation
// fields. A subscription still pins the destination and routine; payloads never
// choose credentials, URLs, routine versions or transport identity.
inline bool relayActivationEvent(JsonObjectConst event){return event["channel"]=="capabilities.events"&&event["type"]=="capabilities.assignment.changed"&&!event["payload"]["activation"].isNull();}
inline bool relayActivationMatches(JsonObjectConst s,JsonObjectConst event){
 if(!relayActivationEvent(event))return true;auto a=event["payload"]["activation"].as<JsonObjectConst>();
 return a["schema_version"]=="agent-invocation/1"&&a["subscription_id"]==s["id"]&&a["routine_id"]==s["invocation"]["routine_id"]&&a["routine_version"]==s["invocation"]["routine_version"]&&text(a["assignment_ref"],200)&&text(a["effect_key"],200)&&integer(a["deadline"],1,9007199254740991ULL)&&text(a["attempt_id"],200)&&integer(a["generation"],1,9007199254740991ULL);
}
inline bool relayMatches(JsonObjectConst s,JsonObjectConst event){
 if(s["enabled"]!=true||s["revoked"]==true||!relayWatches(s,event["channel"])||!relayActivationMatches(s,event))return false;bool type=false;for(auto t:s["match"]["types"].as<JsonArrayConst>())type|=t=="*"||t==event["type"];if(!type)return false;
 for(auto f:s["match"]["filters"].as<JsonArrayConst>()){auto v=event["fields"][f["path"]|""];if(!text(v,512,true))return false;const char* a=v;const char* b=f["value"];if(f["op"]=="prefix"?strncmp(a,b,strlen(b))!=0:strcmp(a,b)!=0)return false;}return true;
}
}
