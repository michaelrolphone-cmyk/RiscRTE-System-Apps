#include "Node.h"
#include "EnrollmentCrypto.h"
namespace Infra {
static bool token(JsonVariantConst v){if(!text(v,64))return false;for(const char* p=v.as<const char*>();*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='_'||*p==':'||*p=='-'))return false;return true;}
static void prepare(Node& n,Action action,const char* op){n.action=action;n.request.clear();n.request["protocol_version"]="relay/1";n.request["operation"]=op;n.request["arguments"].to<JsonObject>();n.relayYield=true;}
static void terminal(Node& n,JsonObject e,const char* status,const char* error=""){e["state"]="result";e["result"]["status"]=status;e["result"]["error_code"]=error;e["result"]["revision"]=n.journal.state["relay"]["subscription_revision"];e["result"]["subscription_id"]=e["payload"]["subscription"]["id"];}
bool Node::enrollmentQueue(JsonObjectConst event){
 auto r=journal.state["relay"];auto p=event["payload"].as<JsonObjectConst>();if(p["relay_id"]!=config["relay"]["relay_id"])return true;
 if(measureJson(p)>6144){copy(relayError,sizeof(relayError),"enroll_malformed");return true;}
 auto list=r["enrollments"].as<JsonArray>();for(auto e:list)if(e["message_id"]==event["id"])return true;if(list.size()>=4){copy(relayError,sizeof(relayError),"enroll_pending_capacity");return false;}
 auto e=list.add<JsonObject>();e["message_id"]=event["id"];e["payload"]=p;e["state"]="pending";char dg[65];canonicalDigest(p,dg);e["digest"]=dg;enrollmentPollAt=0;return true;
}
int Node::enrollmentStep(){
 auto r=journal.state["relay"].as<JsonObject>();auto identity=r["identity"].as<JsonObject>();
 if(identity.isNull()){
  uint8_t sk[32],pk[32];auto result=entropy.fill(entropy.context,sk,sizeof(sk));if(result==RISC_ENTROPY_BUSY||result==RISC_ENTROPY_UNAVAILABLE)return 0;if(result==RISC_ENTROPY_RETAINED)return retained();if(result)return relayFailure("enroll_entropy_failed");
  crypto_x25519_public_key(pk,sk);identity=r["identity"].to<JsonObject>();char value[65];hexEncode(sk,32,value);identity["private_key"]=value;crypto_wipe(sk,32);hexEncode(pk,32,value);identity["public_key"]=value;dtls_sha256_data(reinterpret_cast<const uint8_t*>(value),64,value);identity["key_id"]=value;crypto_wipe(value,sizeof(value));crypto_wipe(pk,32);r["enrollments"].to<JsonArray>();return persist(Phase::Ready);
 }
 if(!relayIdentityReady){prepare(*this,Action::RelayIdentity,"identity");auto a=request["arguments"];a["relay_id"]=config["relay"]["relay_id"];a["installation_id"]=journal.state["installation_id"];a["enroll_public_key"]=identity["public_key"];a["key_id"]=identity["key_id"];a["firmware_version"]="1.4.1";return send(false);}
 if(now()<enrollmentPollAt)return 0;
 auto list=r["enrollments"].as<JsonArray>();if(!list.size())return 0;enrollmentIndex=(enrollmentIndex+1)%list.size();auto e=list[enrollmentIndex].as<JsonObject>();
 if(e["state"]=="result"){
  prepare(*this,Action::RelayEnrollmentComplete,"enrollment_complete");auto a=request["arguments"];a["message_id"]=e["message_id"];a["digest"]=e["digest"];for(const char* k:{"status","subscription_id","revision","error_code"})a[k]=e["result"][k];return send(false);
 }
 prepare(*this,Action::RelayEnrollmentReview,"enrollment_review");request["arguments"]["message_id"]=e["message_id"];return send(false);
}
int Node::enrollmentReceived(JsonObjectConst data){
 auto r=journal.state["relay"].as<JsonObject>();
 if(action==Action::RelayIdentity){if(data["registered"]!=true||data["key_id"]!=r["identity"]["key_id"])return relayFailure("enroll_identity_mismatch");relayIdentityReady=true;phase=Phase::Ready;return 1;}
 auto list=r["enrollments"].as<JsonArray>();auto e=list[enrollmentIndex].as<JsonObject>();if(e.isNull()||data["digest"]!=e["digest"])return relayFailure("enroll_digest_mismatch");
 if(action==Action::RelayEnrollmentComplete){if(data["recorded"]!=true||data["status"]!=e["result"]["status"])return relayFailure("enroll_result_unconfirmed");list.remove(enrollmentIndex);enrollmentPollAt=now()+100;relayHealthAt=0;return persist(Phase::Ready);}
 enrollmentPollAt=now()+5000;
 if(data["state"]=="completed"){list.remove(enrollmentIndex);return persist(Phase::Ready);}
 if(data["state"]=="pending"){phase=Phase::Ready;return 1;}
 if(data["state"]=="rejected"||data["state"]=="expired"){terminal(*this,e,data["state"]);return persist(Phase::Ready);}
 if(data["state"]=="invalid"){terminal(*this,e,"invalid",data["error_code"]|"invalid_request");return persist(Phase::Ready);}
 if(data["state"]!="approved"||!text(data["decision_id"],128)||!digest(data["question_digest"]))return relayFailure("enroll_approval_invalid");
 // Do not remove credentials selected by an in-flight delivery. The approval
 // remains durable at the service and is reconciled again after that delivery.
 if(!r["pending"].isNull()){phase=Phase::Ready;return 1;}
 return enrollmentApply(e);
}
int Node::enrollmentApply(JsonObject e){
 auto r=journal.state["relay"].as<JsonObject>();auto p=e["payload"].as<JsonObjectConst>();const char* op=p["operation"]|"add";
 const char* error="enroll_invalid";bool ok=false;
 do{
  if(!token(p["subscriber_id"])||!token(p["requested_by"])||!token(p["subscription"]["id"])||(!strcmp(op,"add")&&r["subscriptions"].size()>=4)||p["sealed"]["key_id"]!=r["identity"]["key_id"])break;
  // Canonical, lexical AAD order; digest binds all public subscription fields.
  request.clear();request["enrollment_id"]=p["enrollment_id"];request["key_id"]=p["sealed"]["key_id"];request["operation"]=op;request["origin"]=p["origin"];request["relay_id"]=p["relay_id"];request["requested_by"]=p["requested_by"];request["subscriber_id"]=p["subscriber_id"];char dg[65];canonicalDigest(p["subscription"],dg);request["subscription_digest"]=dg;
  char aad[1536]{},plain[1536]{};const size_t length=serializeJson(request,aad,sizeof(aad));if(length>=sizeof(aad)-1)break;
  const bool decrypted=enrollDecrypt(r["identity"]["private_key"]|"",p["sealed"],aad,length,plain,sizeof(plain));
  response.clear();const bool decoded=decrypted&&parse(response,plain,strlen(plain));crypto_wipe(plain,sizeof(plain));crypto_wipe(aad,sizeof(aad));if(!decoded){error="enroll_ciphertext_invalid";break;}
  const char* subid=p["subscription"]["id"]|"";int existing=-1;unsigned i=0;for(auto sub:r["subscriptions"].as<JsonArrayConst>()){if(sub["id"]==subid)existing=i;++i;}
  if((!strcmp(op,"add")&&existing>=0)||(strcmp(op,"add")&&existing<0)){error="enroll_subscription_conflict";break;}
  if(strcmp(op,"add")&&strcmp(op,"rotate")&&strcmp(op,"revoke"))break;
  // Build the entire replacement off to the side, then commit it with secrets,
  // receipt and revision in the existing transactional snapshot.
  JsonDocument candidate;candidate.set(r);auto subs=candidate["subscriptions"].as<JsonArray>();
  if(existing>=0){auto old=subs[existing];candidate["secrets"].remove(old["auth"]["credential_ref"]|"");candidate["secrets"].remove(old["signing_credential_ref"]|"");subs.remove(existing);}
  if(strcmp(op,"revoke")){
   auto z=response.as<JsonObjectConst>();if(!keys(z,{"url_path","auth_header_name","auth_header_value","signing_key"})||!text(z["url_path"],512)||!text(z["auth_header_value"],512)||!text(z["signing_key"],128)||!relayHeader(z["auth_header_name"]|""))break;
   const char* path=z["url_path"];if(path[0]!='/'||path[1]=='/'||relayContains(path,"\\")||relayContains(path,"#"))break;bool control=false;for(auto key:{"url_path","auth_header_value","signing_key"})for(const char* s=z[key].as<const char*>();*s;++s)control|=static_cast<unsigned char>(*s)<32||*s==127;if(control)break;
   char url[800],auth[128],sign[128];snprintf(url,sizeof(url),"%s%s",p["origin"]|"",path);snprintf(auth,sizeof(auth),"enroll:%s:auth",p["enrollment_id"]|"");snprintf(sign,sizeof(sign),"enroll:%s:sign",p["enrollment_id"]|"");
   auto s=subs.add<JsonObject>();s.set(p["subscription"]);s["url"]=url;s["auth"]["header"]=z["auth_header_name"];s["auth"]["credential_ref"]=auth;s["signing_credential_ref"]=sign;candidate["secrets"][auth]=z["auth_header_value"];candidate["secrets"][sign]=z["signing_key"];
  }else if(!keys(response.as<JsonObjectConst>(),{}))break;
  auto origins=candidate["allowed_origins"].to<JsonArray>();auto refs=candidate["webhook_credential_refs"].to<JsonArray>();
  for(auto s:subs){const char* url=s["url"]|"";char origin[257];size_t n=8;while(url[n]&&url[n]!='/'&&url[n]!='?')++n;if(n>=sizeof(origin)){error="enroll_origin_invalid";break;}memcpy(origin,url,n);origin[n]=0;bool seen=false;for(auto o:origins)seen|=o==origin;if(!seen)origins.add(origin);refs.add(s["auth"]["credential_ref"]);refs.add(s["signing_credential_ref"]);}
  request.clear();request.set(config["relay"]);request["subscriptions"]=subs;request["allowed_origins"]=origins;request["webhook_credential_refs"]=refs;
  if(!relayPolicy(request.as<JsonObjectConst>())||measureJson(candidate["secrets"])>8192){error="enroll_capacity_or_policy";break;}
  // Copy only changed registration fields so the enrollment queue keeps its
  // references until the result is set, and preserve all other node state.
  r["subscriptions"]=subs;r["allowed_origins"]=origins;r["webhook_credential_refs"]=refs;r["secrets"]=candidate["secrets"];r["subscription_revision"]=r["subscription_revision"].as<uint64_t>()+1;ok=true;
 }while(false);
 // Decrypted material is never copied into requests, diagnostics or results.
 response.clear();request.clear();terminal(*this,e,ok?"applied":"invalid",ok?"":error);enrollmentPollAt=0;for(auto& at:relayPollAt)at=0;relayHealthAt=0;return persist(Phase::Ready);
}
}
