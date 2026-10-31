#pragma once
#include "Policy.h"
#include "Journal.h"
#include "Transport.h"
#include "Station.h"
#include "Portal.h"
#include "RiscInfraNodeV1.h"
#include <RiscPlatformClockV1.h>
#include <RiscEntropyV1.h>
namespace Infra {
enum class Phase:uint32_t {Cold,Recover,Connect,Nonce,Ready,Send,Receive,Save,Backoff,Drain,Stopped,AuthBlocked,CapacityBlocked,StorageBlocked,Retained,ProtocolBlocked,SetupStart,SetupPortal,SetupSave,SetupFinish,SetupLeave,SetupJoin,SetupRetry};
enum class Action:uint32_t {None,Enroll,Open,Sync,PoolGet,PoolPut,OfferGet,OfferPut,Assignment,Plan,Claim,Prepare,Renew,Result,Stop,Observe,Maintain,Probe,Desired,Review,ProviderGet,DesiredReport,Inventory,ReceiptLookup,RotationPrepare,RotationActivate,RotationConfirm,Snapshot,Control,Progress,Heartbeat,BindingRenew,BindingResult,BindingStop,RejectPrepare,RelayConfig,RelayPoll,RelayPost,RelayDeadletter,RelayAck,RelayHealth,RelayIdentity,RelayEnrollmentReview,RelayEnrollmentComplete,RelayLoad,RelayReceipt,RelayVerify,RelayEnrollmentVerified};
class Node {
 public:
 Journal journal;Transport net;Portal portal;uint64_t setupDeadline=0;bool setupFailed=false,setupRecovery=false;infra_station_v1 station{};risc_platform_clock_api_v1 clock{};risc_entropy_v1 entropy{};
 Allocator configMemory{196608},exchangeMemory{262144},responseMemory{393216};JsonDocument config{&configMemory},request{&exchangeMemory},response{&responseMemory};
 risc_infra_memory_v1 memory{};risc_infra_status_v1 status{};Cadence cadence[8]{};char cursors[8][513]{};
 Phase phase=Phase::Cold,afterSave=Phase::Ready;Action action=Action::None;bool durable=false,loaded=false,rebootRecovery=true,drain=false,initialized=false;
 bool snapshotNeeded=true,snapshotComplete=false;unsigned snapshotStage=0;char snapshotAfter[129]{},controlHash[65]{};uint64_t controlSequence=0;
 uint64_t waitUntil=0,nextSync=0,nextInventory=0,leaseUntil=0,serial=0,saveSequence=0,sessionGeneration=0,cursor=0;
 unsigned failures=0,bindingIndex=0,fairness=0,inventoryIndex=0;bool poolStage=true,inventoryNeeded=true,externalJob=false,poolsEstablished=false,lastCapacity=false,receiptChecked=false;uint64_t inventoryRevision=0;
 char sessionId[128]{},boot[65]{},poolId[160]{},offerId[200]{},assignmentId[128]{},planId[128]{};
 struct Diagnostic{uint64_t at;uint32_t phase,pending;char reason[112];};static_assert(sizeof(Diagnostic)==128,"diagnostic ring slot");Diagnostic diagnostics[128]{};uint32_t diagnosticNext=0;
 bool relayReady=false,relayBlocked=false,relayYield=false,relayUpgradePending=false;unsigned relayIndex=0,relayFair=0;char relaySlot[65]{};uint64_t relayPollAt[RelayAttachmentsMax]{},relayHealthAt=0,relayRetryAt=0;char relayError[64]{};
 bool relayIdentityReady=false,relayVerificationPrepared=false;unsigned enrollmentIndex=0;uint64_t enrollmentPollAt=0;
 char* encoded=nullptr;bool bindingDisabled[8]{};
 uint64_t now()const{return clock.monotonic_ms(clock.context);}
 JsonObjectConst configuration()const{return journal.state["configuration"].is<JsonObjectConst>()?journal.state["configuration"].as<JsonObjectConst>():config["configuration"].as<JsonObjectConst>();}
 JsonArrayConst bindings()const{return configuration()["bindings"].as<JsonArrayConst>();}
 JsonObjectConst bindingAt(unsigned i)const{return bindings()[i].as<JsonObjectConst>();}
 JsonObjectConst endpoint(unsigned i)const{return find(config["endpoints"],"endpoint_policy_id",bindingAt(i)["endpoint_policy_id"]|"");}
 const char* credential(const char* ref)const{if(action==Action::RelayVerify&&journal.state["relay"]["verification"]["secrets"][ref].is<const char*>())return journal.state["relay"]["verification"]["secrets"][ref];return journal.state["relay"]["secrets"][ref].is<const char*>()?(journal.state["relay"]["secrets"][ref]|""):(journal.state["credentials"][ref]|"");}
 void reason(Phase p,const char* why){if(phase!=p||strcmp(status.reason,why)){auto& d=diagnostics[diagnosticNext++%128];d={};d.at=now();d.phase=static_cast<unsigned>(p);d.pending=journal.state["pending"].size();copy(d.reason,sizeof(d.reason),why);status.diagnostic_events=diagnosticNext;}phase=p;copy(status.reason,sizeof(status.reason),why);}
 int retained(){reason(Phase::Retained,"retained_fault");return INFRA_RETAINED;}
 int block(Phase p,const char* why){reason(p,why);return INFRA_BLOCKED;}
 void id(char* out,size_t size){snprintf(out,size,"infra:%.32s:%llu",boot,static_cast<unsigned long long>(++serial));}
 bool initialize(){if(initialized)return true;encoded=static_cast<char*>(std::malloc(16385));initialized=encoded&&net.initialize();return initialized;}
 int bootstrap(){
  if(phase!=Phase::Cold)return phase==Phase::Retained?-2:0;
  if(!initialize())return block(Phase::CapacityBlocked,"allocation_failed");
  uint32_t bytes=0;uint64_t revision=0;auto& b=journal.backend->base;
  auto r=b.stat(b.context,"node-policy.json",&bytes,&revision);if(r==RISC_APP_DATA_RETAINED)return retained();if(r==RISC_APP_DATA_BUSY)return 1;
  if(r||bytes>PolicyMax||!bytes)return block(Phase::StorageBlocked,"policy_missing_or_oversize");
  r=b.read(b.context,"node-policy.json",revision,encoded,16384,&bytes,&revision);if(r==RISC_APP_DATA_RETAINED)return retained();if(r==RISC_APP_DATA_BUSY||r==RISC_APP_DATA_STALE)return 1;
  if(r||!parse(config,encoded,bytes)||!policy(config))return block(Phase::StorageBlocked,"policy_invalid");
  r=journal.load();if(r==-2)return retained();if(r>0)return 1;if(r<0)return block(Phase::StorageBlocked,"state_missing_corrupt_or_schema");
  if(!journal.state["setup_policy"].isNull()){config.set(journal.state["setup_policy"]);if(!policy(config))return block(Phase::StorageBlocked,"saved_setup_policy_invalid");}
  if(!Infra::configuration(configuration(),config["endpoints"],config["service"]["contact_interval_ms"]))return block(Phase::StorageBlocked,"configuration_invalid");
  for(auto kv:journal.state["credentials"].as<JsonObjectConst>())if(!text(kv.value(),1024))return block(Phase::StorageBlocked,"credential_invalid");
  if(!journal.state["credentials"][config["wifi"]["credential_ref"]|""].is<const char*>()||!credential(config["service"]["credential_ref"])[0]){reason(Phase::SetupStart,"phone_setup_required");return 1;}
  if(!text(journal.state["participant_id"],128)&&(!text(journal.state["bootstrap"],1024)||!text(journal.state["enrollment_id"],128)||!digest(journal.state["credential_sha256"])))return block(Phase::AuthBlocked,"enrollment_provisioning_required");
  configureCadence();loaded=true;reason(journal.state["setup_pending"]==true?Phase::SetupJoin:Phase::Connect,"network_connecting");return 1;
 }
 void configureCadence(){unsigned i=0;for(auto b:bindings()){cadence[i]={};cadence[i].interval=b["cadence_ms"];cadence[i].timeout=b["timeout_ms"];cadence[i].required=b["required"];bindingDisabled[i]=false;cursors[i][0]=0;++i;}}
 bool capacity()const{
  auto l=config["low_water"];
  return memory.internal_free>=l["internal_free"].as<uint64_t>()&&memory.internal_largest>=l["internal_largest"].as<uint64_t>()&&memory.dma_free>=l["dma_free"].as<uint64_t>()&&memory.dma_largest>=l["dma_largest"].as<uint64_t>()&&memory.psram_free>=l["psram_free"].as<uint64_t>()&&memory.psram_largest>=l["psram_largest"].as<uint64_t>()&&memory.stack_headroom>=l["stack_headroom"].as<uint32_t>();
 }
 bool bootReady()const{
  // A timer, socket connection or early lifecycle phase is not installation
  // health. Preserve rollback eligibility until this boot has reconciled.
  if(journal.state["protocol_rejections"]["quarantined"]==true||phase!=Phase::Ready||!loaded||rebootRecovery||snapshotNeeded||inventoryNeeded||!poolsEstablished||!sessionId[0]||!status.last_sync_ms||drain||!capacity()||journal.retained||journal.blocked||journal.reload||journal.writeToken||journal.state["pending"].size())return false;
  unsigned i=0;for(auto b:bindings()){
   if(b["required"]==true&&(bindingDisabled[i]||!cadence[i].lastCompleted))return false;
   ++i;
  }
  return true;
 }
 JsonObject make(Action a,const char* family,const char* operation){request.clear();action=a;request["protocol_version"]="1.0";request["family"]=family;request["operation"]=operation;char op[128];id(op,sizeof(op));request["operation_id"]=op;return request["arguments"].to<JsonObject>();}
 void session(JsonObject a){a["participant_id"]=journal.state["participant_id"];a["session_id"]=sessionId;a["generation"]=sessionGeneration;}
 void attempt(JsonObject a){a["attempt_id"]=journal.state["job"]["attempt_id"];a["generation"]=journal.state["job"]["generation"];}
 int persist(Phase next){afterSave=next;saveSequence=journal.seq+1;reason(Phase::Save,"durable_snapshot_pending");return 1;}
 int send(bool critical){
  if(request.overflowed()||measureJson(request)>16384)return block(Phase::CapacityBlocked,"request_exceeds_bound");
  durable=critical;
  if(critical){auto pending=journal.state["pending"].as<JsonArray>();if(pending.size()>=PendingMax)return block(Phase::CapacityBlocked,"pending_full");auto p=pending.add<JsonObject>();p["action"]=static_cast<unsigned>(action);p["boot_id"]=boot;p["request"].set(request);if(measureJson(p)>RecordMax){pending.remove(pending.size()-1);return block(Phase::CapacityBlocked,"durable_record_exceeds_bound");}return persist(Phase::Send);}
  reason(Phase::Send,"request_ready");return 1;
 }
 bool durableAction(unsigned a)const{switch(static_cast<Action>(a)){case Action::Enroll:case Action::Claim:case Action::Prepare:case Action::Result:case Action::Stop:case Action::Observe:case Action::DesiredReport:case Action::RotationPrepare:case Action::RotationActivate:case Action::RotationConfirm:case Action::BindingResult:case Action::BindingStop:case Action::RejectPrepare:return true;default:return false;}}
 int resume(){auto p=journal.state["pending"][0];if(!p["request"].is<JsonObject>()||!durableAction(p["action"].as<unsigned>()))return block(Phase::StorageBlocked,"pending_record_invalid");if(!receiptChecked&&p["action"]!=static_cast<unsigned>(Action::Enroll)){auto a=make(Action::ReceiptLookup,"inspect","operation_get");a["lookup_operation_id"]=p["request"]["operation_id"];return send(false);}receiptChecked=false;request.set(p["request"]);action=static_cast<Action>(p["action"].as<unsigned>());durable=true;reason(Phase::Send,"replay_same_operation_identity");return 1;}
 int finish(Phase next=Phase::Ready){if(durable){journal.state["pending"].as<JsonArray>().remove(0);durable=false;return persist(next);}phase=next;return 1;}
 int backoff(const char* why){uint32_t random=0;auto e=entropy.fill(entropy.context,&random,sizeof(random));if(e==RISC_ENTROPY_RETAINED)return retained();if(e!=0)return block(Phase::ProtocolBlocked,"entropy_unavailable");waitUntil=now()+retryDelay(failures++,random);reason(Phase::Backoff,why);return 1;}
 bool relayAction()const{return action>=Action::RelayConfig&&action<=Action::RelayEnrollmentVerified;}
 bool enrollmentQueue(JsonObjectConst);int enrollmentStep();int enrollmentReceived(JsonObjectConst);int enrollmentApply(JsonObject,bool approved=true);
 JsonObject relayPending(){auto r=journal.state["relay"];return relaySlot[0]?r["deliveries"][relaySlot].as<JsonObject>():r["pending"].as<JsonObject>();}
 int enrollmentVerify();int enrollmentVerified();int relayPost(JsonObjectConst);int relayStep();int relayTransmit();int relayReceived();int relayFailure(const char*);
 int rejectSavedRequest();
 int transmit(){
  if(relayAction())return relayTransmit();
  bool external=action==Action::Maintain||action==Action::Probe;
  if(external&&externalJob&&(drain||now()+cadence[bindingIndex].timeout+1000>=leaseUntil))return stopJob("grant_or_cancellation_before_dispatch");
  if(external&&net.wall(now())*1000>=bindingAt(bindingIndex)["expires_at_ms"].as<uint64_t>()){bindingDisabled[bindingIndex]=true;phase=Phase::Ready;return externalJob?stopJob("binding_expired_before_dispatch"):1;}
  auto e=external?endpoint(bindingIndex):config["service"].as<JsonObjectConst>();
  const char* bearer=credential(e["credential_ref"]|"");const char* site=credential(e["site_credential_ref"]|"");
  if((e["credential_ref"]|"")[0]&&!bearer[0])return block(Phase::AuthBlocked,"credential_reference_unresolved");
  char authorization[1032]{},siteAuthorization[1032]{};snprintf(authorization,sizeof(authorization),"Bearer %s",bearer);snprintf(siteAuthorization,sizeof(siteAuthorization),"Bearer %s",site);
  risc_http_header_v2 h[8]{};unsigned count=0;h[count++]={"Accept","application/json"};if(!external)h[count++]={"X-Infrastructure-Profile","esp32s3-infrastructure/1"};if(bearer[0])h[count++]={"Authorization",authorization};if(site[0])h[count++]={"OAI-Sites-Authorization",siteAuthorization};
  unsigned method=action==Action::Probe?RISC_HTTP_GET:RISC_HTTP_POST;size_t bytes=0;
  if(method==RISC_HTTP_POST){h[count++]={"Content-Type","application/json"};bytes=serializeJson(request,encoded,16385);}
  if(action==Action::Enroll)h[count++]={"X-Enrollment-Id",journal.state["enrollment_id"]|""};
  if(action==Action::Probe){auto cp=journal.state["checkpoints"][bindingAt(bindingIndex)["binding_id"]|""];if(text(cp["etag"],512))h[count++]={"If-None-Match",cp["etag"]};if(text(cp["last_modified"],128))h[count++]={"If-Modified-Since",cp["last_modified"]};}
  auto r=net.begin(e["url"]|"",method,h,count,encoded,bytes,external?bindingAt(bindingIndex)["response_max_bytes"].as<unsigned>():32768,external?cadence[bindingIndex].timeout:5000,config["service"]["time_server"],now());
  volatile char* secret=authorization;for(size_t i=0;i<sizeof(authorization);++i)secret[i]=0;secret=siteAuthorization;for(size_t i=0;i<sizeof(siteAuthorization);++i)secret[i]=0;
  if(r==-2)return retained();if(r<0)return backoff("transport_open_failed");if(r)return 1;status.requested++;reason(Phase::Receive,"request_in_flight");return 1;
 }
 int stopJob(const char* why){if(text(journal.state["job"]["binding_id"],128)){auto a=make(Action::BindingStop,"execution","binding_report");a["binding_id"]=journal.state["job"]["binding_id"];a["generation"]=journal.state["job"]["generation"];a["kind"]="stopped";a["evidence"]["quiescent"]=true;a["evidence"]["process_identity"]=boot;a["evidence"]["reason"]=why;return send(true);}if(!text(journal.state["job"]["attempt_id"],128)){journal.state["job"]=nullptr;return persist(Phase::Ready);}auto a=make(Action::Stop,"execution","stop_report");attempt(a);a["evidence"]["quiescent"]=true;a["evidence"]["process_identity"]=boot;a["evidence"]["reason"]=why;return send(true);}
 int advance(){
  if(phase==Phase::Retained||journal.retained||net.retained)return retained();
  if(phase==Phase::Cold)return bootstrap();
  if(phase>=Phase::SetupStart)return setupStep();
  if(phase==Phase::Save){int r;if(journal.writeToken)r=journal.collect();else if(journal.reload){r=journal.load();}else r=journal.save();if(r==-2)return retained();if(r<0)return block(Phase::StorageBlocked,"state_commit_failed");if(r)return 1;if(journal.seq!=saveSequence||journal.loadedCrc!=journal.desiredCrc){reason(boot[0]?Phase::Recover:Phase::Nonce,"commit_unknown_reloaded");return 1;}phase=afterSave;return 1;}
  if(phase==Phase::Send)return transmit();
  if(phase==Phase::Receive){auto r=net.advance(now());if(r==-2)return retained();if(r)return 1;return received();}
  if(phase==Phase::Backoff){if(now()<waitUntil)return 1;phase=Phase::Connect;return 1;}
  if(phase==Phase::Connect){if(drain){phase=Phase::Drain;return 1;}auto r=station.connect(station.context,config["wifi"]["ssid"],credential(config["wifi"]["credential_ref"]));if(r==-2)return retained();if(r<0){r=station.close(station.context);if(r==-2)return retained();if(r)return 1;setupRecovery=true;reason(Phase::SetupRetry,"wifi_setup_recovery");return 1;}if(r)return 1;phase=boot[0]?Phase::Recover:Phase::Nonce;return 1;}
  if(phase==Phase::Nonce){uint8_t nonce[32];auto r=entropy.fill(entropy.context,nonce,sizeof(nonce));if(r==RISC_ENTROPY_RETAINED)return retained();if(r==RISC_ENTROPY_UNAVAILABLE||r==RISC_ENTROPY_BUSY)return 1;if(r)return block(Phase::ProtocolBlocked,"entropy_failed");for(unsigned i=0;i<32;++i)snprintf(boot+i*2,3,"%02x",nonce[i]);copy(status.boot_id,sizeof(status.boot_id),boot);phase=Phase::Recover;return 1;}
  if(phase==Phase::Recover&&journal.state["protocol_rejections"]["quarantined"]==true)phase=Phase::Ready;
  if(phase==Phase::Recover){if(journal.state["job"]["phase"]=="awaiting_acceptance")return jobStep();if(journal.state["pending"].size())return resume();auto job=journal.state["job"];if(!job.isNull()&&text(job["attempt_id"],128))return stopJob("reboot_or_reconnect_requires_reconciliation");if(snapshotNeeded&&text(journal.state["participant_id"],128))return snapshotStep();rebootRecovery=false;phase=Phase::Ready;}
  if(phase==Phase::Drain){if(net.token){auto r=net.close();if(r==-2)return retained();if(r)return 1;}if(journal.state["pending"].size()){phase=Phase::Recover;return 1;}if(!journal.state["job"].isNull())return stopJob("owner_shutdown");auto r=station.close(station.context);if(r==-2)return retained();if(r)return 1;phase=Phase::Stopped;return 0;}
  if(phase==Phase::Stopped)return 0;
  if(phase!=Phase::Ready)return -1;
  if(drain){phase=Phase::Drain;return 1;}
  // A terminal participant rejection must not suspend the independent relay.
  // Keep all remaining custody intact; only the rejected request is retired.
  if(journal.state["protocol_rejections"]["quarantined"]==true){
   if(int r=relayStep())return r;
   for(unsigned k=0;k<bindings().size();++k){unsigned i=(fairness+k)%bindings().size();auto b=bindingAt(i);if(!bindingDisabled[i]&&cadence[i].required&&cadence[i].ready(now())&&(b["capability"]=="scheduler.drive"||b["capability"]=="service.reconcile")){bindingIndex=i;fairness=(i+1)%bindings().size();return bindingStep(false);}}
   return 1;
  }
  if(journal.state["pending"].size()){if(journal.state["job"]["phase"]=="awaiting_acceptance")return jobStep();return resume();}
  if(!text(journal.state["participant_id"],128)){auto a=make(Action::Enroll,"participant","enroll");a["bootstrap_credential"]=journal.state["bootstrap"];a["identity_binding"]["credential_sha256"]=journal.state["credential_sha256"];a["identity_binding"]["installation_id"]=journal.state["installation_id"];a["display_name"]="ESP32-S3 infrastructure";a["class"]="embedded";a["adapter_kind"]="esp32s3-infrastructure/1";return send(true);}
  if(snapshotNeeded&&(snapshotStage<2||sessionId[0]))return snapshotStep();
  if(!sessionId[0]){auto a=make(Action::Open,"participant","open_session");a["participant_id"]=journal.state["participant_id"];a["boot_id"]=boot;a["adapter_protocols"].to<JsonArray>().add("1.0");a["adapter_version"]="infra.node/1";a["contact_interval_ms"]=config["service"]["contact_interval_ms"];a["journal_summary"]["unresolved"].to<JsonArray>();a["embedded_profile"]="esp32s3-infrastructure/1";a["response_bytes"]=32768;return send(false);}
  if(!journal.state["rotation"].isNull()&&journal.state["job"].isNull())return rotationStep();
  if(text(journal.state["configuration_desired_id"],128)&&net.wall(now())*1000>=journal.state["configuration_expires_at"].as<uint64_t>()&&bindings().size()){journal.state["configuration"]["bindings"].to<JsonArray>();journal.state["configuration"]["enabled_providers"].to<JsonArray>();configureCadence();inventoryNeeded=true;return persist(Phase::Ready);}
  auto job=journal.state["job"];
  if(!job.isNull()&&job["phase"]=="running"&&now()>=leaseUntil)return stopJob("grant_expired");
  if(now()>=nextSync)return sync();
  const bool ready=capacity();if(ready!=lastCapacity){lastCapacity=ready;inventoryNeeded=true;}if(!ready)copy(status.reason,sizeof(status.reason),"low_water_optional_admission_blocked");
  if(!relayYield){int r=relayStep();if(r)return r;}relayYield=false;
  for(unsigned k=0;k<bindings().size();++k){unsigned i=(fairness+k)%bindings().size();if(!bindingDisabled[i]&&cadence[i].required&&cadence[i].ready(now())){bindingIndex=i;fairness=(i+1)%bindings().size();return bindingStep(job["phase"]=="running"&&job["spec"]["inputs"]["binding_id"]==bindingAt(i)["binding_id"]);}}
  if(!job.isNull()&&(job["phase"]!="running"||now()+15000>=leaseUntil))return jobStep();
  if(!job.isNull()&&job["phase"]=="running"){for(unsigned i=0;i<bindings().size();++i)if(bindingAt(i)["binding_id"]==job["spec"]["inputs"]["binding_id"]&&cadence[i].ready(now()))return jobStep();}
  if(inventoryNeeded||now()>=nextInventory)return inventoryStep();
  for(unsigned priority=0;priority<2;++priority)for(unsigned k=0;k<bindings().size();++k){unsigned i=(fairness+k)%bindings().size();auto b=bindingAt(i);if(bindingDisabled[i]||b["required"].as<bool>()!=(priority==0)||!cadence[i].ready(now())||(priority&&(!capacity()||now()+cadence[i].timeout+5000>=nextSync)))continue;bindingIndex=i;fairness=(i+1)%bindings().size();return bindingStep(false);}
  return 1;
 }
 int sync(){auto a=make(Action::Sync,"participant","sync");a["session_id"]=sessionId;a["generation"]=sessionGeneration;char c[24];snprintf(c,sizeof(c),"%llu",static_cast<unsigned long long>(cursor));a["cursor"]=c;a["items"].to<JsonArray>();a["requested_work"]=capacity()&&journal.state["job"].isNull()?1:0;return send(false);}
 int setupStep();bool acceptSetup();
 bool bindingMatches(unsigned)const;bool localPlan(JsonObjectConst)const;int snapshotStep();int control(JsonObjectConst);int inventoryStep();int jobStep();int bindingStep(bool job);int received();int result(JsonObjectConst data);int probeResult();int applySync(JsonObjectConst data);int rotationStep();
 void dispose(){if(phase!=Phase::Stopped&&phase!=Phase::Cold)return;portal.dispose();net.dispose();journal.dispose();config.clear();request.clear();response.clear();if(encoded){volatile char* p=encoded;for(unsigned i=0;i<16385;++i)p[i]=0;std::free(encoded);encoded=nullptr;}}
};
}
