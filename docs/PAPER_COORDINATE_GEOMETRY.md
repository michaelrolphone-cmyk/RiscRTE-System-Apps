# Paper Quick coordinate geometry

This source component corrects native primitive boundaries in the existing
paper Quick sheet. It does not replace the renderer, activate an app recipe,
change service actions, or alter modal input/custody sequencing.

## Measured baseline

System `118c7c27f3b143e34849f70c4a30f2fdbf886e86` already renders paper primitives
and text directly into capability-sized MONO1 surfaces. It does not scale a
completed 240-wide view. Two real-adapter defects nevertheless remained:

* At 410×700, the first legacy tile painted `[27,201) × [264,344)` while fresh
  press/release input accepted `[28,202) × [265,345)`. Its visible leading edge
  missed the action and an invisible trailing pixel activated it.
* At 480×640, resident glyphs, icons and rounded controls disappeared. Their
  one-reference-row spans independently computed `1 * 640 / 800 == 0`.
  The resulting sheet contained only two rules, with 2,080 black pixels.

Frozen baseline images and the source-bound reproduction receipt are in
[`paper-coordinate-geometry`](paper-coordinate-geometry). The 480×800 controls
remain the compatibility oracle. Resident tile decoration intentionally extends
two reference pixels beyond its action rectangle; that existing decoration is
preserved rather than redefined by this correction.

## Coordinate contract

For an existing inverse input map `floor(pixel * reference / extent)`, logical
`[a,b)` is accepted at native `[ceil(a*extent/reference), ceil(b*extent/reference))`.
`PortablePaperGeometry.h` applies those same endpoints to each rectangle axis,
including the signed transition offset. It promotes and clips signed reference
endpoints before multiplication. Ordinary 480/800 references then use safe
32-bit products and division; wide references use at most 16 product comparisons
without 64-bit division or modulo. Nonpositive lengths,
invalid extents/reference sizes, wholly offscreen or collapsed intervals emit
no rectangle. Adjacent downsampled one-row intervals can individually collapse;
their union still covers the projected complete glyph/control. No blanket
minimum-one inflation moves ink outside the endpoint contract.

Only `qp_rect` adopts this projection. Existing input conversion, native paper
text/icon placement, action dispatch, snapshot lifecycle and service calls stay
in their existing paths. At 480×800 the projection is exactly the identity,
including clipped and negative transition coordinates.

## Qualification

* A 175-case baseline/fixed provider matrix covers 108 nonresident cases,
  36 separately loaded resident host/client cases and 31 unchanged original
  clock/launcher cases. Display orientation, reader flip and raw-touch 180°
  rotation are distinct controls. Snapshot, delayed immutable presentation,
  cancellation, restoration, grants/subscriptions and frame ownership are checked.
* The fixed legacy tile paints exactly its unchanged accepted input interval.
  Fixed resident 480×640 pixels equal inverse-floor sampling of every frozen
  480×800 reference pixel in both audio profiles. Reference-size captures are
  byte-identical. Individual primitives remain native raster operations.
* Pure normal and ASan/UBSan checks each exercise 26,362 intervals, 46,629,037
  pixel assertions, 50,938 boundary assertions and nine invalid-input rejections,
  including signed/offscreen coordinates and extreme integer values.
* The unchanged original paper suite separately passes 124 cases plus two
  Home ordered-input runs. The reproducible CI workflow uses pinned Runtime and
  display SDK headers and records compiler non-system dependency hashes.
* The freestanding Xtensa probe has no undefined imports and passes the unchanged
  ELF validator. The actual Wi-Fi paper, Quick Clock, Quick File Browser and full
  16-capability native Settings target builders also pass unchanged admission.

## Target regression retained

Initial published head `95610f4148153ccc1603c30fdb88b657f9809188` passed both
paper host workflows, but its aggregate was 22 successful and four failed runs.
The first endpoint implementation introduced signed 64-bit division/modulo.
Wi-Fi rejected undefined `__divdi3` and `__moddi3` imports. Settings linked those
helpers locally but the unchanged validator first rejected `.rela.dyn` entry
3308 (type 0, offset 0); Quick Clock and Quick File Browser also failed admission.
The arithmetic-only follow-up keeps the same pixel contract while removing the
wide division dependency. The failed-head admission witness and exact ELF hash
are retained in `paper-coordinate-geometry/target-admission-failure`; corrected
target receipts are in `target-results`. No import allowlist or loader policy
was relaxed. Host pixel tests alone are not target ABI acceptance.

This is host source-component evidence. Hardware and complete app integration
remain separate. The stopped shared idle correction is neither included nor
qualified. Existing 410×700 time/UNAVAILABLE text clipping is recorded as a
separate visual acceptance issue; this bounded coordinate correction does not
change typography or wording.
