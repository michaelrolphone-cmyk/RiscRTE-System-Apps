#include "../../Services/biome_node/ProviderState.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace BiomeRelay;
static const std::string Actor(64,'a');
static constexpr const char* Resource="relay6ch.001122aabbcc";
struct Board {
 JsonDocument saved;unsigned saves=0,failSave=0,executions=0,polls=0,cancels=0;
 bool asynchronous=false;unsigned runningPolls=1,cancelPasses=2;
 static bool save(void* p,JsonObjectConst journal){
  auto& b=*static_cast<Board*>(p);if(++b.saves==b.failSave)return false;b.saved.set(journal);return true;
 }
 static bool ready(void*){return true;}
 static int execute(void* p,const char*,JsonObjectConst input,JsonObject output){
  auto& b=*static_cast<Board*>(p);
  // The on-disk intent MUST precede even the first GPIO/UART callback.
  assert(b.saved["command"]["state"]=="armed"&&b.saved["command"]["claimed"]==true);
  ++b.executions;if(b.asynchronous)return 1;
  output["channel"]=input["channel"];output["on"]=input["on"];return 0;
 }
 static int poll(void* p,JsonObject out){
  auto& b=*static_cast<Board*>(p);if(++b.polls<=b.runningPolls)return 1;
  out["steps_completed"]=2;out["status"]="completed";return 0;
 }
 static bool cancel(void* p){auto& b=*static_cast<Board*>(p);return ++b.cancels>=b.cancelPasses;}
 Infra::Hardware hardware(){return {this,ready,execute,poll,cancel};}
 void begin(ProviderState& state){
  assert(state.begin({this,save},hardware(),saved.as<JsonObjectConst>()));
  assert(state.identity(Actor.c_str(),Resource));
 }
};
static JsonDocument fence(unsigned generation=1,const char* lease="lease-one"){
 JsonDocument d;d["resource"]=Resource;d["guard"]=Resource;d["lease"]=lease;d["generation"]=generation;return d;
}
static JsonDocument action(const char* id="action-one",bool batch=false,unsigned generation=1,const char* lease="lease-one"){
 JsonDocument d;d["id"]=id;d["provider"]=Actor;d["contract"]=batch?"device.outputs.batch:1":"relay.bank:1";
 d["resource"]=Resource;d["guard"]=Resource;d["lease"]=lease;d["generation"]=generation;d["state"]="queued";
 if(batch){auto steps=d["inputs"]["steps"].to<JsonArray>();auto a=steps.add<JsonObject>();a["on"].to<JsonArray>().add(1);a["wait_ms"]=100;
  auto b=steps.add<JsonObject>();b["off"].to<JsonArray>().add(1);}
 else{d["inputs"]["channel"]=1;d["inputs"]["on"]=true;}
 return d;
}
static void bind(ProviderState& state){auto f=fence();assert(state.fence(f.as<JsonObjectConst>())==0);}
static void claim(ProviderState& state,JsonDocument& a){a["state"]="claimed";assert(state.claimed(a.as<JsonObjectConst>()));}
static void accept(ProviderState& state,JsonDocument& a){
 a["state"]=state.journal["command"]["state"];a["result"]=state.journal["command"]["result"];
 assert(state.reportAccepted(a.as<JsonObjectConst>()));
}
static void retries(){
 Board board;ProviderState state;board.begin(state);auto a=action();
 assert(!state.stage(a.as<JsonObjectConst>())&&board.executions==0);bind(state);
 assert(state.stage(a.as<JsonObjectConst>()));assert(state.stage(a.as<JsonObjectConst>()));
 assert(state.needsClaim()&&board.executions==0);claim(state,a);
 assert(board.executions==1&&state.outcome()&&state.journal["command"]["state"]=="succeeded");
 assert(state.claimed(a.as<JsonObjectConst>())&&board.executions==1);
 auto changed=action();changed["inputs"]["on"]=false;
 assert(!state.stage(changed.as<JsonObjectConst>())&&board.executions==1);
 auto receipt=action();receipt["state"]="succeeded";receipt["result"]["channel"]=2;receipt["result"]["on"]=true;
 assert(!state.reportAccepted(receipt.as<JsonObjectConst>()));
 accept(state,a);assert(!state.pending());assert(state.stage(a.as<JsonObjectConst>())&&board.executions==1);
 auto next=action("action-two");assert(state.stage(next.as<JsonObjectConst>()));claim(state,next);assert(board.executions==2);
 // Reuse of an older controller ID cannot regain execution permission:
 // provider.claim returns the terminal native receipt, which is adopted.
 accept(state,next);a["state"]="claimed";
 assert(state.stage(a.as<JsonObjectConst>()));a["state"]="succeeded";
 assert(state.claimed(a.as<JsonObjectConst>())&&!state.pending()&&board.executions==2);
}
static void admission(){
 Board board;ProviderState state;board.begin(state);bind(state);
 for(const char* field:{"provider","resource","guard","lease","contract"}){
  auto a=action();a[field]="wrong";assert(!state.stage(a.as<JsonObjectConst>()));
 }
 auto different=fence(1,"another-holder");assert(state.fence(different.as<JsonObjectConst>())<0);
 auto newer=fence(2,"lease-two");assert(state.fence(newer.as<JsonObjectConst>())==0);
 auto older=fence();assert(state.fence(older.as<JsonObjectConst>())<0);
 assert(state.fence(newer.as<JsonObjectConst>())==0&&board.executions==0);
}
static void invalidInputsSettle(){
 for(unsigned scenario=0;scenario<6;++scenario){
  Board board;ProviderState state;board.begin(state);bind(state);
  auto bad=action("invalid",scenario>=2);
  if(scenario==0)bad["inputs"]["channel"]=7;
  else if(scenario==1)bad["inputs"]["pin"]=45;
  else if(scenario==2)bad["inputs"]["steps"][1]["off"][0]=7;
  else if(scenario==3)bad["inputs"]["steps"][0]["wait_ms"]=1.5;
  else if(scenario==4)bad["inputs"]["steps"][0]["wait_ms"]=1.0;
  else{auto steps=bad["inputs"]["steps"].to<JsonArray>();for(unsigned i=0;i<129;++i)steps.add<JsonObject>()["wait_ms"]=1;}
  assert(state.stage(bad.as<JsonObjectConst>())&&board.executions==0);
  // Type mutation from invalid float to valid int cannot reuse an intent.
  if(scenario==4){
   JsonDocument mutated;mutated.set(bad);mutated["state"]="claimed";mutated["inputs"]["steps"][0]["wait_ms"]=1;
   assert(!state.claimed(mutated.as<JsonObjectConst>())&&board.executions==0);
  }
  claim(state,bad);
  assert(state.journal["command"]["state"]=="failed"&&state.outcome()&&board.executions==0&&board.polls==0);
  assert(state.journal["command"]["result"]["reason"]=="invalid_hardware_inputs");
  ProviderState rebooted;board.begin(rebooted);
  assert(rebooted.journal["command"]["state"]=="failed"&&board.executions==0);
  accept(rebooted,bad);
  auto next=action("valid-after-invalid");assert(rebooted.stage(next.as<JsonObjectConst>()));claim(rebooted,next);
  assert(board.executions==1&&rebooted.journal["command"]["state"]=="succeeded");accept(rebooted,next);
 }
}
static void recovery(){
 // Reset before the claim request: only a claim-to-report-uncertain is allowed.
 Board before;ProviderState first;before.begin(first);bind(first);auto a=action();
 assert(first.stage(a.as<JsonObjectConst>()));ProviderState restarted;before.begin(restarted);
 assert(restarted.outcome()&&restarted.journal["command"]["state"]=="uncertain"&&restarted.journal["command"]["claimed"]==false);
 claim(restarted,a);assert(before.executions==0&&restarted.journal["command"]["claimed"]==true);accept(restarted,a);

 // Reset after the durable arm or while an asynchronous output is in flight.
 Board during;during.asynchronous=true;ProviderState running;during.begin(running);bind(running);auto batch=action("batch-one",true);
 assert(running.stage(batch.as<JsonObjectConst>()));claim(running,batch);
 assert(running.active()&&during.saved["command"]["state"]=="armed");
 ProviderState boot;during.begin(boot);assert(!boot.active()&&boot.outcome());
 claim(boot,batch);boot.tick();assert(during.executions==1&&during.polls==0);
 assert(boot.journal["command"]["state"]=="uncertain");accept(boot,batch);

 // A completed durable receipt survives loss of its HTTP result reply.
 Board completed;ProviderState once;completed.begin(once);bind(once);auto done=action();
 assert(once.stage(done.as<JsonObjectConst>()));claim(once,done);
 ProviderState after;completed.begin(after);assert(after.journal["command"]["state"]=="succeeded");
 claim(after,done);assert(completed.executions==1);accept(after,done);
}
static void storageFailures(){
 for(unsigned phase:{0u,1u,2u}){
  Board board;ProviderState state;board.begin(state);bind(state);auto a=action();
  if(phase==0)board.failSave=board.saves+1;
  const bool staged=state.stage(a.as<JsonObjectConst>());
  if(phase==0){assert(!staged&&!state.healthy&&board.executions==0);continue;}
  assert(staged);board.failSave=board.saves+phase; // arm or completion
  a["state"]="claimed";assert(!state.claimed(a.as<JsonObjectConst>()));
  assert(board.executions==(phase==2?1u:0u));assert(!state.healthy);
  board.failSave=0;ProviderState recovered;board.begin(recovered);claim(recovered,a);
  assert(recovered.journal["command"]["state"]=="uncertain"&&board.executions==(phase==2?1u:0u));
 }
 Board corrupt;corrupt.saved["schema_version"]=99;ProviderState refused;
 assert(!refused.begin({&corrupt,Board::save},corrupt.hardware(),corrupt.saved.as<JsonObjectConst>())&&corrupt.executions==0);
}
static void batchesAndFences(){
 Board board;board.asynchronous=true;board.cancelPasses=3;
 ProviderState state;board.begin(state);bind(state);auto batch=action("batch",true);
 assert(state.stage(batch.as<JsonObjectConst>()));claim(state,batch);state.tick();
 assert(state.active()&&board.executions==1&&board.polls==1);
 auto replacement=fence(2,"lease-two");
 assert(state.fence(replacement.as<JsonObjectConst>())==1);
 assert(board.saved["fence"]["generation"]==1&&state.active());
 state.tick();assert(state.active()&&board.saved["fence"]["generation"]==1);
 state.tick();assert(!state.active()&&state.journal["command"]["state"]=="uncertain");
 assert(state.fence(replacement.as<JsonObjectConst>())==0&&board.saved["fence"]["generation"]==2);
 accept(state,batch);assert(board.executions==1&&!state.pending());

 Board completed;completed.asynchronous=true;ProviderState run;completed.begin(run);bind(run);auto program=action("done-batch",true);
 assert(run.stage(program.as<JsonObjectConst>()));claim(run,program);run.tick();run.tick();
 assert(!run.active()&&run.journal["command"]["state"]=="succeeded"&&completed.executions==1);
 assert(run.journal["command"]["result"]["steps_completed"]==2);

 // Installing a new fence also invalidates a live but not yet claimed intent.
 Board preclaim;ProviderState pending;preclaim.begin(pending);bind(pending);auto queued=action();
 assert(pending.stage(queued.as<JsonObjectConst>()));assert(pending.fence(replacement.as<JsonObjectConst>())==0);
 claim(pending,queued);assert(preclaim.executions==0&&pending.journal["command"]["state"]=="uncertain");
}
int main(){retries();admission();invalidInputsSettle();recovery();storageFailures();batchesAndFences();
 puts("Biome provider state: strict admission, invalid input settlement, durable arm, exact retries, conflict rejection, reboot uncertainty, storage failure, and quiescent fences PASS");}
