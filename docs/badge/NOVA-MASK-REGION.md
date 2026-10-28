# Immutable-command black-region proof for the native Springboard mask

Opt-in `--nova-mask-bounds` on the Springboard owning builder (default off),
recorded as `nova_mask_bounds` and `PORTABLE_NOVA_MASK_BOUNDS` in build defines.
Requires the existing sliced native frame selection. It does not change the
mask transfer function, icon sampling, captions, colors, frame cadence or input
geometry. No Runtime/provider ABI, heap allocation or BSS buffer is added.

## Proof and lifetime

At mask capture, an exact full physical opaque black RGB565 first fill establishes
known black. Captured circle/icon/pixel commands conservatively expand one union
rectangle. Circle bounds include the antialiased rim; icon bounds contain both
dimension parities and all bilinear output samples. Wide setup arithmetic clips
extreme coordinates before narrowing. Unknown command kinds, second/partial
fills, bitmaps, layers, clipping, paper offsets, rotated/flipped or partial safe
viewports invalidate the proof and retain the original path. No catalog-specific
positions or app count are encoded.

The rectangle, validity bit and dimensions occupy existing immutable mask-command
arguments, not extra allocation or address-keyed global state. A newly reused
command is zeroed by the existing recorder before its own proof. Immediate/OOM
materialization retains the original mask; only started ordinary replay into its
own software allocation can use bounds. No previous-frame/provider content is
assumed black. All prior fill/draw commands still execute; failure cannot submit
an incomplete frame.

Each row still reaches the existing checkpoint and custody checks before region
skipping. The mask checks current surface pointer/frame/stride and geometry;
once a callback changes identity, it never re-enables bounds within that call,
even if the address/frame later returns. Unknown regions use the unchanged mask.
Caption backing and glyphs remain AFTER the mask, exactly as before.

## Selected-path accounting

The old466×466 mask already skipped opaque interior and skipped arithmetic/stores
when a loaded pixel was black. It still loaded76,906 pixels =153,812 active bytes
per frame; this is an edge-region pass, not a full434,312-byte image read.
The new proof skips those reads and loop iterations outside known touched bounds.
Actual instrumented whole-app workloads using the public nine-entry catalog:

| workload | frames | previous pixel reads | bounded pixel reads | identical pixel writes |
|---|---:|---:|---:|---:|
| steady | 1 | 76,906 | 0 | 0 |
| horizontal | 721 | 55,449,226 | 4,624,659 | 605,553 |
| vertical | 616 | 47,374,096 | 642,907 | 173,853 |
| diagonal | 701 | 53,911,106 | 4,821,255 | 746,161 |

This removes91.1–98.6% of measured panning mask reads, and all steady-frame mask
reads. Reads/writes here are RGB565 pixels (two active payload bytes), not measured
PSRAM bus/cache transactions. The initial full black background fill remains.
This complements, but does not relabel, the separate framebuffer clear/underlay
copy elimination. There is no device FPS or end-to-end performance claim.

## Owning qualification

- `test_nova_mask_region.py`:600 actual adapter comparisons per normal/sanitized
  lane, dimensions160–480 including rectangular800-high surfaces, arbitrary
  positions/radii/opacities and mixed commands, unknown/partial/nonblack fallback,
  padded storage, cancellation, offscreen OOM materialization, repeated command
  reuse. Exact complete buffers, submission/failure and clock callback counts.
- Additional proof guards cover INT32_MIN/MAX centers, clipping, offsets, MONO1,
  rotation and nonblack/incomplete base. Callback frame mutation followed by
  identity restoration proves bounds remain disabled after the first change.
- Existing original-mask oracle:460 geometry/background/band/fault comparisons
  in each normal and ASan+UBSan lane (including flips/insets/guards/padding).
- Four whole-Springboard workloads under ASan+UBSan preserve complete frame and
  input identities.42 composed underlay/Quick/custody cases pass per normal and
  sanitizer lane, including retained callbacks and restoration.
- Exact selected target recipe compiles and links with unchanged imports/exports;
  structural ELF and PSRAM opcode validation pass. Mask selection adds796 bytes
  `.text`, zero `.bss` against this source's unselected target. This is not full
  final product admission. Leak checking is disabled in sanitizer lanes.

The host fixtures model renderer/input behavior, not all physical radio, sleep,
USB or touch timings. Initial compile/fixture failures remain in evidence:
misleading indentation corrected; unsupported SAFE_VIEWPORT+Nova test composition
replaced by the actual selected geometry; per-test pixel-budget state reset before
comparing callback counts. No guards were weakened to produce passing results.
