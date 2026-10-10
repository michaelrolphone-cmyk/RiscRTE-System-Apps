#pragma once
#include <stdbool.h>
/* File handles never cross a poll, sleep, alarm or quick-control boundary.
 * A failed read-handle close retains its grant and blocks further storage I/O.
 * The controller retries Close/Back; it never abandons uncertain ownership. */
#ifdef PORTABLE_TOUCH_SCROLL
#include "PortableTouchScroll.h"
void portable_file_browser_scroll_interrupt(void);
void portable_file_browser_touch_sample(portable_touch_sample *sample,uint32_t *now);
void portable_file_browser_scroll_clip(const portable_scroll_viewport *view);
#endif
bool portable_file_browser_safe(void);
bool portable_file_browser_close(void);
#ifdef PORTABLE_FILE_SHARING
bool portable_file_browser_sharing_active(void);
bool portable_file_browser_stop_pending(void);
bool portable_file_browser_network_owned(void);
bool portable_file_browser_idle_ready(void);
bool portable_file_browser_services_safe(void);
void portable_file_browser_resume(void);
bool portable_file_browser_services_begin(void);
bool portable_file_browser_services_end(void);
#endif

#ifdef PORTABLE_FILE_SETUP
bool portable_file_browser_setup_service(void);
bool portable_file_browser_setup_cleanup(void);
bool portable_file_browser_cleanup_only(void);
bool portable_file_browser_setup_service_supported(void);
bool portable_file_browser_setup_frame_ready(void);
#endif
