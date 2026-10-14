# Healthy controller/catalog investigation

## Result

No recoverable catalog failure reproduced. Four complete controller runs pass: sync and async 410×502, ordinary and ASan/UBSan. They use .41's exact feature flags/forced headers, the production adapter/controller, and the actual deployment-local power-key navigation bridge. Providers are healthy ABI fixtures, including extended Contexts and native atomic KV. The 24 catalog rows are derived from the actual .39 store manifests.

The test-owned 16-slot ledger validates slot identity, API pointer, and generation on release. Catalog is acquired with 8 live grants; peak is 11, including transient RTC inside the automatic atomic-preferences path. Final live grants and tracked allocations are zero. Peak tracked allocations are 5 sync / 6 async, including one fixture-owned framebuffer. The catalog staging allocation is exactly 12,888 bytes (24 × 537). Thus ordinary healthy-provider grant-slot exhaustion and ordinary client allocation-ledger capacity exhaustion are not demonstrated.

Runtime's unchanged NativeAppMemory repeated child/overlay and terminal-retention tests also pass normally and with ASan/UBSan. The physical native allocator still has target-only gates (owner, selected context, retained fence, mutex, actual heap). Initial allocation-context failure prevents app init; failed explicit context switch terminally retains. A healthy UI with a new failure message cannot be explained solely by those terminal paths. An ordinary heap failure remains unmeasured, not established.

## Next required evidence

Read the .41 display's exact stage, result, and row/index. The original text alone does not separate these paths:

- `acquire 0`: ordinary failed acquisition with healthy Runtime and empty returned grant; inspect active app policy/grant state and native rejection branch next. Healthy slot count now has a passing bound.
- `interface -1`: acquired interface failed validation; inspect the actual runtime table identity/header. Target layouts are already independently matched.
- `allocation-failed -1`: failed 12,888-byte catalog allocation for 24 rows. Inspect actual owner/context/lock/ledger and matching heap availability/largest block next; do not infer OOM from this stage alone.
- `snapshot -2`: native publication unavailable. Native source permits this for unrepresentable routes, exhausted generation, or excessive count. The actual fixture's 24 short routes do not trigger it.
- `snapshot 0`: native success but client rejects returned size/count/generation. Requires captured returned metadata.
- `row 0` with index: native success but that row fails client string/path/flags/size validation. Capture precisely that returned row.
- `row -3`: stale generation; inspect publication/availability mutation timing.
- `duplicate-row -1` with index: duplicate UI key or route returned; inspect that pair.
- `snapshot-after` or `generation-mismatch`: inspect both returned generation/count records.

CONTEXT (-4) and RETAINED (-5) fence the client; they deliberately do not produce a fresh on-screen diagnostic. If the watch only freezes on a prior screen, that is a different observation from a responsive newly rendered error.

## Boundaries

This is a host fixture, not physical-hardware qualification. Actual driver internals, FreeRTOS/native heap pressure, selected firmware/bank identity, and live settings-worker publication timing remain target-only. Existing native store admission, zero-I/O catalog, ABI and baseline-ELF evidence are separate. No production edits or release changes were made.

`reproduce.py` rebuilds both fixture variants and reruns all four controller cases. Commands and source are preserved beside this file. `results.json` gives the exact scope and measured counts.

## Reproduction

Use a checkout of `michaelrolphone-cmyk/RiscRTE-Watch` at commit
`6e82fdcaec0b73ed700d2104cfb4d94f011c19d4` and a host C compiler with
AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
python3 docs/t063/healthy-controller/reproduce.py \
  --bridges /path/to/RiscRTE-Watch --output /tmp/t063-healthy
```

The commands use explicit path placeholders resolved by the script. The fixture
uses the current checkout's production System sources. The recorded results
were obtained against System `326396614def41f4a3b11c8c193c3ac1bbb75ab0`, then
rerun after converting the fixture paths to this relocatable form. This evidence
checkpoint changes no production source and does not change the published .41
binary, release source pins, or hardware qualification status. The archived logs
contain no addresses or timing assumptions and are compared exactly on rerun.

The separate Runtime memory-test results are observations from the unchanged
native suite; this script runs the four controller cases only. Target ABI layout
was also checked separately with the real Xtensa compiler; neither check runs
target instructions on hardware.

To repeat the target C/C++ layout comparison, use the official
`xtensa-esp32s3-elf` GCC 8.4.0 esp-2021r2-patch5 toolchain and Runtime source at
`c258c4ba144247b6eaa4c6f28675ffccec582fe1`:

```sh
python3 docs/t063/healthy-controller/verify_target_abi.py \
  --runtime /path/to/RiscRTE \
  --toolchain-bin /path/to/toolchain-xtensa-esp32s3/bin
```

This compiles the same offset/size expressions as C against the client header
and C++ against the native header, extracts the 32-byte constant arrays, and
checks both against the recorded layout. It does not execute either object.
