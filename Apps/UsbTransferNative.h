#pragma once
#include <stdbool.h>
#if defined(PORTABLE_USB_TRANSFER_NATIVE_UI)
#if !defined(PORTABLE_USB_TRANSFER_APP) || !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS)
#error "USB native UI requires the dedicated USB app and native Nova canvas"
#endif
/* Private link-time selector. No provider acquisition, mutation or Runtime ABI. */
__attribute__((visibility("hidden"))) bool portable_usb_transfer_native_available(void);
#endif
