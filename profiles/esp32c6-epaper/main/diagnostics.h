#pragma once
#include <stdbool.h>
#include <stdint.h>
#define DIAGNOSTIC_BYTES 48000
typedef struct {
 int64_t failed_since, refreshed_at;
 bool failing, shown;
} diagnostic_display;
/* First persistent failure after 60s; subsequent snapshots at most every 10min.
 * A fatal stop bypasses the first delay, not the repeated-refresh limit. */
bool diagnostic_due(diagnostic_display *d, int64_t now, bool failing, bool fatal);
void diagnostic_shown(diagnostic_display *d, int64_t now);
void diagnostic_render(uint8_t *out, const char *const *lines, unsigned count);
