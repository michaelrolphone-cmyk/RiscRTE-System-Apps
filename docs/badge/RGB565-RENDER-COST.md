# Exact RGB565 work reduction

The shared adapter now uses affine RGB565 spans by default when Contexts capture
is not selected. MONO1 and Contexts retain their existing paths. The old
PORTABLE_RGB565_SPAN_FILL define remains compatible but is no longer required.
This changes arithmetic/write batching, not sampling cadence, frame ownership,
geometry, images or behavior. The scalar oracle remains generated in the owning
test. 2,520 cases per default/legacy-marker lane compare complete buffers,
stride/edge guards, rotations, clipping and five checkpoint-failure prefixes;
ASan/UBSan passed, including counter wrap. Transform calls fell from33,329,784
to5,400 across that matrix, with65,808 unchanged checkpoints.

PortableRgb565Blend preserves the original floor(channel*255/max) expansion
and integer truncation; it does not approximate with bit replication. Opaque
Settings pixels do not read the destination. Partial channels use exact small
expansion tables and bounded floor(n/255) arithmetic. Normal and ASan/UBSan
validate17,777,216 channel/full-pixel combinations plus all65,026 division inputs.

Six actual Settings scenes at466 with69px insets (328px content) preserve final
pixels and ordered checkpoint prefixes across48 cost/sanitizer variants versus
the prior source. The native geometry/controller lane also passes. Target
Xtensa -Os Settings links without new imports. These are software results,
not hardware frame-rate acceptance. The direct target invocation used the
selected canonical SDK plus an isolated directory containing only the new
helper; placing all bundled headers first was rejected for conflicting Runtime
ABI types, and that failed invocation is retained in evidence.

Timing model separates counted pixel work, CST read costs and transfer latency.
At1us per counted pixel,2.25ms touch read and60ms modeled transfer, original root
render took803ms to submit and863ms to complete; queued touch age was856ms.
An8ms polling-gate counterfactual reduced that to454/514/500ms, but is NOT a
product change or sufficient responsiveness repair. Exact span optimization
necessarily leaves this same per-pixel synthetic charge unchanged. Host-only
uninstrumented-point samples improved root roughly3.55–3.61ms to3.09–3.31ms;
this is not an ESP32 cycle or speed measurement. Actual target captures remain
needed to allocate physical time between drawing, polling and transfer.

Evidence scripts and records: /tmp/shared-raster-timing-audit and
/tmp/settings-rgb565-evidence in the current execution workspace. These paths
are local evidence, not a claim of durable source delivery.

## Nova bilinear icon successor

The icon scaler uses exact quotient/remainder recurrence for horizontal
pixel-center coordinates; vertical interpolation terms and mask vertical
fade terms are row-constant. It retains bilinear coverage, every pixel-call
coordinate/alpha, clipping, and the same downstream custody checks. The source
scalar formula is pinned in `test_nova_icon_exact.py`;8,388,480 recurrence
positions and4,676 actual glyph-stream/mask/failure-prefix cases pass normal and
ASan/UBSan. Ten actual466px Springboard controller scenarios pass ASan/UBSan
with clean teardown. The selected target links with unchanged imports.

Target disassembly is important: old -Os code ALREADY hoists vertical division
out of the inner loop. The actual saved work is horizontal signed division per
output pixel, replaced by four setup divide/remainder operations and bounded
add/carry steps. No target elapsed-time or physical usability claim follows from
that instruction evidence alone. Host-only96px glyph sampling improves in the
isolated arithmetic test; this does not include actual I2C/display/provider time.
