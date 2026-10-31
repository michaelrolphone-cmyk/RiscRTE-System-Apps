#include "Node.h"
#include "Digest.h"
#include "../../lib/RemoteFiles/vendor/tinydtls_sha2/sha2.h"
namespace Infra {
static const char* const poolNames[]={"tls","job","pending","bindings","internal","psram"};
static const char* const poolUnits[]={"transactions","jobs","records","bindings","bytes","bytes"};
static void hashBytes(const char* data,size_t size,char out[65]){dtls_sha256_data(reinterpret_cast<const uint8_t*>(data),size,out);}
static uint64_t grantDeadline(Node& n,JsonObjectConst g){const uint64_t wall=n.net.wall(n.now())*1000,expiry=g["expires_at"].as<uint64_t>();if(!wall||expiry<=wall)return n.now();uint64_t remaining=expiry-wall;const uint64_t ttl=g["lease_ms"]|120000ULL;if(remaining>ttl)remaining=ttl;return leaseDeadline(n.now(),static_cast<unsigned>(remaining),n.now()-n.net.started,2000);}
int Node::rotationStep(){
 auto r=journal.state["rotation"].as<JsonObject>();if(!keys(r,{"phase","new_credential_ref","credential_sha256"})||!text(r["new_credential_ref"],128)||!digest(r["credential_sha256"]))return block(Phase::AuthBlocked,"rotation_record_invalid");
 const char* token=credential(r["new_credential_ref"]);char hash[65];hashBytes(token,strlen(token),hash);if(!token[0]||r["credential_sha256"]!=hash||r["new_credential_ref"]==config["service"]["credential_ref"])return block(Phase::AuthBlocked,"rotation_secret_digest_mismatch");
 const char* operation;Action act;if(r["phase"]=="staged"){operation="rotation_prepare";act=Action::RotationPrepare;}else if(r["phase"]=="prepared"){operation="rotation_activate";act=Action::RotationActivate;}else if(r["phase"]=="activated"){operation="rotation_confirm";act=Action::RotationConfirm;}else return block(Phase::AuthBlocked,"rotation_phase_invalid");
 auto a=make(act,"participant",operation);a["participant_id"]=journal.state["participant_id"];a["credential_sha256"]=r["credential_sha256"];if(act==Action::RotationPrepare){a["session_id"]=sessionId;a["generation"]=sessionGeneration;}return send(true);
}
int Node::inventoryStep(){
 inventoryNeeded=true;auto a=make(Action::Inventory,"participant","inventory");session(a);auto pools=a["pools"].to<JsonArray>();
 uint64_t amounts[]={1,capacity()?1ULL:0ULL,8,8,memory.internal_free,memory.psram_free};
 for(unsigned i=0;i<6;++i){auto p=pools.add<JsonObject>();char id[160],physical[160];snprintf(id,sizeof(id),"%s:%s",journal.state["participant_id"]|"",poolNames[i]);snprintf(physical,sizeof(physical),"%s:%s",journal.state["installation_id"]|"",poolNames[i]);p["pool_id"]=id;p["physical_identity"]=physical;p["resource_kind"]="stock";p["unit"]=poolUnits[i];p["allocatable_limit"]=amounts[i];p["external_usage"]=0;p["accounting_basis"]="external_only";p["inventory_revision"]=now();p["reservation_watermark"]=0;p["valid_until"]=net.wall(now())*1000+120000;}
 auto offers=a["offers"].to<JsonArray>();if(poolsEstablished)for(auto pin:config["service"]["providers"].as<JsonArrayConst>()){
  auto item=offers.add<JsonObject>();item["provider_digest"]=pin["definition_digest"];item["package_digest"]=pin["package_digest"];auto o=item["offer"].to<JsonObject>();char id[200];snprintf(id,sizeof(id),"%s:%s@%u:infra",journal.state["participant_id"]|"",pin["provider_id"]|"",pin["revision"].as<unsigned>());o["offer_id"]=id;o["provider_id"]=pin["provider_id"];o["provider_revision"]=pin["revision"];o["local_instance_id"]="infra";o["offer_generation"]=sessionGeneration;o["installed_state"]="active";bool enabled=false;for(auto b:bindings())enabled|=b["capability"]==pin["capability_id"];o["observed_health"]=capacity()&&enabled?"healthy":"capacity_blocked";o["probe_evidence"]["bootstrap"]="dependencies_storage_and_policy_verified";o["probe_evidence"]["hardware_qualification"]="pending";o["valid_until"]=net.wall(now())*1000+120000;for(const char* name:poolNames){char key[160];snprintf(key,sizeof(key),"%s:%s",journal.state["participant_id"]|"",name);o["pool_bindings"][name]=key;}
 }
 return send(false);
}
bool Node::bindingMatches(unsigned i)const{auto b=bindingAt(i);auto s=journal.state["job"]["spec"];return b["binding_id"]==s["inputs"]["binding_id"]&&b["revision"]==s["inputs"]["binding_revision"]&&b["capability"]==s["capability"]["capability_id"]&&b["operation"]==s["operation"]&&b["effect_class"]==s["recovery_policy"]["effect_class"]&&!bindingDisabled[i];}
bool Node::localPlan(JsonObjectConst p)const{auto pin=find(config["service"]["providers"],"capability_id",p["capability"]["capability_id"]|"");if(p["participant_id"]!=journal.state["participant_id"]||p["session_id"]!=sessionId||p["provider_digest"]!=pin["definition_digest"]||p["dependencies"].size()||p["resources"].size()>8)return false;
 for(auto r:p["resources"].as<JsonArrayConst>()){const char* id=r["pool_id"]|"";bool found=false;uint64_t limits[]={1,1,8,8,memory.internal_free,memory.psram_free};for(unsigned i=0;i<6;++i){char name[160];snprintf(name,sizeof(name),"%s:%s",journal.state["participant_id"]|"",poolNames[i]);if(!strcmp(name,id)&&integer(r["amount"],0,limits[i]))found=true;}if(!found)return false;}return capacity();}
int Node::bindingStep(bool job){
 auto b=bindingAt(bindingIndex);if(job&&!bindingMatches(bindingIndex))return stopJob("binding_revision_changed");if(b.isNull())return block(Phase::ProtocolBlocked,"binding_not_found");
 if(net.wall(now())&&net.wall(now())*1000>=b["expires_at_ms"].as<uint64_t>()){bindingDisabled[bindingIndex]=true;return job?stopJob("binding_expired"):1;}
 if(job&&(now()+cadence[bindingIndex].timeout+1000>=leaseUntil))return stopJob("insufficient_grant_budget");
 externalJob=job;cadence[bindingIndex].start(now());
 if(b["capability"]=="scheduler.drive"||b["capability"]=="service.reconcile"){
  action=Action::Maintain;request.clear();request["protocol_version"]="maintenance/1";request["operation"]="wake";char op[128];id(op,sizeof(op));request["operation_id"]=op;request["binding_id"]=b["binding_id"];auto a=request["arguments"].to<JsonObject>();a["max_items"]=b["max_items"];a["max_elapsed_ms"]=b["max_elapsed_ms"];if(cursors[bindingIndex][0])a["cursor"]=cursors[bindingIndex];return send(false);
 }
 action=Action::Probe;request.clear();return send(false);
}
int Node::jobStep(){
 auto j=journal.state["job"].as<JsonObject>();if(j.isNull())return 1;
 if(j["phase"]=="offered"){
  auto s=j["spec"];auto a=make(Action::Plan,"resolve","plan");a["capability"]=s["capability"];a["requested_operation"]=s["operation"];a["inputs"]=s["inputs"];a["target_policy"]=s["target_policy"];a["resource_request"].set(s["resource_request"].isNull()?JsonObjectConst():s["resource_request"].as<JsonObjectConst>());if(a["resource_request"].isNull())a.remove("resource_request");a["budget"]=8;return send(false);
 }
 if(j["phase"]=="prepared"||j["phase"]=="prepared_dependency"){
  if(j["boot_id"]!=boot||rebootRecovery)return stopJob("old_prepare_cannot_start");auto a=make(Action::Prepare,"execution","prepare_report");session(a);a["group_id"]=j["group_id"];a["confirmation"]="prepared";a["evidence"]["journal_sequence"]=journal.seq;return send(true);
 }
 if(j["phase"]=="awaiting_dependency_start")return now()>=leaseUntil?stopJob("dependency_prepare_expired"):1;
 if(j["phase"]=="running"){
  if(j["boot_id"]!=boot||rebootRecovery)return stopJob("old_grant_cannot_start");
  if(now()+15000>=leaseUntil){if(text(j["binding_id"],128)){auto a=make(Action::BindingRenew,"execution","binding_report");a["binding_id"]=j["binding_id"];a["generation"]=j["generation"];a["kind"]="renew";return send(false);}auto a=make(Action::Renew,"execution","renew");attempt(a);a["lease_ms"]=120000;return send(false);}
  const char* binding=j["spec"]["inputs"]["binding_id"]|"";for(unsigned i=0;i<bindings().size();++i)if(bindingAt(i)["binding_id"]==binding){bindingIndex=i;if(!bindingMatches(i))return stopJob("binding_revision_changed");if(cadence[i].ready(now()))return bindingStep(true);return 1;}return stopJob("binding_revoked");
 }
 if(j["phase"]=="awaiting_acceptance"){
  if(now()<waitUntil)return 1;auto a=make(Action::Review,"assignment","get");a["assignment_id"]=j["assignment_id"];return send(false);
 }
 return stopJob("unknown_job_phase");
}
int Node::snapshotStep(){
 auto a=make(Action::Snapshot,"participant","snapshot");a["participant_id"]=journal.state["participant_id"];a["kind"]=snapshotStage==2?"desired":snapshotStage==1?"execution_binding":"attempt";if(snapshotAfter[0])a["after"]=snapshotAfter;return send(false);
}
int Node::control(JsonObjectConst c){
 auto d=c["data"];const char* kind=c["kind"]|"";
 if(!integer(c["seq"],1,9007199254740991ULL))return block(Phase::ProtocolBlocked,"control_sequence_invalid");
 if(c["seq"].as<uint64_t>()<=cursor)return 0;
 if(c["reference"].is<JsonObjectConst>()){
  if(!integer(c["reference"]["size_bytes"],1,24576)||!digest(c["reference"]["sha256"]))return block(Phase::CapacityBlocked,"control_reference_exceeds_profile");
  copy(controlHash,sizeof(controlHash),c["reference"]["sha256"]);controlSequence=c["seq"];auto a=make(Action::Control,"participant","control_value");a["session_id"]=sessionId;a["generation"]=sessionGeneration;a["control_id"]=c["reference"]["control_id"];return send(false);
 }
 if(!strcmp(kind,"prepare")&&d["binding_ids"].size()){
  if(journal.state["job"]["group_id"]==d["group_id"]){cursor=c["seq"];return 0;}
  if(!journal.state["job"].isNull()||d["binding_ids"].size()!=1||d["bindings"].size()!=1||!localPlan(d["bindings"][0])){cursor=c["seq"];auto a=make(Action::RejectPrepare,"execution","prepare_report");session(a);a["group_id"]=d["group_id"];a["confirmation"]="rejected";a["evidence"]["reason"]="dependency_capacity_or_pin_mismatch";return send(true);}
  leaseUntil=grantDeadline(*this,d);auto plan=d["bindings"][0];auto j=journal.state["job"].to<JsonObject>();j["phase"]="prepared_dependency";j["boot_id"]=boot;j["binding_id"]=d["binding_ids"][0];j["attempt_id"]=d["attempt_id"];j["group_id"]=d["group_id"];j["generation"]=d["generation"];j["spec"]["capability"]=plan["capability"];j["spec"]["operation"]=plan["operation"];j["spec"]["mode"]="job";j["spec"]["inputs"]=plan["inputs"];for(auto b:bindings())if(b["binding_id"]==plan["inputs"]["binding_id"])j["spec"]["recovery_policy"]["effect_class"]=b["effect_class"];
  bool valid=false;for(unsigned i=0;i<bindings().size();++i)valid|=bindingMatches(i);if(!valid)return stopJob("dependency_binding_not_admitted");cursor=c["seq"];return persist(Phase::Ready);
 }
 if(!strcmp(kind,"dependency_start")){
  auto j=journal.state["job"];auto g=d["grant"];if(j["binding_id"]!=g["binding_id"]||j["boot_id"]!=boot||rebootRecovery){cursor=c["seq"];snapshotNeeded=true;snapshotStage=1;snapshotAfter[0]=0;return snapshotStep();}
  auto pin=find(config["service"]["providers"],"capability_id",d["spec"]["capability"]["capability_id"]|"");char hash[65],specHash[65];canonicalDigest(d["spec"]["inputs"],hash);canonicalDigest(d["spec"],specHash);
  if(g["provider_digest"]!=pin["definition_digest"]||g["input_digest"]!=hash||g["spec_digest"]!=specHash||g["session_id"]!=sessionId||g["generation"]!=j["generation"]||g["artifact_ids"].size()||!integer(g["lease_ms"],1000,120000))return stopJob("dependency_grant_mismatch");
  j["spec"]=d["spec"];j["grant"]=g;j["phase"]="running";leaseUntil=grantDeadline(*this,g);cursor=c["seq"];return persist(Phase::Ready);
 }
 if(!strcmp(kind,"dependency_stop")){cursor=c["seq"];if(journal.state["job"]["binding_id"]==d["binding_id"])return stopJob("dependency_cancelled");snapshotNeeded=true;snapshotStage=1;snapshotAfter[0]=0;return snapshotStep();}
 if(!strcmp(kind,"stop")||!strcmp(kind,"abort")||!strcmp(kind,"reconcile")){
  cursor=c["seq"];if(journal.state["job"]["attempt_id"]==d["attempt_id"])return stopJob("cancellation_or_reconciliation_observed");
  snapshotNeeded=true;snapshotStage=0;snapshotAfter[0]=0;return snapshotStep();
 }
 if(!strcmp(kind,"lifecycle")){if(d["lifecycle"]!="enabled")drain=true;}
 else if(!strcmp(kind,"desired")){cursor=c["seq"];auto a=make(Action::Desired,"desired","get");a["desired_id"]=d["desired_id"];return send(false);}
 else if(!strcmp(kind,"start")){
  if(journal.state["job"]["attempt_id"]!=d["grant"]["attempt_id"]){cursor=c["seq"];snapshotNeeded=true;snapshotStage=0;snapshotAfter[0]=0;return snapshotStep();}
 }else if(strcmp(kind,"prepare")&&strcmp(kind,"dependency_hold"))return block(Phase::ProtocolBlocked,"unsupported_control");
 cursor=c["seq"];return 0;
}
int Node::applySync(JsonObjectConst data){
 if(data["generation"]!=sessionGeneration||!data["controls"].is<JsonArrayConst>()||data["controls"].size()>4||data["work_offers"].size()>1)return block(Phase::ProtocolBlocked,"sync_contract_bound");
 nextSync=data["coverage"]["complete"]==true?now()+config["service"]["contact_interval_ms"].as<unsigned>():now();status.last_sync_ms=now();
 if(text(data["target_epoch"],128)&&journal.state["target_epoch"]!=data["target_epoch"]){status.gaps++;snapshotNeeded=true;snapshotStage=0;snapshotAfter[0]=0;rebootRecovery=true;phase=Phase::Recover;return 1;}
 for(auto c:data["controls"].as<JsonArrayConst>()){int r=control(c);if(r){nextSync=now();return r;}}
 if(!drain&&capacity()&&journal.state["job"].isNull()&&data["work_offers"].size()){
  auto offer=data["work_offers"][0];if(!copy(assignmentId,sizeof(assignmentId),offer["assignment_id"]))return block(Phase::ProtocolBlocked,"assignment_identity_bound");auto a=make(Action::Assignment,"assignment","get");a["assignment_id"]=assignmentId;return send(false);
 }
 phase=Phase::Ready;return 1;
}
int Node::result(JsonObjectConst output){
 auto j=journal.state["job"];bool dependency=text(j["binding_id"],128);auto a=make(dependency?Action::BindingResult:Action::Result,"execution",dependency?"binding_report":"result");if(dependency){a["binding_id"]=j["binding_id"];a["generation"]=j["generation"];a["kind"]="result";}else attempt(a);auto c=a["candidate"].to<JsonObject>();char candidate[128];id(candidate,sizeof(candidate));c["candidate_id"]=candidate;
 for(const char* k:{"spec_digest","provider_digest","input_digest"})c[k]=j["grant"][k];c["output"]=output;c["evidence"]["quiescent"]=true;c["evidence"]["executor_identity"]=boot;c["evidence"]["observer"]=journal.state["participant_id"];c["evidence"]["boot_id"]=boot;c["evidence"]["observed_at_ms"]=net.wall(now())*1000;c["evidence"]["monotonic_ms"]=now();c["artifacts"].to<JsonArray>();return send(true);
}
int Node::probeResult(){
 auto b=bindingAt(bindingIndex);const char* bid=b["binding_id"];bool unchanged=!net.outcome&&net.info.status_code==304;
 bool good=!net.outcome&&(unchanged||(net.info.status_code>=b["expected_status_min"].as<int>()&&net.info.status_code<=b["expected_status_max"].as<int>()));
 response.clear();if(good&&!unchanged){if(!parse(response,net.body,net.size))good=false;else if(response.size()>b["collection_bound"].as<unsigned>())good=false;else if(text(b["semantic_field"],64)&&response[b["semantic_field"]|""]!=b["semantic_value"])good=false;}
 // Keep the parsed source in response until a bounded projection is copied.
 request.clear();auto out=request.to<JsonObject>();out["binding_id"]=bid;out["binding_revision"]=b["revision"];out["healthy"]=good;out["unchanged"]=unchanged;out["http_status"]=net.info.status_code;out["transport_outcome"]=net.outcome;out["vantage_point"]=journal.state["participant_id"];out["observed_at_ms"]=net.wall(now())*1000;out["monotonic_ms"]=now();out["freshness_ms"]=b["cadence_ms"];out["coverage"]="latest_state";out["lossless"]=false;out["gap_count"]=status.gaps;
 if(good&&!unchanged){auto projected=out["value"].to<JsonObject>();for(auto f:b["projection"].as<JsonArrayConst>()){if(!response[f.as<const char*>()].isNull())projected[f.as<const char*>()]=response[f.as<const char*>()];}if(measureJson(projected)>1800)return block(Phase::CapacityBlocked,"projection_exceeds_result_bound");}
 char digestValue[65];hashBytes(net.body,net.size,digestValue);out["source_digest"]=unchanged?(journal.state["checkpoints"][bid]["digest"]|""):digestValue;
 if(good&&!externalJob&&(unchanged||journal.state["checkpoints"][bid]["digest"]==digestValue)&&integer(journal.state["checkpoints"][bid]["sequence"],1,9007199254740991ULL)){
  auto a=make(Action::Heartbeat,"observation","heartbeat");char source[200];snprintf(source,sizeof(source),"%s:%s",journal.state["participant_id"]|"",bid);a["source_id"]=source;a["source_generation"]=b["revision"];a["sequence"]=journal.state["checkpoints"][bid]["sequence"];a["measured_at"]=net.wall(now())*1000;a["healthy"]=true;return send(false);
 }
 response.set(request); // freeze output before rebuilding canonical envelope
 if(externalJob){auto r=result(response.as<JsonObjectConst>());if(r<0)return r;auto p=journal.state["pending"][0];p["binding_id"]=bid;if(good&&!unchanged){auto cp=p["checkpoint"].to<JsonObject>();cp["etag"]=net.info.etag;cp["last_modified"]=net.info.last_modified;cp["digest"]=digestValue;}return r;}
 auto a=make(Action::Observe,"observation","publish");char source[200];snprintf(source,sizeof(source),"%s:%s",journal.state["participant_id"]|"",bid);a["source_id"]=source;a["source_generation"]=b["revision"];auto obs=a["observations"].to<JsonArray>().add<JsonObject>();obs["sequence"]=journal.state["checkpoints"][bid]["sequence"].as<uint64_t>()+1;obs["measured_at"]=net.wall(now())*1000;obs["clock_quality"]="sntp_sample_tls_verified";obs["unit"]="latest_state";obs["data"]=response.as<JsonObjectConst>();auto r=send(true);if(r<0)return r;auto p=journal.state["pending"][0];p["binding_id"]=bid;auto cp=p["checkpoint"].to<JsonObject>();cp.set(journal.state["checkpoints"][bid]);cp["sequence"]=obs["sequence"];if(good&&!unchanged){cp["etag"]=net.info.etag;cp["last_modified"]=net.info.last_modified;cp["digest"]=digestValue;}return r;
}
int Node::received(){
 if(relayAction())return relayReceived();
 if(net.outcome){if(net.outcome==RISC_HTTP_TIMEOUT)status.timed_out++;if(net.info.sent_bytes)status.unknown++;if(action==Action::Probe)return probeResult();return backoff(net.info.sent_bytes?"outcome_unknown_same_identity":"network_failure");}
 if(action==Action::Probe){cadence[bindingIndex].lastCompleted=now();return probeResult();}
 if(net.info.status_code==401||net.info.status_code==403){if(action==Action::Maintain){bindingDisabled[bindingIndex]=true;phase=Phase::Ready;copy(status.reason,sizeof(status.reason),"binding_auth_blocked");return 1;}return block(Phase::AuthBlocked,"participant_auth_blocked");}
 if(!parse(response,net.body,net.size))return block(Phase::ProtocolBlocked,"malformed_or_oversize_response");
 if(response["protocol_version"]!=(action==Action::Maintain?"maintenance/1":"1.0"))return block(Phase::ProtocolBlocked,"response_protocol_mismatch");
 if(!response["operation_id"].isNull()&&response["operation_id"]!=request["operation_id"])return block(Phase::ProtocolBlocked,"operation_receipt_identity_mismatch");
 auto d=response["data"].as<JsonObjectConst>();const char* code=response["code"]|"";bool missing=response["status"]=="rejected"&&!strcmp(code,"NOT_FOUND");
 if(response["status"]=="rejected"&&!((action==Action::PoolGet||action==Action::OfferGet)&&missing)){
  if((!strcmp(code,"PLAN_STALE")||!strcmp(code,"STALE_ATTEMPT")||!strcmp(code,"REVISION_CONFLICT"))&&(action==Action::Claim||action==Action::Prepare||action==Action::Renew||action==Action::BindingRenew||action==Action::RejectPrepare)&&response["mutation_outcome"]=="not_applied"){
   if(action==Action::Claim)journal.state["job"]=nullptr;rebootRecovery=true;snapshotNeeded=true;snapshotStage=0;snapshotAfter[0]=0;return finish(Phase::Recover);
  }
  if(!strcmp(code,"STALE_SESSION")){sessionId[0]=0;if(durable){if(response["mutation_outcome"]!="not_applied")return block(Phase::ProtocolBlocked,"stale_session_unknown_custody");return finish(Phase::Recover);}phase=Phase::Recover;return 1;}
  if(action==Action::Maintain&&!strcmp(code,"CURSOR_GAP")){cursors[bindingIndex][0]=0;status.gaps++;phase=Phase::Ready;return 1;}
  if(!strcmp(code,"REVISION_CONFLICT")&&(action==Action::PoolPut||action==Action::OfferPut)){return finish();}
  if(!strcmp(code,"BUSY")||!strcmp(code,"RATE_LIMITED")||net.info.status_code>=500)return backoff("service_retry_same_identity");
  return block(Phase::ProtocolBlocked,code[0]?code:"service_rejected");
 }
 if(!missing&&(response["status"]!="completed"&&response["status"]!="accepted"))return block(Phase::ProtocolBlocked,"unexpected_response_status");
 if(durable&&(!response["receipt"].is<JsonObjectConst>()||response["receipt"]["operation_id"]!=request["operation_id"]||!digest(response["receipt"]["request_hash"])))return block(Phase::ProtocolBlocked,"durable_receipt_missing_or_mismatched");
 if(durable){char expected[65];semanticDigest(request,expected);if(response["receipt"]["request_hash"]!=expected)return block(Phase::ProtocolBlocked,"receipt_semantic_hash_mismatch");}
 failures=0;status.completed++;
 switch(action){
 case Action::RotationPrepare:if(d["credential_sha256"]!=journal.state["rotation"]["credential_sha256"]||d["state"]!="prepared")return block(Phase::AuthBlocked,"rotation_prepare_mismatch");journal.state["rotation"]["phase"]="prepared";return finish();
 case Action::RotationActivate:{auto r=journal.state["rotation"];if(d["phase"]!="activated"||d["credential_sha256"]!=r["credential_sha256"])return block(Phase::AuthBlocked,"rotation_activate_mismatch");journal.state["credentials"][config["service"]["credential_ref"]|""]=journal.state["credentials"][r["new_credential_ref"]|""];journal.state["credential_generation"]=d["credential_epoch"];r["phase"]="activated";sessionId[0]=0;return finish();}
 case Action::RotationConfirm:if(d["phase"]!="confirmed")return block(Phase::AuthBlocked,"rotation_confirmation_missing");journal.state["credentials"].remove(journal.state["rotation"]["new_credential_ref"]|"");journal.state.remove("rotation");return finish();
 case Action::ReceiptLookup:{
  if(d["receipt"].isNull()){receiptChecked=true;return resume();}
  const size_t size=measureJson(d["receipt"]);if(size>32768)return block(Phase::ProtocolBlocked,"receipt_exceeds_bound");serializeJson(d["receipt"],net.body,32769);net.size=size;auto p=journal.state["pending"][0];request.set(p["request"]);action=static_cast<Action>(p["action"].as<unsigned>());durable=true;return received();
 }
 case Action::Snapshot:{
  if(!text(d["target_epoch"],128)||!d["items"].is<JsonArrayConst>()||d["items"].size()>4||!d["coverage"]["complete"].is<bool>())return block(Phase::ProtocolBlocked,"snapshot_bound");
  if(journal.state["target_epoch"]!=d["target_epoch"]){journal.state["target_epoch"]=d["target_epoch"];snapshotStage=0;snapshotAfter[0]=0;snapshotNeeded=true;return persist(Phase::Recover);}
  for(auto item:d["items"].as<JsonArrayConst>()){
   if(!text(item["id"],128))return block(Phase::ProtocolBlocked,"snapshot_identity_bound");copy(snapshotAfter,sizeof(snapshotAfter),item["id"]);
   if(snapshotStage<2){auto j=journal.state["job"].to<JsonObject>();j["attempt_id"]=snapshotStage==1?item["attempt_id"]:item["id"];if(snapshotStage==1)j["binding_id"]=item["id"];j["generation"]=item["generation"];j["assignment_id"]=item["assignment_id"];j["phase"]="reconciling";return stopJob("snapshot_recovered_old_authority");}
   auto a=make(Action::Desired,"desired","get");a["desired_id"]=item["id"];return send(false);
  }
  if(d["coverage"]["complete"]==true){if(snapshotStage<2){snapshotStage++;snapshotAfter[0]=0;}else{snapshotNeeded=false;snapshotStage=0;snapshotAfter[0]=0;rebootRecovery=false;}}
  return finish(snapshotStage==2&&!sessionId[0]?Phase::Ready:Phase::Recover);
 }
 case Action::Control:{char hash[65];canonicalDigest(d["data"],hash);if(d["seq"]!=controlSequence||strcmp(hash,controlHash))return block(Phase::ProtocolBlocked,"control_reference_digest_mismatch");int r=control(d);if(r)return r;phase=Phase::Ready;nextSync=0;return 1;}
 case Action::RejectPrepare:if(d["state"]!="aborted")return block(Phase::ProtocolBlocked,"rejection_not_acknowledged");return finish();
 case Action::Progress:case Action::Heartbeat:return finish();
 case Action::Inventory:if(d["profile"]!="esp32s3-infrastructure/1")return block(Phase::ProtocolBlocked,"inventory_profile_mismatch");if(!poolsEstablished){poolsEstablished=true;inventoryNeeded=true;}else{inventoryNeeded=false;nextInventory=now()+90000;}return finish();
 case Action::Enroll:
  if(!text(d["id"],128))return block(Phase::ProtocolBlocked,"enrollment_identity_missing");journal.state["participant_id"]=d["id"];journal.state.remove("bootstrap");return finish();
 case Action::Open:
  if(!copy(sessionId,sizeof(sessionId),d["id"])||!integer(d["generation"],1,9007199254740991ULL)||d["embedded_profile"]!="esp32s3-infrastructure/1")return block(Phase::ProtocolBlocked,"embedded_profile_not_negotiated");sessionGeneration=d["generation"];cursor=d["cursor"]|0ULL;nextSync=0;inventoryNeeded=true;poolStage=true;inventoryIndex=0;return finish();
 case Action::Sync:return applySync(d);
 case Action::PoolGet:{
  inventoryRevision=missing?0:d["revision"].as<uint64_t>();auto a=make(Action::PoolPut,"catalog","pool_observe");session(a);request["expected_revision"]=inventoryRevision;auto p=a["pool"].to<JsonObject>();p["pool_id"]=poolId;char physical[160];snprintf(physical,sizeof(physical),"%s:%s",journal.state["installation_id"]|"",poolNames[inventoryIndex]);p["physical_identity"]=physical;p["resource_kind"]="stock";p["unit"]=poolUnits[inventoryIndex];uint64_t capacities[]={1,capacity()?1ULL:0ULL,8,8,memory.internal_free,memory.psram_free};p["allocatable_limit"]=capacities[inventoryIndex];p["external_usage"]=0;p["accounting_basis"]="external_only";p["inventory_revision"]=now();p["reservation_watermark"]=d["reservation_watermark"]|0;p["valid_until"]=net.wall(now())*1000+90000;return send(false);
 }
 case Action::PoolPut:if(++inventoryIndex==6){inventoryIndex=0;poolStage=false;}return finish();
 case Action::ProviderGet:{
  auto p=config["service"]["providers"][inventoryIndex];if(d["definition_digest"]!=p["definition_digest"]||d["implementation_ref"]["package_digest"]!=p["package_digest"])return block(Phase::ProtocolBlocked,"provider_package_pin_mismatch");snprintf(offerId,sizeof(offerId),"%s:%s@%u:infra",journal.state["participant_id"]|"",p["provider_id"]|"",p["revision"].as<unsigned>());auto a=make(Action::OfferGet,"catalog","get");a["kind"]="offer";a["entity_id"]=offerId;return send(false);
 }
 case Action::OfferGet:{
  inventoryRevision=missing?0:d["revision"].as<uint64_t>();auto p=config["service"]["providers"][inventoryIndex];auto a=make(Action::OfferPut,"catalog","offer_upsert");session(a);request["expected_revision"]=inventoryRevision;auto o=a["offer"].to<JsonObject>();o["offer_id"]=offerId;o["provider_id"]=p["provider_id"];o["provider_revision"]=p["revision"];o["local_instance_id"]="infra";o["offer_generation"]=sessionGeneration;o["installed_state"]="active";o["observed_health"]=capacity()?"healthy":"capacity_blocked";o["probe_evidence"]["bootstrap"]="dependencies_and_storage_verified";o["probe_evidence"]["hardware_qualification"]="pending";o["valid_until"]=net.wall(now())*1000+90000;for(const char* name:poolNames){char key[160];snprintf(key,sizeof(key),"%s:%s",journal.state["participant_id"]|"",name);o["pool_bindings"][name]=key;}return send(false);
 }
 case Action::OfferPut:if(++inventoryIndex==4){inventoryIndex=0;poolStage=true;inventoryNeeded=false;nextInventory=now()+45000;}return finish();
 case Action::Assignment:{
  auto s=d["spec"].as<JsonObjectConst>();auto input=s["inputs"].as<JsonObjectConst>();auto pin=find(config["service"]["contracts"],"capability_id",s["capability"]["capability_id"]|"");
  if(!keys(input,{"binding_id","binding_revision"})||measureJson(s)>1600||s["capability"]["contract_digest"]!=pin["contract_digest"]||s["capability"]["api_version"]!=1||!digest(d["spec_digest"])||!text(d["id"],128)||(s["mode"]!="job"&&s["mode"]!="desired_state")||s["artifact_refs"].size()||s["acceptance_policy"]["kind"]!="schema_only")return block(Phase::ProtocolBlocked,"assignment_not_admitted");
  bool admitted=false;for(unsigned i=0;i<bindings().size();++i){auto b=bindingAt(i);if(b["binding_id"]==input["binding_id"]&&b["revision"]==input["binding_revision"]&&b["capability"]==s["capability"]["capability_id"]&&b["operation"]==s["operation"]&&b["effect_class"]==s["recovery_policy"]["effect_class"]&&!bindingDisabled[i])admitted=true;}
  if(s["mode"]=="desired_state"&&s["operation"]!="maintain")admitted=false;
  if(!admitted||!capacity())return block(Phase::CapacityBlocked,"assignment_binding_or_capacity");auto j=journal.state["job"].to<JsonObject>();j["phase"]="offered";j["assignment_id"]=d["id"];j["assignment_revision"]=d["revision"];j["spec_digest"]=d["spec_digest"];j["spec"]=s;j["boot_id"]=boot;return persist(Phase::Ready);
 }
 case Action::Plan:{
  auto j=journal.state["job"];auto p=find(config["service"]["providers"],"capability_id",j["spec"]["capability"]["capability_id"]|"");bool admitted=false;
  for(auto c:d["candidates"].as<JsonArrayConst>())if(c["feasibility"]=="ready"&&localPlan(c))admitted=true;
  if(!admitted||!text(d["plan_id"],128))return stopJob("plan_not_locally_admitted");auto a=make(Action::Claim,"execution","claim");session(a);a["assignment_id"]=j["assignment_id"];a["plan_id"]=d["plan_id"];request["expected_revision"]=j["assignment_revision"];return send(true);
 }
 case Action::Claim:{
  auto j=journal.state["job"];if(d["authority"]!="prepare_only"||!text(d["id"],128)||!text(d["group_id"],128)||d["assignment_id"]!=j["assignment_id"])return block(Phase::ProtocolBlocked,"claim_grant_mismatch");j["attempt_id"]=d["id"];j["group_id"]=d["group_id"];j["generation"]=d["generation"];j["phase"]="prepared";return finish(rebootRecovery?Phase::Recover:Phase::Ready);
 }
 case Action::Prepare:{
  auto j=journal.state["job"];if(text(j["binding_id"],128)){if(d["state"]!="starting"&&d["state"]!="preparing")return stopJob("dependency_prepare_not_confirmed");j["phase"]="awaiting_dependency_start";nextSync=0;return finish(rebootRecovery?Phase::Recover:Phase::Ready);}auto g=d["grant"].as<JsonObjectConst>();auto p=find(config["service"]["providers"],"capability_id",j["spec"]["capability"]["capability_id"]|"");if(d["state"]!="starting"||g["attempt_id"]!=j["attempt_id"]||g["generation"]!=j["generation"]||g["spec_digest"]!=j["spec_digest"]||g["provider_digest"]!=p["definition_digest"]||!digest(g["input_digest"])||g["allowed_effects"]!=j["spec"]["recovery_policy"]["effect_class"]||!integer(g["lease_ms"],1000,120000))return block(Phase::ProtocolBlocked,"start_grant_mismatch");char inputHash[65];canonicalDigest(j["spec"]["inputs"],inputHash);if(g["input_digest"]!=inputHash)return block(Phase::ProtocolBlocked,"grant_input_digest_mismatch");j["grant"]=g;j["phase"]="running";leaseUntil=grantDeadline(*this,g);return finish(rebootRecovery?Phase::Recover:Phase::Ready);
 }
 case Action::BindingRenew:if(d["id"]!=journal.state["job"]["binding_id"]||d["ownership"]!="current")return stopJob("dependency_renewal_mismatch");leaseUntil=grantDeadline(*this,d["grant"]);return finish();
 case Action::BindingStop:case Action::BindingResult:{if(d["recorded"]!=true||d["quiescent"]!=true||d["binding_id"]!=journal.state["job"]["binding_id"])return block(Phase::ProtocolBlocked,"dependency_receipt_mismatch");auto p=journal.state["pending"][0];if(p["checkpoint"].is<JsonObject>())journal.state["checkpoints"][p["binding_id"]|""]=p["checkpoint"];journal.state["job"]=nullptr;return finish(rebootRecovery?Phase::Recover:Phase::Ready);}
 case Action::Renew:if(d["id"]!=journal.state["job"]["attempt_id"]||d["generation"]!=journal.state["job"]["generation"]||d["ownership"]!="current")return stopJob("renewal_mismatch");leaseUntil=grantDeadline(*this,d["grant"]);return finish();
 case Action::Result:{
  auto j=journal.state["job"];if(d["accepted"]!=true){j["phase"]="awaiting_acceptance";j["candidate_id"]=request["arguments"]["candidate"]["candidate_id"];waitUntil=now()+15000;return persist(Phase::Ready);}auto p=journal.state["pending"][0];if(p["checkpoint"].is<JsonObject>())journal.state["checkpoints"][p["binding_id"]|""]=p["checkpoint"];journal.state["job"]=nullptr;return finish();
 }
 case Action::Observe:{auto p=journal.state["pending"][0];if(d["sequence"]!=p["request"]["arguments"]["observations"][0]["sequence"])return block(Phase::ProtocolBlocked,"observation_receipt_mismatch");journal.state["checkpoints"][p["binding_id"]|""]=p["checkpoint"];return finish();}
 case Action::Stop:if(d["stopped"]!=true)return block(Phase::ProtocolBlocked,"quiescence_receipt_missing");journal.state["job"]=nullptr;return finish(rebootRecovery?Phase::Recover:Phase::Ready);
 case Action::Maintain:
  if(response["protocol_version"]!="maintenance/1"||d["coalescible"]!=true||!integer(d["progress"]["items_examined"],0,32)||!text(d["target_epoch"],128))return block(Phase::ProtocolBlocked,"maintenance_contract_mismatch");
  if(text(d["cursor"],512))copy(cursors[bindingIndex],513,d["cursor"]);cadence[bindingIndex].lastCompleted=now();if(externalJob){if(journal.state["job"]["spec"]["mode"]=="desired_state"){
   response["last_receipt"]=d;auto a=make(Action::Progress,"execution","progress");attempt(a);a["event_id"]="infra-current";a["observations"]["binding_id"]=bindingAt(bindingIndex)["binding_id"];a["observations"]["desired_revision"]=bindingAt(bindingIndex)["revision"];a["observations"]["last_receipt"]=response["last_receipt"];a["observations"]["freshness_ms"]=cadence[bindingIndex].interval;a["observations"]["observed_at_ms"]=net.wall(now())*1000;return send(false);
  }return result(d);}return finish();
 case Action::Desired:{
  if(d["state"]=="withdrawn"||d["state"]=="expired"||d["expires_at"].as<uint64_t>()<=net.wall(now())*1000){if(journal.state["configuration_desired_id"]==d["id"]){journal.state["configuration"]["bindings"].to<JsonArray>();journal.state["configuration"]["enabled_providers"].to<JsonArray>();configureCadence();inventoryNeeded=true;return persist(snapshotNeeded?Phase::Recover:Phase::Ready);}return finish(snapshotNeeded?Phase::Recover:Phase::Ready);}
  if(d["logical_target"]!="infra.configuration"||d["expires_at"].as<uint64_t>()<=net.wall(now())*1000||!Infra::configuration(d["desired"],config["endpoints"],config["service"]["contact_interval_ms"])||d["desired"]["revision"].as<uint64_t>()<configuration()["revision"].as<uint64_t>())return block(Phase::ProtocolBlocked,"desired_configuration_not_admitted");
  journal.state["configuration_desired_id"]=d["id"];journal.state["configuration_expires_at"]=d["expires_at"];journal.state["configuration"]=d["desired"];journal.state["configuration_revision"]=d["desired"]["revision"];configureCadence();inventoryNeeded=true;auto a=make(Action::DesiredReport,"desired","observe");a["desired_id"]=d["id"];a["desired_revision"]=d["desired_revision"];a["provider_generation"]=sessionGeneration;a["participant_id"]=journal.state["participant_id"];a["actual"]["configuration_revision"]=configuration()["revision"];a["actual"]["binding_count"]=bindings().size();a["actual"]["observed_at_ms"]=net.wall(now())*1000;return send(true);
 }
 case Action::DesiredReport:return finish(snapshotNeeded?Phase::Recover:Phase::Ready);
 case Action::Review:if(d["state"]=="completed"&&d["accepted_result"]==journal.state["job"]["candidate_id"]){auto p=journal.state["pending"][0];if(p["checkpoint"].is<JsonObject>())journal.state["checkpoints"][p["binding_id"]|""]=p["checkpoint"];journal.state["job"]=nullptr;durable=true;return finish();}waitUntil=now()+15000;phase=Phase::Ready;return 1;
 default:return block(Phase::ProtocolBlocked,"unexpected_operation_response");
 }
}
}
