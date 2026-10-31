#include "Node.h"
#include "EnrollmentCrypto.h"
#include "RelayWire.h"
namespace Infra {
static bool token(JsonVariantConst v){if(!text(v,64))return false;for(const char* p=v.as<const char*>();*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='_'||*p==':'||*p=='-'))return false;return true;}
static void prepare(Node& n,Action action,const char* op){n.action=action;n.request.clear();n.request["protocol_version"]="relay/1";n.request["operation"]=op;n.request["arguments"].to<JsonObject>();n.relayYield=true;}
static void terminal(Node& n,JsonObject e,const char* status,const char* error=""){e["state"]="result";e["result"]["status"]=status;e["result"]["error_code"]=error;e["result"]["revision"]=n.journal.state["relay"]["subscription_revision"];e["result"]["subscription_id"]=e["payload"]["subscription"]["id"];}
bool Node::enrollmentQueue(JsonObjectConst event){
 auto r=journal.state["relay"];auto p=event["payload"].as<JsonObjectConst>();if(p["relay_id"]!=config["relay"]["relay_id"])return true;
 if(measureJson(p)>6144){copy(relayError,sizeof(relayError),"enroll_malformed");return true;}
 auto list=r["enrollments"].as<JsonArray>();for(auto e:list)if(e["message_id"]==event["id"])return true;if(list.size()>=4||measureJson(journal.state)+measureJson(p)+512>StateMax-4096){copy(relayError,sizeof(relayError),"enroll_pending_capacity");return false;}
 auto e=list.add<JsonObject>();e["message_id"]=event["id"];e["payload"]=p;e["state"]="pending";char dg[65];canonicalDigest(p,dg);e["digest"]=dg;enrollmentPollAt=0;return true;
}
int Node::enrollmentStep(){
 auto r=journal.state["relay"].as<JsonObject>();auto identity=r["identity"].as<JsonObject>();
 if(identity.isNull()){
  uint8_t sk[32],pk[32];auto result=entropy.fill(entropy.context,sk,sizeof(sk));if(result==RISC_ENTROPY_BUSY||result==RISC_ENTROPY_UNAVAILABLE)return 0;if(result==RISC_ENTROPY_RETAINED)return retained();if(result)return relayFailure("enroll_entropy_failed");
  crypto_x25519_public_key(pk,sk);identity=r["identity"].to<JsonObject>();char value[65];hexEncode(sk,32,value);identity["private_key"]=value;crypto_wipe(sk,32);hexEncode(pk,32,value);identity["public_key"]=value;dtls_sha256_data(reinterpret_cast<const uint8_t*>(value),64,value);identity["key_id"]=value;crypto_wipe(value,sizeof(value));crypto_wipe(pk,32);r["enrollments"].to<JsonArray>();return persist(Phase::Ready);
 }
 if(!relayIdentityReady){prepare(*this,Action::RelayIdentity,"identity");auto a=request["arguments"];a["relay_id"]=config["relay"]["relay_id"];a["installation_id"]=journal.state["installation_id"];a["enroll_public_key"]=identity["public_key"];a["key_id"]=identity["key_id"];a["firmware_version"]="1.5.0";return send(false);}
 if(!r["verification"].isNull()){if(auto step=enrollmentVerify())return step;if(r["verification"]["verified"]!=true)return 0;if(r["verification"]["reported"]!=true){for(unsigned i=0;i<r["enrollments"].size();++i){auto e=r["enrollments"][i];if(e["payload"]["enrollment_id"]==r["verification"]["enrollment_id"]){enrollmentIndex=i;prepare(*this,Action::RelayEnrollmentVerified,"enrollment_verified");request["arguments"]["message_id"]=e["message_id"];request["arguments"]["digest"]=e["digest"];return send(false);}}}}
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
 if(action==Action::RelayEnrollmentVerified){if(data["recorded"]!=true)return relayFailure("enroll_verification_unconfirmed");r["verification"]["reported"]=true;enrollmentPollAt=0;return persist(Phase::Ready);}
 if(action==Action::RelayEnrollmentComplete){if(data["recorded"]!=true||data["status"]!=e["result"]["status"])return relayFailure("enroll_result_unconfirmed");list.remove(enrollmentIndex);enrollmentPollAt=now()+100;relayHealthAt=0;return persist(Phase::Ready);}
 enrollmentPollAt=now()+5000;
 if(data["state"]=="completed"){if(r["verification"]["enrollment_id"]==e["payload"]["enrollment_id"])r.remove("verification");list.remove(enrollmentIndex);return persist(Phase::Ready);}
 if(data["state"]=="pending"){
  // Verification discloses no application data and is performed by this same
  // executor while the single owner decision is pending. Delivery stays off.
  if(e["payload"]["subscription"]["destination"]["kind"]=="mcp_events"&&text(data["decision_id"],128)&&digest(data["question_digest"])&&r["pending"].isNull()&&r["deliveries"][e["payload"]["subscription"]["id"]|""].isNull()&&r["verification"].isNull())return enrollmentApply(e,false);
  phase=Phase::Ready;return 1;
 }
 if(data["state"]=="rejected"||data["state"]=="expired"||data["state"]=="invalid"){if(r["verification"]["enrollment_id"]==e["payload"]["enrollment_id"])r.remove("verification");terminal(*this,e,data["state"],data["error_code"]|"");return persist(Phase::Ready);}
 if(data["state"]!="approved"||!text(data["decision_id"],128)||!digest(data["question_digest"]))return relayFailure("enroll_approval_invalid");
 // Do not remove credentials selected by an in-flight delivery. The approval
 // remains durable at the service and is reconciled again after that delivery.
 if(!r["pending"].isNull()||!r["deliveries"][e["payload"]["subscription"]["id"]|""].isNull()||(!r["verification"].isNull()&&r["verification"]["enrollment_id"]!=e["payload"]["enrollment_id"])){phase=Phase::Ready;return 1;}
 if(!r["verification"].isNull()&&r["verification"]["verified"]!=true){phase=Phase::Ready;return 1;}
 return enrollmentApply(e);
}
int Node::enrollmentApply(JsonObject e,bool approved){
 auto r=journal.state["relay"].as<JsonObject>();auto p=e["payload"].as<JsonObjectConst>();const char* op=p["operation"]|"add";
 const char* error="enroll_invalid";bool ok=false;
 do{
  if(!token(p["subscriber_id"])||!token(p["requested_by"])||!token(p["subscription"]["id"])||(!strcmp(op,"add")&&r["subscriptions"].size()>=RelaySubscriptionsMax)||p["sealed"]["key_id"]!=r["identity"]["key_id"])break;
  // Canonical, lexical AAD order; digest binds all public subscription fields.
  request.clear();request["enrollment_id"]=p["enrollment_id"];request["key_id"]=p["sealed"]["key_id"];request["operation"]=op;request["origin"]=p["origin"];if(p["protocol_version"]=="relay-enroll/2")request["protocol_version"]="relay-enroll/2";request["relay_id"]=p["relay_id"];request["requested_by"]=p["requested_by"];request["subscriber_id"]=p["subscriber_id"];char dg[65];canonicalDigest(p["subscription"],dg);request["subscription_digest"]=dg;
  char aad[1536]{},plain[1536]{};const size_t length=serializeJson(request,aad,sizeof(aad));if(length>=sizeof(aad)-1)break;
  const bool decrypted=enrollDecrypt(r["identity"]["private_key"]|"",p["sealed"],aad,length,plain,sizeof(plain),p["protocol_version"]=="relay-enroll/2");
  response.clear();const bool decoded=decrypted&&parse(response,plain,strlen(plain));crypto_wipe(plain,sizeof(plain));crypto_wipe(aad,sizeof(aad));if(!decoded){error="enroll_ciphertext_invalid";break;}
  const char* subid=p["subscription"]["id"]|"";int existing=-1;unsigned i=0;for(auto sub:r["subscriptions"].as<JsonArrayConst>()){if(sub["id"]==subid)existing=i;++i;}
  if((!strcmp(op,"add")&&existing>=0)||(strcmp(op,"add")&&existing<0)){error="enroll_subscription_conflict";break;}
  if(strcmp(op,"add")&&strcmp(op,"rotate")&&strcmp(op,"revoke"))break;
  if(p["protocol_version"]=="relay-enroll/2"&&existing>=0&&(p["subscription"]["generation"].as<uint64_t>()<=r["subscriptions"][existing]["generation"].as<uint64_t>()||(!r["subscriptions"][existing]["principal"].isNull()&&p["subscription"]["principal"]!=r["subscriptions"][existing]["principal"]))){error="enroll_generation_or_owner";break;}
  // Build the entire replacement off to the side, then commit it with secrets,
  // receipt and revision in the existing transactional snapshot.
  JsonDocument candidate;candidate.set(r);auto subs=candidate["subscriptions"].as<JsonArray>();
  if(existing>=0){auto old=subs[existing];for(const char* key:{"signing_credential_ref","previous_signing_credential_ref"})candidate["secrets"].remove(old[key]|"");candidate["secrets"].remove(old["auth"]["credential_ref"]|"");subs.remove(existing);}
  if(strcmp(op,"revoke")){
   auto z=response.as<JsonObjectConst>();const bool v2=p["protocol_version"]=="relay-enroll/2";const bool mcp=p["subscription"]["destination"]["kind"]=="mcp_events";
   if(v2){if(!keys(z,{"url_path"},{"headers","signing"})||(!z["headers"].isNull()&&!z["headers"].is<JsonObjectConst>())||z["headers"].size()>1)break;}
   else if(!keys(z,{"url_path","auth_header_name","auth_header_value","signing_key"}))break;
   if(!text(z["url_path"],512))break;const char* path=z["url_path"];if(path[0]!='/'||path[1]=='/'||relayContains(path,"\\")||relayContains(path,"#"))break;
   const char* header=v2?"":z["auth_header_name"]|"";const char* value=v2?"":z["auth_header_value"]|"";
   if(v2)for(auto h:z["headers"].as<JsonObjectConst>()){header=h.key().c_str();value=h.value()|"";}
   if((*header&&(!relayHeader(header)||(!*value||strlen(value)>512)))||(!mcp&&!*header))break;
   const char* key=v2?z["signing"]["key"]|"":z["signing_key"]|"";
   if(mcp){uint8_t decoded[64];size_t size=0;bool valid=v2&&z["signing"]["alg"]=="standard-webhooks"&&relayKey(key,decoded,size);crypto_wipe(decoded,64);if(!valid)break;}
   else if(v2&&!z["signing"].isNull()&&z["signing"]["alg"]!="hmac-sha256")break;
   if(strlen(key)>128)break;bool control=false;for(const char* v:{path,value,key})for(;*v;++v)control|=static_cast<unsigned char>(*v)<32||*v==127;if(control)break;
   char url[800],auth[128],sign[128];snprintf(url,sizeof(url),"%s%s",p["origin"]|"",path);snprintf(auth,sizeof(auth),"enroll:%s:auth",p["enrollment_id"]|"");snprintf(sign,sizeof(sign),"enroll:%s:sign",p["enrollment_id"]|"");
   auto sub=subs.add<JsonObject>();sub.set(p["subscription"]);sub["url"]=url;
   if(*header){sub["auth"]["header"]=header;sub["auth"]["credential_ref"]=auth;candidate["secrets"][auth]=value;}
   if(*key){sub["signing_credential_ref"]=sign;candidate["secrets"][sign]=key;}
   if(mcp&&existing>=0){auto old=r["subscriptions"][existing];const char* previous=old["signing_credential_ref"]|"";if(*previous){sub["previous_signing_credential_ref"]=previous;sub["rotation_until"]=net.wall(now())*1000+300000;candidate["secrets"][previous]=r["secrets"][previous];}}
  }else if(!keys(response.as<JsonObjectConst>(),{}))break;
  auto origins=candidate["allowed_origins"].to<JsonArray>();auto refs=candidate["webhook_credential_refs"].to<JsonArray>();
  for(auto s:subs){const char* url=s["url"]|"";char origin[257];size_t n=8;while(url[n]&&url[n]!='/'&&url[n]!='?')++n;if(n>=sizeof(origin)){error="enroll_origin_invalid";break;}memcpy(origin,url,n);origin[n]=0;bool seen=false;for(auto o:origins)seen|=o==origin;if(!seen)origins.add(origin);if(text(s["auth"]["credential_ref"],128))refs.add(s["auth"]["credential_ref"]);for(const char* key:{"signing_credential_ref","previous_signing_credential_ref"})if(text(s[key],128))refs.add(s[key]);}
  request.clear();request.set(config["relay"]);request["subscriptions"]=subs;request["allowed_origins"]=origins;request["webhook_credential_refs"]=refs;
  if(!relayPolicy(request.as<JsonObjectConst>())||measureJson(candidate["secrets"])>8192){error="enroll_capacity_or_policy";break;}
  if(p["subscription"]["destination"]["kind"]=="mcp_events"&&strcmp(op,"revoke")&&r["verification"]["verified"]!=true){
   auto verify=r["verification"].to<JsonObject>();verify["enrollment_id"]=p["enrollment_id"];verify["subscription"]=subs[subs.size()-1];verify["secrets"]=candidate["secrets"];verify["allowed_origins"]=origins;verify["webhook_credential_refs"]=refs;verify["attempts"]=0;verify["not_before_ms"]=0;relayVerificationPrepared=false;if(measureJson(journal.state)>StateMax-4096){r.remove("verification");error="enroll_journal_capacity";break;}response.clear();request.clear();return persist(Phase::Ready);
  }
  if(!approved){response.clear();request.clear();phase=Phase::Ready;return 1;}
  // Copy only changed registration fields so the enrollment queue keeps its
  // references until the result is set, and preserve all other node state.
  if(!strcmp(op,"revoke"))r["health"].remove(subid);
  r["subscriptions"]=subs;r["allowed_origins"]=origins;r["webhook_credential_refs"]=refs;r["secrets"]=candidate["secrets"];r["subscription_revision"]=r["subscription_revision"].as<uint64_t>()+1;r.remove("verification");ok=true;
 }while(false);
 // Decrypted material is never copied into requests, diagnostics or results.
 response.clear();request.clear();terminal(*this,e,ok?"applied":"invalid",ok?"":error);enrollmentPollAt=0;if(ok)relayReady=false;for(auto& at:relayPollAt)at=0;relayHealthAt=0;return persist(Phase::Ready);
}

