#pragma once
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include <string.h>
/* Local app library, not a new runtime ABI. Snapshot is authoritative; a new
 * invocation and every gap/fault require neutral input before rearming. */
typedef struct {
  risc_runtime_capability_v1 grant;
  const risc_touch_api_v1 *api;
  uint64_t subscription;
  bool neutral, down, moved;
  uint16_t x, y;
  uint8_t contact_id;
} portable_touch;
static bool portable_touch_open(portable_touch *t,
                                const risc_runtime_api_v1 *r) {
  memset(t, 0, sizeof(*t));
  t->grant.struct_size = sizeof(t->grant);
  if (!r->acquire("input.touch.raw", 1, 0, &t->grant))
    return false;
  t->api = t->grant.api;
  if (!t->api || t->api->api_version != 1 ||
      t->api->struct_size < sizeof(*t->api) || !t->api->subscribe ||
      !t->api->unsubscribe || !t->api->poll || !t->api->next ||
      !t->api->snapshot)
    return false;
  t->subscription = t->api->subscribe(t->api->context);
  return t->subscription != 0;
}
static bool portable_touch_close(portable_touch *t,
                                 const risc_runtime_api_v1 *r) {
  if (t->subscription && !t->api->unsubscribe(t->api->context, t->subscription))
    return false;
  t->subscription = 0;
  if (t->grant.api && !r->release(&t->grant))
    return false;
  memset(t, 0, sizeof(*t));
  return true;
}
/* Return a completed tap only. No retained event or borrowed driver memory. */
static bool portable_touch_tap(portable_touch *t, uint16_t *x, uint16_t *y) {
  if (!t->subscription)
    return false;
  bool ok = t->api->poll(t->api->context, 1);
  for (unsigned i = 0; i < 32; ++i) {
    risc_touch_event_v1 e;
    int n = t->api->next(t->api->context, t->subscription, &e);
    if (n <= 0) {
      if (n < 0)
        ok = false;
      break;
    }
  }
  risc_touch_snapshot_v1 s = {0};
  if (!t->api->snapshot(t->api->context, &s) || !ok || s.contact_count > 1) {
    t->neutral = t->down = false;
    return false;
  }
  if (!s.contact_count) {
    bool tap = t->neutral && t->down && !t->moved;
    t->neutral = true;
    t->down = false;
    if (tap) {
      *x = t->x;
      *y = t->y;
    }
    return tap;
  }
  if (!t->neutral)
    return false;
  if (!t->down) {
    t->x = s.contacts[0].x;
    t->y = s.contacts[0].y;
    t->down = true;
    t->moved = false;
    t->contact_id = s.contacts[0].id;
  }
  if (t->contact_id != s.contacts[0].id) {
    t->neutral = t->down = false;
    return false;
  }
  int dx = (int)s.contacts[0].x - t->x, dy = (int)s.contacts[0].y - t->y;
  if (dx > 12 || dx < -12 || dy > 12 || dy < -12)
    t->moved = true;
  return false;
}
