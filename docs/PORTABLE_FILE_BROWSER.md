# NOVA-7 File Browser (portable 1.5.8)

The portable profile of `Apps/file_browser.c` uses ordinary runtime capabilities.
A retaining, monochrome portrait display selects the shared paper presentation;
the Watch retains its existing two-row color presentation and read-only
`storage.installed-files@1` authority. The legacy Reader profile and its published
1.3.2 ELF remain byte-for-byte unchanged.

## Paper presentation

- Logical 480×800 with six 88-pixel rows, a static white/black layout, genuine
  licensed Font Awesome glyphs, and the shared NOVA typography.
- Native X4 800×480 MONO1 output uses `--display-rotation 90`. Touch stays in the
  provider's logical 480×800 space. No board-name checks select the UI.
- Natural directory-first ordering uses bounded streaming pages, with no
  arbitrary folder inventory cutoff. Stable natural ties preserve every item.
  Previous/Next and completed vertical swipes navigate pages without animation.
- Options provide case-insensitive filename filtering, refresh, hidden files and
  root navigation. The paper keyboard exposes all 95 printable ASCII keys,
  preserves literal case, applies on Done and discards on Cancel/Back.
- Text/hex previews preserve explicit byte ranges and represent control/non-ASCII
  bytes with safe placeholders. They are byte previews, not document decoders.
- Each writable-volume row has its own ellipsis action target, including folders.
  The file action menu pages through Open, Open with, Preview, Rename, Move, Copy,
  and Delete. Watch's installed executable store does not expose these actions.

## Reader operations on an admitted writable volume

The workflows are adapted from `T5S3-Reader` commit
`34d8e694d89a1e72d8854403d8592c289fae3ddc`, `Apps/file_browser.c`.
The portable copy of `RiscStorageVolumeV1.h` now uses the canonical Runtime
header, including the reserved sleep suffix required by app-data exports;
`lib/PortableApps/SOURCES.json` records its immutable origin and content hash.
`risc_storage_volume_extension()` validates version and full extension size
before rename or checked directory operations are accessed. The legacy Reader
SDK snapshot is retained separately to preserve exact release bytes.

- Rename edits a draft name. Empty names, `.`, `..`, path separators, controls,
  overlong paths and existing destinations are rejected. Same-name Done is a no-op.
- Move selects a destination folder on the same volume. A folder cannot move
  into itself or its descendants. Provider rename rejects existing destinations.
- Copy selects a destination folder, on the current volume or an explicitly
  configured secondary volume. Directory copy is unsupported, matching Reader.
  Files use exclusive creation, bounded 4096-byte buffers, partial read/write
  loops, scheduler yields and exact byte counts. Transfer failure aborts the new file;
  existing destination files are never overwritten. A source-size recheck catches
  growth/shrinkage, but does not promise snapshot consistency against same-size
  concurrent edits.
- Delete requires an explicit Cancel/Delete confirmation, with Cancel initially
  selected. Folder deletion includes hidden descendants, uses bounded path memory
  and closes directory handles before mutations. Failure stops immediately and
  may leave a partially deleted tree; deletion is not a recoverable Trash action.
- Failed file/directory close retains the owning grant and original commit/abort
  intent, blocks further storage operations, and provides Back retry, except for
  the exact tagged export contract below. A failed source close aborts the
  destination. Failed grant releases are retried without releasing already
  released grants again. Confirmed cleanup precedes return.
  Destination Cancel also retries retained closes before restoring the source
  volume. Native resident-custody failures instead retain the invocation and
  stop further provider calls; Back cannot override that custody fence.

## Explicit app-data export volume

`storage.app-data.export@1` projects only the deployment's configured logical
names. The browser uses its ordinary volume prefix for listing, snapshot reads,
and exclusive copies. It does not discover namespaces, use native paths, invoke
revision replacement directly, or add remove/rename/power/sleep authority.
The copied `RiscAppDataExportV1.h` and `RiscAppDataV1.h` are byte-identical Runtime
SDK files with provenance in `lib/PortableApps/SOURCES.json`.

Failed commits are handled only after the full export size, tag, version and
`write_status` callback have been checked. A known negative status other than
CONTEXT or RETAINED permits `file_close(false)` to release the transient writer.
The failed copy remains failed even when this cleanup succeeds. NO_SPACE can
therefore be followed by a new, explicit copy after the cause has been resolved.
Cleanup does not replay replacement or silently retry the copy.

COMMIT_UNKNOWN means publication may have happened. The browser says
"Copy publication unknown. Reload destination." Cancellation releases memory
only and never deletes a possibly published file. That warning is preserved
while cleanup is retained and when leaving the destination picker. Failed
cancellation retains the handle and grant with cancellation intent; Back retries
only cleanup. Unknown statuses, PENDING, CONTEXT, RETAINED, missing callbacks,
short/wrong-tag/wrong-version interfaces and legacy volumes retain their existing
checked-close behavior and original intent.

This optional build command selects the existing capability parameter; it does
not grant the capability or change a product's deployment:

```sh
python scripts/build_portable_file_browser.py \
  --storage-capability storage.app-data.export \
  --return-app springboard.elf \
  --output-dir dist/portable/file-browser-appdata-export
```

The Runtime export capability uses instance zero. To retain the existing Watch
installed-files view and its grant, configure export as the second view instead:

