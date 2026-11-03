#pragma once
namespace Infra {
inline int Node::hardwareStep(){
 auto j=journal.state["job"];const char* cap=j["spec"]["capability"]["capability_id"]|"";
 if(drain||j["boot_id"]!=boot||rebootRecovery||!hardwareReady()||now()+8000>=leaseUntil)return stopJob("hardware_grant_expired_or_reboot_outcome_unknown");
 if(!hardwareInput(cap,j["spec"]["operation"]|"",j["spec"]["inputs"])||j["grant"]["allowed_effects"]!=hardwareEffect(cap))return stopJob("hardware_input_or_effect_changed");
 if(j["phase"]=="running"){j["phase"]="hardware_intent";return persist(Phase::Ready);}
 if(j["phase"]=="hardware_armed"){
  // The acknowledged remote intent AND durable local phase precede I/O.
  // This boot executes once; neither a recovered grant nor a saved intent
  // can replay a UART transmission or buzzer tone after a reset.
  auto out=j["hardware_output"].to<JsonObject>();int r=hardware.execute(hardware.context,cap,j["spec"]["inputs"],out);
  out["status"]=r==0?"succeeded":"unknown";j["hardware_outcome"]=r==0?"succeeded":"unknown";j["phase"]="hardware_done";
  if(measureJson(out)>1800)return block(Phase::CapacityBlocked,"hardware_result_exceeds_bound");return persist(Phase::Ready);
 }
 if(j["phase"]=="hardware_result")return result(j["hardware_output"].as<JsonObjectConst>());
 const bool intent=j["phase"]=="hardware_intent";auto a=make(intent?Action::HardwareIntent:Action::HardwareOutcome,"execution","effect_report");attempt(a);
 auto e=a["effect"].to<JsonObject>();e["logical_key"]="board-operation";e["request_hash"]=j["grant"]["input_digest"];e["effect_class"]=hardwareEffect(cap);e["outcome"]=intent?"intent":(j["hardware_outcome"]|"unknown");return send(true);
}
}
