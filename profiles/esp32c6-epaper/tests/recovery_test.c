#include "recovery.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 const char pending[]="{\"pending\":{\"operation_id\":\"same-after-reset\"},\"cursor\":42}";
 unsigned char old[256]={0},next[256]={0},cut[256];uint32_t seq;size_t length;
 recovery_pack(old,pending,sizeof pending,7);
 assert(recovery_valid(old,sizeof pending+RECOVERY_HEADER_BYTES,&seq,&length));assert(seq==7&&length==sizeof pending);
 recovery_pack(next,"{}",3,8);
 /* Every interrupted new-slot write leaves a usable old slot and its exact
  * pending operation/cursor. A fully written new slot supersedes it. */
 for(size_t n=0;n<RECOVERY_HEADER_BYTES+3;n++){
  memset(cut,0xff,sizeof cut);memcpy(cut,next,n);
  assert(!recovery_valid(cut,n,&seq,&length));
  assert(recovery_valid(old,sizeof pending+RECOVERY_HEADER_BYTES,&seq,&length));assert(seq==7);
 }
 for(size_t i=0;i<RECOVERY_HEADER_BYTES+sizeof pending;i++){
  memcpy(cut,old,sizeof cut);cut[i]^=1;
  assert(!recovery_valid(cut,RECOVERY_HEADER_BYTES+sizeof pending,&seq,&length));
 }
 assert(recovery_valid(next,RECOVERY_HEADER_BYTES+3,&seq,&length));assert(seq==8);
 assert(recovery_newer(8,7)&&!recovery_newer(7,8)&&!recovery_newer(7,7));
 assert(recovery_newer(0,UINT32_MAX)&&!recovery_newer(UINT32_MAX,0));
 puts("recovery: interrupted writes, integrity, exact intent and sequence wrap passed");
}
