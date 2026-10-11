# Opt-in native time Settings controller

`--settings-profile x4-native-time` builds the explicit future Settings **1.3.7**
profile. It composes the paper controls, all 419 app-owned timezone choices,
six Clock faces, Light/Deep preference, language and 180-degree orientation with
native UTC display and checked Set Time. The ordinary Settings/Watch/paper
profiles remain unchanged. The build contract is in
[NATIVE_TIME_SETTINGS_BUILD.md](NATIVE_TIME_SETTINGS_BUILD.md). No product lock,
installed ELF, bundle, BIN, hardware or remote publication is changed here.

## Invocation and ownership

Module init validates the **complete canonical** Runtime 0.1.51 table from
`602ae9bd618e13407b5b94bcad86cdabc23c99ea`, including `retain_invocation`, and
initializes software only. It does not acquire providers, query preferences or
sample native time. The first supported `t5_app_get_api` call from the production
Settings `app_main` opens its normal foreground providers.

Each sample opens one CONTROL grant in read-only TIMER_ONLY helper mode, loads
no RTC, reads a checked native snapshot, and closes before returning to UI.
Set Time opens a separate CONTROL helper only for an explicit Save. Its RTC,
native and metadata operations follow `PortableSetTime`'s checked ordering;
its native grant closes before rendering, polling or Home/Back handoff. No
reader grant or provider-promotion capability is acquired. The helper objects
are zero-initialized static state of this invocation; they are never copied,
persisted, reused after uncertainty, or kept across app reload.

`PORTABLE_NATIVE_CUSTODY_FENCE` exposes the hidden app-local
`portable_adapter_retain()` hook. Sparse Clock uses the same underlying native
fence; there is no second retention implementation or guessed Runtime suffix.
A rejected owner fence is a no-I/O fail-stop. A successful fence pins the
invocation, its current surface and every grant, makes drawing callbacks inert,
and prevents subsequent provider calls, polling, cleanup and frees. Adapter
broker and KV proxies keep the original grant/table for uncertain operations,
including failed releases that mutate the temporary release argument.

The optional alarm overlay stops before a subsequent status/stop/storage call
on any result outside OK/PENDING, including the forthcoming ALARM_RETAINED=-9
result without relying on an unpinned header revision. Exact-token acknowledge,
refresh, status, stop-only and finalization use that same fence. This is app
custody integration; it does not substitute for real Runtime owner/generation,
provider storage and teardown barriers.

## Time, policy and explicit Save

The root row and initial editor date come from native UTC projected through the
selected frozen-catalog IANA ID. They never infer validity from a plausible
external RTC date. Native UNSET is visible and starts an editable 2000-01-01
draft; no automatic RTC recovery or seed occurs in Settings. Missing timezone
is visibly UTC (default). Corrupt/unavailable timezone cannot silently authorize
UTC conversion: Save requires a valid selected zone. Choosing a timezone writes
only that preference after its own explicit Save and never updates either clock.

RTC basis loads read-only. A loaded local/UTC basis is preserved. Missing basis
is displayed as **Local (default on Save)** before the user can confirm. Corrupt
basis is displayed as invalid and blocks Save. A typed storage IO error is
shown as unavailable and also blocks Save; neither is treated as a missing
record. CONTEXT/unknown results and uncertain provider acquisition or cleanup
fence the invocation without trying to render through retained custody.

Save computes the timezone inverse before any clock-provider I/O. A gap is
rejected. A repeated local time opens separate **Save earlier UTC** and
**Save later UTC** choices. Held controls require a neutral cycle before the
second confirmation, so opening the fold page never commits it. Back, Home,
Cancel, interrupted/replaced contacts, and held gestures do not save a draft.

The checked helper validates Gregorian fields and the native 2038 limit, writes
and verifies the exact RTC calendar or proven next-second tick within 250 ms,
releases RTC, seeds and verifies native UTC within the explicit 1-second readback
budget, then writes/verifies namespace 1 `rtc_basis`. It never writes `time_zone`.
The result retains independent RTC, native and metadata attempted/verified
states. The paper view shows three outcome lines; a later error does not claim
an earlier successful write was rolled back. A safe mismatch, native IO, or documented typed metadata failure permits
another explicit Save after checked release. Metadata IO after commit may
already have persisted the record; the screen still says unconfirmed. No
failure automatically retries or rolls back a write. CONTEXT/unknown results
or an uncertain release fence custody, with no cleanup or new result screen.

## Reuse by later app integrations

`settings_native_clock(hour, minute)` is the optional Quick Actions time hook.
It delegates to `snt_read_local`, which opens and closes its own control grant
for every sample, projects the fresh snapshot through the current timezone,
and returns unavailable rather than using the legacy toolbar RTC. It is an
app-local integration pattern, not a new Runtime/provider ABI. A later toolbar
or Alarms/Countdown/Points client should likewise share its app-owned realtime
helper or take a fully closed per-sample grant. Runtime permits only one live
reader grant per app; callbacks and grants must not be duplicated among toolbar
and controller owners. No other app is switched by this Settings slice.

Optional Quick radio callbacks and temporary grants are guarded too. A failed
activation, radio state change or release cannot lead to restoration calls,
rendering or cleanup after uncertainty. Quick preference writes remain their
existing explicit actions. This does not change boot radio policy.

## Verification limits

`test_native_time_settings.py` runs the actual `Apps/settings.c`, adapter,
controller, rasterizer and checked helpers in fresh host processes, both normal
and ASan/UBSan. It exercises both 480×800 and 400×600 paper profiles, alarm-off and
alarm-on paths, live navigation/touch flows, partial Set Time failures, basis
errors, UNSET, folds/gaps, timezone changes, flip and no-I/O/no-free retention.
Lossless production framebuffer captures and exact 180-degree pixel comparisons
are recorded. The production builder validates pinned Xtensa 8.4.0 ELF structure,
imports/exports, complete source/SDK identities and default profile byte parity.

The real Runtime/CpuPort realtime and retained-app suites run separately, as
does the real Runtime/Graph/CpuPort gate test. The real Runtime KV lifecycle
and NVS suites verify that documented IO errors retain ordinary ownership,
close NVS handles and permit checked release/fresh acquisition. Those results
support the isolated `PortableSetTime` typed-metadata-error correction. These prove those source barriers,
not that the Settings mock-provider fixture is a real-device execution. Sparse
Clock/controller/adapter/X4-client regressions remain independent checks. None
of this qualifies physical RTC writes/ticks, panel refresh quality, touch,
power, oscillator accuracy or device sleep retention.
