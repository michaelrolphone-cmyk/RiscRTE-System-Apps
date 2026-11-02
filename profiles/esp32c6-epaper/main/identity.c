#include "identity.h"
#include <string.h>

bool identity_valid(const char *id) {
 if (!id || strncmp(id,"participant_",12) || strlen(id)<=12 || strlen(id)>=128) return false;
 for (const unsigned char *p=(const unsigned char *)id;*p;p++)
  if (!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')) return false;
 return true;
}
identity_action identity_resolve(const char *journal_id,const char *provisioned_id) {
 const char *j=journal_id?journal_id:"",*p=provisioned_id?provisioned_id:"";
 if ((*j&&!identity_valid(j))||(*p&&!identity_valid(p))) return IDENTITY_INVALID;
 if (*j&&*p&&strcmp(j,p)) return IDENTITY_INVALID;
 if (*j) return IDENTITY_JOURNAL;
 return *p?IDENTITY_RESTORE:IDENTITY_ENROLL;
}
