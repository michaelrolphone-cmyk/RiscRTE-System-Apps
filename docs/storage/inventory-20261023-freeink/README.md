# Nested Free-Ink source coverage, 2026-10-23

This additive inventory closes one previously unexpanded source boundary, not the
all-applications storage migration task. Existing 2026-10-22 and Garden reports
remain unchanged. It does not update any submodule, application, driver or runtime.

## Immutable provenance

`provenance.json` records the two Git mode-160000 edges and `.gitmodules` blob
identities that select this source, rather than following current upstream main:

1. [RiscRTE-Productivity bb8d0d9](https://github.com/michaelrolphone-cmyk/RiscRTE-Productivity/tree/bb8d0d99b3a857103d2b01e6b4c756b913aa14d3)
   selects `third_party/crosspoint` at `3607ea7bb95d65b4cf3edf1aa89b2b0970985793`.
2. [CrossPoint 3607ea7](https://github.com/crosspoint-reader/crosspoint-reader/tree/3607ea7bb95d65b4cf3edf1aa89b2b0970985793)
   selects `freeink-sdk` at `425d200a8ea447326b4b9696e4e47dc84ad9d7f6`.
3. [Free-Ink 425d200](https://github.com/Free-Ink/freeink-sdk/tree/425d200a8ea447326b4b9696e4e47dc84ad9d7f6)
   has tree `31d7c76148c12e2111ea9d159e4271fa87075b98`.

CI checks both gitlink identities, both `.gitmodules` blob identities and URLs,
and the SDK tree before regenerating the report. It never initializes submodules
or executes upstream SDK/build scripts.

## Coverage and migration state

The unchanged immutable-blob scanner accounts for 891 tracked non-directory
entries: 769 scanned, 121 excluded by extension, and one undescended gitlink.
There are 83 candidate-bearing files, not 83 applications or proven defects.
Path-based roles include headers, shared code, configuration, generated/assets,
tests, third-party code and host tooling. These hints are not build selection or
semantic proof. `evidence/index.json` binds the manifest and report by SHA-256;
individual file records include Git blob, content hash, immutable URL and hit lines.

Manual source inspection establishes a concrete dependency route:

- [CrossPoint platformio.ini line 144](https://github.com/crosspoint-reader/crosspoint-reader/blob/3607ea7bb95d65b4cf3edf1aa89b2b0970985793/platformio.ini#L144)
  selects SDCardManager from the nested SDK. Its
  [HalStorage adapter](https://github.com/crosspoint-reader/crosspoint-reader/blob/3607ea7bb95d65b4cf3edf1aa89b2b0970985793/lib/hal/HalStorage.cpp#L36-L69)
  delegates mount/readiness and locked sleep shutdown, and conditionally detaches
  the filesystem for USB ownership, attempting remount if USB startup fails.
- [BookStorage.h](https://github.com/Free-Ink/freeink-sdk/blob/425d200a8ea447326b4b9696e4e47dc84ad9d7f6/libs/book/FreeInkBook/include/BookStorage.h#L5-L64)
  defines app-supplied random-access input and streaming cache interfaces. It
  specifies short reads/errors, interrupted-write expectations and optional
  read-back during a write. The scanner's SD comment hit is not proof of a
  physical API call; this contract classification comes from manual inspection.
- [SDCardManager.h](https://github.com/Free-Ink/freeink-sdk/blob/425d200a8ea447326b4b9696e4e47dc84ad9d7f6/libs/hardware/SDCardManager/include/SDCardManager.h#L45-L107)
  exposes filesystem operations and raw-device custody. Streaming reads accept
  positive short writes but may emit a prefix on failure. `replaceFile` removes
  an existing destination before rename: no stronger atomic replacement guarantee
  is established. Filesystem access must stop during USB custody, and remount is
  required after release. Shutdown requires file users to have stopped.
- [UsbMassStorage.h](https://github.com/Free-Ink/freeink-sdk/blob/425d200a8ea447326b4b9696e4e47dc84ad9d7f6/libs/hardware/UsbMassStorage/include/UsbMassStorage.h#L3-L65)
  separately documents ownership and conditional USB capability; host suspend is
  only a hint, not proof of physical disconnect.

Migration state: identified upstream dependency interfaces, not migrated or
qualified in this increment. Inventory presence does not establish that every SDK
component is selected in a RiscRTE binary. A future accepted adapter change must
trace the selected build and preserve these contracts with executable tests.
No hardware, USB-host behavior, sleep/wake, flashing, or app migration was tested.

## Explicit remaining boundary

`libs/assets/Icons/lucide` remains an unresolved mode-160000 gitlink at
`c81680e066f45b640743ca78ae36cdedda3f0318`. Its contents are not inspected and
are not declared storage-free. Extension exclusions and heuristic blind spots
remain. This increment does not establish exhaustive transitive coverage or
complete the all-applications storage requirement. Next: reconcile remaining
source boundaries and selected consumer adapters against the existing inventory.

## Reproduce

Provide an exact Git object checkout named `freeink-sdk` beneath the checkout root:

```sh
python scripts/audit_storage_refs.py \
  --manifest docs/storage/inventory-20261023-freeink/sources.json \
  --checkout-root /path/to/source-checkouts --output-dir /tmp/freeink-report
diff -ru docs/storage/inventory-20261023-freeink/evidence /tmp/freeink-report
python -m unittest discover -s tests -p 'test_storage*inventory.py' -v
```

The workflow also fetches both pinned parents and verifies the provenance chain.
Local regressions validate report integrity and scope; they do not execute SDK
code or substitute for target qualification.
