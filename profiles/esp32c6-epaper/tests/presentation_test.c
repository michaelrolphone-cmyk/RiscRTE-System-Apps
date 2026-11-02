#include "presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
 presentation p={0};const char *sha="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
 assert(presentation_needs_logo(&p,0));
 presentation_set_logo(&p);assert(!presentation_needs_logo(&p,100000));
 assert(!presentation_same_frame(&p,sha,0));
 assert(presentation_set_frame(&p,"display",2,sha,1000));
 assert(!presentation_needs_logo(&p,999));assert(presentation_same_frame(&p,sha,999));
 presentation_withdraw(&p,"other",3);assert(!presentation_needs_logo(&p,999));
 presentation_withdraw(&p,"display",1);assert(!presentation_needs_logo(&p,999));
 assert(presentation_needs_logo(&p,1000));assert(!presentation_same_frame(&p,sha,1000));
 presentation_set_logo(&p);assert(!presentation_same_frame(&p,sha,1001));
 assert(presentation_set_frame(&p,"display",3,sha,2000));
 presentation_withdraw(&p,"display",3);assert(presentation_needs_logo(&p,1001));
 presentation_set_logo(&p);assert(!presentation_needs_logo(&p,1001));
 assert(presentation_set_frame(&p,"display",4,sha,3000));
 /* Same bitmap with a new desired revision renews its ownership and expiry. */
 assert(presentation_same_frame(&p,sha,2000));
 assert(presentation_set_frame(&p,"display",5,sha,4000));
 presentation_withdraw(&p,"display",4);assert(!presentation_needs_logo(&p,3500));
 char long_id[257];memset(long_id,'x',256);long_id[256]=0;
 assert(!presentation_set_frame(&p,long_id,6,sha,5000));
 assert(p.revision==5 && p.expires_ms==4000);
 puts("presentation: boot, expiry, withdrawal, renewal and visible-frame dedup passed");
}
