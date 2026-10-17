#pragma once
/* Generic shared native-pixel primitives. Preserve existing RF callers without
 * selecting any app, font subset, display profile or device automatically. */
#if defined(PORTABLE_RF_NATIVE_CANVAS) && !defined(PORTABLE_NATIVE_CANVAS)
#define PORTABLE_NATIVE_CANVAS
#endif
