# Historical ELF mismatch classification — 2026-10-01

All ten remaining current development/published ELF mismatches are explained by
build lineage. Applying the pinned compiler's `strip --strip-unneeded` to local
copies of each verified published ELF produces the corresponding existing
current development ELF byte-for-byte. The current builder is not defective;
no production code, SDK, manifest, build profile or version needs changing.

The exact published-byte result remains **8/18**. The ten transformed comparisons
are diagnostic evidence, not permission to call different published bytes equal,
reissue them under existing identities, or weaken the eight mandatory parity gates.

## Previous evidence and the one unknown

The earlier `historical-build-probe.json` reproduced eleven of thirteen old
unstripped releases. Two of those eleven (Clear Cache and OTA Update) were later
refreshed and now have exact current-byte parity; Button Remap's old unmatched
1.0.0 was superseded by the verified 1.0.1 refresh. Thus nine of today's ten
mismatches already had historical no-strip matches. Driver Manager 1.0.6 was the
only genuinely unresolved current package.

The old Driver Manager probe produced 18,628 bytes, hash
`f3a584d5758f47c91b881f65502dec895b66add17326a780ea8c4c0f2b053cf2`.
Its dynamic symbol table contains unresolved `__udivdi3`. The **actual release
builder** detects that import and rebuilds with the source-owned
`UnsignedDivisionCompat.c`; the simplified old probe omitted this step.
Adding exactly that historical helper produces the downloaded 18,792-byte
published ELF, hash `ce32b180f0fc1bbea117076a7159dd993915e53f937fab92db1f5619d72649c1`.
Stripping it produces the existing 16,940-byte development ELF, hash
`a7960e50f745b7e5ecda77014e7abb7d84e974d4b58556cbd2f369f02fbaa54a`.
Both app source and helper are identical between the release and current inputs.
The current external builder already includes this helper correctly.

## Exact release lineage and representative reproduction

- Settings 1.0.0 tag resolves to `3af3c24f3c02e33af12019852393936c82367226`.
- Driver Manager 1.0.6 tag resolves to `ff08d329489c62af107c906036d0a926f1a0241f`.
- Both release builders have blob `0c51ac88c30b87baeefb75a699dc025be76c506a`;
  they perform no stripping and conditionally include unsigned division support.
- Helper blob: `6c692b28aae97379f5daf017861d3dc651339830`.
- Compiler: Xtensa ESP32-S3 GCC 8.4.0, `esp-2021r2-patch5`; strip version
  `2.35.1.20201223`. Only a task-local copy of the existing pinned toolchain was used.

Settings was the representative ordinary app: release-tag source and headers
rebuild its 3,252-byte published ELF exactly, hash
`68344680f7125a2b460ce2e9b5e2ed570c1292e0de1493ed3569a0e1e77cd901`.
Stripping yields the current 2,652-byte development ELF, hash
`57353530d9d8233fce0686a8fb94e3407e3a36292b5a2a5778b1df769bf99ff2`.
Current headers yield the same result; header drift is not the cause in these
representatives. Settings' release manifest lacks later category metadata,
but that does not enter ELF compilation; its source is identical.

For the two historical reproductions, files were read via `git show COMMIT:PATH`
into isolated scratch directories, never written into Reader or external sources.
Use the release-tag `lib/NativeApps/include` headers and the recorded source with:

```sh
xtensa-esp32s3-elf-gcc -std=c11 -Os -fPIC -mtext-section-literals -mlongcalls \
  -fvisibility=hidden -nostdlib -nostartfiles -shared \
  -Irelease-headers -Isdk/driver -Wl,--hash-style=sysv \
  APP.c OPTIONAL_UNSIGNED_DIVISION_HELPER.c -o scratch.elf
# Omit the helper argument for Settings; include it for Driver Manager.
# Compare scratch.elf with the downloaded published bytes before stripping.
xtensa-esp32s3-elf-strip --strip-unneeded scratch.elf
# Compare again with the existing development build.
```

[Machine-readable evidence](historical-elf-lineage.json) records exact release
commits, input/header blobs, package versions, asset URLs, sizes and before/after
SHA-256 values. Downloaded bytes were verified against immutable release-index
`572746f4fcf3fde19947a066b7e5c8028cd76d21` before any local transformation.

## Complete classification without repeated rebuilds

Driver Manager, File Transfer, Font Selection, Image Viewer, Language,
Package Manager, SD Firmware Update, Settings, Status Bar and Time Zone all
satisfy `strip(copy(downloaded published ELF)) == existing current build ELF`.
Only Settings and Driver Manager were rebuilt for this investigation. The other
eight used the already-recorded historical matches and a direct transformation
of downloaded published bytes, avoiding another package-wide rebuild loop.

This is a diagnostic classification, not a runtime equivalence proof. It neither
qualifies U1 ZIP/ABI behavior nor publishes independent artifacts. No unknown
cause remains among these ten at the recorded snapshots, but their original
published bytes still differ from the normal development outputs. Historical
records remain unchanged; this evidence supersedes only the earlier statement
that Driver Manager's build context was unknown. Maintenance claim #7 records
exact PR-head and postmerge verification for this documentation-only increment.
