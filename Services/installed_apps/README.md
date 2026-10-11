# Installed app catalog

The selected `software.installed-apps` provider loads cached metadata without SD
access. Springboard submits its cached frame before starting copied asynchronous
SD discovery. Native admission checks capability requirements against the boot
installation envelope. Native image/import validation still runs on the actual
load snapshot; there is no whole-file checksum on touch or catalog lookup.

## Private storage

An optional boot `installation.namespace_range` object contains `first` and
`count` (1–128). Private `storage.app-data` or `storage.key-value` grant templates
must use `instance_id: 0`; the native broker substitutes the reserved namespace
for the catalog-assigned slot. The reserved range may not overlap static app,
shared-file, bound-provider-file or bound-provider-key authority. Provider-bound
and shared storage cannot be requested implicitly by an installed app.

Cache schema 3 stores the identity-to-slot ledger alongside compact manifests.
Removed identities remain as tombstones, preventing namespace reuse. The entire
cache is atomically replaced by a copied native worker before publishing native
policy. A cancelled pending write must be collected; a successful write can be
rehydrated on the next entry. A failed write publishes no new grants. Range
exhaustion denies new private identities without lending an existing namespace.
The bound `installed.apps` file must remain assigned to the same provider and
namespace across product updates. Native cohort admission preserves the range.

This is opt-in development source. Product boot/store composition, App Store SD
installation transactions, driver provisioning, and migration of existing static
app namespaces still need integration. No hardware qualification is claimed.

## App Store view

The optional `build_portable_updates.py --installed-catalog` selection builds
App Store 1.3.0. It displays cached installed SD entries alongside existing release
rows using the current scrolling renderer and controller. Installed rows cannot
invoke the bank installer. Rendering/get/status do not refresh or read SD. On
entry, the protected index is rehydrated; Springboard performs async media scans.
The existing release feed and update transactions remain available. SD package
installation/uninstall controls are a separate remaining integration step.

### 0.3.2 durable removal checkpoint

Cache schema 4 records a single removal intent and the reduced app inventory
**before** SD mutation or native withdrawal. Reboot can therefore render the
reduced cached inventory without opening SD. Cleanup reads only the selected
manifest, matches its exact ID/version, then submits version-3 async file
removal. It never deletes an image, private settings, a directory, or a
replacement identity/version at the same path. Missing manifests complete
idempotently. The journal is cleared only after checked cleanup; an unavailable
card retains the intent while permitting Home/USB handoff. Rediscovery follows
journal completion. Schemas 1–3 remain readable.

UBSan provider tests restore the durable pre-delete journal and the
post-delete/pre-clear journal across simulated process restarts, cover 1,000
pending-write polls, offline cancellation, and replacement-version protection.
This checkpoint supplies the provider transaction; App Store action wiring,
staged SD installation, legacy authority migration and SD-first provisioning
remain separate work. Existing release-update rows are unchanged.

App Store 1.3.1 now exposes removal through its existing two-choice confirmation
and scrolling controller. The full installed identity, version, launch path and
catalog generation are rechecked after confirmation. SD instance 9 is acquired
only for the explicit action and retained across pending cleanup. Ordinary
pending cancellation does not mark the invocation irrecoverably retained. The
release feed and bank-update API remain separate; removal uses no Wi-Fi, UTC or
restart. Host tests cover the actual controller confirmation, stale selection,
1,000 pending polls, checked asynchronous cancellation and all 38 existing
release-controller scenarios. The image and settings are deliberately retained.

0.3.3 also drains a pending journal-clear write after the SD session has ended.
Its regression holds 1,000 Cancel polls at that exact boundary and proves the
native cache operation is collected before release. A null borrowed SD pointer
must not bypass independent cache-write custody.

### 0.4.0 staged install/update transaction

App Store 1.4.0 lists cached staged descriptors separately from installed apps.
`Apps/<id>/install.json` contains `{manifest, size, sha256}` and selects an
immutable `image-<64 lowercase SHA256 hex>.elf` in the same directory. The complete
descriptor is limited to 4096 bytes; images retain the native 2 MiB limit.
`scripts/stage_sd_app.py` prepares this layout without creating manifest.json.
This local staging path does not enable an unconfigured X4 network release feed.

The provider rereads the selected descriptor and checks its cached size/digest,
ID and version, then checks the native capability/namespace transaction without
publishing it. The scoped worker validates the actual image once. Cache schema 5
persists the activation journal, previous manifest and namespace ledger while
keeping the old installed list. Only then is a synced staging manifest replaced
through the existing filesystem recovery transaction. Final cache publication
precedes native registry publication. The previous image and private settings
remain intact. Equal/older versions and nonmatching recovery manifests reject;
no user content is silently overwritten. Restart recovery validates the selected
image again for that interrupted admission, not for ordinary launch.

The staged list stores only bounded display/selection metadata in the protected
cache. Normal Springboard/App Store count/get calls do no SD work or image hashing.
A staged manifest scan shares the existing bounded background directory pass.
Cache-first startup remains SD-free. The install/update controller needs no radio,
UTC, restart or bank authority. Runtime 0.2.10 can opt legacy SD identities into dormant catalog authority.
Their original grants remain reserved for preserving settings while installation
and removal become catalog-controlled. Springboard 1.8.1 excludes those SD rows
from its compiled installed list and keeps their labels/icons as appearance only.
Unselected bootstrap identities remain protected.

Home 0.4.9 opts into the same catalog client after its first submitted frame.
It hydrates native metadata from the cache, then polls background discovery.
Pending cancellation preserves launch/sleep intent; the SD and catalog grants
are released before child entry or sleep. This closes the cold-boot shortcut
gap when legacy authority migration is selected. Actual resident adapter tests
cover first-frame ordering, 1000 pending progress/cancel polls, child handoff
and deferred sleep. No additional display/input rendering path is introduced.

Format-2 native settings use scalable immutable SD pages plus a verified atomic index. The copied worker runs every filesystem operation. Existing NVS migration, v1 compatibility, per-key journal replay and missing-media authority are native responsibilities. A new write does not disappear when an older checkpoint completes. Original private native values are retained only as a migration backup; after the durable SD anchor, they are never fallback defaults.
