# Shared RGB565 arithmetic repair

Base: System `83da63b8002976bfdcf05a68e8e2a2f5b6b93678`.
This is source/software evidence, not measured device FPS or a claim the reported
2–3 Hz interaction is resolved. No cadence, touch sampling, action maps, receipts,
controller transitions, retained-state fences, or transport policy changes.

## Changes

* Settings opaque rectangles map clipped logical endpoints once and write affine
  RGB565 spans. MONO1 retains the previous path. Snapshot bands are clipped before
  native transformation. Span checkpoints remain at at most 512 written pixels;
  terminal checkpoint failure stops without another callback/write. A successful
  checkpoint rechecks local writable authority before any span write. The previous
  cell-major visitation order and counts for rejected pixels are not retained;
  these are not action/frame identity contracts. No alpha is reordered.
* Compact Nova/Springboard and native Nova primitives use the existing exact
  RGB565 blend helper (floor expansions, not bit replication). Native opaque
  writes skip destination read and all blend arithmetic. Existing geometry,
  pixel checkpoint, surface and custody tests remain in their original positions.
* Springboard caption backing recognizes its solid interior before integer-root
  and smoothstep work. The complete ordered pixel/alpha call stream is identical.

No builder switch: these are exact-arithmetic implementations in the existing
selected primitives. Historical monochrome paths remain unchanged. Round Settings
geometry must adjust x/y/w/h BEFORE `sv_native_rect` in `sv_rect`; text fitting is
independent and not edited here.

## Owning gates

`python scripts/test_settings_affine_rect.py --output DIR` runs normal and
ASan/UBSan: 5,760 complete raster comparisons (six sizes, four orientation
transforms, clipped/full replay bands and randomized rectangles), plus 19 terminal
checkpoint fences and a successful-checkpoint surface-unbind fence. Representative 328-square transforms fall 107,584 -> 3 and
logical scale evaluations 272,640 -> 4. Those counts are fixture observations,
not CPU cycles. Safe inset/stride padding starts nonzero and is preserved.

`python scripts/test_shared_pixel_blend.py --output DIR` compares actual prior
and new compact/native pixel functions: 12,000 band, coordinate, alpha and failed
checkpoint cases per normal/sanitized lane. Call/checkpoint and failed state parity
are asserted, not just pixel colors. Existing `test_rgb565_blend.py` validates
17,777,216 channel/full-pixel cases plus all 65,026 bounded division inputs.

`python scripts/test_caption_opaque_backing.py --output DIR` compares 1,400
complete ordered pixel streams per normal/sanitized lane. A representative
200x24 backing performs 8,897 -> 2,952 integer-root calls.

ASan uses detect_leaks=0 (container); UBSan has no exclusions. Generated executable
outputs are deleted after terminal test completion; fixtures/commands/receipts stay.

## Target evidence and remaining integration

Pinned Xtensa ESP32-S3 compiler, -Os microkernel assembly: prior scalar blend
contains six multiply-high instructions to implement constant divisions; exact
helper contains none, uses 96 bytes of existing expansion tables, and has a short
opaque branch. Function text is 158 vs 156 bytes, so code size alone is not a speed
claim. This microkernel is not the final app ELF/import admission or physical cycle
measurement. Final native product, installed async action/frame identity and
scrolling/Quick/alarm regression gates remain with the integrator. Full Settings
and Springboard elapsed-frame costs need real selected-device phase recordings.
