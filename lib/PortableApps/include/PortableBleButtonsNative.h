#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_BLE_BUTTONS_NATIVE_UI
/* Statically linked Buttons-only presentation ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_BLE_BUTTONS_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_BLE_BUTTONS_PRIVATE
#endif
PORTABLE_BLE_BUTTONS_PRIVATE void portable_ble_buttons_native_begin(void);
PORTABLE_BLE_BUTTONS_PRIVATE void portable_ble_buttons_native_layout(uint64_t layout_id);
PORTABLE_BLE_BUTTONS_PRIVATE void portable_ble_buttons_native_present(bool full);
/* Read only: sequence belongs to the same raw provider used by the app's
 * independent subscription. Always returns current generation, consumes no
 * events, and never delays HID polling, releases or cleanup. */
PORTABLE_BLE_BUTTONS_PRIVATE bool portable_ble_buttons_native_ready(uint64_t *generation,uint64_t *visible_sequence);
#undef PORTABLE_BLE_BUTTONS_PRIVATE
#endif
