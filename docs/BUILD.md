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

The 17 app host interaction fixtures are pinned to the applications' actual
source baseline, `525e32689203502a7b22f6350b7ef04f272271db`, individually recorded
in the lock. Image Viewer lacks an upstream app fixture and receives compiler,
manifest/import and ELF structure checks only. These are focused host fixtures,
not hardware/runtime certification or exhaustive behavior tests.

Newer Reader Springboard fixtures were tried and failed against the older
external Springboard, confirming the reported source drift. They were not
weakened or treated as a passing compatibility claim: baseline fixtures exercise
the baseline app. Updating apps and their matching fixtures is subsequent work.
The source audit records five upstream-only app updates. Build success does not
mean current-upstream, release-byte, U1, or on-device parity.

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
