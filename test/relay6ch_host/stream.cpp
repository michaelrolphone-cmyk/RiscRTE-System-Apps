#include <cassert>
#include <cstring>
#include "../../firmware/waveshare-relay6ch/main/FreshIntent.h"
int main(){
 BiomeRelay::FreshIntent s;const char* a="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
 const char* e="eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
 assert(s.bind(a,4,e));assert(s.output(0)==0);
 assert(s.accept(a,4,e,1,1,9900,10400,10000,100));
 assert(s.output(499)==1);assert(s.output(500)==0);
 assert(!s.accept(a,4,e,1,2,10000,10500,10001,101));
 assert(!s.accept(a,3,e,2,2,10000,10500,10001,101));
 assert(!s.accept(a,4,"old",2,2,10000,10500,10001,101));
 assert(!s.accept(a,4,e,2,2,9000,9500,10000,101));
 assert(s.accept(a,4,e,2,1,10000,10500,10001,101));
 assert(s.output(101)==1);s.detach();assert(s.output(102)==0);
 assert(!s.accept(a,4,e,2,1,10000,10500,10001,102));
 assert(s.accept(a,4,e,3,1,10000,10500,10001,103));
 s.revoke();assert(s.output(104)==0);
 assert(!s.accept(a,4,e,4,1,10000,10500,10001,104));
}
