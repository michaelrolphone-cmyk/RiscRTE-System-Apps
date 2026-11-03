# Shared semantic pointer surface
The optional `RiscScenePointerV1.h` suffix preserves every existing scene,
components, keypad, canvas and held-input ABI. A pointer component delegates
geometry, rendering, contact ownership and gesture recognition to the presenter.
Applications receive copied relative movement, button/click and two-axis wheel
intent, never physical coordinates, a device class or a display backend.

The recognizer is extracted from Utilities BLE Touchpad at
6b2047500905f8f45087cb49989103a926fc5c63, preserving tap, two-finger right click,
tap-and-drag, fractional scrolling, axis rails and cancellation semantics.
Existing native application source is not modified by this provider addition.
Pairing confirmation reuses the existing held-input confirmation contract.

An initial pointer gesture must follow a completed current document and neutral
input. Same-document movement does not require a redraw. Interrupted polling,
queue gaps, mixed contact identity, topology and lifecycle boundaries cancel.
A live pointer boundary returns BUSY with a RESET to drain before retrying.
The caller must neutralize its domain after RESET; the provider owns no HID
session and does not promise remote release after terminal native retention.

Qualification is hosted source, controller and target-ELF validation. It is not
physical hardware timing, Bluetooth interoperability, product deployment or
completion of the full BLE Touchpad delivery. Product integration must preserve
the exact full/color-dynamic/watch-compact provider selection and original data.
