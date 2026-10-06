# Portable software updates (development)

The existing System Apps own product UI. `PORTABLE_UPDATE_APP` selects their
shared portable controller; ordinary Reader builds remain unchanged. The
existing PortableApps display/touch/navigation adapter and Settings typography
render the screens. There is no second firmware UI stack.

## Ownership and authority

Two separately compiled ordinary ELF providers expose
`software.update.firmware@1` and `software.update.apps@1`. App grants select
one action kind; apps never receive the provider-only native HTTP/bank tables.
Both providers require `platform.http-client@1`, `platform.bank-store@1` and
`platform.clock@1`. Product URLs, release parsing, version comparisons,
selection and progress are outside Runtime. Native bank admission independently
validates downloaded bytes, exact size/SHA, ELF/imports, existing app identity,
unchanged executable paths and requirements, and a strictly newer app manifest.
Catalog URLs, descriptions and manifests never expand installed boot grants.
Driver, provider and arbitrary new-app installation are out of scope. No package
signing system is introduced.

The client requires display, touch, configured `rtc.clock@2`, namespace-6
`storage.key-value@1`, `net.wifi@1` and its action-specific service. Deployment
must explicitly specify the configured RTC offset when building, rather than
assuming display time is UTC. Watch currently uses fixed UTC+08. Invalid clock
values fail before HTTPS. Networking uses the existing saved-profile codec;
credentials are never logged or passed to the update service. Opening the app
does not connect until Check. Radio is checked/drained/released on catalog
completion, error, Cancel, Back or ordinary idle sleep. No background policy
or reconnect is added.

HTTP sessions close before checked radio cleanup. Inactive-bank transactions
abort before exit/sleep. Cleanup refusal leaves the interactive app or invocation
retained, blocks storage-backed alarm work and permits cleanup retry. Native
retained sleep permits no late rendering/provider work. Ordinary display/input
failure drains update/radio ownership before app_main returns, before the
Runtime pre-finalizer barrier. Activation closes transport/radio first, then
uses native activation; verified bytes are never labeled installed. Activated
or uncertain activation keeps the bank token and presents Restart, never another
install, abort or success claim. Storage-backed alarm pumps remain paused while
restart is required.

## Bounded memory and loader admission

Each provider owns its fixed JSON, catalog and manifest parsing workspaces in ELF `.bss`,
not generic heap allocations. The target build requires 512–704 KiB of BSS per
provider, limits each compiled provider function frame to 2 KiB, and rejects generic malloc/calloc/free imports or global constructors.
The coordinated Runtime loader must use `CONFIG_ELF_LOADER_LOAD_PSRAM=1`:
`esp_elf_malloc` requests `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` exactly once, with
no internal-RAM fallback. Failure to allocate the provider data image produces
`-ENOMEM` before provider entry/start. `python scripts/check_update_loader_memory.py --runtime /path/to/RiscRTE`
executes that selected checkout's exact allocation/section-loader functions with
mock PSRAM allocation against both target provider ELFs, including text/data OOM.
Its evidence records exact source hashes and the Runtime revision. It never
executes Xtensa code or writes a device. Deployment must retain this loader gate;
structural ELF verification alone does not prove an arbitrary loader's policy.

Cleanup retries preserve the native storage guard: first close HTTP/abort, then
attempt checked radio cleanup, then retry bank abort once if radio recovery
made storage safe. The native CPU refuses radio leave while HTTP is retained.
Repeated failures retain ownership; no native safety guard is bypassed.

## Catalog and additive future OTA records

The read-only catalog is:
https://raw.githubusercontent.com/michaelrolphone-cmyk/RiscRTE-T-Watch-S3/release-index/release-index.json

