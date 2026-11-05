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
scene close, even after a later component/keyboard update. This allocation
capacity differs from the copied bitmap length: public page geometry rounds
width down to a multiple of eight and subtracts chrome height. Wave's 410-wide
surface therefore has viewport stride 51 but retains 25,727-byte blocks.
Rotations swap logical dimensions and preserve their product. A modal child may
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
interrupt or task-stack headroom. On System source `9e0a66b2ab85263de2939f5d8cce8d61bb4a9e71`, all three
profile witnesses are byte-identical to the shipped ELF. `modal_open` is 96
bytes and `modal_restore` 48 bytes, both static. These measurements rule out
a checkpoint-sized temporary in those emitted frames, not downstream
call-chain usage. See [scene job 127203673589](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42015130536/job/127203673589)
and [three-profile job 127203674107](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42015130503/job/127203674107).

To establish product headroom, retain this request vector and exact native
build/config identity; measure allocator charges/pools, display demand and
the actual caller chain; execute the real product's modal sequence with its
enabled RF/storage/resident workload and record allocation outcomes plus
before/peak/after pool observations. Repeat restore/close cycles and failure
injection must show bounded live allocations and recovery. Host fixture peaks,
layout witnesses and matching target builds are useful evidence, but do not
constitute that native execution or physical acceptance.

No limits, allocation policy or product behavior are changed by these receipts.

## Reproducible allocation sequence fixture

The existing modal fixture now has a `memory` mode, run by
`python scripts/test_scene_telemetry.py --runtime <pinned-runtime>`.
It invokes the real component/page/modal/checkpoint APIs and instruments only
the provider translation unit's malloc/free in the test adapter. No allocator
hook is shipped. The checkout SHA, staged SDK and CI compiler identify this
host test; target ABI sizes remain the separate target receipts above.

It checks the three request vectors above, including a page-to-components
transition that retains the parent page pair. Three successful repetitions
must finish with zero live allocations. Refusal at each ordinal 1 through 5
of the maximum vector must release every partial page pair; a refused
checkpoint leaves the paused parent resumable; refused child buffers leave
the checkpoint and parent pair intact until normal modal close. Closing the
child must return to the parent's pair, and parent close must return to zero.
The first failed page allocation still permits the second allocation attempt,
then both results are freed. Tests record requested payload/peak block count,
not libc charge, native display memory, or product headroom.

System `81368f3d5e336b974c6146b8c9a5101c4c24545d` passed
[scene job 127213310611](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42018011651/job/127213310611):
280 sequence cases across normal/ASan/UBSan, full/compact, admitted geometry
and both continuity modes. All checked closes ended with zero live blocks;
maximum live block counts were 1, 3 and 5. The fixture host ABI checkpoint
measured 29,864 bytes, so its full page-pair peaks were 58,664 / 132,772 /
221,864 bytes for Watch / Wave / X4 geometries. These are host requested
payloads, not native charged bytes. The target checkpoint remains 29,856.

The same source's
[Wi-Fi job 127213311122](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42018011609/job/127213311122)
passed 43/43 scenarios normally and with sanitizers, including actual shared
text alarm restoration/cancellation. All three target profile witnesses in
[job 127213311089](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42018011609/job/127213311089)
matched their production ELF bytes, with 96-byte modal_open and 48-byte
modal_restore static frames. That job's later product gate failed; neither
these passing fixtures nor stack witnesses establish product admission.

## Required native budget proof

Let `C(pool,n)` be the selected allocator's actual charge for an n-byte request,
including its exact alignment/metadata/poisoning configuration. For parent
geometry P and child geometry Q, the maximum live provider payload vector is
`[A(P), A(P), 29856, A(Q), A(Q)]`, not one contiguous allocation of its sum.
For each allocator pool, admission must cover the sum of C for simultaneously
live requests assigned to that pool, plus other live users and required
reserves. A largest-block condition is also required at every allocation
ordinal. Pool placement/fallback must be proven from the selected allocator
and workload; assuming that the checkpoint always lands in PSRAM is invalid.
Where placement is not reserved, either bound every permitted fallback or
provide a fail-closed admission mechanism. A free-heap snapshot is neither.

The actual Wi-Fi/alarm path requires one C(pool,29856), beyond its already
loaded static/ELF and native dependencies. The reusable worst case requires
all five charges. Pixel count bounds are derived from admitted geometry;
different parent/child dimensions use separate values. Native display buffers,
touch/navigation subscriptions, retained RF operation, storage receipts,
loaded providers, cached images and allocator fragmentation need their own
bounded coexistence record. This fixture deliberately does not invent them.

Runtime `fb6d0323b7ab6fa57af8fd5551ce45fdf52c743d` runs application entry synchronously on its owner task:
`src/main.cpp` calls setup from IDF app_main, and
`src/bootstrap/Runtime.cpp` invokes the loaded entry directly. Therefore the
required owner stack is the maximum reachable sum across Runtime entry,
Wi-Fi, alarm/text callbacks, scene modal/layout/raster work and native
callbacks, with compiler/ABI spill, library/assembly and applicable interrupt
usage accounted for. The rebuild_hits call during restore is part of this
chain. Per-function .su records alone, or adding modal_open to modal_restore,
are not that proof. Include dynamic .su qualifiers and resolve indirect
callbacks from the exact loaded cohort. Record actual task allocation and
stack location, high-water under the reachable worst paths, guard results,
and the static unresolved-call/recursion list. High-water alone is observed
coverage, not a bound on unexercised paths.

Exact X4 native successor receipt:
[run 42014116122](https://github.com/michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO/actions/runs/42014116122),
artifact 13409920368, Runtime `fb6d0323b7ab6fa57af8fd5551ce45fdf52c743d`, product `762e5a458d56e9582b839c2fbe30109911f36f92`,
SDK `fff9895c82d744c7237be8847347bdd1b07c6643`.
It selects SPIRAM malloc with preferred-internal cutoff 16,384 and internal
reserve 32,768, poisoning disabled and task tracking disabled. These are
allocator policy inputs, not modal headroom. The successful link's 16-byte
margin before the IQ bank is a static-layout constraint, not available heap.
These full pins identify the successful native receipt; a later Runtime pin
requires its own exact native evidence.

Installed Watch/Wave native-to-store pairing remains unresolved. Do not
reuse X4 allocator settings, stack configuration or this successful link for
those products. A native admission receipt needs full source hashes, firmware
and provider ELF hashes, selected profile/config, allocator implementation and
charge/placement evidence, stack composition, exact live request sequence,
coexisting workload/reserve bounds, failure/recovery observations and the
explicit scope of what remains unmeasured.
