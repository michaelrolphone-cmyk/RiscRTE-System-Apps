#include "identity.h"
#include <assert.h>
#include <string.h>
int main(void) {
 const char *id="participant_c74a4f85-9f94-47af-bb84-506feacf41d6";
 assert(identity_resolve("","")==IDENTITY_ENROLL);
 assert(identity_resolve(NULL,NULL)==IDENTITY_ENROLL);
 assert(identity_resolve(id,"")==IDENTITY_JOURNAL);
 assert(identity_resolve("",id)==IDENTITY_RESTORE);
 assert(identity_resolve(id,id)==IDENTITY_JOURNAL);
 assert(identity_resolve(id,"participant_other")==IDENTITY_INVALID);
 assert(identity_resolve("not-a-participant",id)==IDENTITY_INVALID);
 assert(identity_resolve("","participant_")==IDENTITY_INVALID);
 assert(identity_resolve("","participant_bad/path")==IDENTITY_INVALID);
 char too_long[129];memset(too_long,'a',128);memcpy(too_long,"participant_",12);too_long[128]=0;
 assert(!identity_valid(too_long));
 return 0;
}