The Watch schema-1 app rows carry an ELF and complete manifest. They are not
Reader `.rte.zip` bundles. The existing Reader bounded JSON cursor/work budget
is reused with immutable provenance in `Services/update/PROVENANCE.json`;
`T5PackageVersion.h` remains unchanged. Limits are 512 KiB, 128 app rows,
4 KiB manifests, 10 JSON levels and 64 simultaneously active object keys.
All keys, including ignored metadata, are checked for duplicates before decode.
Identity-bearing fields are unescaped ASCII. Tags, exact repository asset URLs,
versions, size, lowercase SHA and manifest identity must match. Selection uses
the native live admitted app inventory, not stale compiled versions. Current,
unsupported and USB-only rows stay visible without an enabled install action.

Existing firmware 1.0.0 is an 8 MiB merged USB image at flash offset zero. Its
outer record is immutable and can NEVER be downloaded as an OTA image. Absent
an explicit `ota` field the UI says USB install only. A future publisher can
add the following metadata to a *new* firmware record (example only):

```json
"ota": {
  "kind": "runtime-image",
  "runtime_version": "1.1.0",
  "layout": "riscrte-paired-16m-v1",
  "store_abi": 1,
  "asset": "riscrte-runtime-1.1.0.bin",
  "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-T-Watch-S3/releases/download/firmware-v1.1.0/riscrte-runtime-1.1.0.bin",
  "size": 123456,
  "sha256": "<64 lowercase hex characters>"
}
```

The OTA tag is the outer immutable firmware tag; runtime_version independently
uses strict numeric MAJOR.MINOR.PATCH. Layout, native store ABI and capacity must
match current native status. Native OTA changes only Runtime's native image and
clones the current store. It does not replace board configuration, grants or
drivers. This implementation does not modify a production release/index or
make an existing single-bank USB installation OTA-capable. Paired-bank bootstrap
requires the coordinated deployment prerequisite and physical qualification.

## Verification

`python scripts/test_portable_update.py` runs fake HTTPS/native-bank service
fault tests for both action-kind ELFs, and the production controller/renderer/
adapter with fake Wi-Fi, storage, navigation, alarm and sleep providers in two
touch orientations, plain C/C++ and ASan/UBSan. Fixtures use only synthetic data.
Native bank power-loss/SHA/path/ELF/import safety is tested separately by Runtime;
mock service admission is not evidence that native flash admission passed.

`NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python
scripts/build_portable_updates.py --rtc-utc-offset-seconds 28800 --wifi-instance
15 --alarm-client --navigation --full-frames` builds both providers and portable
app ELFs with strict import/export and structural-loader checks. Select
`--services-only` on the shared prerequisite branch, or `--app ota_update` /
`--app app_store` on each app branch. Generated build records include source
hashes and actual compiler; outputs are development artifacts, not a release.
The standalone builder does not link Watch-local sleep/navigation materializers
or define UPDATE_RETURN_APP. Actual Watch deployment must separately compile
PORTABLE_APP_SLEEP_LOCAL, its local navigation hook and UPDATE_RETURN_APP with
the existing Watch implementations. The real controller fixtures exercise those
hooks; standalone target ELF validation alone is not deployed sleep/return proof.

Run the normal legacy aggregate build, interaction fixtures and release-byte
comparison independently. Host and ELF checks do not qualify real TLS/radio,
physical power-loss behavior, display ergonomics or flash wear.

## Explicit paired-layout ABI handling (service 0.1.1)

The provider-only bank-store function table remains API v1. Its live status now
supplies the selected paired-layout ABI; the historical SDK constant `1` is not
an instruction to overwrite that value. Both app and Runtime transactions carry
`status.store_abi` and the fresh active-store digest to native admission.

For Runtime updates, layout, ABI and firmware capacity are rechecked immediately
before beginning, as well as during catalog classification. A stale/mismatched
selection causes no transaction. A new-layout ABI2 device can receive only a
matching catalog/firmware image; native Runtime still rejects wrong markers,
wrong journal/layout and changed app authority. This does not offer a partition
migration, install app-data on old hardware, authorize a new app or weaken the
old-layout rejection of ABI2 images. ABI1 and ABI2 service tests cover both kinds
of update, invalid status, mismatched catalogs and changes after classification.
