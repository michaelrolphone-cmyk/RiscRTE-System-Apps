# Full native software qualification and test bundles

Both native configurations linked successfully from Runtime `81845e8027b94abc5c7a36a45e0004884ada73b6`. The complete stores were rebound to their actual firmware hashes and independently assembled and verified as 16 MiB initial test images. No hardware, flashing or master merge occurred.

| Target | Native BIN | Slot margin | Static DRAM | Gap before IQ bank | Free boot-store blocks |
|---|---:|---:|---:|---:|---:|
| X4 | 1,435,648 | 1,054,720 | 72,448 | 7,824 | 552 |
| Watch | 1,382,448 | 1,107,920 | 65,776 | 20,064 | 76 |

All byte counts are bytes; store blocks are 4 KiB. The linker DRAM region remainders excluding the IQ reservation are 124,304 bytes (X4) and 136,544 (Watch). These are address-space bounds, **not measured free heap or contiguous allocation guarantees**. Receipts account separately for nominal 8 MiB PSRAM, two Runtime objects (553,616 bytes), up to two app allocation ledgers (98,304 bytes), and X4 diagnostic buffers (131,072 PSRAM + 8,192 internal heap). Resident images, graphics, networking, transient allocations and allocator metadata require additional space. Hardware interoperability and live memory margins are untested.

## X4 link correction

The original X4 link failed its IQ guard: static DRAM overlapped the fixed reservation by 360 bytes. Commit `92f5a1122b7e7111ecbda604b1cada8352c6aa65` moves the unchanged 8 KiB early diagnostic prefix to the already initialized internal heap before Arduino startup. Allocation failure latches the normal startup gate. The full prefix and later PSRAM trace remain available; no functionality or capacity was reduced. Production logger tests (including allocation failure and UBSan) and pinned Arduino startup/hold/failure/reset tests pass. Mach-O tests rename only the wrapper entry symbols because Darwin lacks GNU --wrap; source pin verification remains enforced.

## Composition evidence

`qualification-2026-10-11/full-link/` contains final native proofs, link logs, exact source composition, memory reports, store packing and final admission evidence. New images pass ESP checksums and appended digests, exact source markers, TLS/rollback proofs, partition MD5/geometry, IQ reservation, complete native role/import/cohort admission, and independent final-image filesystem extraction. Both active-bank journals name the newly linked firmware and exact store; inactive banks and unused gaps are erased.

X4 includes 26 apps / 32 providers / 42 boot-store ELFs, plus all 16 SD application ELFs and matching manifests/fonts from the preserved complete baseline. All SD roles and imports were admitted against the new X4 native image. Watch includes all 25 apps / 30 providers / 54 ELFs and needs no SD payload because that baseline has no SD provider. Its complete canvas, touch2.2 and Buttons1.22 inputs remain intact. Reader, DailyPaper, Wi-Fi, MSC, SD and panel inputs are retained on X4. Compaction only removed proven non-loader metadata; final cohort.json was intentionally refreshed for the new native image.

The published qualified Files baseline is retained. The separate install task's unfinished transaction/journal and tagged ops100–103 work is excluded. Original v2/v3 requests and ops1–7 remain unchanged.

## Reproduction and delivery

Use the exact source/component receipts from SOURCE_BUILD_READY.md, replacing X4 native `b455a10` with `92f5a11`. Build one job per target. `scripts/assemble_shared_files_bundle.py --help` lists explicit inputs for the verified native directory, packed store, matching SD tree, pinned tools, Runtime, initial AppData image and product source. It refreshes native cohort identity and assembles the image; then rerun X4's `qualify_shared_files.py` against that final store and actual native BIN. Run strict app ELF/import admission for the SD paths listed in boot.json. Bundle SHA256SUMS and ZIP round-trip checks cover every delivered byte.

Each test bundle includes full initial image, individual components, matching store/SD contents, native ELF/map, exact source archives, build and qualification receipts, and a scope README. These are initial full-replacement test images, not releases, OTA packages or data-preserving reflashes. Hardware checks remain separate. GitHub source checkpoints are published; delivery uses Library.
