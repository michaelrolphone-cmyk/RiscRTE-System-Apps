# Watch BLE shared client reservation

Reserved 2026-10-07. The opt-in PORTABLE_BLE_BROADCAST shared client acquires
telemetry.broadcast@1, polls at settled foreground boundaries and retains a
provider-owned boot-session publication across ordinary app switches.
PORTABLE_BLE_FOREGROUND prevents publication for the full BLE Scanner/Touchpad/
Buttons invocation so an explicit foreground session always has priority.
Before Quick Controls, low-battery radio apply, sleep or abnormal app exit,
publication closes. Cleanup refusal retains the invocation for retry.
The Clock's custom adapter must implement the same lifecycle.

The Watch integration owns fresh deployment versions for every relinked app;
legacy source manifests and delivered files stay unchanged. Telemetry SDK tables
are copied without layout changes from Drivers 4088b689 and Utilities' new
telemetry-broadcast0.1.0. No Runtime ABI or automatic task is introduced.

Validation: production adapter tests cover normal polling, matching radio-state
reload, modal controls and alarm/cue pause, one-shot low-battery crossing with a
later manual radio override, retained sleep, cleanup-only retry, and healthy
app-grant release without stopping publication. Normal and ASan/UBSan pass.
Utilities' end-to-end Battery fixture links the actual adapter and service and
proves two invocations retain the same publication token. Hardware is untested.

The service has no storage dependency. This app-side client uses existing shared
namespace1 for ble_broadcast and quick_radio, passes copied policy, and retries
retained storage-grant cleanup before any further normal reads. Reads are paced
to once per second and invalidated at explicit control boundaries. No new
provider may bind an already-used namespace during a preserving cohort update.
The Battery action writes and checks exact readback; an uncertain save pauses
publication and offers Retry for that same action.

App-data coexistence: the app-local PortableBroadcastAppData facade stops RF
before each native stat/read/replace. Once native storage returns RETAINED, the
facade and app perform no further radio or storage I/O. It preserves original
outcomes/revisions/bytes. Pause refusal prevents the storage call entirely and
keeps the invocation inside a cleanup-only retry loop; it does not synthesize a
storage-retained result without the corresponding native fence. Spectrum/RF use this fence; Timecard integration must
bind its actual app-data capability through the same facade before its bridge.
The facade's normal/sanitized tests cover ordering, uncertainty and no-I/O-after-
retention; the actual Spectrum temporal app also passes with the BLE flag.
