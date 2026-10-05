#pragma once
#include <stdbool.h>
#include <stdint.h>
/* App-local shared renderer, never a Runtime export or firmware UI API. */
#define PORTABLE_WIFI_VIEW_ROWS 17u
#include "PortableWatchKeyboard.h"
#define PORTABLE_WIFI_KEY_COUNT PWK_COUNT
#define PORTABLE_WIFI_KEYS_PER_PAGE PWK_CHARACTERS
typedef struct {
  const char *title, *message, *detail;
  const char *rows[PORTABLE_WIFI_VIEW_ROWS];
  const char *values[PORTABLE_WIFI_VIEW_ROWS];
  unsigned count, selected;
  bool keyboard;
  unsigned key_page;
  const char *entry; /* Already masked by the controller for passwords. */
} portable_wifi_view;
void portable_wifi_render(const portable_wifi_view *view);
/* Called only after a complete adapter-owned input frame. */
int portable_wifi_hit(const portable_wifi_view *view,int x,int y);
/* Private app/client lifecycle. A false suspend blocks native sleep. A false
 * close blocks handoff/unmapping until the runtime can safely revoke grants. */
/* Reconcile radio health before bound services may read persistent storage. */
bool portable_wifi_services_safe(void);
bool portable_wifi_suspend(void);
void portable_wifi_resume(void);
bool portable_wifi_close(void);
