#pragma once
#include <stdint.h>
#include <string.h>
namespace BiomeRelay {
// This validates desired state only. Socket ping, lease renewal and attachment
// never call accept(), and cannot extend the local intent deadline.
struct FreshIntent {
 char authority[65]{},epoch[33]{};uint64_t generation=0,sequence=0,deadline=0,ackSequence=0;
 uint8_t desired=0;bool seen=false,bound=false;
 bool bind(const char* a,uint64_t g,const char* e){
  if(!a||strlen(a)!=64||!e||strlen(e)!=32||!g)return false;
  if(generation==g&&!strcmp(authority,a)&&!strcmp(epoch,e)){detach();bound=true;return true;}
  strcpy(authority,a);strcpy(epoch,e);generation=g;sequence=deadline=ackSequence=0;desired=0;seen=false;bound=true;return true;
 }
 bool accept(const char* a,uint64_t g,const char* e,uint64_t seq,uint8_t mask,
             uint64_t issuedUtcMs,uint64_t expiryUtcMs,uint64_t utcMs,uint64_t monoMs){
  if(!bound||!a||!e||strcmp(a,authority)||strcmp(e,epoch)||g!=generation||
     (seen&&seq<=sequence)||seq>(UINT64_MAX-2)/2||mask>63||!utcMs||issuedUtcMs>utcMs||
     expiryUtcMs<=utcMs||expiryUtcMs<=issuedUtcMs||expiryUtcMs-issuedUtcMs>1000)return false;
  sequence=seq;seen=true;desired=mask;deadline=monoMs+(expiryUtcMs-utcMs);return true;
 }
 uint8_t output(uint64_t now)const{return bound&&seen&&now<deadline?desired:0;}
 // Derive acknowledgment order from the server intent so a reboot can resume
 // with the next fresh intent without resetting this lane's sequence.
 uint64_t acknowledge(uint64_t seq,bool applied){
  if(seq>(UINT64_MAX-2)/2)return 0;
  const uint64_t next=seq*2+(applied?2:1);
  if(next<=ackSequence)return 0;
  ackSequence=next;return next;
 }
 void detach(){desired=0;deadline=0;/* sequence remains high across reconnect */}
 void revoke(){detach();bound=false;}
};
}
