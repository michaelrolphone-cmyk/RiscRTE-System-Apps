# Runtime-installed application discovery

Settings, Springboard and the shared Contexts workflow client consume the
read-only `app.catalog@1` capability. Product inventories are metadata owned by
Runtime. No product-sized C array or per-product compatibility bitset is linked
into these applications.

## Contract

`RiscAppCatalogV1.h` defines a bounded native publication: at most 128 entries,
a nonzero generation, an exact canonical relative launch route (256-byte
buffer), native application ID (96), display name (96), icon (24), and optional
minimum firmware string (32). Strings must be terminated and untruncated.
`AVAILABLE` means native launch readiness; it is not a firmware-version verdict.
The native owner preserves admitted identity, permissions, installed-directory
metadata and launch fencing. `launcher_visible:false` omits a launcher row
without revoking the underlying native policy. The service cannot scan caller
paths, install, update, change permissions, or acquire bank-store authority.

The client copies a consistent publication. A successful empty publication is
shown as empty; missing or malformed publication is shown as unavailable. No
compiled fallback, synthetic installation, truncation or skipped malformed row
is used. Native optional descriptive-metadata normalization is separate from
client validation of the capability response.

The historical T5 UI ABI retains its 128-byte filename field. Short canonical
routes remain unchanged. For a longer route the UI key is `id:` followed by the exact native ID;
the adapter separately retains and launches the complete canonical route.
The colon is forbidden in canonical native routes, preventing collisions with
short route keys. Contexts resolves the same UI key before checking permission and launching.

Launches carry copied `(generation, index, route)` tickets. The ticket is checked
both before and after launch guards / display-drain callbacks. Any replacement,
removal or reordered publication blocks that request. The controller refreshes
its invalidated view and discards the pending selection rather than retrying a
shifted index. No new polling cadence is introduced: ordinary discovery refresh
remains on initial entry, explicit refresh, and this stale-launch recovery.

`CONTEXT` or `RETAINED`, an ambiguous acquire, or refused grant release causes a
terminal invocation fence. No later provider callback, grant release or client
allocation free is permitted. Ordinary malformed/unavailable/stale responses
have releasable custody. The catalog grant is acquired lazily and released after
pending canvas work has drained, before other normal cleanup.

## Build and verification

The standard Settings/Springboard builders do not emit or link catalog C files;
they require `app.catalog@1` and scan all ELF symbols for the removed array names.
Springboard's historical `--catalog-limit` argument is accepted but explicitly
ignored; the service has one hardware-independent bound.

- `python scripts/test_runtime_app_catalog.py`: the same executable receives
  empty, one-entry, 23-entry, different, 128-entry and long-route inventories at
  runtime. Both 240x240 and 410x502 run with ASan/UBSan, malformed/custody/stale
  cases, guard/drain mutation, real controller recovery, and full 23-entry pixel
  comparison against public source `2c3f44dd773e27e3a142462088a96328119a79e7`.
  Lossless verified frame ZIPs and hash receipts remain under the build output.
- `python scripts/test_runtime_context_catalog.py`: actual Contexts workflow
  dispatch, including ID-to-long-route resolution, absent/unavailable rows and
  changed generation.
- `python scripts/test_runtime_catalog_build.py --compiler <Xtensa gcc>
  [--exact-inputs <archived recipe directory>]`: target app ELF byte independence
  from changed legacy inventory sources, complete symbol scans, standard
  builder import/export/structure checks and optional original-feature recipes.
- `scripts/prove_shared_catalog_target.py` repeats the complete original-feature
  target link with immutable public generic bridge revision
  `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`, native-height/navigation-retention
  flags, and separate pinned Settings/Springboard Utilities inputs. It records
  every actual preprocessor dependency, source hash, import/export and final
  ELF hash; changed inventory JSON and forbidden legacy C files cannot enter
  the link. Only small application ELFs are retained, never firmware images.
- Existing native-height Settings, exact-feature Springboard, optional-service,
  navigation-retention and async-canvas suites remain applicable.

Host fixtures prove client and controller behavior. Native directory admission,
capability grant custody, actual product composition and hardware operation are
separate qualification boundaries. Exact original-feature recipes preserve
archived local bridge sources; this test does not qualify a replacement bridge.
