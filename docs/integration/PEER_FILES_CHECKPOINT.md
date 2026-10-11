# Peer Files software qualification

The end-to-end software path is implemented and tested: shared Files UI selects
a BLE peer, displays and confirms the comparison number, receives the temporary
session descriptor, releases BLE, authenticates WebDAV, transfers a file, closes
resources, and requests its declared application through the existing file.open
broker. The target components are built for both Watch and X4. This is not a
flashable release and does not claim physical radio or device qualification.

## Exact integration inputs

- Runtime `90aefae2227856c1ae456bd4eb431542d14fd852`, including installer
  `6c658149de54067d7bcc1f4915906cd51ee42a6d`, outbound connector `1a1bd64629081a64de7e582adab51292178b1519`
  and the Darwin UBSan fixture correction. The failure was extra `.dSYM`
  directories in an exact-inventory test store; admission was not relaxed.
- System lineage merges `c2bed31` through `ce5a56b42ae4a62e335d58a3988077fb77273631`.
  Peer Files is optional `--peer-files`, component version 1.5.25.
- Drivers peer broker `b241951`, BLE setup 0.1.2, unchanged vendored NimBLE
  `da7e3256da3ba80b232df880f40d8359311cc62e`.
- X4 `e77d3458231e9138567a69b03f7b8d5520f13abb` incorporates installer `80b9008`.
  The exact latest installed-home-final-sharing store matches every hash in
  `minimal/evidence/installed-packages/migration-and-sharing.json`.
  Home 0.4.9, catalog 0.4.0, SD 0.2.20, Reader, DailyPaper, Wi-Fi and MSC survive.
- Watch baseline `089868395268b696c05651c33816345bfe0425df`, full 25-app canvas,
  Touch 0.2.2 / Buttons 0.1.22; product metadata branch `e73c91e` remains intact.
  Utilities `814502e51125d799a7ca625b78707753ca12d4e5` supplies existing Watch interfaces.
- All Runtime software service selectors remain zero. No native capacity or
  dynamic installer policy was overwritten.

## Passing evidence

`peer-evidence/` contains logs and source/ELF hashes, including:

- Two production BLE brokers: advertisement discovery, explicit selection,
  mutual Secure Connections numeric comparison, matching ephemeral keys,
  authenticated GATT descriptor/live-validity reads, rejection, expiry and close.
  All 25 independent production-peripheral regression scenarios also pass.
- The descriptor emitted by that BLE test feeds the actual WebDAV client and
  Digest server. Tests cover fragmented nonblocking transport; PUT/GET both
  directions; listing, MKCOL/MOVE/COPY/DELETE; exclusive creation; path confinement;
  wrong authentication, expiry, cancellation and unavailable/exported local media.
- Production Runtime AppData export maps all eight existing Watch files listed
  in `WATCH_USER_FILES_SCOPE.md`: read, revision-checked replacement, stale-write
  rejection, and no backend access for private paths.
- Actual shared Nova and paper frontend with production transfer/controller:
  upload, download, numeric confirmation only after the frame is displayed,
  user stop, expiry, unavailable destination, borrowed and owned Wi-Fi cleanup,
  and declared Model Viewer launch only after successful local publication and
  peer/network cleanup. A handoff finalization bug found by this test was fixed.
- Runtime native TCP and broker tests pass under UBSan, including ownership,
  partial/blocked/EOF, retained close and no I/O after context loss. Runtime.cpp,
  CpuPort.cpp and the production outbound lwIP connector compile for Xtensa.
- Both Files target ELFs and BLE/TCP providers pass target structural/import
  validation. Complete source stores pass actual Runtime self-admission under
  UBSan: Watch 54 distinct ELFs (25 apps / 30 providers), X4 42 distinct ELFs
  (26 apps / 32 providers). Admission performs zero hardware/storage calls.
  Source-preserving X4 transition from the current sharing store also passes.

These are UBSan results, not ASan results. Boundary fixtures supply storage,
controller wire and network bytes; no physical network, credentials or device
was used. Transfers are cooperative, with one bounded socket action per client
tick. AppData's atomic commit remains bounded by its 64-KiB file contract.

## Composition path and genuine remaining block

1. Build current System with `build_watch_shared_files.py --peer-files` or
   the existing complete X4 Files selections plus `--peer-files`; preserve all
   existing feature flags/grants. Build current TCP facade and Drivers BLE setup.
2. Run `prepare_peer_files_overlay.py` against the exact current full stores.
   For Watch, select `watch-user-files-export.json`. It verifies all eight
   owner/namespace grants and preserves all unrelated app/provider bytes.
   X4 inherits its dynamic installer, namespace ledger, SD bindings and settings.
   The experimental documents provider is NOT selected as a substitute.
3. Run `qualify_shared_files.py` with current Runtime; target and self-cohort
   admission already pass with current limits. The old native images used as
   import witnesses are NOT firmware pairings for the new outbound connector.
4. Before a preserving release, the single integrator must resolve the explicit
   export-authority migration. `AppDataExportRuntime.inc:124` makes an installed
   export map immutable, including added files or a label change. Accordingly,
   Watch Timecard-only -> eight-file map rejects before ELF admission. This is
   expected enforcement, not an ELF or UI failure. The current native first-map
   delegation contract cannot rewrite an existing map. An explicit, reviewed,
   source-bound migration mechanism is required; no bypass was added.
   Likewise, the older X4 staged store with no export cannot jump directly to
   the sharing store without its first-map delegation and authority-preserving
   bridge. The current sharing -> peer store path passes.
5. After that authority input is qualified, rebuild the native source using the
   current TCP connector and existing capacity/IQ/partition selections, then run
   full paired-bank migration, resource and package admission. Only then assemble
   a full firmware. No new full firmware, master merge, flashing or release
   publication occurred here.

Limitations are explicit: configured AppData paths support read/create/replace,
not arbitrary rename/delete/mkdir; those operations are not present in that
storage ABI. X4 SD supports the full generic operations. Remote listings are
bounded to 64 entries / 32 KiB XML and reject oversized folders without silently
truncating them. Files use authenticated HTTP, so file contents are not encrypted
on Wi-Fi; BLE protects temporary setup. Hardware coexistence and live peak heap
remain device qualification tasks. OTS remains backlog.
