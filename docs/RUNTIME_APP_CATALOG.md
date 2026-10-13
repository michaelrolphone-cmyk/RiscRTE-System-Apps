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
- `python scripts/prove_shared_catalog_target.py --compiler <Xtensa gcc>
  --bridges <RiscRTE-Watch checkout>`: reproduce the complete original-feature
  target links with only this checkout, Python 3.11, the compiler below, and
  immutable public bridge revision
  `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`. The runner reads bridge Git objects,
  never dependency working-tree modifications. Checked-in
  `test/native_apps/fixtures/catalog_target/recipes.json` locks full flags,
  source order, actual transitive bytes, compiler identity, imports/exports,
  sizes, and target hashes. The existing local Watch alarm profile includes the
  vendored `AlarmServiceV1.h`, byte-identical to both audited Utilities revisions;
  no Utilities checkout or archived absolute path is needed. Unused historical
  include paths are omitted. Native height and navigation retention stay enabled.
  Each app is linked twice with different runtime inventories and a forbidden
  legacy catalog source. Only small app ELFs and JSON receipts are retained.
  This successor changes test/recipe/docs/CI inputs only; compiled app sources
  remain public System `72f435fcee376ec62d17ece6180753385ea547d9`.
- Existing native-height Settings, exact-feature Springboard, optional-service,
  navigation-retention and async-canvas suites remain applicable.

### Fresh-checkout target reproduction

Check out this System revision (including its Git history for legacy host parity),
then fetch the public bridge into any local path:

```sh
git clone https://github.com/michaelrolphone-cmyk/RiscRTE-Watch.git watch-bridge
git -C watch-bridge checkout 6e82fdcaec0b73ed700d2104cfb4d94f011c19d4
python -m pip install platformio==6.1.19
pio pkg install --global --tool 'espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5'
python scripts/prove_shared_catalog_target.py \
  --compiler "$HOME/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc" \
  --bridges watch-bridge
```

The pinned compiler identifies as `xtensa-esp32s3-elf-gcc (crosstool-NG
esp-2021r2-patch5) 8.4.0`. Expected SHA-256:

- Settings: `4ee26705175a325f62af18e6dc98edc9e28aa51b9bf239bc946d8908cbce1385`
- Springboard: `f33ff27719ffd1faa1fb5f671579ada881aac29c9bd4c4673779798064470b17`

`.github/workflows/runtime-app-catalog.yml` runs the catalog/Contexts host suites
and this exact target proof, checking out the bridge at the immutable revision.
The workflow does not consume archived build recipes or local workstation paths.

Host fixtures prove client and controller behavior; target app links prove the
locked UI byte contract and import boundary. The committed qualification receipt
in `test/native_apps/fixtures/catalog_target/qualification.json` binds these
results to native contract and independent review pins. Native product
composition, full firmware builds and hardware operation remain separate
qualification boundaries and are not claimed by these tests.
