#pragma once
/* Test-only host observations. These are not Runtime APIs or capability
 * implementations. The host exports them to separately loaded fixture ELFs. */
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

const char *monitor_fixture_mode(void);
uint32_t monitor_fixture_now(void);
void monitor_fixture_advance(uint32_t milliseconds);
void monitor_fixture_delay(uint32_t milliseconds);
void monitor_fixture_event(const char *event);
const char *monitor_fixture_output_dir(void);
unsigned monitor_fixture_width(void);
unsigned monitor_fixture_height(void);
/* The host synchronously copies/saves a P6 PPM from this RGB565 image. The
 * callback does not retain the borrowed pointer or modify its pixels. */
void monitor_fixture_frame(const void *rgb565, unsigned width, unsigned height,
                           unsigned stride_bytes, uint64_t token);
/* A display observation, not a custody or scheduling mechanism. */
void monitor_fixture_present_pending(bool pending);

#ifdef __cplusplus
}
#endif
