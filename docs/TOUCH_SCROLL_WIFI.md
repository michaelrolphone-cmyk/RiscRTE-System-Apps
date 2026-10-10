# Selected Wi-Fi touch scrolling

Build Wi-Fi 1.1.11 with `--time-profile x4-native-time --paper-transitions
--touch-scrolling` and the usual pinned native/alarm SDK inputs. This opt-in
uses `PortableTouchScroll.h` for the saved-draft/action list, scan networks,
privacy list and confirmation list. The paper list has a fixed Back button,
clipped partial rows and a bounded scroll thumb. Watch builds keep their
existing controls and the motion-only native package stays 1.1.10.

The controller still owns explicit Connect, Save, Forget and scan cleanup.
Dragging or stopping momentum does not activate a row. Keyboard layout and
character paging remain unchanged. Physical navigation reveals the chosen row.
Confirm first reveals an off-screen selection and requires its completed image
before activating it.
A pending display token never blocks input collection or permits buffer writes.
Row hits use the completed image's offset captured at touch-down. Page or scan
identity changes cancel a contact; stale results cannot select a different
network. Back, Home, sleep, Quick Actions and alarms cancel list momentum even
when native cleanup refuses to complete.

`python scripts/test_touch_scroll_wifi.py --sdk <selected-target>/native-time-sdk/include`
runs the actual controller, adapter, MONO1 renderer and shared helpers with fake
providers. The 76 cases cover both 480x800 and 400x600, normal and ASan/UBSan:
root and scan dragging, momentum, reverse motion, bounds, row selection,
a 500 ms transfer with new input and completed-image hit identity, stop tap,
Back/Home/horizontal Back, replaced contacts, retained input failure, footer
rejection, Quick modal interruption, queued UP coordinates, scan-cancel retry,
changed scan data and password keyboard entry/cancel. Pixel assertions protect
fixed header/footer areas and pending framebuffer bytes. Production captures
were inspected at both geometries.

Compatibility: the existing 384 Watch/controller cases, credential/saved-client
fault suites and 354 paper cases pass. Motion-only Wi-Fi 1.1.10 reproduces its
prior ELF bytes exactly. The selected Xtensa ELF passes import/export and real
loader structural checks. These are simulated provider timings, not physical
panel cadence or touch-latency qualification. No real radio, network, device,
credentials, source publication or product grants were used or changed.
