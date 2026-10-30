#include "Node.h"
#include <cassert>
#include <cstdio>
int main(){
 Infra::Node node;
 node.loaded=true;node.rebootRecovery=false;node.snapshotNeeded=false;
 node.inventoryNeeded=false;node.poolsEstablished=true;node.sessionId[0]='s';node.status.last_sync_ms=100;
 node.phase=Infra::Phase::Ready;
 node.config["configuration"]["bindings"][0]["required"]=true;
 node.config["configuration"]["bindings"][1]["required"]=false;
 node.cadence[0].lastCompleted=100;
 assert(node.bootReady());
 for(auto phase:{Infra::Phase::Cold,Infra::Phase::Connect,Infra::Phase::Nonce,Infra::Phase::Recover,Infra::Phase::Backoff,Infra::Phase::Drain,Infra::Phase::Retained,Infra::Phase::AuthBlocked}){node.phase=phase;assert(!node.bootReady());}
 node.phase=Infra::Phase::Ready;
 node.status.last_sync_ms=0;assert(!node.bootReady());node.status.last_sync_ms=100;
 node.snapshotNeeded=true;assert(!node.bootReady());node.snapshotNeeded=false;
 node.rebootRecovery=true;assert(!node.bootReady());node.rebootRecovery=false;
 node.inventoryNeeded=true;assert(!node.bootReady());node.inventoryNeeded=false;
 node.cadence[0].lastCompleted=0;assert(!node.bootReady());node.cadence[0].lastCompleted=100;
 node.bindingDisabled[0]=true;assert(!node.bootReady());node.bindingDisabled[0]=false;
 node.journal.state["pending"][0]["action"]=1;assert(!node.bootReady());node.journal.state["pending"].clear();
 node.journal.writeToken=1;assert(!node.bootReady());node.journal.writeToken=0;
 node.journal.blocked=true;assert(!node.bootReady());node.journal.blocked=false;
 node.config["low_water"]["internal_free"]=1;assert(!node.bootReady());node.memory.internal_free=1;
 assert(node.bootReady());puts("Boot health: early/connect/backoff/unreconciled/unsynced/required-target/storage/capacity states rejected; healthy current boot accepted");
}
