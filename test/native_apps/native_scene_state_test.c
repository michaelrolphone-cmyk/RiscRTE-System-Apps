#include <assert.h>
#include <stdio.h>
#include "PortableNativeSceneState.h"
int main(void){
 portable_native_scene_state s={0};assert(pns_accept(&s,false,false,true,false,false,false,0,0));
 assert(pns_begin(&s)&&!s.known&&!s.visible);assert(pns_layout(&s,10));
 pns_submit(&s);uint64_t generation=s.generation;assert(pns_layout(&s,11));pns_complete(&s);assert(!s.awaiting_observation);
 assert(pns_layout(&s,11)&&s.generation==generation+1);pns_submit(&s);pns_complete(&s);assert(s.awaiting_observation&&!s.visible);
 pns_observe(&s,100);assert(s.visible&&!s.gesture);
 assert(!pns_accept(&s,true,true,false,true,true,false,1,100));
 assert(!pns_accept(&s,true,true,false,false,false,true,1,101));
 assert(pns_accept(&s,true,true,false,true,true,false,1,102));
 pns_submit(&s);pns_complete(&s);assert(s.gesture&&s.visible&&!s.awaiting_observation);
 assert(!pns_accept(&s,true,true,false,false,true,false,2,103));
 assert(pns_accept(&s,true,true,false,false,false,true,1,104));
 assert(!s.gesture);assert(pns_accept(&s,true,true,false,false,false,false,0,104));
 assert(pns_invalidate(&s)&&!s.visible);pns_submit(&s);pns_complete(&s);pns_observe(&s,200);
 assert(!pns_accept(&s,false,true,false,true,true,false,1,201));
 for(unsigned n=0;n<1000;++n){assert(pns_layout(&s,n));pns_submit(&s);assert(pns_invalidate(&s));pns_complete(&s);assert(!s.visible&&!s.awaiting_observation);}
 s.generation=UINT64_MAX;assert(!pns_invalidate(&s)&&s.exhausted&&!s.visible);assert(!pns_begin(&s));
 puts("PASS native scene pure state: generation, stale/equal-map completion, fresh edge, contact ownership, foreign invalidation, overflow");
}
