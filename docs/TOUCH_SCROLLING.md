# Selected X4 touch scrolling

Settings 1.3.13 adds an explicitly selected, app-side touch scrolling path for
Time Zone regions and cities. Build with `--settings-profile x4-native-time
--paper-transitions --touch-scrolling`. Motion-only Settings remains 1.3.12;
ordinary Watch and paper profiles do not select the new code. The option does
not add capabilities, runtime exports, background work, or persistent settings.

`lib/PortableApps/include/PortableTouchScroll.h` is the shared foundation:

- Logical-pixel viewport, bounded Q8 position/velocity and constant storage.
- Direct finger tracking, vertical/horizontal locking, drag versus tap, a
  momentum-stopping first touch and deceleration bounded by content edges.
- Row identity/hit testing and rectangle clipping use the same viewport.
- Time wraparound and long interruption handling do not accumulate animation
  or render backlog. The helper never calls a provider or submits a frame.
- Snapshot-only touch releases retain the last observed contact position; the
  raw provider may otherwise report the original DOWN coordinate on release.
  A real queued UP event retains its final coordinate through selected
  `PortableTouch.h` sample metadata.

Settings integrates the helper in `settings_timezone_view.inc`. The selected
paper view removes PREV/NEXT and page counters, uses the space for list rows,
and displays a bounded scroll thumb. Selection and explicit Save still use the
existing timezone controller and preference format. Buttons/crown can reveal
items in either direction; the Watch selector retains its original paging.

Input continues while a display token is pending. Only the newest dirty list
state is rendered after completion. The displayed offset is tracked separately
from the desired offset; a fresh tap identifies the row in the completed image
that the user touched, including when a newer scrolled frame is pending. A tap
cannot act on an as-yet-unshown page. Back, Home, page changes and Quick/alarm
modal consumption cancel the list gesture and velocity. The fixed title,
message and footer are excluded from row raster writes.

## Validation

Run `python scripts/test_touch_scroll_settings.py --runtime /path/to/RiscRTE
--tagged-alarm-utilities /path/to/RiscRTE-Utilities` for the selected tagged
alarm composition. Omitting the alarm option also checks legacy composition.
The runner reads the canonical Runtime SDK from the existing immutable pin and
runs actual `Apps/settings_native_entry.c`, `Apps/settings.c`, the adapter,
Settings controller and MONO1 renderer with capability-provider doubles. It
covers both 480x800 and 400x600, normally and with ASan/UBSan:

- Drag, release momentum, reverse drag, top/bottom bounds and stop-tap.
- Region scrolling, city selection/Save, UI flip and horizontal Back; queued
  UP displacement and rapid re-entry while another page remains pending.
- Physical Back/Home, replaced contact, retained provider failure, footer drag
  rejection and Quick Actions modal interruption.
- A 500 ms simulated transfer while multiple new samples arrive; only three
  list frames are submitted, and a tap saves the row from the older completed
  image rather than the pending offset. Pending framebuffer bytes stay fixed.
- Pixel assertions protect every pixel above/below the viewport. Captured
  production frames are inspected at both geometries.

The shared controller fixture additionally covers 32-bit clock wrap, malformed
content bounds, viewport clipping, release coordinates, cancellation, momentum
stop and a long suspension. Timing is simulated; these tests do not establish
physical X4 display cadence, touch latency or readability on hardware.

The selected matrix contains 68 fresh-process cases, plus the controller
sanitizer fixture. Compatibility checks include the existing paper Settings fixture, native
Settings controller suite, full timezone selector suite (including both Watch
orientations), repository Python tests, and a target Xtensa build with import,
export and structural ELF validation. Scrolling-disabled motion Settings
1.3.12 and Watch Settings 1.3.4 reproduce their prior ELF bytes exactly.
The scrolling version is distinct from
the urgent power/motion package and can be integrated independently.

## List conversion inventory

The initial increment changes timezone region/city lists. Selected Wi-Fi and
Files extensions are documented below; the other entries remain implementation
inventory, not claims that those lists already scroll:

1. System Settings root and Time and Date fields:
   `lib/PortableApps/src/settings_paper.inc` (`sp_first`, `sp_field_first`,
   `sp_poll`) and `settings.inc` (`settings_render`, `settings_touch`). Root
   still has MORE/FIRST PAGE; fields still page after a vertical swipe.
2. System File Browser, destination selector, action list and handler picker:
   `Apps/file_browser_portable.inc`, `Apps/file_browser_paper.inc`. Data is
   loaded in bounded pages, so integrate a small visible-window cache as well
   as scrolling; preserve file identity and selection during reload/filter.
   Preview text is a separate document viewport, not a list conversion.
   Completed in the optional [Files 1.5.9 scrolling profile](TOUCH_SCROLL_FILES.md).
3. System Wi-Fi root and scan results: `Apps/wifi_settings_portable.inc`,
   `lib/PortableApps/src/wifi_paper.inc`, `wifi_view.inc`. Separate keyboard
   pages from list scrolling and keep scan ownership/cancel behavior intact.
   Completed in the optional [Wi-Fi 1.1.11 scrolling profile](TOUCH_SCROLL_WIFI.md).
4. System App Store/Firmware Update release lists: `Apps/update_portable.inc`,
   `Apps/update_paper.inc`. The controller materializes 16 release records and
   the paper view shows four. Preserve explicit install confirmation and
   binding between selected release ID and displayed row.
5. Productivity Points: `Apps/points_paper.inc`, `Apps/points_in_time.c`:
   configured points, editor fields, type/mode/day choices. Several views
   use `pe_first`, `pe_edit_first`, `pe_choice_first` and swipe-on-release.
6. Productivity Timecard: `Apps/timecard_paper.inc`,
   `Apps/timecard_portable.c`: five-row record/project/editor lists and
   `tcp_paper_first`. Keep store mutation behind its existing explicit action.
7. Utilities BLE Scanner: `Apps/ble_scanner_paper.inc`, `Apps/ble_scanner.c`:
   device/sensor lists and detail lines use row/page offsets. Preserve device
   identity while scan results change, and keep the naming keyboard separate.
8. Utilities Battery details: `Apps/battery_paper.inc`,
   `Apps/battery_power.inc` have four diagnostic detail pages. Converting
   these is a bounded diagnostic-content viewport, after the power repair.

Additional non-paper inventory, outside the selected 14-app X4 cohort:
Utilities `Apps/lora_messages.c` (history/RF fields), `Apps/audio_spectrum.c`
(label lists and controls), `Apps/spectrum_controls.inc`,
`Apps/rf_controls.inc` and related signature/temporal list includes;
Productivity `Apps/text_editor.c` (document/editor viewport). Legacy
provider-owned System lists (`Apps/time_zone.c`, `language_settings.c`,
`font_selection.c`, `font_manager.c`, `package_manager.c`, `driver_manager.c`)
need the provider/UI ownership contract checked before an app-side conversion.

Springboard is a horizontally paged app grid and reader page turns are content
navigation, so neither is silently reclassified as a vertical list. The other
selected paper utility panels (Clock, Alarms, Countdown, Stopwatch, Calculator,
BLE Buttons/Touchpad) do not currently contain overflow lists.

## Existing Quick Actions

The selected path in `lib/PortableApps/src/quick_actions.c` already maps paper
finger displacement to the sheet position and animates the final target in
`pqa_animate`. It is preserved. Its concrete remaining difference is in
`release_panel`: the paper branch chooses open/close from displacement
thresholds, whereas the compact branch uses measured release velocity. A
velocity-sensitive paper sheet release is a separate small unit, not part of
this list implementation.
