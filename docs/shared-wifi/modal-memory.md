# Shared Wi-Fi modal memory evidence

## Measured target layout

System source `f0b834abe7201c28504fc5b09472d2ca4e99f73b`, Runtime
`712ecc3f894dba99d9c9638941778a82f5856b97`:
[product job 127179184036](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42007895242/job/127179184036)
printed target-compiled layout witnesses from
`build/product-providers/{watch,wave,x4}/scene-host/build.json`.

All three selected profiles (watch-compact, color-dynamic, full) measured:
- `sizeof(scene_model)`: 29,728 bytes.
- `sizeof(modal_checkpoint)`: 29,856 bytes.
- Maximum modal depth: one. Parent pixel buffers transfer ownership; they are not copied.

These are target ABI sizes, not host sizeof or measured available heap.
Normal and ASan/UBSan modal custody tests and repeat target builds passed
[scene run 42007903042](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42007903042).
Tests cover allocation refusal before any parent mutation or provider I/O,
ordinary open rollback, input continuity, retained native callbacks, parent-token
mutation fences, pending close and abandoned-parent cleanup.

## Actual Wi-Fi path versus general scene API

The Wi-Fi component/keyboard plus text-only alarm path allocates **one
29,856-byte checkpoint** in scene-host. Neither those components nor the alarm
text document requests page pixels. Text-input-host has no dynamic allocation.

For the reusable scene API, page/reading/preview allocate two pixel blocks
of `A = floor(logical_width * logical_height / 8)` bytes. These persist until
scene close, even after a later component/keyboard update. A modal child may
also request page pixels. Live request sequences are therefore:

- Fresh keyboard/component parent + text-only alarm: `[29856]`.
- Retained parent page + text-only child: `[A, A, 29856]`.
- Retained parent page + page child: `[A, A, 29856, A, A]`.

| Profile geometry | One pixel block A | Parent page + text child | Parent page + page child |
| --- | ---: | ---: | ---: |
| Watch 240 x 240 | 7,200 | 44,256 | 58,656 |
| Wave 410 x 502 | 25,727 | 81,310 | 132,764 |
| X4 480 x 800 (rotated surface 800 x 480) | 48,000 | 125,856 | 221,856 |

These are sums of requested payload, not allocator charges or whole-product
budgets. The largest request is `max(A,29856)`, not the sum. If geometries differ
between parent and child, use their separate A values. Partial allocation
failures free partial child buffers; terminal retention keeps owned state and
does not authorize another modal.

The two always-present logical/raster models are static provider storage,
already part of ELF mapping demand. Do not add them again as modal heap.
Display-provider acquired/submitted frame storage, loaded ELF sections, caller
stack, text/alarm provider static state, RF, storage and other resident work
remain additional demand. Parent scene display/input ownership drains before
child acquisition; this does not prove the display provider has only one buffer.

## Allocator and admission limits

At the inspected Runtime pin, `AppAllocationLedger.h` explicitly excludes
provider/firmware allocations. Scene-host imports ordinary `malloc/free`.
The public loader allocator is libc, or its image-pressure retry wrapper when
that firmware option is enabled. It does not guarantee PSRAM placement.
ELF mapping into PSRAM is a separate allocation path.

Exact charged demand is the sum of the selected SDK allocator's charge for
each live request, including alignment, headers and configured poisoning.
No numerical overhead is assumed here. Installed Watch/Wave native build
pairing, allocator configuration and display-provider allocation evidence were
not established by the layout receipt. No available heap or reserve is guessed.

The existing `memory.heap` API reports DEFAULT/INTERNAL/EXTERNAL/DMA free and
largest blocks. Its contract explicitly says a snapshot is not a lease.
Neither a total-free comparison nor a largest-block comparison alone proves
that the whole sequence fits under fragmentation and concurrent work.
Native provider identity/import admission is not a heap reservation.

## Stack receipt and remaining qualification

`build_scene_services.py` emits `scene-host/stack-usage.su` and
`stack-usage.json`, summarized under `build.json.stack`. It compiles the original
host translation unit with the native builder's identical flags plus
`-fstack-usage`, links the witness and requires byte-for-byte equality with the
shipped ELF. The receipt preserves compiler qualifiers, all emitted functions,
source/SDK hashes and source pins. Empty entries mean inlined/not emitted,
never a zero-byte frame. Per-function values are not cumulative call-chain,
interrupt or task-stack headroom. In particular, the checkpoint aggregate
assignment must be checked for a compiler-materialized temporary.

To establish product headroom, retain this request vector and exact native
build/config identity; measure allocator charges/pools, display demand and
the actual caller chain; execute the real product's modal sequence with its
enabled RF/storage/resident workload and record allocation outcomes plus
before/peak/after pool observations. Repeat restore/close cycles and failure
injection must show bounded live allocations and recovery. Host fixture peaks,
layout witnesses and matching target builds are useful evidence, but do not
constitute that native execution or physical acceptance.

No limits, allocation policy or product behavior are changed by these receipts.
