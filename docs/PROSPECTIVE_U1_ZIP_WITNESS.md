# Prospective U1 ZIP witness — non-live

Button Remap 1.0.1 supplies one representative existing external application.
Its independent build reproduces the already published ELF: 3,812 bytes,
SHA-256 `46ff711c7a3cf9ca5ed2bab41088b57e0821dde6c5be0e3abb891c6a5f17f154`.
The optional witness packages that exact ELF and its existing integrity sidecar
using the existing upstream stored-ZIP packer and ordinary-manifest validator.
No app/SDK/payload/version changes or package-manager implementation are involved.

## Immutable tools and scope

The source is U1 `c80bdee158eb7ef98096c7b000cd92f339c7442b`, independently checked
green in [workflow 36835619954](https://github.com/michaelrolphone-cmyk/T5S3-Reader/actions/runs/36835619954)
and [workflow 36835620195](https://github.com/michaelrolphone-cmyk/T5S3-Reader/actions/runs/36835620195).
The newer moving U1 head is not used. `sdk/prospective-u1-tools.json` pins the
commit plus Git blob and SHA-256 identities of four existing Python modules.
They are read with `git show` into a temporary directory at execution, checked
before import, and removed afterward. No upstream runtime code is imported.
The release-index mutation CLI is never invoked: only its pure manifest
validation function is used. The witness itself performs no network operations.

Normal SDK compilation remains pinned to Reader `524e2cb3ea27311b062f3d7f182668c6912829fc`;
master payload provenance remains `1e0188c1ff0234dd33fe054c9a6fb4fde36596df`.
The witness requires agreement between current source/manifest blobs, build
evidence, sidecar, and immutable published ELF version/hash/size. It stages only
a schema-1 application manifest following the pinned upstream app builder shape,
including `min_runtime_api: 2` and mandatory capability conversion through the
upstream helper. This legacy app declares no capability requirements; that does
not prove availability of its firmware-host APIs on a target device.

## Running and verification

After the ordinary independent build (full build or `--id button_remap`):

```sh
python scripts/witness_u1_zip.py --reader /path/to/read-only/Reader
```

The checkout must already contain the exact pinned U1 commit. The script does
not fetch, checkout, or modify Reader. Output is isolated under
`build/prospective-u1/`, outside the normal `dist/apps` development artifact.
The dedicated CI workflow builds just this one external payload and checks out
upstream separately with credentials persistence disabled. It has only read
permissions and uploads a temporary 14-day **prospective** artifact. The normal
independent-build/parity workflow and its exact-byte gates remain unchanged.

Checks include upstream manifest validation, two identical packer outputs,
ZIP stored-entry inventory/CRC and exact entry bytes, payload corruption refusal,
and traversal-path refusal. Local result: 4,881 bytes, SHA-256
`3819cf9c31d148de24494c17ba7b5da6c177b83e0380e2d376c68ca11107280f`.
The JSON witness records external commit, source blobs, SDK/compiler, tools,
manifest, archive hash and explicit limits. Claim #9 and the PR record exact-head
CI, authorized merge and postmerge evidence.

## What this does not establish

This is future host packaging evidence for one application against one U1
snapshot. It does not certify the SDK for U1, exercise the production C++ ZIP
installer on hardware, test module activation, dependency resolution, resource-only
packages, upgrades/recovery, memory budgets or physical controls. No runtime
installation, deployment, flashing or device access occurred.

No release record, install catalog, URL, tag, live index, ownership switch or
publication is produced. The ZIP is an unpublished test wrapper around the
unchanged released payload; it must not be published under an existing release
identity merely because host validation passed. `parity_ready_count` remains zero.
Current-master parity and the ten explained historical ELF profile differences
remain separate. New formats or moving U1 candidates require fresh review.
