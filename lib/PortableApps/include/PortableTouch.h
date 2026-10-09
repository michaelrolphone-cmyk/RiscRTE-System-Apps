#pragma once
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include <string.h>
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#include "PortableNativeCustody.h"
#endif
#ifndef PORTABLE_TOUCH_ROTATION
#define PORTABLE_TOUCH_ROTATION 0
#endif
#if PORTABLE_TOUCH_ROTATION != 0 && PORTABLE_TOUCH_ROTATION != 180
#error "Portable touch supports explicit 0 or 180 degree deployment rotation"
#endif
static inline void portable_touch_orient(uint16_t width,uint16_t height,uint16_t *x,uint16_t *y) {
#if PORTABLE_TOUCH_ROTATION == 180
  *x=(uint16_t)(width-1-*x);*y=(uint16_t)(height-1-*y);
#else
  (void)width;(void)height;(void)x;(void)y;
#endif
}
/* App-local copies only. Every invocation/fault requires neutral before tap.
 * A consumer may adopt its INITIAL held contact as drag-only, never as a tap. */
typedef struct {
  risc_runtime_capability_v1 grant;
  const risc_touch_api_v1 *api;
  uint64_t subscription;
  bool neutral, down, moved;
  bool home_neutral, home_down;
  uint16_t x, y;
  uint8_t contact_id;
} portable_touch;
typedef struct {
  bool valid, down, began, released, cancelled, tap_eligible, moved;
  uint16_t x, y;
  bool home_pressed;
} portable_touch_sample;
static bool portable_touch_open(portable_touch *t,
                                const risc_runtime_api_v1 *r) {
  memset(t, 0, sizeof(*t));
  t->grant.struct_size = sizeof(t->grant);
  if (!r->acquire("input.touch.raw", 1, 0, &t->grant)) return false;
  t->api = t->grant.api;
  if (!t->api || t->api->api_version != 1 ||
      t->api->struct_size < sizeof(*t->api) || !t->api->subscribe ||
      !t->api->unsubscribe || !t->api->poll || !t->api->next ||
      !t->api->snapshot) return false;
  t->subscription = t->api->subscribe(t->api->context);
  return t->subscription != 0;
}
static bool portable_touch_close(portable_touch *t,
                                 const risc_runtime_api_v1 *r) {
  if (t->subscription && !t->api->unsubscribe(t->api->context, t->subscription))
    return false;
  t->subscription = 0;
  if (t->grant.api && !r->release(&t->grant)) return false;
  memset(t, 0, sizeof(*t));
  return true;
}
static void portable_touch_read(portable_touch *t, portable_touch_sample *out) {
  memset(out, 0, sizeof(*out));
  if (!t->subscription) { out->cancelled = true; return; }
  bool ok = t->api->poll(t->api->context, 1);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(!ok){portable_adapter_retain();out->cancelled=true;return;}
#endif
  unsigned drained = 0;
  bool saw_up = false, replaced = false, home_edge=false;
  bool home_down=t->home_down,home_neutral=t->home_neutral;
  uint16_t up_x=0,up_y=0;
  for (; drained < RISC_TOUCH_QUEUE_LENGTH; ++drained) {
    risc_touch_event_v1 e={0};
    int n = t->api->next(t->api->context, t->subscription, &e);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(n<0){portable_adapter_retain();out->cancelled=true;return;}
#endif
    if (n <= 0) { if (n < 0) ok = false; break; }
    if(e.id==0 && e.kind==RISC_TOUCH_EVENT_BUTTON_DOWN){
      home_edge|=home_neutral&&!home_down;home_down=true;
    }else if(e.id==0 && e.kind==RISC_TOUCH_EVENT_BUTTON_UP){home_down=false;home_neutral=true;}
    if (e.kind == RISC_TOUCH_EVENT_UP && t->down && e.id == t->contact_id) {
      saw_up=true;up_x=e.x;up_y=e.y;
    }
    if ((e.kind == RISC_TOUCH_EVENT_MOVE || e.kind == RISC_TOUCH_EVENT_UP) && t->down) {
      if (e.id != t->contact_id) replaced = true;
      int dx=(int)e.x-t->x,dy=(int)e.y-t->y;
      unsigned ax=(unsigned)(dx<0?-dx:dx),ay=(unsigned)(dy<0?-dy:dy);
      if (ax+ay>=6) t->moved=true;
    }
    if (e.kind == RISC_TOUCH_EVENT_DOWN && t->down) replaced = true;
  }
  /* An exhausted budget cannot prove the event stream is current. */
  if (drained == RISC_TOUCH_QUEUE_LENGTH) ok = false;
  risc_touch_snapshot_v1 s = {0};
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  bool snapshot_ok=t->api->snapshot(t->api->context, &s);
  if(!snapshot_ok){portable_adapter_retain();out->cancelled=true;return;}
  if (!snapshot_ok || !ok || replaced
#else
  if (!t->api->snapshot(t->api->context, &s) || !ok || replaced
#endif
      || (saw_up && (s.contact_count || up_x>=s.width || up_y>=s.height)) || s.contact_count > 1 ||
      (s.contact_count && (s.contacts[0].x >= s.width || s.contacts[0].y >= s.height))) {
    t->neutral = t->down = false;
    t->home_neutral=t->home_down=false;
    out->cancelled = true;
    return;
  }
  /* Snapshot is authoritative after the bounded event drain. A fresh or
   * faulted invocation cannot turn an inherited held Home into a press. */
  bool held=!!(s.buttons&RISC_TOUCH_BUTTON_PRIMARY);
  out->home_pressed=home_edge||(home_neutral && held && !home_down);
  t->home_down=held;t->home_neutral=home_neutral||!held;
  out->valid = true;
  if (!s.contact_count) {
    out->released = t->down;
    out->tap_eligible = t->neutral;
    out->moved = t->moved;
    out->x = saw_up?up_x:t->x; out->y = saw_up?up_y:t->y;
    t->neutral = true; t->down = false;
    if(out->released)portable_touch_orient(s.width,s.height,&out->x,&out->y);
    return;
  }
  if (t->down && t->contact_id != s.contacts[0].id) {
    t->neutral = t->down = false;
    out->valid = false; out->cancelled = true;
    return;
  }
  out->down = true; out->began = !t->down;
  out->tap_eligible = t->neutral;
  out->x = s.contacts[0].x; out->y = s.contacts[0].y;
  if (!t->down) {
    t->x = out->x; t->y = out->y;
    t->down = true; t->moved = false; t->contact_id = s.contacts[0].id;
  }
  int dx = (int)out->x - t->x, dy = (int)out->y - t->y;
  if (dx > 12 || dx < -12 || dy > 12 || dy < -12) t->moved = true;
  out->moved = t->moved;
  portable_touch_orient(s.width,s.height,&out->x,&out->y);
}
static inline bool portable_touch_tap(portable_touch *t, uint16_t *x, uint16_t *y) {
  portable_touch_sample s;
  portable_touch_read(t, &s);
  if (!s.released || !s.tap_eligible || s.moved || s.cancelled) return false;
  *x = s.x; *y = s.y;
  return true;
}
