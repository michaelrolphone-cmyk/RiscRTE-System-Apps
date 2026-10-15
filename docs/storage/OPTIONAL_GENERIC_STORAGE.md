# Optional, backend-neutral storage

Storage is an optional capability. A RiscRTE project with no storage does not
provision or load a file provider. Applications which use storage consume a
logical service contract; a hardware or software implementation supplies it.
The same consumer must not select SPIFFS, SD, SCSI, USB disks or cloud storage.

This applies everywhere in RiscRTE: all applications, shared consumers, libraries,
services and every device class. No application list defines or bounds the
migration. Provisioned namespace IDs and labels are opaque logical identities.
No storage is silently substituted after failure.

## Contract and reusable code

- `storage.files@1`: opaque handles, open/close/read/write/seek/info/sync,
  truncate, stat, mkdir, rename/remove, directory enumeration and cursors.
- `storage.files.catalog@1`: read-only enumeration of authorized namespaces.
  Discovery does not activate, mount or probe a provider.
- `PortableFilesVolume.h`: internal compatibility view for existing shared
  browser and protocol algorithms. Its external dependency is only files@1.
  It never forwards old mount, refresh, media-export or power operations.
  Historical algorithm types are an implementation detail, not a second
  application grant or physical provider dependency.
- USB mass storage is a block protocol, not file CRUD. The application acquires
  `usb.device.msc`; its native provider owns the provisioned block-export
  dependency, USB hardware and exclusive custody. Generic files must report
  unavailable while a block export owns the same underlying medium.

Only the provisioning-file bootstrap may couple directly to SPIFFS. Physical
storage code belongs behind native provider contracts. A historical API name
alone is not evidence of a hardware coupling: inspect implementation, supplied
paths, lifecycle calls and grant selection together.

## Scope and evidence

Every application and shared storage consumer must use a mechanism-independent
contract. Trace direct calls and transitive helper dependencies, asset access,
persistence, file handoffs, service callbacks and product composition everywhere.
Do not treat an inventory's matching files, repositories or current checkouts as
a bounded migration requirement. File-level inventory is evidence only.

The companion JSON records observed source lines and hashes from ten available
working-source checkouts and their base commit IDs. It is not proof that every
hit is a defect, that all source repositories were available, or that their
latest default branches were inspected. Re-run it against final pinned sources
and expand evidence coverage wherever more storage consumers exist.

Preserve sound higher-level semantic contracts while removing mechanism
selection behind them. A generic CRUD view is insufficient where an existing
service requires asynchronous cursors, package installation/download, atomic
publication or other stronger behavior. Such service contracts need an explicit
backend-neutral implementation; passing a smaller interface and disabling the
behavior is not migration. Provider-owned transport, custody and lifecycle
remain separate from application file operations.

All product manifests, grants, source pins and build recipes must provision
logical namespaces only where requested. A target ELF is not product integration
or physical-device qualification. Storage-free compositions stay storage-free.
The provisioning-file bootstrap is the sole explicit direct-SPIFFS exception.

## Acceptance gates

- Storage-absent Runtime/project builds and apps continue to run.
- At least two implementations serve the same unmodified consumer contract.
- Test actual consumer browse/read/copy/create/rename/delete, interrupted
  transfers, unavailable services and retained ownership. Errors are not EOF
  or missing-file results. Never retry uncertain mutation/close operations.
- Test USB export custody and reavailability after checked completion.
- Test all migrated persistence, file and asset consumers across all supported
  device classes and compositions. Do not infer hardware success from host
  tests or reuse a firmware package built from older source.
- Publish incremental source checkpoints and verify exact pushed commits.
  Whole-platform completion requires the dependency map to be closed, not
  a selected subset of consumers.

Status: migration in progress. This document deliberately records remaining
legacy branches and integration work; it does not declare architecture closure.
