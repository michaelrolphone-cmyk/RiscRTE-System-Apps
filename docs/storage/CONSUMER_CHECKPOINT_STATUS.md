# Consumer source checkpoint, 2026-10-15

This is preserved work in progress, not a release or a declaration that storage
migration is complete. Scope remains all storage consumers everywhere in RiscRTE.
The file lists below record changed source, not a boundary on required work.

## Published and tested before this branch

`2da0f5a1cd7ff16c77d7da8a07153b86ca5ac05c` on
`work/unified-default-capabilities`:

- `Apps/usb_sd_transfer.c`: neutral storage wording. The application already
  acquires the generic `usb.device.msc` provider and performs no SD operations.
  Transport implementation and exclusive-custody semantics were preserved.
- `scripts/test_usb_transfer.py`: preinclude the actual PaperFrame ABI needed
  by the shared adapter. The actual app+adapter suite passed 31 normal and
  31 ASan/UBSan lifecycle/protocol cases against the current MSC driver SDK.
- Optional-storage scope document, inventory scanner and source-evidence JSON.
  Evidence covers available checkouts; it is not migration closure.

## Independently testable transitional helper

- `lib/PortableApps/include/PortableFilesVolume.h`
- `test/remote_files/GenericFilesFixture.h`
- `test/remote_files/generic_files_view_test.cpp`
- `scripts/test_generic_files_view.sh`

The helper forwards only generic files operations to existing shared algorithms.
Strict C11/C++17 header compilation and normal/ASan/UBSan tests cover CRUD,
exclusive create/abort, directory cursors, EOF versus I/O errors, denied
operations, unavailable recovery and terminal failed-close ownership.

It is transitional: the legacy algorithm type remains internal. Cached readiness
or observation is not proof that a backend is currently available. Every call's
result remains authoritative. Tests above do not qualify all clients, partial
I/O plus errors, package-service extensions or asynchronous service semantics.
No code may infer a clean-release permission from this checkpoint.

Current implementation choices: four simultaneous tracked handles per view,
16 discovered namespace slots in the browser, 512-byte paths, 128-byte names,
4096-byte transfers and 64-byte labels. The last four are inherited from the
current generic ABI. These are implementation limits, not requirements of a
storage mechanism. The namespace snapshot is cached for the invocation.
Atomic replacement/recovery is not implemented by this helper. Its legacy
compatibility callbacks explicitly reject those unsupported operations.

## Preserved but not behaviorally qualified consumer edits

- `Apps/file_browser_portable.inc`: catalog discovery and namespace acquisition.
- `Apps/file_browser_operations.inc`: namespace switch acquisition.
- `Apps/file_browser_sharing.inc`, `Apps/file_browser_direct.inc`: selected
  logical namespace passed to sharing.
- `Apps/native/file_browser.json`: generic files/catalog requirements.
- `lib/PortableApps/include/PortableFileSharing.h`,
  `lib/RemoteFiles/PortableFileSharing.cpp`: generic file namespace acquisition.

The browser translation unit compiles against the legacy test fixture with a
fixture-only capability macro; the sharing controller compiles with strict C++17.
Existing behavioral fixtures and target recipes still encode old capability
selection and are not migrated or claimed passing. Existing path-based open
handoff branches also remain. This source must not replace qualified packages.
The actual service's asynchronous/package extensions cannot be replaced by a
CRUD-only view without a corresponding provider implementation.

## Dependency-ordered next units

1. Establish the accepted optional generic contract and provider status/custody
   semantics, with storage-absent behavior verified.
2. Qualify the client helper's result propagation and ownership against those
   finalized semantics. Remove transitional lifecycle interpretation wherever
   it can misrepresent operation outcomes.
3. Complete one consumer integration and its production fixtures/build recipe
   against a real provisioned generic provider; verify before proceeding.
4. Continue independently testable integration units until all consumers and
   their transitive storage dependencies are migrated everywhere.
5. Compose/rebuild affected products from verified source, preserving features
   and data, and request physical-device validation for the actual new package.

No additional rewrite or consumer expansion is started from this checkpoint
while its prerequisite generic contract/provider work is still being completed.
