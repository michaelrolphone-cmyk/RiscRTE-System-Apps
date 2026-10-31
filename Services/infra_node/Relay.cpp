#include "Node.h"
#include "Digest.h"
namespace Infra {
static void hmac(const char* key,const char* timestamp,const char* body,size_t size,char hex[65]){
 uint8_t pad[64]{},inner[32];dtls_sha256_ctx c;const size_t n=strlen(key);if(n>64){dtls_sha256_init(&c);dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(key),n);dtls_sha256_final(pad,&c);}else memcpy(pad,key,n);
 for(auto& b:pad)b^=0x36;dtls_sha256_init(&c);dtls_sha256_update(&c,pad,64);dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(timestamp),strlen(timestamp));const uint8_t dot='.';dtls_sha256_update(&c,&dot,1);dtls_sha256_update(&c,reinterpret_cast<const uint8_t*>(body),size);dtls_sha256_final(inner,&c);
 for(auto& b:pad)b^=0x36^0x5c;dtls_sha256_init(&c);dtls_sha256_update(&c,pad,64);dtls_sha256_update(&c,inner,32);dtls_sha256_final(inner,&c);for(unsigned i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",inner[i]);Portal::wipe(pad,sizeof(pad));Portal::wipe(inner,sizeof(inner));Portal::wipe(&c,sizeof(c));
}
static JsonObject busRequest(Node& n,Action a,const char* op){n.action=a;n.request.clear();n.request["protocol_version"]="relay/1";n.request["operation"]=op;return n.request["arguments"].to<JsonObject>();}
static void publication(Node& n,Action a,const char* channel,const char* key,JsonObjectConst payload){auto p=busRequest(n,a,"publish");p["channel"]=channel;p["type"]=channel;p["version"]=1;p["idempotency_key"]=key;p["payload"]=payload;}
static bool secretsReady(Node& n,JsonObjectConst s){return n.credential(s["auth"]["credential_ref"]|"")[0]&&n.credential(s["signing_credential_ref"]|"")[0];}
int Node::relayStep(){
 auto policy=config["relay"].as<JsonObjectConst>();if(policy.isNull()||relayBlocked||now()<relayRetryAt)return 0;
 if(!relayPolicy(policy)){relayBlocked=true;copy(relayError,sizeof(relayError),"relay_policy_invalid");return 0;}
 auto r=journal.state["relay"].as<JsonObject>();
 if(r.isNull()){r=journal.state["relay"].to<JsonObject>();r["schema_version"]=1;r["subscriptions"]=policy["subscriptions"];r["subscription_revision"]=0;r["attachments"].to<JsonArray>();r["pending"]=nullptr;r["allowed_origins"]=policy["allowed_origins"];r["webhook_credential_refs"]=policy["webhook_credential_refs"];r["secrets"].to<JsonObject>();for(const char* k:{"subscriptions","allowed_origins","webhook_credential_refs"})if(r[k].isNull())r[k].to<JsonArray>();return persist(Phase::Ready);}
 if(r["schema_version"]!=1||r["subscriptions"].size()>4||r["attachments"].size()>5){relayBlocked=true;copy(relayError,sizeof(relayError),"relay_state_invalid");return 0;}
 if(!relayReady){busRequest(*this,Action::RelayConfig,"configuration");relayYield=true;return send(false);}
 if(now()>=relayHealthAt){
  response.clear();response["relay_id"]=policy["relay_id"];response["uptime_ms"]=now();response["queue_depth"]=r["pending"].isNull()?0:1;response["last_error"]=relayError;response["subscriptions"]=r["subscriptions"].size();for(auto a:r["attachments"].as<JsonArrayConst>())response["last_seq"][a["channel"]|""]=a["cursor"];
  char key[160];snprintf(key,sizeof(key),"health:%.32s:%llu",boot,static_cast<unsigned long long>(now()));publication(*this,Action::RelayHealth,"relay.health",key,response.as<JsonObjectConst>());relayYield=true;return send(false);
 }
 if(auto step=enrollmentStep())return step;
 auto p=r["pending"].as<JsonObject>();
 if(!p.isNull()){
  if(!integer(p["attachment"],0,r["attachments"].size()-1)||!integer(p["cursor"],0,9007199254740991ULL)||p["targets"].size()>4){relayBlocked=true;copy(relayError,sizeof(relayError),"relay_pending_invalid");return 0;}
  relayIndex=p["attachment"];unsigned index=p["index"]|0;
  if(index>=p["targets"].size()){
   auto a=busRequest(*this,Action::RelayAck,"ack");a["attachment_id"]=r["attachments"][relayIndex]["id"];a["event_cursor"]=p["cursor"];relayYield=true;return send(false);
  }
  if(net.wall(now())*1000<p["not_before_ms"].as<uint64_t>())return 0;
  auto s=p["targets"][index].as<JsonObjectConst>();if(!relaySubscription(s,journal.state["relay"]["allowed_origins"],journal.state["relay"]["webhook_credential_refs"])||!secretsReady(*this,s)){copy(relayError,sizeof(relayError),"relay_secret_or_subscription_invalid");relayRetryAt=now()+30000;return 0;}
  const char* message=p["event"]["id"]|"";
  if((p["attempts"]|0)>=5){
   response.clear();response["relay_id"]=policy["relay_id"];response["subscription_id"]=s["id"];response["message_id"]=message;response["channel"]=p["event"]["channel"];response["seq"]=p["event"]["seq"];response["attempts"]=p["attempts"];response["reason"]="webhook_delivery_failed";
   char key[65];canonicalDigest(response.as<JsonObjectConst>(),key);publication(*this,Action::RelayDeadletter,"relay.deadletter",key,response.as<JsonObjectConst>());relayYield=true;return send(false);
  }
  action=Action::RelayPost;request.clear();request["protocol_version"]="relay-delivery/1";request["relay_id"]=policy["relay_id"];request["subscription_id"]=s["id"];auto e=p["event"];request["message_id"]=e["id"];for(const char* k:{"channel","seq","type","correlation","created"})request[k]=e[k];
  char summary[256];snprintf(summary,sizeof(summary),"%s on %s",e["type"]|"event",e["channel"]|"channel");for(char* c=summary;*c;++c)if(static_cast<unsigned char>(*c)<32||*c==127)*c=' ';request["summary"]=summary;if(e["payload_omitted"]==true)request["payload_omitted"]=true;else request["payload"]=e["payload"];
  relayYield=true;return send(false);
 }

 for(unsigned k=0;k<r["attachments"].size();++k){unsigned i=(relayIndex+1+k)%r["attachments"].size();if(now()<relayPollAt[i])continue;relayIndex=i;auto a=busRequest(*this,Action::RelayPoll,"receive");a["attachment_id"]=r["attachments"][i]["id"];auto summaries=a["subscriptions"].to<JsonArray>();for(auto s:r["subscriptions"].as<JsonArrayConst>()){auto t=summaries.add<JsonObject>();for(const char* key:{"channel","match","enabled"})t[key]=s[key];}auto fields=a["fields"].to<JsonArray>();for(auto s:r["subscriptions"].as<JsonArrayConst>())for(auto f:s["match"]["filters"].as<JsonArrayConst>()){bool has=false;for(auto old:fields)has|=old==f["path"];if(!has&&fields.size()<8)fields.add(f["path"]);}relayYield=true;return send(false);}
 return 0;
}
int Node::relayTransmit(){
 const bool post=action==Action::RelayPost;auto policy=config["relay"].as<JsonObjectConst>();auto p=journal.state["relay"]["pending"];auto sub=p["targets"][p["index"]|0].as<JsonObjectConst>();
 if(post&&(!relaySubscription(sub,journal.state["relay"]["allowed_origins"],journal.state["relay"]["webhook_credential_refs"])||!secretsReady(*this,sub)))return relayFailure("relay_secret_missing");
 const char* bearer=credential(policy["credential_ref"]|"");if(!post&&!bearer[0]){relayBlocked=true;return relayFailure("relay_bus_credential_missing");}
 const size_t bytes=measureJson(request);if(request.overflowed()||bytes>12288)return block(Phase::CapacityBlocked,"relay_request_bound");serializeJson(request,encoded,16385);
 char auth[1032]{},site[1032]{},signature[72]{},timestamp[24]{};risc_http_header_v2 headers[8]{};unsigned count=0;headers[count++]={"Content-Type","application/json"};headers[count++]={"Accept","application/json"};
 if(post){snprintf(timestamp,sizeof(timestamp),"%llu",static_cast<unsigned long long>(net.wall(now())));if(!net.wall(now()))return relayFailure("relay_clock_unavailable");strcpy(signature,"sha256=");hmac(credential(sub["signing_credential_ref"]),timestamp,encoded,bytes,signature+7);headers[count++]={"Idempotency-Key",p["event"]["id"]|""};headers[count++]={"X-Relay-Timestamp",timestamp};headers[count++]={"X-Relay-Signature",signature};headers[count++]={sub["auth"]["header"]|"",credential(sub["auth"]["credential_ref"]|"")};}
 else{snprintf(auth,sizeof(auth),"Bearer %s",bearer);headers[count++]={"Authorization",auth};const char* gate=credential(policy["site_credential_ref"]|"");if(gate[0]){snprintf(site,sizeof(site),"Bearer %s",gate);headers[count++]={"OAI-Sites-Authorization",site};}}
 const auto rc=net.begin(post?(sub["url"]|""):(policy["url"]|""),RISC_HTTP_POST,headers,count,encoded,bytes,post?4096:12288,action>=Action::RelayIdentity?25000:5000,config["service"]["time_server"],now());Portal::wipe(auth,sizeof(auth));Portal::wipe(site,sizeof(site));Portal::wipe(signature,sizeof(signature));
 if(rc==-2)return retained();if(rc<0)return relayFailure("relay_transport_open");if(rc)return 1;status.requested++;reason(Phase::Receive,post?"relay_webhook_in_flight":"relay_bus_in_flight");return 1;
}
int Node::relayFailure(const char* why){
 copy(relayError,sizeof(relayError),why);relayRetryAt=now()+5000;relayYield=true;phase=Phase::Ready;
 if(action==Action::RelayPost){auto p=journal.state["relay"]["pending"];const unsigned failures=(p["attempts"]|0)+1;p["attempts"]=failures;const uint32_t delays[]={5000,30000,120000,600000};p["not_before_ms"]=net.wall(now())*1000+(failures<=4?delays[failures-1]:0);return persist(Phase::Ready);}return 1;
}
int Node::relayReceived(){
 if(net.outcome||net.info.status_code<200||net.info.status_code>=300){if(action!=Action::RelayPost&&(net.info.status_code==401||net.info.status_code==403))relayBlocked=true;return relayFailure(net.outcome?"relay_network_failure":"relay_http_failure");}
 auto r=journal.state["relay"].as<JsonObject>();auto p=r["pending"].as<JsonObject>();
 if(action==Action::RelayPost){p["index"]=(p["index"]|0)+1;p["attempts"]=0;p["not_before_ms"]=0;relayError[0]=0;status.completed++;return persist(Phase::Ready);}
 if(!parse(response,net.body,net.size)||response["protocol_version"]!="relay/1"||response["status"]!="completed")return relayFailure("relay_response_invalid");
 auto data=response["result"].as<JsonObjectConst>();status.completed++;
 if(action>=Action::RelayIdentity)return enrollmentReceived(data);
 if(action==Action::RelayConfig){
  if(data["relay_id"]!=config["relay"]["relay_id"]||!data["attachments"].is<JsonArrayConst>()||data["attachments"].size()>5||!data["attachments"].size())return relayFailure("relay_configuration_mismatch");
  for(auto a:data["attachments"].as<JsonArrayConst>())if(!text(a["id"],128)||(!relayChannel(a["channel"]|"")&&a["channel"]!="relay.subscriptions"&&a["channel"]!="relay.enroll"))return relayFailure("relay_attachment_invalid");
  // Existing IDs must retain their channel. Append newly admitted control
  // attachments without rewriting cursors or pending delivery indices.
  unsigned additions=0;for(auto a:data["attachments"].as<JsonArrayConst>()){bool found=false;for(auto old:r["attachments"].as<JsonArrayConst>())found|=old["id"]==a["id"];if(!found)++additions;}if(r["attachments"].size()+additions>5)return relayFailure("relay_attachment_capacity");
  for(auto a:data["attachments"].as<JsonArrayConst>()){bool found=false;for(auto old:r["attachments"].as<JsonArrayConst>())if(old["id"]==a["id"]){if(old["channel"]!=a["channel"])return relayFailure("relay_attachment_changed");found=true;}if(!found){auto added=r["attachments"].as<JsonArray>().add<JsonObject>();added.set(a);added["cursor"]=0;}}
  if(r["allowed_origins"].isNull()){r["allowed_origins"]=config["relay"]["allowed_origins"];r["webhook_credential_refs"]=config["relay"]["webhook_credential_refs"];}
  relayReady=true;return persist(Phase::Ready);
 }
 if(action==Action::RelayHealth){relayHealthAt=now()+180000;phase=Phase::Ready;return 1;}
 if(action==Action::RelayDeadletter){p["index"]=(p["index"]|0)+1;p["attempts"]=0;p["not_before_ms"]=0;return persist(Phase::Ready);}
 if(action==Action::RelayAck){if(data["acknowledged"]!=true)return relayFailure("relay_ack_unconfirmed");r["attachments"][relayIndex]["cursor"]=p["cursor"];r["pending"]=nullptr;relayPollAt[relayIndex]=now();return persist(Phase::Ready);}
 if(action!=Action::RelayPoll)return relayFailure("relay_action_invalid");
 auto attachment=r["attachments"][relayIndex];if(!integer(data["event_cursor"],attachment["cursor"].as<uint64_t>(),9007199254740991ULL)||!data["events"].is<JsonArrayConst>()||data["events"].size()>1)return relayFailure("relay_cursor_or_batch_invalid");
 bool active=attachment["channel"]=="relay.subscriptions"||attachment["channel"]=="relay.enroll";for(auto s:r["subscriptions"].as<JsonArrayConst>())active|=s["enabled"]==true&&s["channel"]==attachment["channel"];relayPollAt[relayIndex]=now()+(active?5000:30000);
 if(data["event_cursor"]==attachment["cursor"]&&data["events"].size()==0){phase=Phase::Ready;return 1;}
 p=r["pending"].to<JsonObject>();p["attachment"]=relayIndex;p["cursor"]=data["event_cursor"];p["index"]=0;p["attempts"]=0;p["not_before_ms"]=0;auto targets=p["targets"].to<JsonArray>();
 if(data["events"].size()){
  auto e=data["events"][0].as<JsonObjectConst>();if(!text(e["id"],128)||!text(e["type"],128)||e["channel"]!=attachment["channel"]||e["kind"]!="event"||!integer(e["seq"],attachment["cursor"].as<uint64_t>()+1,data["event_cursor"].as<uint64_t>())||measureJson(e)>12288){r["pending"]=nullptr;return relayFailure("relay_event_invalid");}
  p["event"]=e;
  if(e["channel"]=="relay.enroll"){if(!enrollmentQueue(e)){r["pending"]=nullptr;relayPollAt[relayIndex]=now()+5000;phase=Phase::Ready;return 1;}}
  else if(e["channel"]=="relay.subscriptions"){
   // Enrollment is the only control path for webhook registrations.
   // Drain the old channel without applying unsigned replacement tables.
   copy(relayError,sizeof(relayError),"relay_use_approved_enrollment");
  }else for(auto s:r["subscriptions"].as<JsonArrayConst>())if(relayMatches(s,e))targets.add(s);
 }
 // Full event, selected subscription revisions and completion index are saved
 // before any POST or ACK. Power loss can repeat a delivery with the same key;
 // it cannot skip a not-yet-completed subscriber.
 return persist(Phase::Ready);
}
}
