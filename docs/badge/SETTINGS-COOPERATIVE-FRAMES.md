# Cooperative native Settings frames

Explicit `--sliced-native-frames` reuses the shared immutable raster command,
8 ms work budget, frame receipt and work-conserving poll path. It requires the
native-time-touch/native Settings UI selection; historical default selection,
paper profiles, provider ABI and touch acquisition interval are unchanged.

Previously a complete native Settings redraw captured touch reports inside
512-pixel checkpoints, but did not reduce them until the synchronous redraw
returned. CST9217 acquisition includes its required register-settle delay.
Increasing an arbitrary sampling interval reduced I/O but left hundreds of
milliseconds of logical input delay. That cadence-only workaround is not used.

The selected renderer captures commands and owned strings, then runs bounded
row slices between ordinary input/controller passes. Native Settings pixel,
rectangle, text, button and fade operations are callback-free during each row;
the existing slice exit samples input. Conservative glyph/button bounds avoid
replaying empty rows. Pixel order, exact blends, labels and geometry are retained.
Safe-viewport RGB565 replay clears the whole software image at image start,
matching the existing `acquire_surface` behavior before its complete-image copy.
This prevents undefined outside-content allocator bytes from reaching the panel.

## Displayed action identity

One pending image carries its page epoch, scroll offsets and timezone list
identity. Only its exact completed app receipt installs that visible map; idle
following discard/replacement does not. DOWN snapshots the visible map; a
replacement completed image before UP cancels the stale action. Page transitions,
including returning to the same enum, change the epoch. Root/field row actions
use the captured scroll offset, not a newer logical offset. Timezone row actions
use captured list position. Drag updates remain live between slices.

Mode/timer saves require the matching completed app image, not merely an idle
display. Pending Quick/alarm modal presentation cannot satisfy that save barrier.
The established retained-result behavior still forbids later provider callbacks.
Allocation pressure uses the shared immediate compatibility fallback, preserving
all commands/pixels rather than silently dropping a frame or control.

## Owning evidence and limits

`test_settings_sliced_frames.py --runtime-sdk ... --output ... [--sanitize]`
checks nine complete native pages against the synchronous renderer, completed
map/scroll/page/replacement behavior, command/offscreen allocation fallback,
and 15 mode/idle/deep save cancellation, retained and display-fault cases.
It also models actual asynchronous adapter/controller flow at 100/200 Hz with
alternating ±2 ms report intervals, 2.25 ms report acquisition (2 ms settle plus
assumed combined I2C cost), 16 ms panel completion and 100/1000 ns pixel costs.
Only sampled controller reports become provider events; captured-event draining
is not a guarantee that every physical transient is sampled.

At 100 ns/pixel, modeled max completed frame improves 97–99 to 86 ms and
max logical dispatch age 124–152 to 4–8 ms. At 1000 ns/pixel, max completed
frame improves 819 to 592 ms; synchronous queue gaps discard the gesture,
while the cooperative path drains all captured events with 5–10 ms max age.
These are source/model results, not measured device FPS. The high-cost model
still takes roughly 0.6 seconds per frame: this change alone does not establish
physical usability. Final target, installed Runtime, combined trace/Quick and
physical behavior remain separate integration evidence.
