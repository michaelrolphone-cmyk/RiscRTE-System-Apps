# Optional immutable raster / alarm-underlay role exchange

Select `PORTABLE_RASTER_UNDERLAY_EXCHANGE` together with the existing snapshot
and alarm-client composition. Default off. This does not change the provider
ABI, add a framebuffer, borrow provider pixels, or mark a pending image visible.

After an ordinary complete RGB565 software image has been copied into a provider
lease, its exact software pointer and provider-frame ID are recorded. At present,
only a matching replay can replace the alarm-underlay row copy. The two existing
app-owned allocation roles exchange only after accepted submission with a nonzero
token and live custody. Underlay validity still follows the existing completion
path. Failure or retention never exchanges/frees either allocation afterward. A once-per-submit identity/pin recheck detects unexpected callback mutation before exchange; the selected terminal composition retains silently. Older compositions use their existing supported retention helper.

An alarm can pin `saved=alarm_pixels` before an earlier sealed app image finishes.
That modal is therefore ineligible even when the sealed image's captured owner
is the app. It uses the unchanged copy into the pinned address. Quick modal and
restore paths, immediate/OOM fallback, MONO1, and retained-handoff/crossfade or
resident-host compositions also preserve the existing copy. No visual transform
is disabled to make a frame eligible. New capture, materialize, discard and
initialization invalidate copy provenance.

The optimization removes one 434,312-byte payload copy per eligible 466×466
frame, preserving offscreen→provider copying. At most the existing two distinct
application buffers exchange roles; provider memory is never one of them.

Owning `scripts/test_underlay_exchange.py --output DIR [--sanitize]` exercises
repeated exchanges and frame receipts, pending and partial-raster alarm pins,
actual Quick/alarm dismissal with exact ACK, restoration, immediate/OOM/modal
fallback, rejected and retained callbacks, invalid token, completion failure,
cleanup, plus the existing 29 custody/fallback cases. All 42 cases pass in each
normal and ASan/UBSan lane (leak checking disabled). Separate four whole-app
alarm-enabled panning/steady workloads match pixels and delivered input/frame
identity under sanitizer. Full selected Xtensa module validation/import/export
closure passes; default-off ELF reproduces baseline exactly. Selected change:
+404 bytes text and +16 bytes BSS.

Whole-workload host `-Os` timings are noisy: a pre-identity-guard matched six-pair run had medians
0.879/0.881/0.832 of baseline (horizontal/vertical/diagonal), but an earlier
pre-token-guard run was1.025/0.942/1.051. Final guarded-source ratios were0.891/0.893/0.905, still with broad ranges. All runs and ranges are retained. This
establishes copy elimination, not a promised target-time or physical-FPS gain.
Full integrated product and device qualification remain separate.
