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

### 0.9.0 explicitly selected shared settings

The append-only Runtime shared-settings suffix selects namespaces explicitly from
boot policy. Shared slots follow the private range; the coordinator processes
them before private package settings or discovery. Owners are `shared-%08x` and
SD checkpoints live beneath `/RTECFG/%08X`. Existing private paths and namespace
capacity keep their original meaning. Older Runtime tables remain private-only.

The append-only catalog `settings_pending` callback is RAM-only. Home and
Springboard poll it after their initial requested refresh finishes, acquire the
SD grant only for pending maintenance, and drain cancellation before USB/Home
handoff. Native KV acceptance establishes native-journal durability; Wi-Fi Save
does not claim that the SD checkpoint is already confirmed.

Files and WebDAV reserve every RTECFG path component, case-insensitively, including
trailing dot/space aliases. Listing, direct open, mutations, copy sources and
destinations, recursive traversal, and parent-folder moves are covered. This is
UI/protocol protection, not encryption or a secure credential vault; physical SD
access can read the checkpoint. No product profile is enabled by this change.

The build receipt records `shared_settings_sd`, `shared_settings_root` and
`idle_settings_maintenance` for explicit composition checks. The shared flag is
false when built with an older Runtime SDK.

### 0.9.1 application image downloads

An optional HTTPS `url` in the existing selected `install.json` descriptor now
allows the app ELF to be absent. The catalog persists the resolved descriptor,
URL, expected digest and namespace before starting any download mutation. It
uses the existing copied-worker `SdPackageDownload` transaction for both
providers and the application. Only fully validated immutable images proceed to
the existing activation journal and manifest publication. Network ownership is
requested through the existing App Store capability flow; cold Home recovery
never connects Wi-Fi itself. Recovery uses the protected plan even if the public
descriptor disappears. A matching admitted image permits offline recovery.

`stage_sd_app.py --url HTTPS_URL` emits the descriptor without copying the image;
the supplied local ELF is still checked and hashed to describe exact bytes.
Without `--url`, existing local staging is unchanged. This is selected package
installation, not an online native catalog feed or automatic core inventory.

New host coverage adds 160 interruption/restart points through the real catalog
coordinator and worker, descriptor removal, missing-media retry, and catalog
refresh. Existing 110 provider provisioning cuts, 65 private settings cuts,
153 paged settings cuts, and durable uninstall tests remain passing. HTTP, ELF
admission and media are boundary fixtures; no hardware qualification is claimed.

Still separate: live placement across SD/internal volumes, a configured native
core-package feed, and product activation. The selected .111 installation API
binds one volume (SD 9). Its app-data capability grants only named complete files;
it is not an internal executable directory. The offline placement planner and
native boot-time full-store installer cannot safely be treated as a live fallback
callback. Failed or retained SD custody never triggers a storage switch.
