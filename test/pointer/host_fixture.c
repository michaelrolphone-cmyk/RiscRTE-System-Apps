#include "../../Services/scene_host/host.c"
void pointer_test_hit(uint32_t id,int *x,int *y){
 for(unsigned i=0;i<visible.count;i++)if(visible.items[i].node==id){
  *x=visible.items[i].x+visible.items[i].w/2;*y=visible.items[i].y+visible.items[i].h/2;return;}
 *x=*y=-1;
}
int pointer_test_fault(void){return hold_local_fault();}

void pointer_test_focus(uint32_t id){path.focus[path.depth-1]=id;}

void pointer_test_exhaust(void){event_serial=UINT64_MAX;}
void pointer_test_kind(uint32_t kind,int*x,int*y){for(unsigned i=0;i<visible.count;i++)if(visible.items[i].kind==kind){*x=visible.items[i].x+visible.items[i].w/2;*y=visible.items[i].y+visible.items[i].h/2;return;}*x=*y=-1;}
