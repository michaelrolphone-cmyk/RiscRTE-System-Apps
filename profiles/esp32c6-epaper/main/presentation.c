#include "presentation.h"
#include <string.h>

bool presentation_needs_logo(const presentation *p, int64_t now) {
 return !p->logo && (!p->frame || now >= p->expires_ms);
}
bool presentation_same_frame(const presentation *p, const char *sha, int64_t now) {
 return p->frame && now < p->expires_ms && !strcmp(p->sha256, sha);
}
bool presentation_set_frame(presentation *p, const char *id, int revision,
                            const char *sha, int64_t expires_ms) {
 if (!id[0] || strlen(id) >= sizeof p->desired_id || strlen(sha) != 64) return false;
 *p=(presentation){.frame=true,.revision=revision,.expires_ms=expires_ms};
 strcpy(p->desired_id,id);strcpy(p->sha256,sha);return true;
}
void presentation_withdraw(presentation *p, const char *id, int revision) {
 if (p->frame && !strcmp(p->desired_id,id) && revision >= p->revision) p->frame=false;
}
void presentation_set_logo(presentation *p) { *p=(presentation){.logo=true}; }
