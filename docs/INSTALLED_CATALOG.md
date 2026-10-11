# Installed application catalog

Current integration: Home and Springboard hydrate from the internal protected
cache and scan through the copied SD worker. App Store selects staged native
packages, downloads declared missing provider images and durably activates the
graph before publishing the app. See the catalog 0.6.0 section below for limits.
SD-first private settings and an online native package feed remain unfinished.

## Earlier model checkpoints

The following sections preserve the progression of the original implementation;
statements about missing integration describe those earlier checkpoints.

`lib/InstalledCatalog` provides a bounded, allocation-free metadata model for
applications and drivers. The owner allocates its object in PSRAM and serializes
access. A successful complete scan replaces one medium's entries atomically;
internal records survive an SD scan. Enumeration/admission/close failures abort
publication. Media absence disables launch eligibility without deleting labels.
Duplicate identities or paths invalidate the candidate. Updates invalidate queued
touch selections using a generation plus copied package ID; a stale index never
silently selects another app. Generation exhaustion fails launch closed.

The versioned binary codec uses explicit little-endian fields and bounded strings,
not native struct padding. It binds the cache to an exact product policy/cohort key
and detects accidental metadata corruption. Its checksum is not an authenticity
mechanism and never processes executable bytes. Cached records restore labels,
versions and paths, but NOT admission authority or media readiness. A native
coordinator must rehydrate policies and confirm volume readiness before launch.

`InstalledCatalogStore` now persists `installed.idx` through the existing
`storage.app-data@1` atomic replacement API. It reuses that backend's sync,
readback, revision and crash-selection guarantees instead of adding a journal.
Load reads only the cache, never SD. Stale revisions, capacity failures,
COMMIT_UNKNOWN and RETAINED propagate unchanged; corruption preserves the current
snapshot. Scratch memory and the live namespace grant belong to the caller.
The adapter is tested against fault-injected backend callbacks; an X4 catalog
provider still needs its explicit namespace grant and owner-safe scheduling.

Integration steps:

1. Expose snapshots from an explicitly authorized installed-catalog provider.
   Keep `PortableApps`' generated catalog fallback for older products.
2. Bind existing Springboard count/get/launch callbacks to a stable snapshot.
   Copy an identity/generation at press; resolve it at release. Retain all current
   scrolling, input decoupling, rendering and launch-failure behavior.
3. Schedule the incremental Runtime metadata reader after first frame, outside
   touch/display polling. Its `Complete` state, checked closes and coordinator
   admission are prerequisites for `finish(true)`. A time slice alone cannot make
   a blocking storage backend asynchronous; the provider needs an owner-safe
   storage job integration before enabling this on X4.
4. App Store release catalog remains remote availability. Successful package
   installation publishes bytes, manifest and installation revision last, then
   admits and commits the installed generation. Changing a package's image without
   its installation revision is not a supported cache update protocol.
5. Driver entries are installation inventory, never launcher cells or permission
   to start hardware. Activate a complete admitted graph at a settled boundary.

Evidence: `bash scripts/test_installed_catalog.sh` exercises atomic publication,
media absence, updated/removed packages, stale selections, malformed paths,
policy-key mismatch, every single-byte cache corruption and every truncation.
UBSan: `SANITIZE=1 SANITIZERS=undefined bash scripts/test_installed_catalog.sh`.
ASan on the current macOS toolchain deadlocks in sanitizer initialization before
main; it is not counted as passed. Production launcher behavior is unchanged in
this checkpoint. No device timing or hardware operation has been qualified.

The catalog C source also compiles with the installed Xtensa ESP32-S3 GCC under
C11 with `-Wall -Wextra -Werror`. Full-capacity round-trip and overflow rollback
are covered by the host tests. This is not a linked Springboard target build.

Persistence adapter tests cover empty storage, save/reload, stale revision, no space, retained cleanup, ambiguous commit followed by reload, and corrupted saved bytes.

## Experimental provider integration

`Services/installed_apps` now binds the native installation API and one existing
bound app-data file. Startup refresh rehydrates metadata from that internal
cache without SD calls. Complete scans stage candidates, persist the cache and
then invoke one native commit. Failed scans and failed cache writes preserve the
published view. Uninstall checks native mutability before renaming a manifest;
protected boot identities cannot be renamed. A native failure after rename
attempts rollback and fences the provider if rollback fails.

Host integration regression:

