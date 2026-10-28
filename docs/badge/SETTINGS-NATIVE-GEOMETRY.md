# Badge Settings native geometry repair

## Cause and preserved behavior

The existing color renderer centered a fixed240×240 canvas within the Badge's
reported330×330 safe content region on the466×466 physical display. Its11–15px
fonts,26px actions and124px first-list-row offset made the screen too small and
left only one complete root row visible. Earlier clipping checks did not prove
usability. Touch and raster used the same fixed offsets; the primary defect was
profile/layout allocation, not a different drawn and touched coordinate system.

`--native-settings-ui` selects capability-derived color layout with the original
Orbitron/Rajdhani families rasterized at native sizes from retained licensed TTFs.
It does not zoom a framebuffer, alter the provider's physical dimensions, or
change settings controllers, stores, modes, defaults, alarm/clock contracts,
Save/readback semantics, cancellation, or feature availability. Compact240 and
paper keep their established presentation. The reference design coordinates map
through one geometry module; inverse touch mapping uses the same cell boundaries.

Native root geometry gives three complete TIME rows at entry; longer labels and
values occupy separate baselines. Scrolling/focus exposes the final row clear of
its short edge fade and fixed footer. In adaptive color layouts with safe-content side at least300px, buttons have
complete44px-or-larger native-height hit rectangles. Smaller color content uses
the unchanged240px fallback; no44px minimum is claimed for that fallback or paper. Timezone pagination and footer derive shared bounds from available
content, including a300px content region. About distinguishes physical display
geometry from safe content instead of calling330×330 the display.

## Integration

Base: System `ee0f51138c52060bd6e240cf0aa3e08c7e22cb43`, preserving the pending
Timecard/Quick completed-frame and cancellation repairs. Add only
`--native-settings-ui` to the existing Settings builder command in the successor
Badge recipe. It emits Settings1.4.2. Do not modify published6b or pending sealed
artifacts. No device/publication commands were used in this work.

Shared overlap: `raster_snapshot_replay.inc` splits Settings PIXEL/RECT spans from
other compact-renderer spans and maps them through Settings geometry. This is
separate from Springboard's in-progress raster scheduling changes. No adapter.c
edit. Snapshot compile checked; selected target uses the existing direct/full-frame
profile, not a claim of hardware-sliced rendering acceptance.

## Verification

Evidence directory: `/tmp/settings-native-evidence`.

- Actual controller geometry matrix: compact240, native300, native466 with68px
  safe insets/touch rotation0/180, rectangular400×480 with30px insets, normal and
  ASan/UBSan. Mode, both timers, cancel, save verification/failure and reopen.
- Actual native466 timezone controller:419 zones each through navigation and
  touch,28 interruption/fault cases, normal0° and ASan/UBSan180° (1,732 cases).
- Real native page/editor scenes and app root→Time/Date→Cancel→root Back, normal
  and ASan/UBSan. Inverse-edge and complete action/final-row bounds assertions.
- Compact240:20 existing controller cases in baseline, successor-off and selected
  native240; all7 captured rasters byte-identical in all3 profiles.
- Existing Nova272 cases and paper18 ASan/UBSan cases passed. Environment's
  LeakSanitizer cannot run under ptrace; reruns use detect_leaks=0, retaining
  address/undefined sanitizers. Native-time controller6 cases passed.
- Legacy and native retained/async save matrices36 cases each: completed pending
  frame before write, cancellation/repeat handling, IO/failure and true retained
  custody stop rules.
- Standalone native-time-touch Settings target `/tmp/settings-native-target`:
  ESP32-S3 compiler, native layout, ELF structure and imports/exports passed.
  Full coherent Badge/foreground integration is owned by the main build task.

Rendered evidence: all-pages-466.png, individual all-pages/*.png (including
root-last-focused.png), and timezone-frames/*-final.png. These are modeled real
controller raster outputs, **not device acceptance**. User hardware behavior,
physical touch comfort, response latency and exact installed release remain to
be verified by the main owner; no crash/freeze resolution claim is made here.

Initial font/size preview caught status/footer overlap and long root-label/value
collision; these were corrected before final modeling. Native list fade/focus was
reviewed separately. The reported timezone-heading anomaly was disproved by
identical original/recheck pixel bounds and controller capture (America270 lit
pixels); no workaround was made for it.

Resource errors retained in evidence: initial missing compiler override (no target
execution), two temporary-file ENOSPC capture/compile failures and ptrace LSAN.
Recovery only converted this task's complete derived PPMs to lossless PNGs with
byte-equal decoded pixels; incomplete generated PPMs were discarded. Production
sources/history and other workers' artifacts were untouched. No permission denial
was retried or bypassed.

### Time/date status clearance
The native Time/date footer reserves the area below its three full rows for the
status text, placing the actual glyph bounds between row end and expanded action
bounds. The selected328px safe side has4px clearance on both sides for the draft
message; compact300px has only1/2px available, without overlap. Row count, row hit
geometry, action targets, persistence, and compact240/Nova rendering are retained.
Modeled layout checks cover300/328/330/340/360/400/480px and draft/error messages;
these checks are not physical-device acceptance.
