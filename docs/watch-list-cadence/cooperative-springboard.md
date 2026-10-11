# Cooperative Springboard raster pump

## Established mechanism

The deployed Watch display provider's `present_status` reports its stored state;
it does not send rows. Its cooperative `display_poll` sends at most32 of240
rows per visit, and the Runtime invokes provider polling from `yield_ms`.
Springboard still paints a complete icon scene whenever dirty and40ms are due.
With the asynchronous canvas, a slow paint previously performed no provider
polling while that accepted token remained active. Repainting replaced the
pending image but could not advance its predecessor on the glass.

The provider advertises ASYNC_PRESENT and BRIGHTNESS, not PARTIAL_DAMAGE.
Both old and new deployed paths therefore transmit full frames. Partial damage
is not the explanation for this regression.

## Correction

During an active token and an unfinished private paint, existing raster
checkpoints admit one yield after at least4ms since the previous pump finishes.
Each yield is followed immediately by token validation, with failure retaining
the invocation before further callbacks. Checkpoints are enabled for async
builds without Contexts too. An unsealed canvas is never copied or submitted.
Rendering, final complete state, animation timing, and one-token/one-latest-image
ownership are preserved. Runtime already guards recursive yields; the canvas
also guards pump reentry. This does not wait for frame completion.

## Reproducible host regression

Run `python scripts/test_watch_springboard_cooperative.py`, or set `SANITIZE=1`
for ASan/UBSan. It compiles the real Springboard gesture/render loop and RGB565
renderer, compares the exactf1d8d5d adapter with this repair, and includes the
byte-exact deployed common display state machine. Its SPI row function charges
100us/row; provider progress happens only through the Runtime-yield fixture.
A selectable round-robin period represents one panel visit per1 or4yields.
The full-feature product's raster checkpoints remain active, with unrelated
Contexts services excluded. Real host thread CPU time is reported separately
from a deterministic work clock charged at existing raster checkpoints.

This is not a physical Watch benchmark. No measured Watch CPU cost, panel FPS,
or visual smoothness is claimed. At512us per checkpoint, an unpumped real paint
costs about95ms in the explicitly injected work clock. Over the4s drag:

- Panel each yield: original5 completed frames, max773ms between submits;
  repair31 completed frames, max240ms between submits.
- Panel each4yields: original1 completed frame, max2280ms between submits;
  repair16 completed frames, max302ms between submits.
- Normal host raster CPU time was approximately1.2–1.9ms/frame on this runner;
  sanitizer time is reported separately and is not a target estimate.
- The provider's accepted bytes are checked immutable on every transmitted row.
  Submitting an unfinished paint fails an assertion.

The existing async suite now has178 normal/ASan/UBSan cases, including pump
interval, completion during paint, and failure during a raster yield. Existing
mailbox, padded stride, latest-frame, modal, crown, and retention tests remain.

## On-device acceptance still required

A production trace should record paint begin/end, raw input and logical input
reduction, Runtime yields, panel row progress, submit, completion, and dropped
unsubmitted image count using a bounded memory ring. Compare1.1.6,1.1.7 and this
repair with the same installed catalogue and hardware. Keep tracing off during
a second visual check. The user has now tested1.1.6 and reports icon scrolling
works;1.1.7 has the large regression. Host results explain the starvation
mechanism and qualify the repair's ownership behavior, not hardware acceptance.

## Source custody

- System baseline: f1d8d5d53f80611520feb705f2250295ec07c195
- Product provider: michaelrolphone-cmyk/RiscRTE-T-Watch-S3,
  af4feb98d184603d71b75350d824c1843ccb77dd, include/common/display.h
- Provider fixture is SHA256-checked by the test before compilation.
