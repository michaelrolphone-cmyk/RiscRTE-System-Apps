# Native Springboard input/render scheduling

Explicit `--sliced-native-frames` current Watch native-time profile; historical
selection remains unchanged. This is software evidence, not a device-speed or
physical acceptance claim.

## Reproduced mechanism

The native Nova controller consumed one queued contact per outer poll, then
synchronously drew its full frame. Raster checkpoints captured raw reports but
never ran the app reducer. Sustained changed reports therefore accumulated even
when raw sampling looked fast. CST9217 performs two I2C transactions and a
measured minimum 2 ms register-select settling wait for each report; there is no
provider sampling throttle. The adapter timestamps sampling *after* that wait,
so it does not automatically read on every checkpoint. Conversely, an outer
poll immediately following a checkpoint could redundantly read again.

The actual Nova/controller/adapter C harness uses a separately identified
synthetic renderer cost, 50 Hz physical-time drag reports, 32-edge GAP semantics,
2 ms settling plus explicit paired bus latency, scheduler yield cost and panel
completion latency. It is not an ESP32 benchmark. Counted-pixel charges omit
uncounted mask/arithmetic costs; neither a fast host nor those charges establish
real target frame speed. The zero-cost control is retained. Source-level target
disassembly separately confirms per-pixel coordinate calls and bilinear work.

With 100 ns per counted pixel and 2 ms/read, the synchronous predecessor can
produce a 56 ms frame, 619 ms queue-to-reducer age and a GAP cancellation despite
raw polls no more than 6 ms apart. Increasing the queue only postpones this.

## Opt-in composition

- Reuse immutable raster commands and app-owned offscreen replay, with bounded
  row/time/pixel work between controller passes. Logical motion/input continues
  while drawing is pending; complete pixels are still submitted atomically.
- Capture once after a bounded replay slice for the known callback-free Nova
  primitives. The existing file/custody checks still precede slice deferral;
  other primitives retain their original checkpoints. No provider operation is
  reentered from a logical app callback.
- An explicit 2 ms capture interval coalesces an already-fresh raw sample;
  ordered queue reduction still runs on each controller pass. It does not drop,
  merge or fabricate DOWN/MOVE/UP/cancel edges, enlarge the queue or weaken GAP.
- Keep one Nova image outstanding. Its catalog page and offsets become the hit
  map only after completion. A newer logical animation position is not treated
  as the displayed icon map. Existing first-ring launch/far-icon centering,
  navigation and launch animation remain; selector-off uses its original path.
- The optional RGB565 span fill hoists the affine coordinate transform, while
  preserving logical write order, clipping, stride padding and exact
  checkpoint-before-pixel positions. Equal-byte spans use memset. Contexts
  retains its existing per-pixel context checkpoint path. No glyph, palette,
  antialiasing, image resolution or frame content is reduced.

## Allocation and timing boundaries

The established command/offscreen allocation-refusal fallback remains
synchronous. Both refusals are explicitly tested for usable clean teardown and
byte-identical first image. This fallback is **not** latency-bounded: memory
pressure can still cause long drawing and queue GAP cancellation, which remains
safe rather than a fabricated tap. No unconditional latency guarantee is made.
A single provider transfer can also consume its own bounded deadline. Real
panel/radio/I2C timings and device interaction remain unmeasured here.

## Owning evidence

`test_springboard_sliced_frames.py` compiles actual app/client code with clearly
labelled timing instrumentation, compares exact first-frame pixels, verifies all
121 drag edges/no GAP on sliced runs, and injects a retained display callback
with zero later provider entries. Known command and offscreen allocation
refusals are separate fallback cases, not counted as bounded successes.

`test_rgb565_span_fill.py` compares complete buffers with memcmp against the
original fill on every case, including padded strides, clipping, both aspect
ratios, four rotation/flip states, counter wrap and failed checkpoint prefixes.
It also compares callback counts/state. The initial 2,520-case oracle preserved
65,808 checkpoint calls while reducing 33,329,784 coordinate transforms to 5,400.
This is an operation-count reduction, not a measured target throughput number.

`springboard_completed_map_test.c` covers capture, busy frame, incomplete frame,
page changes, first-ring identity while logical motion is ahead, and cancellation.
The existing ordinary Nova input/slow-display scenarios are also rerun.
Full product target admission, application/sleep/alarm integration and actual
hardware behavior are separate integrator gates; this source note does not
claim those passed.

Exact private frame receipt IDs now bind capture to submit token/completion.
An idle queue after discard, or completion of a different/modal image, cannot
promote the captured hit map. Zero/failed/exhausted receipts are unavailable.
The rate sweep additionally covers 100/200 Hz physical sampling, interval jitter,
repeated contacts/IDs and yield overhead: it compares every actually sampled
edge with delivered edges, not an impossible promise to recover hardware states
which existed entirely between physical reads.

At the target ABI, a command is 196 bytes and a 32-command slab is 6,276 bytes.
A 466×466 offscreen image adds 434,312 bytes. Nine/ten-icon plans use 22/24
commands (one slab): 440,588 added heap bytes before allocator overhead. A full
19-icon plan uses 42 commands (two slabs): 446,864 bytes. Small receipt/map scalar
state is additional; existing display/alarm image buffers are unchanged.
Modal/restore high-water allocation and actual device free memory remain product
admission checks. OOM fallback does not silently allocate another frame cache.

## Selected bounded cadence and ASYNC correction

The selected native builder records `raster_work_budget_ms=8` and defines
`PORTABLE_RASTER_WORK_BUDGET_MS=8`. Other profiles retain the existing 2 ms
replay budget. Per-row custody/checkpoint checks, 256-step/65536-pixel caps and
frame receipts are unchanged. The 20 ms outer poll wait already advances replay
inside its 1 ms provider-yield loop; it is not a 20 ms pause per slice.

The owning selected fixture now advertises ASYNC_PRESENT as the actual CO5300
does and starts input after first COMPLETE. Earlier flags=0 timing receipts
remain historical synchronous-provider evidence, not exact selected composition.
Isolated realistic ASYNC sweeps showed observed physical-edge→dispatch maxima
21/22 ms for 2/8 ms budgets with no queued losses, versus 849 ms and 16 GAPs for
the immediate renderer; these are synthetic results, not device guarantees.
Larger slices amortized scheduler/provider cost but did not establish better
frame throughput than immediate rendering. The explicit 8 ms selection is a
bounded scheduling tradeoff, not a frame-speed acceptance claim.
