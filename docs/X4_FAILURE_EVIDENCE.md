# X4 copied crash and restart evidence

The selected resident Home is reserved as **0.3.19**. The 2026-10-09 14:10 UTC
System PR reservation check found no existing 0.3.19 claim. The preceding Home
0.3.18 and product .44 artifacts remain frozen. This is an isolated development
source change; it does not publish a System PR or install a product image.

A healthy configured host reads Runtime's optional `failure_evidence` suffix.
The record is copied, and reading does not consume it. Pending records are
presented before the host resumes normal Home or a pending foreground. The
screen shows the boot/record sequence, current reset reason, captured reset hint
when available, last committed Runtime activity, phase and invocation, retention
status, detail, bounded recent phase history, exception PC/SP, and raw stack
frames. The application is explicitly labelled as last activity, not proof of
fault origin. Missing, unsupported, invalid and truncated stack data have
separate labels. No symbol names or unavailable crash details are invented.

Text is measured with the existing paper font and wrapped onto additional pages.
The layout supports portrait and landscape without clipping long copied strings
into the footer. Navigation arrows and Back change pages; Confirm advances until
it selects the final Continue. Touch selects the labelled footer buttons. Home
never acknowledges the record. Each page requires a completed physical
presentation, and acknowledgement is made only after an explicit final Continue.
Held input is neutralized after every page and after returning to Home.

The exact record boot and sequence are acknowledged. A stale response rereads the
new record and starts review at its first page. A denied acknowledgement leaves
it pending. An acquire, malformed surface, submit, status, timeout or input custody
failure leaves it pending and stops the System path. In particular, no UI,
provider work, input polling or diagnostic I/O follows retained display failure.

This is a healthy-host renderer, not a panic-time display driver. Once Runtime
has retained unsafe device custody, System cannot repaint the panel. Native
capture and reset handling own that boundary; the copied evidence is available
when a healthy host can next present it. RTC-backed evidence may not survive
power loss. This change does not add an automatic restart or alter power, startup
selection, the Home/desk/drawer artwork, or GameBoy.

## Compatibility and build provenance

The Runtime suffix, copied client and record are version/size checked. Missing
suffix or missing backend uses the existing resident failure fallback. A NONE or
already acknowledged record does not redisplay a legacy prior-reset message.
Guarded Runtime-table copies are zero-initialized and bounded by the supplied
size, so extending the compile-time SDK does not over-read an older table.

The resident builder stages the failure header only when the selected Runtime
SDK declares its suffix. Build receipts hash both SDK headers and the new System
renderer. Older SDKs remain buildable. The existing reference renderer assets
and ordinary non-resident profile selection stay unchanged.

The focused test uses the real System adapter, paper renderer, separate ELF
host/legacy app instances, and production Runtime/ProviderGraph. Only physical
providers and the native evidence backend are fixtures:

```
python scripts/test_failure_evidence.py \
  --runtime /path/to/runtime-0193 \
  --display-sdk /path/to/display-sdk \
  --old-system /path/to/frozen-pre-evidence-system
```

It checks full, missing and malformed-bounded evidence, portrait/landscape,
unsupported and invalid stack capture, retention status, stale and denied
acknowledgements, inherited navigation/touch, previous-page traversal, repeated
legacy handoffs, a genuinely short pre-suffix Runtime allocation, old System
sources compiled against the frozen .92 SDK, and every first-presentation failure.
Normal and ASan/UBSan executions emit completed-frame captures and must have
identical pixels. The three-host legacy test reads the pending record twice,
reloads the host to present/acknowledge it, then reloads again to verify it stays
acknowledged.

The existing resident-failure suite is run against both old and new Runtime;
Home reference, shared drawer controller and sparse desk timer regressions are
also run. The target build uses the existing qualified Home recipe with the new
System source, .93 SDK and reserved Home version, pinned Xtensa GCC 8.4.0, a new
output directory, structural ELF/import validation and fresh source/SDK hashes.
Physical X4 hardware has not been qualified by these host and target checks.
