#pragma once
#include <stdbool.h>
#include <stdint.h>

/* RAM-only knowledge of this boot's completed panel refresh. Journal receipts
 * describe past effects, not the bitmap currently on the panel. */
typedef struct {
 bool logo, frame;
 char desired_id[256], sha256[65];
 int revision;
 int64_t expires_ms;
} presentation;
bool presentation_needs_logo(const presentation *p, int64_t now);
bool presentation_same_frame(const presentation *p, const char *sha, int64_t now);
bool presentation_set_frame(presentation *p, const char *id, int revision,
                            const char *sha, int64_t expires_ms);
void presentation_withdraw(presentation *p, const char *id, int revision);
void presentation_set_logo(presentation *p);
