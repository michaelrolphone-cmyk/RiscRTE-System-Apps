# Explicit X4 native BLE telemetry client

This opt-in source uses the accepted Watch service/client contract. It adds no
Runtime radio policy, global tick hook, new storage namespace, temperature or PMU
measurement. Compile each selected native app with `PORTABLE_BLE_BROADCAST` and
`PORTABLE_BLE_BROADCAST_DEFAULT_OFF`. These require the native custody fence.
Scanner, HID and IQ apps additionally select `PORTABLE_BLE_FOREGROUND`.

Every app declares `telemetry.broadcast@1`, mapped to singleton instance 0, plus
its existing shared `storage.key-value@1` namespace 1. The service never receives
app storage or pointers. `ble_broadcast` uses the accepted checked four-byte
record; absent X4 records mean disabled without writing. Valid saved on/off is
preserved; malformed/unread state fails closed. `quick_radio` remains authoritative:
absent means Bluetooth off, and explicit Bluetooth off/airplane prevents publishing.
Enable in Battery saves broadcast intent; Bluetooth must separately be enabled
in Quick Controls. These are open BTHome advertisements without pairing.

The native adapter borrows and releases service grants synchronously, preserving
copied client policy between calls. It holds at most a service plus one transient
storage grant, and no service grant during controls, sleep or handoff teardown.
The ordinary provider remains alive through Runtime's existing promoted boot
ownership. Pause precedes storage, competing radio controllers, controls, alarms
and sleep. Failed acquire/release or radio cleanup latches the canonical native
invocation fence; no normal operations follow. File Browser pauses before storage
work. AppData consumers bind `PortableBroadcastAppData.h` around every stat/read/
replace call, including Timecard and RF temporal data.

Clock's `--ble-broadcast` builder emits 0.3.11, fifteen unique declarations and
`ble_broadcast` custody in its receipt. Product mapping expands its KV namespaces
to sixteen policy grants. Timer-only startup/refresh/finalization never acquires
the telemetry service or reads preferences. Foreground promotion admits the
ordinary provider graph before telemetry is acquired.

Validation uses the actual Clock, adapter and selected Home-lock sleep source:
220 normal/ASan/UBSan Home/deep/GPIO/refusal/interruption/retained tests passed.
Observed Clock peaks were twelve live grants in foreground deep entry and six
on timer-only wake; timer wake performs zero telemetry acquisitions. The separate
product Runtime fixture loads actual production battery-telemetry, BLE-telemetry
and broadcast ELFs, proves the real sixteen-slot limit and rejects a seventeenth,
checks the exact three-field BTHome packet, cross-app provider lifetime, radio
policy, no timer activation and retained close failure. Battery controller tests
and app-data/client fixtures are separate. Clock and utilities target ELFs pass
imports/exports and loader validation. No hardware or radio transmission occurred.

This source is local development only. Publication of inherited System ancestry
remains blocked; these commits do not authorize a push, package release or flash.

## System builder selection

Settings, Springboard, Files and Wi-Fi expose `--ble-broadcast` through
`scripts/portable_broadcast_build.py`. Only explicit native UTC/alarm-API2 builds
accept it. The builder chooses Settings1.3.15, Springboard1.7.7, Files1.5.10 or
Wi-Fi1.1.12, compiles both telemetry defines, declares the capability and emits
its exact singleton grant. It never edits a finished manifest to claim support.
All native admission receipts include build_defines, required_grants and the
telemetry source/header hashes. Settings now emits its selected admission inside
the builder; diagnostic/snapshot SDK custody remains complete.

The source combines Home-lock Settings70c2282 with final Files/Wi-Fi8c202449.
Scrolling, hidden Confirm guards, Files volume9 and handlers, Springboard
crossfade, paper motion and stage logs remain independently selected. Run
`qualify_broadcast_system_builders.py` with the Runtime, Utilities, display SDK,
catalog and pinned compiler paths. Its `builder-qualification.json` contains the
exact four target command arrays, validated receipts and Watch byte comparisons
against the immutable pre-builder merge. All four Watch flag-off ELFs/manifests
remain byte-identical.

The storage audit added pause-before-access to the native KV facade, the
app-owned timezone reader and Wi-Fi's private credential table. Broadcast's own
policy reads pause before accessing storage, with an explicit recursion guard so
they do not reacquire/release their own service grant. Wi-Fi returns immediately
when these wrappers latch native retention. Existing Files storage iteration
pauses remain in force. The storage fixture begins with a live advertiser and
verifies that get/put sees it stopped, failed pause reaches no backend operation,
retention forbids subsequent I/O, and own-policy reload does not recurse.

Selected controller qualification covers48 Settings/Home-lock/storage-fence
cases,124 Files scrolling/keyboard cases,76 Wi-Fi scrolling cases and72 ordinary
native-app lifecycle cases, all across normal and ASan/UBSan builds. Repository
Python tests pass80 cases. These are simulated hardware tests and target ELF
checks, not physical RF or display qualification.

## Update applications and shared reader orientation

Updater commits0b3a7b8 and7b92c390 are included in this combined source.
`build_portable_updates.py --product x4 ... --ble-broadcast` selects OTA and App
Store1.2.2. Each exact native admission has12 grants, preserving credential
namespace6 and shared namespace1, net.wifi15, Bluetooth16, readonly native time,
alarm API2 and its action-specific update service. No HTTP or bank authority is
added to an application, and the product feed remains explicitly unconfigured.

Check/connect, private credentials, native UTC sampling and active network/bank
steps pause telemetry before potentially retained work. The pure
`portable_update_broadcast_safe` query prevents resuming it during an active
check/install, cleanup or pending restart. A failed pause returns through native
retention without performing subsequent update/storage operations.
`qualify_broadcast_updates.py` records the exact recipe and validates both target
admissions; unselected Watch app/provider ELFs and manifests remain byte-identical
to7b92c390. The X4 update policy/cohort matrix and32 native-controller cases pass
normally and under ASan/UBSan, including live-advertiser Check, busy exclusion and
failed-pause retention. This ptraced executor requires
`ASAN_OPTIONS=detect_leaks=0`; address/undefined checks remain active.

All six selected System telemetry profiles explicitly define
`PORTABLE_PAPER_PREFERENCES`, so the existing reader_flip_ui record applies to
Springboard, Files, Wi-Fi and the update apps as well as Settings. No namespace
or authority is added. Pixel and touch transforms use the existing shared
adapter. Actual flipped Files/Wi-Fi controller tests read a saved180-degree
record, receive physically rotated raw snapshots and queued UP events, preserve
viewport pixels and select the intended logical rows at both supported test
geometries. The flipped suite covers124 Files and76 Wi-Fi cases, normal and
ASan/UBSan. Unselected Watch profiles keep their original bytes. No translations
were added: the frozen22 Reader language IDs retain their existing fallbacks.

The reported product prepare_sdk_test header-count mismatch is pre-existing
(expected3, generated4 on the original final product). This System-only
integration does not change that product script/test, so it remains a separate
baseline limitation rather than being silently rewritten here.
