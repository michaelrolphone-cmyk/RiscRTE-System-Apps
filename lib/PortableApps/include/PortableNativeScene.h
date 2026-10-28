#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Private statically linked app presentation contract; no provider/T5 ABI. */
#if defined(PORTABLE_NATIVE_SCENE)
#if defined(__GNUC__)
#define PNS_PRIVATE __attribute__((visibility("hidden")))
#else
#define PNS_PRIVATE
#endif
typedef struct { bool valid,began,down,released,cancelled; int x,y; } portable_native_scene_contact;
PNS_PRIVATE bool portable_native_scene_available(void);
PNS_PRIVATE void portable_native_scene_begin(void);
PNS_PRIVATE void portable_native_scene_layout(uint64_t identity);
PNS_PRIVATE void portable_native_scene_present(bool full);
PNS_PRIVATE bool portable_native_scene_ready(uint64_t *generation,uint64_t *sequence);
PNS_PRIVATE bool portable_native_scene_repaint(void);
PNS_PRIVATE bool portable_native_scene_contact_read(portable_native_scene_contact *out);
PNS_PRIVATE void portable_native_scene_back(void);
#undef PNS_PRIVATE
#endif