```
RUNTIME_REPO=/path/to/RiscRTE bash scripts/test_installed_apps_service.sh
RUNTIME_REPO=/path/to/RiscRTE SANITIZE=1 bash scripts/test_installed_apps_service.sh
python3 scripts/build_installed_apps.py --runtime-repo /path/to/RiscRTE --output-dir /tmp/installed-apps
```

The provider compiles and links for Xtensa with only the expected libc imports.
Its receipt records source hashes, repository state, compiler commands and the
ELF hash. This proves target linking only. The current prototype's explicit
`step` still performs synchronous I/O and MUST NOT be called from the production
UI. An attempted panel-settled idle integration was removed: it violates the
requirement that discovery progress independently of physical display settling.
Existing Springboard, input, render and launch code remains unchanged.

Required next work is an owner-safe asynchronous SD contract (including lazy
activation), then the minimal shared adapter binding and a background-cache-to-
launch acceptance test. App Store installation UI, package validation receipts,
driver provisioning/activation, and SD-first settings/AppData migration remain
separate incomplete integration work. The cached metadata UI is not yet shipped.

Catalog-model regressions also cover internal/SD copies of the same identity,
SD preference with internal fallback, repair after corrupt-cache revision reads,
and the smallest valid empty cache encoding.

### Durable provider graph replay (catalog 0.5.0)

Cache schema 6 adds a bounded `providers` array of previously admitted ordinary
provider manifests, immutable SD image paths and selected instance IDs. A cold
start replays the complete batch through the runtime installation authority
before registering any dependent apps. Replay does not open the SD card or hash
images; physical availability and retained media custody remain runtime launch
checks. Busy graph publication is retried without publishing dependent apps;
rejected graphs leave app registration untouched. Every subsequent catalog save
preserves the graph. Schemas 1–5 and the original app-only installation API prefix
remain supported. Nonempty graphs require the explicitly enabled provider API.

The host catalog regression exercises graph-before-app ordering, absent-SD
restart, busy retries, rejected graph metadata and old-prefix compatibility. This
checkpoint adds replay only; the App Store provider download transaction and
SD-first private settings are still separate integration work.


## Catalog 0.6.0: selected dependency installation

A staged `Apps/<id>/install.json` may include a `providers` array describing the
selected native dependency closure. Each entry contains `manifest`, `instance`,
`size`, lowercase `sha256` and an HTTPS `url`. The native ordinary manifest uses
an immutable `image-<sha256>.elf` file name. Instances are explicit selected board
bindings; neither package metadata nor a URL grants additional native authority.
The complete proposed graph is checked against the firmware-selected envelope.
The current bound is eight dynamic providers and a 4096-byte selected descriptor.

Before any download mutation the service atomically saves the selected plan and
private namespace in the protected catalog cache. It reuses `SdPackageDownload`
for copied-worker image writes, digest/ELF admission, replacement and close.
Only after every selected image is admitted does it persist the full next graph,
then publish it through native transactional admission. The app subsequently uses
the existing staged manifest transaction. Cached graph replay runs before app
registration. Installation/recovery validates executable bytes; input handling,
cached rendering and launch do not repeat whole-file digest checks.

App Store 1.5.0 keeps the SD grant while connecting saved Wi-Fi when requested by
the service. HTTPS remains a provider-private dependency; the app supplies only
the validated UTC time. Cancel drains the worker and HTTP before releasing media
or radio ownership. Background Home recovery does not open Wi-Fi. App Store can
resume the matching cached selection. Removed apps retain their settings and
installed dependencies; this transaction does not garbage-collect shared drivers.

Validation: actual service/coordinator/worker fixture, 110 interrupted owner ticks
with cancellation and restart, missing-media retry, cache-before-graph ordering,
missing-image HTTPS, zero-SD cache replay, offline recovery from admitted bytes,
plus 38 prior App Store controller scenarios and dependency-network/cancel cases.
The service target-links for Xtensa using the pinned 8.4.0 compiler. Native runtime
ELF launch is separately exercised by the runtime `catalog-providers` fixture.
Boundary doubles in the service tests are not hardware execution.

Remaining product work: online native catalog publication/resolution, general
board-specific dependency selection, SD-first private settings, exact combined
Files bridge/final cohort qualification, and hardware testing. Legacy T5 catalog
records do not automatically become native ordinary providers. This is not a
published firmware release or a claim that the complete provisioner is finished.
