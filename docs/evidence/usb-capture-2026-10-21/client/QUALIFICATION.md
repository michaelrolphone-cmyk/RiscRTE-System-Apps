# Portable serial client local candidate

Commit: `f9d55dbeb941ba40fb965f53172ce3e1b85af376`
Base: `0651a55513b0aa0dff1b6a9072303def08a97378`
Branch: `tasker/portable-serial-client-20261021`
Tree: `d959bd31c689ea9a038069eb3bf0961a958d8539`
Workspace: `/workspace/scratch/c744abbbbd60/usb-native-cdc/System`

## Result

Implemented the missing stream-only PortableSerialClient library. Existing Runtime witness interface is preserved: init/open/control/configure/queue/tick/cancel/close, with explicit inventory index and optional exact provider-instance initialization. No raw serial fallback, automatic device selection, ABI change, application UI, product pin or remote publication.

Normal and ASan/UBSan committed-source replays pass. Each includes local invalid/partial/malformed/stale/retained lifecycle tests, strict C compilation, opt-out behavior, C++ header smoke, and the unchanged Runtime 22-case production integration suite (11 cases under eager and demand). The source checker verifies all six copied declarations and pins Runtime to 6a3b3d46f5b2857eacc6e9863799dec1f466b036. The native Reader provenance pin is c98e4222048244b1e729869ed47aa99130025fd3.

Independent review's three findings are resolved and regression-tested: open reserves the broker's possible rollback budget; malformed positive control statuses retain without cleanup; ordinary empty acquisition refusal is distinguished from retention by a provider-free same-context Runtime getter. Open rejects a total 1 ms budget before I/O because broker rollback can consume a second sub-budget; existing-session controls support 1 ms.

ASan and UBSan remain enabled, but LeakSanitizer cannot operate under this runner's ptrace environment. The successful sanitizer command uses ASAN_OPTIONS=detect_leaks=0. Initial failure preserved in lsan-ptrace-unavailable.log; this is not leak qualification. No target toolchain was found by the client worker, so no Xtensa build is claimed here. No hardware/USB/serial/radio action was performed.

## Reproduce

```sh
cd /workspace/scratch/c744abbbbd60/usb-native-cdc/System
python3 scripts/test_portable_serial_client.py --runtime /workspace/scratch/c744abbbbd60/usb-serial-monitor/RiscRTE --output /workspace/scratch/c744abbbbd60/usb-native-cdc/client-artifacts/normal-replay
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_portable_serial_client.py --runtime /workspace/scratch/c744abbbbd60/usb-serial-monitor/RiscRTE --output /workspace/scratch/c744abbbbd60/usb-native-cdc/client-artifacts/sanitized-replay --sanitize
```

Final normal evidence: `normal-final/evidence.json`, `normal-final/tests.log`.
Final sanitizer evidence: `sanitized-committed/evidence.json`, `sanitized-committed/tests.log`.
Exact hashes/commands/state: `SOURCE_LOCKS.json` plus the evidence JSONs.
Library documentation: `/workspace/scratch/c744abbbbd60/usb-native-cdc/System/docs/PORTABLE_SERIAL_CLIENT.md`.

## Restore

`portable-serial-client-recovery.tar.gz` is a self-contained source recovery kit. It contains the available source-history bundle, its required shallow-boundary marker and a checked restore script. It was restored locally; exact commit and git fsck both pass. Earlier upstream history is not included. Git's bundle format does not encode shallow markers; an ordinary clone without that marker fails and is recorded in shallow-clone-limitation.log.

```sh
tar -xzf portable-serial-client-recovery.tar.gz
sh recovery/restore.sh /new/empty/System-client
```

`portable-serial-client.patch` applies with git am on the exact base. A separate worktree verified that the restored tree equals the committed candidate tree. `portable-serial-client.bundle` is a smaller incremental bundle requiring the exact base. No artifacts were uploaded or published.
