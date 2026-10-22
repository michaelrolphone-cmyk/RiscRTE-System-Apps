# Native Serial Monitor UI local candidate

Commit: `5843d1bf2079e66f837aec5eb03a1f15b9a95ae7`
Tree: `14c2b4e7a1566ea2910d7783864d969677280235`
Branch: `tasker/native-serial-monitor-ui-20261021`
Base client: `f9d55dbeb941ba40fb965f53172ce3e1b85af376`
Source: `/workspace/scratch/c744abbbbd60/usb-native-cdc/System-monitor-ui`

## Result

Implemented and locally qualified the native receive-only monitor controller,
explicit nonzero instance and endpoint snapshot selection, baud choice, bounded
ASCII/escaped-byte terminal, touch/button navigation and checked retained-custody
exit. The app remains **unmanifested and unpublished**. Its final utility/product
placement is intentionally undecided because the foundational System inventory
excludes MCU/debug utilities. No automatic connection, TX, DTR/RTS, flashing,
role selection, device discovery, physical action or product pin was introduced.

Final ordinary and ASan/UBSan replays each pass:

- 25 actual-controller/actual-client scenarios, including touch pagination at
  240 pixels, repetition, selection, stale snapshots, interrupted input,
  all 256 byte values, terminal bounds and retained failures.
- 20 real native adapter cases at 240×240 and 480×800 RGB565, with guarded
  captured frames, cleanup-before-launch/finalization, terminal silence,
  and continued RX during an immutable 600 ms pending frame.
- Nine intentionally rejected unsafe/unqualified compile compositions.
- The unchanged PortableSerialClient suite and 22 pinned real Runtime/client
  integration cases, plus opt-out and C++ header checks.

An independent review reran the final matrix on the exact clean commit,
inspected the rendered selection/receiving/refusal frames, verified licensed
font reproduction and found no remaining blocking issues. Its two earlier
findings are fixed and regression-tested. `INDEPENDENT_REVIEW.md` records the
scope and evidence. A closing screen is intentionally absent because uncertain
closure forbids any later drawing.

Three existing non-monitor adapter profiles preprocess identically to the
preserved client parent. This is not a complete release-byte rebuild. Existing
SDK baselines, profile/manifest/builders and the client itself are unchanged.

## Evidence and replay

- `final-normal/` and `final-sanitized/`: exact UI/native source hashes, commands,
  logs, captures and contact sheets.
- `final-client-normal/` and `final-client-sanitized/`: unchanged client and pinned
  Runtime integration evidence.
- `independent-review/`: separate clean-source replay receipts and logs.
- `SOURCE_LOCKS.json`: exact sources, dependencies, artifacts and recovery hashes.
- `existing-profile-parity.json`: unchanged-profile preprocessing identity.
- Source instructions: `docs/NATIVE_SERIAL_MONITOR_UI.md` in the candidate.

ASan/UBSan are enabled; LeakSanitizer is disabled because ptrace prevents its
operation here. Native rendering qualification uses fake RGB565 providers,
not an X4 e-paper or physical display. No full Xtensa/IDF/controller/ELF build,
callback timing, complete aggregate CI, product admission or physical pass is
claimed.

## Recovery

`native-serial-monitor-ui.patch` contains both local commits and applies on the
exact base client. `native-serial-monitor-ui.bundle` is incremental and requires
that base. `native-serial-monitor-ui-recovery.tar.gz` includes the full available
source-history bundle, its shallow-boundary marker and a checked restore script:

```
tar -xzf native-serial-monitor-ui-recovery.tar.gz
sh recovery/restore.sh /new/empty/System-monitor
```

The restoration was executed locally. Exact final commit/tree and `git fsck`
pass. Earlier upstream history preceding the shallow boundary is not included.
The original System client, Drivers `77980f897e49829a8986df323024132c759671f2`
and Runtime `6a3b3d46f5b2857eacc6e9863799dec1f466b036` remain clean and unchanged.

## Still unfinished

Full target build and callback timing; safely scheduled discovery/role work;
product utility/launch/instance admission; authoritative X4 VBUS/role-root proof;
physical acceptance; and all existing publication holds. This local slice does
not complete the broader USB serial monitoring task or request hardware tests.
