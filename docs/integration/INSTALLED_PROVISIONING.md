
## Catalog 0.7 / SD settings checkpoint

Native `provider_catalog` descriptors now select only missing exact capability/API
providers, expand their dependencies and reject ambiguous/missing matches before
Runtime validates the whole graph. Explicit selected `providers` remain supported
for updates. Persisted intent contains the resolved selection, so restart does not
repeat a mutable catalog choice.

When the native profile explicitly selects SD private settings, the catalog
restores and checkpoints them through copied worker commands. Cache metadata is
still immediately available; cold private-settings apps remain unavailable until
restoration. Dirty native journals survive interrupted checkpoints, removal and
export ownership. No settings or directory I/O is added to touch/launch checks.
The Runtime model's 16-key quota and KV@1 value limits apply; AppData and existing
static/shared settings retain their original storage.

Validation: real catalog/worker fixture covers 65 durable-state interruption cuts,
110 provisioning cuts, export exclusion and removal retry. Native Runtime tests
execute a newly registered SD app twice through the private KV broker and enforce
cold readiness. Target Xtensa catalog links successfully. Hardware qualification
and final product composition remain separate.

Catalog 0.7.1 recognizes Runtime 0.2.17's legacy-native settings state. Delivered
namespaces that already exist in NVS keep their original backend and key count;
they are available after restoration and are never checkpointed into the new
16-key SD snapshot. New namespaces still select SD-first settings. The actual
Runtime fixture preserves 32 existing values and grows the namespace to 65 keys.
