#include "../../Services/scene_host/host.c"
void pointer_test_hit(uint32_t id,int *x,int *y){
 for(unsigned i=0;i<visible.count;i++)if(visible.items[i].node==id){
  *x=visible.items[i].x+visible.items[i].w/2;*y=visible.items[i].y+visible.items[i].h/2;return;}
 *x=*y=-1;
}
int pointer_test_fault(void){return hold_local_fault();}
