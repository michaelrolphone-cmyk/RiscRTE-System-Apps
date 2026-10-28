# Complete-overwrite initialization proof

`PORTABLE_RASTER_OVERWRITE_CLEAR` is opt-in and defaults off. It removes stores,
not commands, input servicing, custody checks, frame receipt checks or animation.
It changes no Runtime/display ABI and adds no allocation.

## Dominating writes

The software-buffer zero is redundant only for RGB565, no rotated paper transform,
no captured paper offset, no clipping, a full physical safe viewport, and an exact
first opaque full-frame `RS_FILL`. The immutable command stream performs that fill
before any blend/read command. A failure during filling does not submit. The
allocation remains private until replay completes. Partial content, MONO1,
transformed, clipped and bitmap-first paths retain their original initialization.

Only completed software-frame copying requests the second optimization. A validated
provider lease receives every active RGB565 pixel before submission. The old clear
is omitted only outside retained-handoff and resident-host compositions. Copy
checkpoints and terminal fences are unchanged; interrupted copying never submits.
Provider stride padding is neither cleared nor copied, just as before. The old
unconditional last-byte read is now restricted to MONO1 edge-bit preservation;
RGB565 copying does not read provider pixels before overwriting them. Ordinary
immediate rendering and offscreen-allocation-failure materialization still clear.

Neither initial white fill nor following black fill is folded: commands can have
service/cancellation boundaries and this patch does not assume intermediate
software state is unobservable. This leaves an additional candidate for separately
proved command dominance, rather than silently claiming its saving here.

## Selected traffic accounting

P = 466 * 466 * 2 = 434,312 active payload bytes (padding/metadata excluded).
Actual selected Settings command accounting reports two RS_FILL commands/frame.
Before glyphs/overlays, its software zero + two fills + provider zero + software
copy read/write total 6P. Alarm underlay copying adds 2P; native packing reads P
from the provider and writes P to internal DMA. Thus fixed PSRAM-side traffic is
9P = 3,908,808 bytes, not a measured physical bus-transfer count. This patch removes
2P writes = 868,624 bytes, reducing that count to 7P. Springboard has no selected
SAFE_VIEWPORT clears, so this patch claims no savings there. Separate guarded
underlay-role exchange can remove another 2P; it is not enabled by this macro.

Selected app code, font data and provider BSS also reside in PSRAM. Default large
app allocations prefer PSRAM (with allocator fallback); actual pointer placement
is not asserted by this test. CPU240MHz, octal PSRAM80MHz, instruction cache16KB,
data cache32KB/32B lines differ substantially from x86 host conditions. Payload
accounting is not cache-miss measurement or a prediction of physical FPS.

## Owning proof

`python scripts/test_raster_overwrite.py --output DIR` runs actual adapter code
in default/selected normal and ASan+UBSan configurations: 60 cases each spanning
240/353/466 widths, rectangular heights, full/inset viewport, padded strides,
clock cancellation, invalid lease geometry, rejected submission and allocation
failure with immediate fallback. Guard assertions additionally reject clipped,
partial, transformed, MONO1 and bitmap-first inputs. Pixels, padding and guard
bytes are checked; memset byte counters prove the exact removed active stores.
Leak checking is disabled; address and undefined behavior checks are enabled.

`test_portable_raster_custody.py --overwrite-clear` retains 18 existing cases;
its sanitized lane, 10 sanitized native-terminal cases and sanitized clipped-begin
case also pass. These cover Quick interruption/restoration, OOM, orphaned captures,
terminal callback fences and clipped drawing. Product/target admission and actual
device performance remain separate integration work.

## Explicit build integration and composed proof

Both owning builders expose separate default-off `--raster-overwrite-clear` and
`--raster-underlay-exchange`. They require sliced native frames; exchange checks
the *effective* alarm-client define after foreground-idle configuration (not
merely an explicit CLI argument). Selection is recorded as `raster_memory` and
in `build_defines`; the shared selector helper is a hashed build input. Suggested
current selection is Settings both options, Springboard exchange only because
Springboard has no SAFE_VIEWPORT clear to remove.

The isolated composition preserves the prior underlay exchange feature and its
post-submit identity/pin fence (original 1cfab07 + 996465c). No overlapping provider
acquisition or cache-size experiment is selected. Exact selected Settings replay
of 25 frames has zero clears instead of 21,715,600 clear bytes, 25 role exchanges
and zero underlay payload copies; software→provider copy remains 10,857,800 bytes.
All 25 completed pixel hashes and visible hit-map scroll values match baseline;
budget 22,367,487 and 45,161 checkpoints also match in this workload. Checkpoint
count equality is evidence of preserved servicing, not a universal required count.

Four actual whole-Springboard steady/horizontal/vertical/diagonal workloads retain
identical pixel/input/frame identities under ASan+UBSan. Original underlay payload
copy totals are respectively 434,312 / 313,138,952 / 267,536,192 / 304,452,712 bytes;
composed totals are zero, replaced by 1 / 721 / 616 / 701 ownership exchanges.
Those byte totals count copy payload, so memory reads+writes avoided are twice as
large. Whole-app mocks model logical input and rendering, not real device timing.
Selected Settings fixed PSRAM-side payload accounting becomes 5P from 9P;
Springboard becomes 4P from 6P. Neither accounting includes variable overlays,
fonts, blending, metadata, code fetches or physical cache writeback effects.
