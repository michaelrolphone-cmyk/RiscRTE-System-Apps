# NOVA-7 reusable intent components

`ui.scene@1` now exposes the optional, tagged `RiscSceneComponentsV1.h`
suffix. Existing scene/lifecycle layouts and old Alarms binaries are unchanged.
Use `risc_scene_components_get_v1()` before accessing the suffix. The presenter
copies the whole bounded document; it retains no app pointer or callback.

The first client is the Productivity Lists app. Its executable has no screen
coordinates, font assets, display API, touch API, or device conditional. The
same ELF renders on Watch and X4. Further clients use the same component kinds.

| Semantic component | Application supplies | Shared presenter supplies |
| --- | --- | --- |
| Header / header action | Title, subtitle, Back/action IDs, symbol | Typography, circular controls, sticky paper header |
| Section / explanatory or empty state | Text and state | Spacing, wrapping, empty-state treatment |
| Navigation row | Label, subtitle, marker/symbol, badge, intent | Row layout, divider, focus bar, touch target |
| Checklist row | Checked value, toggle intent, separate detail intent | Separate checkbox/detail targets, strike-through |
| Switch | Boolean value and intent | OFF/ON controls; caption is inert |
| Stepper | Bounded integer, selected text, intent | Independent minus/plus targets; caption is inert |
| Segmented choices | Pipe-separated choices, selected index, intent | Selected inversion and independent targets |
| Marker selector | One of six semantic marker values | Shape and color on Watch; shape on paper |
| Progress card/bar | Completed count and total | Bounded bar, card and optional tap target |
| Action button | Label, intent, primary/destructive/disabled state | Density-specific outline/inversion and focus |
| Time picker | Minute of day and intent | Finger-follow/snap wheels on Watch, large +/- on paper |
| Keyboard | Draft text, length limit, intent | Shared native keyboard and layer/key events |
| Confirmation / alert | Copied content and explicit cancel intent | Safe default focus, Back/outside cancellation, no stale toast overlay |
| Toast | Message, new token, optional undo intent | Four-second lifetime; paper omits Undo |

All components use the attached component bible's color/monochrome classes and
licensed Orbitron/Rajdhani fonts. No application data or labels are embedded in
the renderer. Geometric symbols and six markers are generated in the presenter.

## Client contract

Initialize `api_version=1`, exact `struct_size`, nonzero revision/root/screen key,
route definitions, nodes and matching detail records. At most 96 nodes and eight
routes are copied. Node IDs are unique. Revisions increase strictly. A changed
screen key resets scrolling; an update to the same screen retains/clamps it.
Components use existing copied `risc_scene_event_v1` action/value events.
Applications reject stale revisions, replayed sequences, disabled controls and
out-of-domain values before applying an intent.

The original TEXT, ACTION and KEYBOARD kinds also work in component documents.
Use the new SWITCH, STEPPER, TIME_PICKER and ROW kinds for the other controls.
Old base documents continue to use the original navigation and rendering.

`RiscSceneResidentV1.h` is an optional generic bridge to the resident host.
It latches input activity and requests policy work only at safe checkpoints.
Close the scene completely before a CONTROLS/POLICY checkpoint; then reopen the
copied document/navigation. The foreground never links a second Quick Actions
renderer. Standalone deployments can omit the resident shell.

## Input and rendering

Finger-follow scrolling changes the logical hit map immediately. Rasterization
uses immutable frame snapshots in bounded eight-row slices; slow physical
refresh never publishes or rolls back input state. Gesture cancellation,
revision fencing, provider retention, and close/drain ownership continue to use
the existing scene host. A late UP with displaced coordinates cannot toggle a
crossed row even if a MOVE report was absent.

## Calendar policy

`time.civil@1` is a separate read-only provider, useful to any calendar client.
The Watch package selects its existing UTC+08 RTC to Denver display policy. The
X4 package reads native UTC and the already admitted `time_zone` preference.
Neither changes the clock, timezone, or alarm scheduler. Invalid/unset time is
reported unavailable. Dates are civil days since 2000-01-01; they are not elapsed
durations. Foreground clients do not guess a device or timezone.

## Verification and builds

```sh
python scripts/test_scene_host.py --runtime ../Runtime
python scripts/test_scene_components.py --runtime ../Runtime
python scripts/test_civil_clock.py --runtime ../Runtime
python scripts/build_scene_services.py --runtime ../Runtime --output ../build/scene
python scripts/build_civil_clock.py --runtime ../Runtime --output ../build/civil
```

The old scene suite runs 179 presenter/profile cases. New component tests use
60-row checklists on both profiles, test independent targets during a pending
frame, caption inertness, malformed documents and suffix discovery. Calendar
provider tests cover both real clock policies and terminal context loss. Lists'
cross-repository tests exercise its real reducer through this renderer and
produce the screenshots in Productivity's `docs/lists/` folder.

These are target ELF builds and host tests. They do not certify physical display
latency, power draw, or device memory headroom. Native capacity and final product
cohort binding remain part of firmware packaging.
