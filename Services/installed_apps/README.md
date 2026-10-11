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
