# Ordinary paper input and completion

Native X4 paper controllers can now process input while their last submitted
image is queued, transferring or physically refreshing. The private
`Apps/PaperFrame.h` contract opts retaining MONO1 providers with
`ASYNC_PRESENT` into one outstanding token. It does not change a provider,
Runtime or T5 API. Legacy and Watch builds retain their existing presentation
path; synchronous paper still uses the existing bounded wait.

`paper_frame_ready()` has no provider calls. A controller checks it before a
complete paint and keeps its latest dirty state when it returns false. The
adapter advances completion from foreground `poll()`. While a token is pending,
the caller's existing wait is divided into 1 ms Runtime yields, preserving the
previous provider-pump cadence. The app still receives input at its requested
poll interval. A single 20 ms yield per provider chunk would otherwise throttle
the selected X4 driver's 512-byte slices across its two 60,000-byte planes.
A work-driven fixture requires 240 provider steps by 240 ms to catch that
regression; it does not measure physical-panel timing. Each wait also has a
finite slice bound when a fixture clock is stalled.
A pending surface stays
provider-owned and immutable: there is no replacement framebuffer, draw queue,
or replay of obsolete intermediate images. Settings, Files, Wi-Fi, update,
launcher and foreground Clock/Home Points use this contract. Sparse timer
reconstruction and its checked sleep presentation remain synchronous.

The submitted image becomes valid damage/restoration history only on COMPLETE.
A failed status, failed/superseded token or exhausted existing 10-second budget
retains the native invocation, token and owned resources. Modal entry waits for
the settled foreground image. Quick and alarm keep their existing modal
presentation ownership and input handling. Orientation changes, app launch,
actual controller return and finalization explicitly settle pending work;
nested Back and ordinary state edits do not drain the display.

## Deterministic timing, not hardware measurements

`scripts/test_paper_present_input.py` links the actual launcher controller and
native adapter to strict provider doubles. It compares production source at
`c1ff014792c9ecbc195c402e9b4c3e2c3c9d34d3` with the candidate. The provider
models 0–40 ms QUEUED, 40–100 ms transfer and 100–240 ms BUSY. Input begins at
40 ms and releases at 60 ms. Catalog entries in this comparison remain
unavailable so both controllers stay open and draw the same selection/error
state.

| Observation | Baseline | Candidate |
| --- | ---: | ---: |
| Raw contact detected | 48 ms | 40 ms |
| Controller selection dispatched | 241 ms | 60 ms |
| Updated image first completes | 481 ms | 480 ms |
| Status calls over two images | 482 | 24 |

The useful improvement is 181 ms earlier state dispatch in this fixture.
Detection differs with sampling phase. Physical visibility still waits for the
current refresh and the next one; these numbers do not establish a hardware
latency, power or SPI throughput improvement. Three complete tap actions plus a
moved contact during BUSY update state and produce only one latest-state redraw.

## Active icon before launch

Native paper Springboard now records an eligible released tap, fills the active
icon's circle, and waits for that feedback image to complete before requesting
launch. This also changes the already-selected icon visibly. New selection or
a later canceled/dragged contact replaces or cancels the pending launch intent;
held entry contacts, multitouch and drag release do not launch. Refusal removes
the loading indication and permits a fresh attempt.

In the same 240 ms display fixture, the selection changes at 60 ms, feedback
submits at 240 ms, feedback completes at 480 ms, and launch is requested at
480 ms. Changing to a neighboring icon submits physical damage
`(88, 192, 96, 225)`, 5.625% of the 800 × 480 panel. Pressing the same icon submits
`(88, 326, 96, 87)`, 2.175%. These are measured fixture rectangles, not universal
bounds: the existing single bounding rectangle can grow when changed regions
are farther apart. The test verifies changed-byte coverage and the actual
white-to-black feedback pixel. The script also saves both submitted PBM frames
for these cases under its output directory.

A newer target while the feedback image is BUSY is drawn after completion and
only that target is launched. A presentation failure during feedback retains
ownership and never requests launch. Feedback is an intentional additional
image before admission, rather than a claim that app loading itself is faster.

## Timezone region Next page

The actual Settings region selector now avoids a complete raster pass when
Next/Previous or a page swipe cannot change the page. In the 480 × 800 fixture,
four settled boundary taps previously caused four discarded full paints:
six surface acquisitions for two submitted images. The guarded controller uses
two acquisitions for those same two images. Held and moving samples cause no
paints.

The async region fixture holds each image for a simulated 1000 ms. The first
Next release is detected and applied at 220 ms; the changed page completes at
2000 ms, after the original and replacement refreshes. On the 400 × 600 layout,
releases at 220, 340 and 460 ms apply three page changes during BUSY. Only the
latest page is rasterized/submitted afterward, so intermediate pages never
accumulate. In the synchronous comparison that initial input burst is canceled
by the existing unconsumed-gesture rule; fresh later taps are needed.

Changed-page damage is `(168, 32, 480, 416)` in the 800 × 480 physical fixture and
`(168, 32, 280, 336)` in the 600 × 400 fixture. The tests validate every changed
byte against the completed image and retain Quality/partial presentation.
These fixtures reproduce controller delay and wasted raster work; they do not
establish the cause or resolution of a reported 10–15 second device delay.
Actual panel transfer, Runtime cost and hardware BUSY timing require separate
measurement.

## Verification

- 64 production baseline/current/feedback cases, normal and ASan/UBSan: repeated
  taps/movement, latest state, queued/transfer/BUSY, immutable submission,
  completed-frame damage, Quick and alarm interruption, refusal/retry,
  same-icon feedback, newer target, cancellation, early exit, cleanup and
  submission/status/failed/superseded/timeout retention.
- Native Settings fixtures exercise repeated button and raw-touch editing
  during a 1000 ms token, preserve clean-refresh debt, draw the latest draft on
  neutral completion, settle early exit before fini, and stop all I/O on native
  retention with a pending token.
- Native System app interaction/custody suite, pinned Xtensa app builds,
  native-toolbar harnesses, Watch Quick, paper Quick/Home and selected sparse
  Clock/Home Points compositions cover the shared-path regressions.

Reproduce the focused adapter suite with:

```sh
python3 scripts/test_paper_present_input.py --runtime-sdk /path/to/Runtime/sdk/app
```

The JSON receipt records fixture results and tested source hashes. Build
artifacts are local development evidence. No device, hardware timing, product
BIN, catalog, installation, flash or publication is included. Crossfade and
Quick swipe animation are separate work.
