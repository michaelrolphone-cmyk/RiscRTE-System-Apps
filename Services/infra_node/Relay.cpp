#include "Node.h"
#include "RelayWire.h"
namespace Infra {
static JsonObject busRequest(Node& n,Action a,const char* op){n.action=a;n.request.clear();n.request["protocol_version"]="relay/1";n.request["operation"]=op;return n.request["arguments"].to<JsonObject>();}
static void publication(Node& n,Action a,const char* channel,const char* key,JsonObjectConst payload){auto p=busRequest(n,a,"publish");p["channel"]=channel;p["type"]=channel;p["version"]=(!strcmp(channel,"relay.health")||!strcmp(channel,"relay.deadletter"))?2:1;p["idempotency_key"]=key;p["payload"]=payload;}
static bool secretsReady(Node& n,JsonObjectConst s){return (s["auth"].isNull()||n.credential(s["auth"]["credential_ref"]|"")[0])&&(s["signing_credential_ref"].isNull()||n.credential(s["signing_credential_ref"]|"")[0]);}
static bool isEnrollment(Action a){return a==Action::RelayIdentity||a==Action::RelayEnrollmentReview||a==Action::RelayEnrollmentComplete||a==Action::RelayEnrollmentVerified;}
static void projections(JsonObject a,JsonArrayConst subscriptions,const char* only=""){
 auto summaries=a["subscriptions"].to<JsonArray>();auto fields=a["fields"].to<JsonArray>();for(auto s:subscriptions){if(*only&&s["id"]!=only)continue;auto t=summaries.add<JsonObject>();for(const char* key:{"id","channel","channels","match","enabled","expires_at","revoked"})if(!s[key].isNull())t[key]=s[key];for(auto f:s["match"]["filters"].as<JsonArrayConst>()){bool has=false;for(auto old:fields)has|=old==f["path"];if(!has&&fields.size()<8)fields.add(f["path"]);}}
}
static JsonObjectConst target(JsonObjectConst p){return p["targets"][p["index"]|0].as<JsonObjectConst>();}
int Node::relayPost(JsonObjectConst event){
 auto p=relayPending();auto s=target(p);action=Action::RelayPost;request.clear();
 if(relayMcp(s)){request["eventId"]=event["id"];request["name"]=s["destination"]["event_name"];char stamp[32];relayIso(event["created"].as<uint64_t>(),stamp);request["timestamp"]=stamp;request["cursor"]=nullptr;relayBody(request["data"].to<JsonObject>(),config["relay"]["relay_id"],s,event);}
 else relayBody(request.to<JsonObject>(),config["relay"]["relay_id"],s,event);
 relayYield=true;return send(false);
}
int Node::relayStep(){
 auto policy=config["relay"].as<JsonObjectConst>();if(policy.isNull()||relayBlocked||now()<relayRetryAt)return 0;
 if(!relayPolicy(policy)){relayBlocked=true;copy(relayError,sizeof(relayError),"relay_policy_invalid");return 0;}
 auto r=journal.state["relay"].as<JsonObject>();
 if(r.isNull()){r=journal.state["relay"].to<JsonObject>();r["schema_version"]=1;r["subscriptions"]=policy["subscriptions"];r["subscription_revision"]=0;r["attachments"].to<JsonArray>();r["pending"]=nullptr;r["allowed_origins"]=policy["allowed_origins"];r["webhook_credential_refs"]=policy["webhook_credential_refs"];r["secrets"].to<JsonObject>();r["deliveries"].to<JsonObject>();for(const char* k:{"subscriptions","allowed_origins","webhook_credential_refs"})if(r[k].isNull())r[k].to<JsonArray>();return persist(Phase::Ready);}
 if(r["schema_version"]!=1||r["subscriptions"].size()>RelaySubscriptionsMax||r["attachments"].size()>RelayAttachmentsMax||r["deliveries"].size()>RelaySubscriptionsMax){relayBlocked=true;copy(relayError,sizeof(relayError),"relay_state_invalid");return 0;}
 if(!relayReady){relayUpgradePending=!r["pending"].isNull()&&r["custody_version"]!=2;auto a=busRequest(*this,Action::RelayConfig,"configuration");if(r["pending"].isNull()){a["custody_version"]=2;auto subs=a["subscriptions"].to<JsonArray>();for(auto s:r["subscriptions"].as<JsonArrayConst>()){auto v=subs.add<JsonObject>();v["id"]=s["id"];auto channels=v["channels"].to<JsonArray>();if(s["channels"].is<JsonArrayConst>())channels.set(s["channels"]);else channels.add(s["channel"]);}}relayYield=true;return send(false);}
 if(now()>=relayHealthAt){
  response.clear();response["relay_id"]=policy["relay_id"];response["uptime_ms"]=now();response["queue_depth"]=r["deliveries"].size()+(r["pending"].isNull()?0:1);response["last_error"]=relayError[0]?relayError:(journal.state["protocol_rejections"]["last_error"]|"");response["subscriptions"]=r["subscriptions"].size();
  for(auto a:r["attachments"].as<JsonArrayConst>()){const char* c=a["channel"]|"";uint64_t cursor=a["cursor"];auto seq=response["last_seq"][c];if(seq.isNull()||cursor<seq.as<uint64_t>())response["last_seq"][c]=cursor;}
  auto health=response["subscription_health"].to<JsonArray>();for(auto s:r["subscriptions"].as<JsonArrayConst>()){auto h=health.add<JsonObject>();h["id"]=s["id"];h["enabled"]=relayEnabled(s,net.wall(now())*1000);auto old=r["health"][s["id"]|""];h["last_delivered_seq"]=old["last_delivered_seq"].as<uint64_t>();h["consecutive_failures"]=old["consecutive_failures"].as<unsigned>();h["last_error_code"]=old["last_error_code"]|"";}
  char key[160];snprintf(key,sizeof(key),"health:%.32s:%llu",boot,static_cast<unsigned long long>(now()));publication(*this,Action::RelayHealth,"relay.health",key,response.as<JsonObjectConst>());relayYield=true;return send(false);
 }
 if(auto step=enrollmentStep())return step;
 // All destinations and migrated v1 custody use this same delivery transition
 // loop. A slot owns one subscription; another slot can run during its backoff.
 const unsigned fair=relayFair;
 for(unsigned k=0;k<=r["subscriptions"].size();++k){
  if(k==0){if(r["pending"].isNull())continue;relaySlot[0]=0;}
  else {unsigned i=(fair+k-1)%r["subscriptions"].size();const char* id=r["subscriptions"][i]["id"]|"";if(r["deliveries"][id].isNull())continue;copy(relaySlot,sizeof(relaySlot),id);relayFair=(i+1)%r["subscriptions"].size();}
  auto p=relayPending();if(!integer(p["attachment"],0,r["attachments"].size()-1)||!integer(p["cursor"],0,9007199254740991ULL)||p["targets"].size()>RelaySubscriptionsMax){copy(relayError,sizeof(relayError),"relay_pending_invalid");continue;}
  relayIndex=p["attachment"];unsigned index=p["index"]|0;
  if(!p["receipt"].isNull()){char key[65];canonicalDigest(p["receipt"],key);publication(*this,Action::RelayReceipt,"relay.delivery",key,p["receipt"]);relayYield=true;return send(false);}
  if(index>=p["targets"].size()){
   auto a=busRequest(*this,Action::RelayAck,"ack");a["attachment_id"]=r["attachments"][relayIndex]["id"];a["event_cursor"]=p["cursor"];relayYield=true;return send(false);
  }
  if(net.wall(now())*1000<p["not_before_ms"].as<uint64_t>())continue;
  auto s=target(p);if(!relaySubscription(s,r["allowed_origins"],r["webhook_credential_refs"])||!secretsReady(*this,s)){p["terminal"]=true;p["error_code"]="relay_secret_or_subscription_invalid";}
  if(!relayEnabled(s,net.wall(now())*1000)){p["terminal"]=true;p["error_code"]="subscription_expired_or_revoked";}
  const char* message=p["event"]["id"]|"";
  if((p["attempts"]|0)>=5||p["terminal"]==true){
   response.clear();response["relay_id"]=policy["relay_id"];response["subscription_id"]=s["id"];response["message_id"]=message;response["channel"]=p["event"]["channel"];response["seq"]=p["event"]["seq"];response["attempts"]=p["attempts"];response["reason"]=p["error_code"]|"webhook_delivery_failed";
   char key[65];canonicalDigest(response.as<JsonObjectConst>(),key);publication(*this,Action::RelayDeadletter,"relay.deadletter",key,response.as<JsonObjectConst>());relayYield=true;return send(false);
  }
  if(p["event_digest"].is<const char*>()){auto a=busRequest(*this,Action::RelayLoad,"event");a["attachment_id"]=r["attachments"][relayIndex]["id"];a["message_id"]=message;auto paths=a["fields"].to<JsonArray>();for(auto f:s["match"]["filters"].as<JsonArrayConst>())paths.add(f["path"]);relayYield=true;return send(false);}
  return relayPost(p["event"]);
 }
 // No matching slot owns this subscription until the complete selected event
 // identity is committed. Independent Bus attachments retain unacknowledged data.
 for(unsigned k=0;k<r["attachments"].size();++k){unsigned i=(relayIndex+1+k)%r["attachments"].size();auto attachment=r["attachments"][i];const char* id=attachment["subscription_id"]|"";if((r["custody_version"]==2&&!*id&&relayChannel(attachment["channel"]|""))||now()<relayPollAt[i]||(*id&&!r["deliveries"][id].isNull()))continue;
  if(*id){auto s=find(r["subscriptions"],"id",id);if(s.isNull()||!relayEnabled(s,net.wall(now())*1000))continue;}
  if(!*id&&!r["pending"].isNull())continue;relayIndex=i;copy(relaySlot,sizeof(relaySlot),id);auto a=busRequest(*this,Action::RelayPoll,"receive");a["attachment_id"]=attachment["id"];projections(a,r["subscriptions"],id);relayYield=true;return send(false);
 }
 return 0;
}
int Node::relayTransmit(){
 const bool post=action==Action::RelayPost||action==Action::RelayVerify;auto policy=config["relay"].as<JsonObjectConst>();auto p=relayPending();auto sub=action==Action::RelayVerify?journal.state["relay"]["verification"]["subscription"].as<JsonObjectConst>():target(p);
 auto scope=action==Action::RelayVerify?journal.state["relay"]["verification"].as<JsonObjectConst>():journal.state["relay"].as<JsonObjectConst>();
 if(post&&(!relaySubscription(sub,scope["allowed_origins"],scope["webhook_credential_refs"])||!secretsReady(*this,sub)))return relayFailure("relay_secret_missing");
 const char* bearer=credential(policy["credential_ref"]|"");if(!post&&!bearer[0]){relayBlocked=true;return relayFailure("relay_bus_credential_missing");}
 const size_t bytes=measureJson(request);if(request.overflowed()||bytes>16384)return relayFailure("relay_request_bound");serializeJson(request,encoded,16385);
 char auth[1032]{},site[1032]{},signature[104]{},timestamp[24]{};risc_http_header_v2 headers[8]{};unsigned count=0;headers[count++]={"Content-Type","application/json"};headers[count++]={"Accept","application/json"};
 if(post){snprintf(timestamp,sizeof(timestamp),"%llu",static_cast<unsigned long long>(net.wall(now())));if(!net.wall(now()))return relayFailure("relay_clock_unavailable");const char* id=action==Action::RelayVerify?journal.state["relay"]["verification"]["id"]|"":p["event"]["id"]|"";
  headers[count++]={"Idempotency-Key",id};
  if(relayMcp(sub)){
   if(!relaySignature(credential(sub["signing_credential_ref"]|""),id,timestamp,encoded,bytes,signature))return relayFailure("relay_signing_key_invalid");
   if(sub["rotation_until"].as<uint64_t>()>net.wall(now())*1000&&text(sub["previous_signing_credential_ref"],128)){signature[47]=' ';if(!relaySignature(credential(sub["previous_signing_credential_ref"]),id,timestamp,encoded,bytes,signature+48))return relayFailure("relay_signing_key_invalid");}
   headers[count++]={"webhook-id",id};headers[count++]={"webhook-timestamp",timestamp};headers[count++]={"webhook-signature",signature};headers[count++]={"X-MCP-Subscription-Id",sub["id"]|""};
  }else if(text(sub["signing_credential_ref"],128)){uint8_t mac[32];const char* key=credential(sub["signing_credential_ref"]);relayMac(reinterpret_cast<const uint8_t*>(key),strlen(key),nullptr,timestamp,encoded,bytes,mac);strcpy(signature,"sha256=");for(unsigned i=0;i<32;++i)snprintf(signature+7+2*i,3,"%02x",mac[i]);Portal::wipe(mac,sizeof(mac));headers[count++]={"X-Relay-Timestamp",timestamp};headers[count++]={"X-Relay-Signature",signature};}
  if(!sub["auth"].isNull()){if(count>=8)return relayFailure("relay_header_capacity");headers[count++]={sub["auth"]["header"]|"",credential(sub["auth"]["credential_ref"]|"")};}
 }else{snprintf(auth,sizeof(auth),"Bearer %s",bearer);headers[count++]={"Authorization",auth};const char* gate=credential(policy["site_credential_ref"]|"");if(gate[0]){snprintf(site,sizeof(site),"Bearer %s",gate);headers[count++]={"OAI-Sites-Authorization",site};}}
 const auto rc=net.begin(post?(sub["url"]|""):(policy["url"]|""),post&&relayMcp(sub)?RISC_HTTP_POST_PUBLIC:RISC_HTTP_POST,headers,count,encoded,bytes,post?4096:16384,isEnrollment(action)?25000:5000,config["service"]["time_server"],now());Portal::wipe(auth,sizeof(auth));Portal::wipe(site,sizeof(site));Portal::wipe(signature,sizeof(signature));
 if(rc==-2)return retained();if(rc<0)return relayFailure("relay_transport_open");if(rc)return 1;status.requested++;reason(Phase::Receive,post?"relay_webhook_in_flight":"relay_bus_in_flight");return 1;
}
static void receipt(Node& n,bool accepted,const char* error){auto p=n.relayPending();auto s=target(p);unsigned attempts=(p["attempts"]|0)+1;p["attempts"]=attempts;auto r=p["receipt"].to<JsonObject>();r["relay_id"]=n.config["relay"]["relay_id"];r["subscription_id"]=s["id"];r["message_id"]=p["event"]["id"];char id[65];relayInvocationId(s,p["event"],id);r["invocation_id"]=id;r["attempt"]=attempts;r["http_status"]=n.net.info.status_code;r["observed_at"]=n.net.wall(n.now())*1000;p["accepted"]=accepted;p["error_code"]=error;auto h=n.journal.state["relay"]["health"][s["id"]|""];h["consecutive_failures"]=accepted?0:(h["consecutive_failures"]|0)+1;h["last_error_code"]=error;if(accepted)h["last_delivered_seq"]=p["event"]["seq"];}
int Node::relayFailure(const char* why){
 copy(relayError,sizeof(relayError),why);relayYield=true;phase=Phase::Ready;
 if(action==Action::RelayVerify){net.outcome=-1;return enrollmentVerified();}
 if(action==Action::RelayPost){auto p=relayPending();receipt(*this,false,why);if(relayMcp(target(p))&&(net.info.status_code==410||net.info.status_code==413)){p["terminal"]=true;p["error_code"]=net.info.status_code==410?"callback_gone":"payload_rejected";if(net.info.status_code==410)for(auto sub:journal.state["relay"]["subscriptions"].as<JsonArray>())if(sub["id"]==target(p)["id"])sub["enabled"]=false;}const unsigned failures=p["attempts"];const uint32_t delays[]={5000,30000,120000,600000};p["not_before_ms"]=net.wall(now())*1000+(failures<=4?delays[failures-1]:0);return persist(Phase::Ready);}
 relayRetryAt=now()+5000;return 1;
}
int Node::relayReceived(){
 if(action==Action::RelayVerify)return enrollmentVerified();
 if(net.outcome||net.info.status_code<200||net.info.status_code>=300){if(action!=Action::RelayPost&&action!=Action::RelayVerify&&(net.info.status_code==401||net.info.status_code==403))relayBlocked=true;return relayFailure(net.outcome?"relay_network_failure":"relay_http_failure");}
 auto r=journal.state["relay"].as<JsonObject>();auto p=relayPending();
 if(action==Action::RelayPost){receipt(*this,true,"");relayError[0]=0;status.completed++;return persist(Phase::Ready);}
 if(!parse(response,net.body,net.size)||response["protocol_version"]!="relay/1"||response["status"]!="completed")return relayFailure("relay_response_invalid");
 auto data=response["result"].as<JsonObjectConst>();status.completed++;
 if(isEnrollment(action))return enrollmentReceived(data);
 if(action==Action::RelayConfig){
  if(data["relay_id"]!=config["relay"]["relay_id"]||!data["attachments"].is<JsonArrayConst>()||data["attachments"].size()>RelayAttachmentsMax||!data["attachments"].size())return relayFailure("relay_configuration_mismatch");
  for(auto a:data["attachments"].as<JsonArrayConst>())if(!text(a["id"],128)||(!relayChannel(a["channel"]|"")&&a["channel"]!="relay.subscriptions"&&a["channel"]!="relay.enroll")||(!a["subscription_id"].isNull()&&!text(a["subscription_id"],64)))return relayFailure("relay_attachment_invalid");
  // Configuration can remove revoked grants. Rebind each durable slot by the
  // original attachment ID before replacing indices, preserving local cursors.
  JsonDocument next;next["attachments"]=data["attachments"];auto incoming=next["attachments"].as<JsonArray>();unsigned ni=0;
  for(auto a:incoming){for(unsigned j=0;j<ni;++j)if(incoming[j]["id"]==a["id"])return relayFailure("relay_attachment_duplicate");++ni;for(auto old:r["attachments"].as<JsonArrayConst>())if(old["id"]==a["id"]){if(old["channel"]!=a["channel"]||old["subscription_id"]!=a["subscription_id"])return relayFailure("relay_attachment_changed");a["cursor"]=old["cursor"];}if(a["cursor"].isNull())a["cursor"]=0;}
  auto map=[&](JsonObject pending)->bool{if(pending.isNull())return true;const char* id=r["attachments"][pending["attachment"].as<unsigned>()]["id"]|"";unsigned i=0;for(auto a:incoming){if(a["id"]==id){pending["attachment"]=i;return true;}++i;}return false;};
  // Validate the whole mapping first; a partial remap is never committed.
  JsonDocument custody;custody["pending"]=r["pending"];custody["deliveries"]=r["deliveries"];if(!map(custody["pending"].as<JsonObject>()))return relayFailure("relay_attachment_in_custody");for(auto kv:custody["deliveries"].as<JsonObject>())if(!map(kv.value().as<JsonObject>()))return relayFailure("relay_attachment_in_custody");
  r["pending"]=custody["pending"];r["deliveries"]=custody["deliveries"];r["attachments"]=incoming;for(auto& at:relayPollAt)at=0;
  if(data["custody_version"]==2){r["custody_version"]=2;for(unsigned i=0;i<r["attachments"].size();++i){auto a=r["attachments"][i];if(a["subscription_id"].isNull()&&relayChannel(a["channel"]|""))relayPollAt[i]=UINT64_MAX;}}
  if(r["allowed_origins"].isNull()){r["allowed_origins"]=config["relay"]["allowed_origins"];r["webhook_credential_refs"]=config["relay"]["webhook_credential_refs"];}
  relayReady=true;return persist(Phase::Ready);
 }
 if(action==Action::RelayHealth){relayHealthAt=now()+180000;phase=Phase::Ready;return 1;}
 if((action==Action::RelayLoad||action==Action::RelayPoll)&&data["disabled"]==true){for(auto s:r["subscriptions"].as<JsonArray>())if(s["id"]==relaySlot)s["enabled"]=false;if(!p.isNull()){p["terminal"]=true;p["error_code"]="subscription_access_revoked";}return persist(Phase::Ready);}
 if(action==Action::RelayLoad){char hash[65];canonicalDigest(data["event"],hash);if(strcmp(hash,p["event_digest"]|"")){p["terminal"]=true;p["error_code"]="relay_event_identity_changed";return persist(Phase::Ready);}return relayPost(data["event"]);}
 if(action==Action::RelayReceipt){if(p["accepted"]==true){p["index"]=(p["index"]|0)+1;p["attempts"]=0;p["not_before_ms"]=0;}p.remove("receipt");p.remove("accepted");return persist(Phase::Ready);}
 if(action==Action::RelayDeadletter){p["index"]=(p["index"]|0)+1;p["attempts"]=0;p["not_before_ms"]=0;p.remove("terminal");p.remove("error_code");return persist(Phase::Ready);}
 if(action==Action::RelayAck){if(data["acknowledged"]!=true)return relayFailure("relay_ack_unconfirmed");r["attachments"][relayIndex]["cursor"]=p["cursor"];if(relaySlot[0])r["deliveries"].remove(relaySlot);else {r["pending"]=nullptr;if(relayUpgradePending){relayReady=false;relayUpgradePending=false;}}relayPollAt[relayIndex]=now();return persist(Phase::Ready);}
 if(action!=Action::RelayPoll)return relayFailure("relay_action_invalid");
 if(!data["attachment_id"].isNull()){
  bool found=false;for(unsigned i=0;i<r["attachments"].size();++i){auto a=r["attachments"][i];if(a["id"]==data["attachment_id"]&&relaySlot[0]&&a["subscription_id"]==relaySlot){relayIndex=i;found=true;break;}}
  if(!found)return relayFailure("relay_selected_attachment_invalid");
 }
 auto attachment=r["attachments"][relayIndex];if(!integer(data["event_cursor"],attachment["cursor"].as<uint64_t>(),9007199254740991ULL)||!data["events"].is<JsonArrayConst>()||data["events"].size()>1)return relayFailure("relay_cursor_or_batch_invalid");
 bool active=attachment["channel"]=="relay.subscriptions"||attachment["channel"]=="relay.enroll";for(auto s:r["subscriptions"].as<JsonArrayConst>())active|=relayEnabled(s,net.wall(now())*1000)&&relayWatches(s,attachment["channel"]);relayPollAt[relayIndex]=now()+(active?1000:30000);
 if(data["event_cursor"]==attachment["cursor"]&&data["events"].size()==0){phase=Phase::Ready;return 1;}
 p=relaySlot[0]?r["deliveries"][relaySlot].to<JsonObject>():r["pending"].to<JsonObject>();p["attachment"]=relayIndex;p["cursor"]=data["event_cursor"];p["index"]=0;p["attempts"]=0;p["not_before_ms"]=0;auto targets=p["targets"].to<JsonArray>();
 if(data["events"].size()){
  auto e=data["events"][0].as<JsonObjectConst>();if(!text(e["id"],128)||!text(e["type"],128)||e["channel"]!=attachment["channel"]||e["kind"]!="event"||!integer(e["seq"],attachment["cursor"].as<uint64_t>()+1,data["event_cursor"].as<uint64_t>())||measureJson(e)>12288){if(relaySlot[0])r["deliveries"].remove(relaySlot);else r["pending"]=nullptr;return relayFailure("relay_event_invalid");}
  p["event"]=e;
  if(e["channel"]=="relay.enroll"){if(!enrollmentQueue(e)){r["pending"]=nullptr;relayPollAt[relayIndex]=now()+5000;phase=Phase::Ready;return 1;}}
  else if(e["channel"]=="relay.subscriptions")copy(relayError,sizeof(relayError),"relay_use_approved_enrollment");
  else {for(auto s:r["subscriptions"].as<JsonArrayConst>())if((!relaySlot[0]||s["id"]==relaySlot)&&relayMatches(s,e)&&relayEnabled(s,net.wall(now())*1000))targets.add(s);if(relaySlot[0]){char hash[65];canonicalDigest(e,hash);p["event_digest"]=hash;p["event"].remove("payload");p["event"].remove("fields");}}
 }
 if(measureJson(journal.state)>StateMax-2048){if(relaySlot[0])r["deliveries"].remove(relaySlot);else r["pending"]=nullptr;copy(relayError,sizeof(relayError),"relay_journal_capacity");phase=Phase::Ready;return 1;}
 return persist(Phase::Ready);
}
}
