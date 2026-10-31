#pragma once
#include "Digest.h"
namespace Infra {
// Standard Webhooks uses a decoded whsec_ key and signs id.timestamp.body.
// Keep serialization in the caller: the signed bytes are exactly the POST bytes.
inline bool relayKey(const char* secret,uint8_t key[64],size_t& length){
 length=0;if(strncmp(secret,"whsec_",6))return false;const char* p=secret+6;size_t n=strlen(p);if(!n||n%4||n>88)return false;
 auto value=[](char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
 for(size_t i=0;i<n;i+=4){int a=value(p[i]),b=value(p[i+1]),c=p[i+2]=='='?0:value(p[i+2]),d=p[i+3]=='='?0:value(p[i+3]);if(a<0||b<0||c<0||d<0||(p[i+2]=='='&&p[i+3]!='=')||((p[i+2]=='='||p[i+3]=='=')&&i+4!=n)||(p[i+2]=='='&&(b&15))||(p[i+3]=='='&&p[i+2]!='='&&(c&3)))return false;
  unsigned count=p[i+2]=='='?1:p[i+3]=='='?2:3;if(length+count>64)return false;key[length++]=(a<<2)|(b>>4);if(count>1)key[length++]=(b<<4)|(c>>2);if(count>2)key[length++]=(c<<6)|d;
 }return length>=24;
}
inline void relayBase64(const uint8_t* input,size_t n,char* out){const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t at=0;for(size_t i=0;i<n;i+=3){unsigned a=input[i],b=i+1<n?input[i+1]:0,c=i+2<n?input[i+2]:0;out[at++]=alphabet[a>>2];out[at++]=alphabet[((a&3)<<4)|(b>>4)];out[at++]=i+1<n?alphabet[((b&15)<<2)|(c>>6)]:'=';out[at++]=i+2<n?alphabet[c&63]:'=';}out[at]=0;}
inline void relayMac(const uint8_t* key,size_t n,const char* id,const char* timestamp,const char* body,size_t bytes,uint8_t out[32]){
 uint8_t pad[64]{},inner[32];dtls_sha256_ctx c;if(n>64){dtls_sha256_init(&c);dtls_sha256_update(&c,key,n);dtls_sha256_final(pad,&c);}else memcpy(pad,key,n);
 for(auto& b:pad)b^=0x36;dtls_sha256_init(&c);dtls_sha256_update(&c,pad,64);const uint8_t dot='.';
 if(id){dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(id),strlen(id));dtls_sha256_update(&c,&dot,1);}dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(timestamp),strlen(timestamp));dtls_sha256_update(&c,&dot,1);dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(body),bytes);dtls_sha256_final(inner,&c);
 for(auto& b:pad)b^=0x36^0x5c;dtls_sha256_init(&c);dtls_sha256_update(&c,pad,64);dtls_sha256_update(&c,inner,32);dtls_sha256_final(out,&c);Portal::wipe(pad,sizeof(pad));Portal::wipe(inner,sizeof(inner));Portal::wipe(&c,sizeof(c));
}
inline bool relaySignature(const char* secret,const char* id,const char* timestamp,const char* body,size_t bytes,char* out){uint8_t key[64]{},mac[32];size_t n=0;bool ok=relayKey(secret,key,n);if(ok){relayMac(key,n,id,timestamp,body,bytes,mac);memcpy(out,"v1,",3);relayBase64(mac,32,out+3);}Portal::wipe(key,sizeof(key));Portal::wipe(mac,sizeof(mac));return ok;}
inline void relayInvocationId(JsonObjectConst sub,JsonObjectConst event,char out[65]){
 dtls_sha256_ctx c;dtls_sha256_init(&c);auto add=[&](const char* p){dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(p),strlen(p));};
 add(sub["id"]|"");add("|");add(sub["invocation"]["routine_id"]|"relay-notice");add("@");add(sub["invocation"]["routine_version"]|"1");add("|");add(event["channel"]|"");add("|");add(event["id"]|"");uint8_t hash[32];dtls_sha256_final(hash,&c);for(unsigned i=0;i<32;++i)snprintf(out+2*i,3,"%02x",hash[i]);
}
inline void relayIso(uint64_t ms,char out[32]){
 // Civil UTC date from Unix days, without platform timezone or time_t limits.
 uint64_t seconds=ms/1000,z=seconds/86400+719468,era=z/146097;unsigned doe=z-era*146097,yoe=(doe-doe/1460+doe/36524-doe/146096)/365;int y=int(yoe)+int(era)*400;unsigned doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153,d=doy-(153*mp+2)/5+1,m=mp+(mp<10?3:-9);y+=m<=2;
 snprintf(out,32,"%04d-%02u-%02uT%02u:%02u:%02u.%03uZ",y,m,d,unsigned(seconds/3600%24),unsigned(seconds/60%60),unsigned(seconds%60),unsigned(ms%1000));
}
inline void relayBody(JsonObject out,const char* relay,JsonObjectConst sub,JsonObjectConst event){
 out["protocol_version"]="relay-delivery/1";out["relay_id"]=relay;out["subscription_id"]=sub["id"];out["message_id"]=event["id"];for(const char* k:{"channel","seq","type","correlation","created"})out[k]=event[k];char summary[256];snprintf(summary,sizeof(summary),"%s on %s",event["type"]|"event",event["channel"]|"channel");for(char* c=summary;*c;++c)if(static_cast<unsigned char>(*c)<32||*c==127)*c=' ';out["summary"]=summary;
 if(event["payload_omitted"]==true||measureJson(event["payload"])>8192)out["payload_omitted"]=true;else out["payload"]=event["payload"];
 char id[65];relayInvocationId(sub,event,id);auto inv=out["invocation"].to<JsonObject>();inv["schema_version"]="agent-invocation/1";inv["invocation_id"]=id;inv["routine_id"]=sub["invocation"]["routine_id"]|"relay-notice";inv["routine_version"]=sub["invocation"]["routine_version"]|"1";inv["assignment_ref"]=sub["invocation"]["assignment_ref"]|"";inv["deadline"]=sub["invocation"]["deadline"].as<uint64_t>();inv["effect_key"]=text(sub["invocation"]["effect_key"],200)?sub["invocation"]["effect_key"].as<const char*>():id;if(text(sub["fanout_group"],128))inv["fanout_group"]=sub["fanout_group"];
}
}
