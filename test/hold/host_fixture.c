/* White-box fault injection only; the provider implementation is included verbatim. */
#include "../../Services/scene_host/host.c"
int hold_test_local_fault(void){return hold_local_fault();}
void hold_test_hit(uint32_t id,int *x,int *y){for(unsigned i=0;i<visible.count;i++)if(visible.items[i].node==id){*x=visible.items[i].x+visible.items[i].w/2;*y=visible.items[i].y+visible.items[i].h/2;return;}*x=*y=-1;}
void hold_test_queue_full(void){raw_count=RAW_QUEUE_SIZE;}