int Node::enrollmentVerify(){
 auto v=journal.state["relay"]["verification"].as<JsonObject>();if(v["verified"]==true)return 0;
 if(net.wall(now())*1000<v["not_before_ms"].as<uint64_t>())return 0;
 if((v["attempts"]|0)>=5){for(auto e:journal.state["relay"]["enrollments"].as<JsonArray>())if(e["payload"]["enrollment_id"]==v["enrollment_id"])terminal(*this,e,"invalid","callback_verification_failed");journal.state["relay"].remove("verification");enrollmentPollAt=0;return persist(Phase::Ready);}
 if(!relayVerificationPrepared){uint8_t bytes[32];auto rc=entropy.fill(entropy.context,bytes,32);if(rc==RISC_ENTROPY_RETAINED)return retained();if(rc)return 0;char challenge[65];hexEncode(bytes,32,challenge);crypto_wipe(bytes,32);v["challenge"]=challenge;char id[128];snprintf(id,sizeof(id),"verify:%.32s:%llu",challenge,static_cast<unsigned long long>(now()));v["id"]=id;v["expires_at"]=net.wall(now())*1000+60000;relayVerificationPrepared=true;return persist(Phase::Ready);}
 action=Action::RelayVerify;request.clear();request["type"]="verification";request["challenge"]=v["challenge"];relayYield=true;return send(false);
}
int Node::enrollmentVerified(){
 auto v=journal.state["relay"]["verification"].as<JsonObject>();const char* expected=v["challenge"]|"";bool parsed=net.outcome==0&&net.info.status_code>=200&&net.info.status_code<300&&parse(response,net.body,net.size);const char* actual=parsed?(response["challenge"]|""):"";unsigned difference=strlen(actual)^strlen(expected);for(unsigned i=0;i<64;++i)difference|=(i<strlen(actual)?actual[i]:0)^(i<strlen(expected)?expected[i]:0);
 if(!difference&&strlen(expected)==64&&net.wall(now())*1000<v["expires_at"].as<uint64_t>()){v["verified"]=true;enrollmentPollAt=0;}
 else{v["attempts"]=(v["attempts"]|0)+1;v["not_before_ms"]=net.wall(now())*1000+5000;copy(relayError,sizeof(relayError),"callback_challenge_failed");}
 v.remove("challenge");relayVerificationPrepared=false;response.clear();return persist(Phase::Ready);
}
}