```sh
python scripts/build_portable_file_browser.py \
  --secondary-storage-capability storage.app-data.export \
  --secondary-storage-instance 0 \
  --return-app springboard.elf
```

This emits both requirements without changing existing namespace owners. The
product must explicitly admit both grants and the exact export map. An explicitly
configured secondary `storage.volume` can also supply files to an export-primary
profile. Provisioning, product permissions, target builds, publishing
and physical-device behavior are outside this host-tested change.

## File handlers and native applications

`--file-handlers` enables the canonical Reader `T5FileOpenApi.h` table as
`file.open@1`, acquired through `risc_runtime_get_api`. It does not introduce a
new runtime import or storage ABI. The service receives bounded absolute Reader
VFS paths such as `/sd/Books/document.txt`. Only its declared handlers are shown,
with bounded six-row pages (Reader's 128-handler cap). Open dispatches a single
handler directly; Open with always presents the choice. A successful
`open_request(path, app_id, cookie)` is terminal: the browser cleans up and returns
from `app_main` without issuing another launch. Matching return/failure results
are consumed on startup. Folder/filter selection is invocation-local; a fresh
launch starts at the volume root.

A missing/ungranted/invalid broker and unsupported system-reader handler have
explicit status screens. The app does not invent associations or claim a
platform service is deployed. The X4 Runtime supplies the broker and file-argument
handoff. The selected X4 bundle currently has no application declaring
`supported_file_types`, so Open truthfully reports no declared handler. A receiver
in the Runtime test fixture demonstrates handoff; it is not added to deployment.

An SD `.elf` is not an installed boot-store application. Open tells the user to
install/admit it first. The browser never strips `/` from an arbitrary SD path
and passes it to `request_launch`. That runtime call resolves the immutable
configured boot store, not the writable SD volume.

## Volume selection and build profiles

Watch defaults are unchanged: `storage.installed-files@1`, instance 0, returning
to `springboard.elf`. The X4 SD profile uses `storage.volume@1`, returning to
`springboard.elf`. The clock home occupies `default.elf`; the launcher alone
returns there. Instance 0 requires a uniquely authorized provider. The selected
native X4 deployment explicitly acquires `storage.volume@1` instance 9. Runtime's
installed-files service exists only at instance 0; the builder rejects a nonzero
installed-files selection before compilation.

A secondary volume is optional and is not composed by this app. Only an
explicit `--secondary-storage-instance` adds a selector. Its capability defaults
to `storage.volume`; `--secondary-storage-capability` selects another explicitly
admitted volume-shaped capability. Secondary `storage.volume` retains its
nonzero-instance requirement; a different capability can use unique instance 0.
Two providers of the same capability require distinct nonzero primary/secondary
instance IDs, both admitted
by the boot profile. There is no registry-order fallback and no guessed hardware
ID. Switch volume / destination VOL report absence or denied acquisition.

```sh
ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_file_browser.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_paper.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_operations.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_appdata_export.py

# Existing Watch profile
python scripts/build_portable_file_browser.py --alarm-client --navigation

# X4 paper / SD profile
python scripts/build_portable_file_browser.py --display-rotation 90 \
  --storage-capability storage.volume --return-app springboard.elf \
  --output-dir dist/portable/file-browser-paper
```

Use `--file-handlers` only when the deployment admits `file.open@1`. A configured
secondary profile can add `--storage-instance <primary-id>` and
`--secondary-storage-instance <secondary-id>`; IDs belong to the deployment.

The export host fixture includes the actual browser/controller and adapter. It
checks normal exclusive copy, collision, NO_SPACE cleanup and fresh explicit
retry, both possible COMMIT_UNKNOWN publication outcomes, cancellation failure
retention, no replacement replay, and extension/status guards in normal and
ASan/UBSan builds. The existing operations fixture checks legacy close retry.
`detect_leaks=0` is needed only on ptrace-based executors where LeakSanitizer
cannot run; address and undefined-behavior checks remain enabled.

The builder validates its selectors, checks Xtensa ELF32 ET_DYN, runs the real
structural loader validator, enforces exact imports/exports, records every
implementation/header/font source hash and bundles the glyph/font licenses.
CI builds Watch, X4 and a fake-instance integration-contract profile. The latter
is compile evidence, not a deployable hardware configuration.

See [paper file-browser evidence](nova/FILE_BROWSER_EVIDENCE.md) for fixture
coverage and rendered screens. Hardware touch/media, USB-host enumeration and
physical e-paper qualification remain unclaimed. No hardware has been flashed.

## Explicit color-only deployment

`--rgb-only` selects the existing compact RGB565 interface at compile time and
omits the unreachable monochrome presentation. It changes no volume operations,
capability grants, storage callbacks or cleanup behavior. The default remains
display-selected. Color file actions, destination selection, cancellation,
rename keyboard, deletion confirmation and notices use the same bounded shared
controller as paper. A declared second volume stays reachable from Options even
when the initial installed-files view is read-only. Closing and reacquiring keeps
the exact configured capability/instance selector.

`--read-copy-only` is a separate explicit operation profile for deployments where
neither selected volume supports delete, rename, mkdir or file handlers. It
retains enumeration, filter, paging, details, preview, secondary selection and
exclusive copy with the same checked failure/cleanup contract. The profile
rejects capability tables advertising remove/rename/mkdir; the builder refuses
it alongside declared file handlers. Default builds retain all operations.
This is not a substitute for product evidence that both selected tables and the
exact installed manifests meet that narrower interface.
