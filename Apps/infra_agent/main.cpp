#include <RiscRuntimeV1.h>
#include <RiscMemoryHeapV1.h>
#include "../../Services/infra_node/RiscInfraNodeV1.h"
#include <cstdio>
#include <cstring>
extern "C" __attribute__((visibility("default"))) void app_main(){
 const auto* rt=risc_runtime_get_api(1);if(!rt||rt->struct_size<RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE||!rt->retain_invocation)return;
 rt->diagnostic("infra startup=runtime-ready");
 risc_runtime_capability_v1 heap{sizeof(heap)},grant{sizeof(grant)};
 rt->diagnostic("infra startup=memory-acquire");
 if(!rt->acquire("memory.heap",1,0,&heap)){rt->diagnostic("infra startup=memory-acquire-failed");return;}
 rt->diagnostic("infra startup=node-acquire");
 if(!rt->acquire("infra.node",1,0,&grant)){rt->diagnostic("infra startup=node-acquire-failed");rt->release(&heap);return;}
 rt->diagnostic("infra startup=node-ready");
 auto* node=static_cast<const risc_infra_node_v1*>(grant.api);auto* mem=static_cast<const risc_memory_heap_api_v1*>(heap.api);
 if(!node||node->api_version!=1||node->struct_size<sizeof(*node)||!mem||mem->api_version!=1||mem->struct_size<sizeof(*mem)){rt->diagnostic("infra startup=api-rejected");rt->retain_invocation();return;}
 bool bootConfirmed=false,stopping=false,firstAdvance=true;uint64_t lastReport=0;uint32_t previousState=~0u;char previousPending[128]{},previousRejected[128]{};
 rt->diagnostic("infra startup=memory-snapshot");
 for(;;){
  risc_infra_memory_v1 m{};m.struct_size=sizeof(m);uint64_t* free[]={&m.internal_free,&m.psram_free,&m.dma_free};uint64_t* largest[]={&m.internal_largest,&m.psram_largest,&m.dma_largest};
  for(unsigned i=0;i<3;++i){risc_memory_heap_snapshot_v1 s{sizeof(s),i+1};if(mem->snapshot(mem->context,&s)==0){*free[i]=s.free_bytes;*largest[i]=s.largest_block;}}
  risc_memory_heap_snapshot_v1 stack{sizeof(stack),RISC_MEMORY_STACK_HEADROOM};if(mem->snapshot(mem->context,&stack)==0)m.stack_headroom=stack.free_bytes;
  int r=node->memory(node->context,&m);if(r==-2){rt->retain_invocation();return;}
  if(firstAdvance)rt->diagnostic("infra startup=bootstrap");
  stopping|=rt->exit_requested&&rt->exit_requested();r=stopping?node->shutdown(node->context):node->advance(node->context);
  if(firstAdvance){rt->diagnostic("infra startup=bootstrap-returned");firstAdvance=false;}
  if(r==-2){rt->retain_invocation();return;}
  if(stopping&&r==0)break;
  risc_infra_status_v1 s{};s.struct_size=sizeof(s);if(node->snapshot(node->context,&s)==-2){rt->retain_invocation();return;}
  if(!bootConfirmed&&s.ready_for_boot_confirmation){bootConfirmed=rt->confirm_boot&&rt->confirm_boot();}
  if(s.state!=previousState||s.uptime_ms-lastReport>=15000){char line[256];snprintf(line,sizeof(line),"infra state=%u reason=%.70s pending=%u writes=%llu bytes=%llu internal=%llu largest=%llu dma=%llu psram=%llu stack=%u",s.state,s.reason,s.pending,(unsigned long long)s.storage_writes,(unsigned long long)s.storage_bytes,(unsigned long long)m.internal_free,(unsigned long long)m.internal_largest,(unsigned long long)m.dma_free,(unsigned long long)m.psram_free,m.stack_headroom);rt->diagnostic(line);previousState=s.state;lastReport=s.uptime_ms;}
  if(strcmp(previousPending,s.pending_operation_id)||strcmp(previousRejected,s.rejected_operation_id)){char line[384];snprintf(line,sizeof(line),"infra request action=%u operation_id=%.127s rejected_action=%u rejected_operation_id=%.127s code=%.40s",s.pending_action,s.pending_operation_id,s.rejected_action,s.rejected_operation_id,s.rejection_code);rt->diagnostic(line);strcpy(previousPending,s.pending_operation_id);strcpy(previousRejected,s.rejected_operation_id);}
  rt->yield_ms(10);
 }
 if(!rt->release(&grant)){rt->retain_invocation();return;}if(!rt->release(&heap)){rt->retain_invocation();return;}
}
