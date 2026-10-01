# Independent development build

This pipeline compiles only this repository's `Apps/` sources. It needs Python
3.11+, a host C compiler, and the official Xtensa S3 compiler. It does not fetch
Reader, compile firmware, create a release or modify a live catalog.

```sh
python -m pip install platformio==6.1.19
pio pkg install --global --tool 'espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5'
python -m unittest discover -s tests -v
python scripts/test_apps.py
python scripts/build_all_apps.py
python scripts/check_release_parity.py
# Optional selective development build; clears previous dist/apps outputs:
python scripts/build_all_apps.py --id settings
```

Use `PLATFORMIO_CORE_DIR` for an alternate package directory or `NATIVE_APP_CC`
for an explicit compiler executable. The evidence records its actual version.
Builds retain upstream C11/PIC/hidden/shared/SysV-hash flags, bounded compiler
helpers instead of blanket libgcc, strip-unneeded behavior, public import checks
and the real firmware structural ELF validator plus malformed-header/section/
relocation mutations. Every successful full build emits 18 ELF+JSON pairs and
`build-evidence.json` with source/manifest blobs, version, size, SHA-256, SDK
commit, actual compiler, repository commit and dirty state. This evidence is
explicitly **not an install catalog**. CI uploads it as a 14-day development
artifact; no release credentials or publication permissions are needed.

## Provenance and test limits

`sdk/baseline.json` pins compiler, SDK headers, compiler helpers, manifest and
integrity validation, ELF validator, and host fixtures by Git blob and SHA-256.
SDK/ABI baseline is Reader `524e2cb3ea27311b062f3d7f182668c6912829fc`.
The public symbol snapshot is derived from actual host/libc/compat tables listed
in `export_inputs`; it excludes privileged provider-only inventories. No API is
implemented by the snapshot and no capability permissions are granted by it.
The vendored source retains its upstream license and comments.

There are 18 host interaction fixtures: one each for 17 apps plus Springboard's
live video fixture (drag/settle, cancellation, vertical locking, backpressure and
cleanup). The five synchronized apps' fixtures are pinned to Reader
`be82695ea0ecb14525c0de1ddc78cd0c77e4614b`; unchanged apps retain their recorded
`525e3268` fixtures. All origins are individually recorded in the SDK lock.
Image Viewer lacks an upstream app fixture and receives compiler, manifest/
import and ELF checks only. The File Browser oversized-USB-handle regression
also runs with sanitizers in CI. These are focused host fixtures, not exhaustive
hardware certification.

All 38 app source/manifest/helper files now match the audited Reader commit.
Springboard's `springboard_video.inc` and `springboard_slide.h` are tracked as
additional source inputs, including their blobs in both audits and build evidence.

`python scripts/check_release_parity.py` compares actual emitted ELF bytes,
lengths and versions against `sdk/release-baseline.json`. All five synchronized
apps reproduce their published bytes exactly and are mandatory CI checks. The
other 13 older released artifacts differ from these development builds; the
report preserves those mismatches rather than hiding them. A [historical no-strip probe](historical-build-probe.json) reproduced 11 of those
13 exactly; Button Remap and Driver Manager still differ. That diagnostic does
not replace the normal builder or its reported parity result. Their remaining
build-context differences still need investigation. See [release parity](release-parity.json).
None is ready for independent publication or Reader removal: ZIP/index and
runtime integration remain separate unfinished work.

## Safe ongoing synchronization

```sh
python scripts/check_baseline.py --reader /path/to/read-only/Reader \
  --ref FULL_COMMIT_SHA --output docs/source-drift.json
```

This reads immutable Git objects and classifies each app source and manifest as
unchanged, converged, upstream-only, external-only or conflict against its
recorded baseline blob. It never changes app files. Without Reader it checks
local provenance only and explicitly cannot establish upstream freshness.
Review upstream-only changes with their versioned manifests and docs. Preserve
external-only edits; reconcile both-side conflicts before advancing the baseline.
SDK lock updates require inspection of pinned APIs/exports and rerunning all
checks. Do not use a passing build to automatically copy or cut over apps.
