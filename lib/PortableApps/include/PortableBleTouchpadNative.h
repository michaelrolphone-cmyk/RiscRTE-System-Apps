#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_BLE_TOUCHPAD_NATIVE_UI
/* Statically linked, color-only Touchpad ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_BLE_TOUCHPAD_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_BLE_TOUCHPAD_PRIVATE
#endif
#ifdef __cplusplus
extern "C" {
#endif
PORTABLE_BLE_TOUCHPAD_PRIVATE void portable_ble_touchpad_native_begin(void);
/* Change the identity whenever geometry, controls or their actions change.
 * Telemetry/pressed-state repaints retain the identical hit mapping identity. */
PORTABLE_BLE_TOUCHPAD_PRIVATE void portable_ble_touchpad_native_layout(uint64_t layout_id);
PORTABLE_BLE_TOUCHPAD_PRIVATE void portable_ble_touchpad_native_present(bool full);
/* Read only: no poll, snapshot, event consumption or wait. Always writes the
 * current generation and the first completed frame's provider sequence fence.
 * An already-visible mapping stays ready during its own unchanged repaint.
 * The app's independent multi-contact subscription must discard through this
 * watermark and require neutral before adopting contacts in a new generation.
 * ready does not replace that subscription's fault/neutral/release handling.
 * Paper returns false and retains the ordinary adapter presentation/input. */
PORTABLE_BLE_TOUCHPAD_PRIVATE bool portable_ble_touchpad_native_ready(uint64_t *generation,uint64_t *visible_sequence);
#ifdef __cplusplus
}
#endif
#undef PORTABLE_BLE_TOUCHPAD_PRIVATE
#endif
