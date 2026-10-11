# Installed catalog model (not yet connected to Springboard)

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
