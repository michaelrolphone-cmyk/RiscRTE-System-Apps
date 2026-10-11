# X4 .90 shared-settings writer inventory

Audit input: the actual `composition-contexts-preflight/{store,SD,receipts}`
selection, including `store/boot.json`, plus delivered `seed/qualification.json`
and all eight seed receipts. Starting gate document: System
`b4d9daace86af542cd8752b3d5ce3f3deccbe52d`.

This is a source/authority inventory, not an activation decision. Shared settings
remain absent from boot installation selection, and the composition receipt says
`shared_settings_enabled: false`. No live client, ABI, manifest, product or
production code is changed. No hardware or end-to-end persistence result is
claimed.

## Evidence and coverage

`WRITER_INVENTORY.json` records every selected app/provider, actual ELF and
manifest hashes, routing, exact KV grants, source classifications and provenance
gaps. `check_writer_inventory.py` verifies those inputs against the supplied
composition and source checkouts. It rejects omitted modules, altered grants,
images, source files, receipts and accidental shared-settings enablement. It does
not automatically prove the manual semantic classifications below.

- 26 selected apps; 20 have namespace 1, five have namespace 6, 21 have either.
- 32 selected providers. Only Alarm and Civil Clock have namespace-1 authority,
  and their grants are exact-key read-only. No provider has namespace-6 authority.
- Five app rebuild receipts match selected images: Home, Springboard, Files,
  Wi-Fi, Contexts. Installed Apps has its separate service rebuild receipt and no
  namespace-1/6 KV grant.
- 72 reviewed source files and 16 evidence files are hashed. Source inputs:
  - S: System `6213aa7ae4256a0e639bcfb4a88de32ab2565e3e`, the actual composition
    source checkout, rather than assuming the experimental branch built .90.
  - U: Utilities `fc0bdbbbc600ce891e60c44d091fce0586ade7d8`.
  - P: Productivity `0dea3d6c96513559f8eed8634e19a2e6c1454633`, a located reference
    source for Points and Timecard, explicitly not proof of their selected ELFs.
- Seed receipts establish preserved-byte identity, not compile flags or current
  source equivalence. They do not upgrade preserved clients to audited rebuilds.
  A version match alone is insufficient: a located Game Boy 1.3.24 build has a
  different ELF hash from the selected Game Boy 1.3.24 image.

S/U/P path and line references below refer to those exact source inputs. The JSON
retains full image hashes rather than repeating every hash in prose.

## Selected app coverage

“Reference” means the checked source behavior is known, but the selected preserved
binary's exact source/build equivalence remains **unverified**. Namespace grants
are whole-namespace KV1 grants; a source-level read-only classification does not
reduce the existing authorization.

| Selected app/version | Namespace 1 source behavior | Namespace 6 source behavior | Evidence |
| --- | --- | --- | --- |
| Home/default 0.4.11 | Explicit Quick Controls; automatic low-battery and Contexts; preference reads | No grant | Rebuilt, receipt-bound S |
| Springboard 1.8.4 | Read-only preferences/broadcast policy; host owns controls | No grant | Rebuilt, receipt-bound S |
| Files 1.5.28 | Read-only preferences/broadcast policy | Existing-network reader; Direct Share does not read/write saved profiles | Rebuilt, receipt-bound S |
| Wi-Fi 1.1.27 | Read-only general/radio/display preferences | Explicit Save/Forget; deferred remember after successful user Connect | Rebuilt, receipt-bound S |
| Contexts 0.4.2 | Explicit enable/source/timing; automatic rule settings | No grant | Rebuilt, receipt-bound U+S |
| Settings 1.3.28 | Explicit preferences; user-triggered RTC/native/basis transition | No grant | Reference S; selected binary unverified |
| BLE Scanner 0.2.24 | Explicit identity aliases and Enable Bluetooth; radio-policy reads | No grant | Reference U; selected binary unverified |
| Battery 1.1.18 | Explicit BLE broadcast enable/disable; reads | No grant | Reference U; selected binary unverified |
| Alarms 0.2.20 | Native app's timezone/time-format proxy is read-only; schedules use namespace 3 | No grant | Reference U; selected flags unverified |
| Countdown 0.1.24 | Same native read-only preference proxy; schedule writes use namespace 3 | No grant | Reference U; selected flags unverified |
| Stopwatch 0.1.25 | No app-domain shared writer found; record writes use namespace 2 | No grant | Reference U; selected adapter unverified |
| Calculator 0.1.25 | No app-domain shared writer found; common adapter reads | No grant | Reference U; selected adapter unverified |
| BLE Touchpad 0.1.23 | Own domain reads quick_radio | No grant | Reference U; selected adapter unverified |
| BLE Buttons 0.1.25 | Own domain reads quick_radio; assignments use namespace 11 | No grant | Reference U; selected adapter unverified |
| Waterfall 0.3.2 | Own domain reads quick_radio; current temporal flags suppress Contexts rule/preset application | No grant | Reference U; exact selected temporal adapter unverified |
| Points in Time 0.6.17 | Reference native preference proxy is read-only; domain writes use namespace 5 | No grant | Reference P; selected binary unverified |
| Timecard 0.2.15 | Reference native proxy reads time format/timezone; files use app-data | No grant | Reference P; selected binary unverified |
| OTA Update 1.2.15 | Reference common preference reads | Reference default-profile reader only | Reference S; selected binary unverified |
| App Store 1.5.1 | Reference common preference reads | Reference default-profile reader only | Reference S; selected binary unverified |
| Game Boy 1.3.24 | Still unverified | No grant | Preserved bytes; located same-version source build mismatches |
| Daily Paper/daily_digest 0.1.0 | No grant | Still unverified | Preserved bytes; historical X4 ledger names source ba486e56 but source call graph not verified here |
| USB Transfer 0.1.10; Lists 0.1.1; Model Viewer 1.3.0; Hollow Trail 1.2.1; eBook Reader 0.1.8 | No direct namespace-1 grant | No direct namespace-6 grant | Authority checked; no claim about unrelated persistence |

Utilities resident builders link the System adapter and reject independent
embedded QuickActions/idle builds (U `scripts/resident_client_build.py:70-79`).
Do not multiply Home's automatic writer list across every foreground app simply
because it includes common headers. The current Waterfall temporal recipe defines `PORTABLE_RADIO_SESSION` and
`PORTABLE_BLE_FOREGROUND` (U `scripts/build_temporal_x4.py:44`), so S
`contexts_policy.inc:131-155` excludes rule/preset application despite linking
Contexts monitoring. Preserved-binary flags must still be recovered before
asserting those paths are absent from an older ELF.

## Namespace 6: origins and complete operations

### Wi-Fi edit paths

The selected receipt enables both `PORTABLE_WIFI_PROFILES` and
`PORTABLE_WIFI_REMEMBER_CONNECTED`.

- Explicit Save: S `Apps/wifi_settings_portable.inc:586-625,866-873` copies the
  draft, searches by SSID, then runs the profile codec. Capture explicit identity,
  original draft and expected prior record before any asynchronous work; never
  reconstruct intent from a later default or changed UI cache.
- Explicit Forget: `:773-799` waits for checked radio disconnection and uses the
  selected profile. The future operation must bind the intended logical SSID and
  expected record, not retain only the numeric slot that a returning card may
  assign to another profile.
- Successful-Connect remember: `:332-355,410-420,489-499,529-541,608-610` snapshots
  the requested draft and only queues Save after the existing successful-link
  condition. Classify this as a user-triggered deferred edit. Capturing a draft
  and its preconditions before the asynchronous join **must not durably commit a
  profile at Connect start**. Failed/cancelled joins keep the existing no-save
  behavior; Back can cancel pending saving (`:904`).
- The legacy single-profile branches (`:792,870`) remain source paths but are not
  selected by this receipt. They need their own admission decision if reused.

S `PortableWifiProfiles.h:14-84` and `PortableWifiCredentials.h:232-338` define the
atomic application group required by the experimental staging layer:

| Operation | Required single native publication |
| --- | --- |
| Append Save | `wifi.profiles` count + inactive two credential chunks + selector + original intent/operation ID |
| Update/reuse Save | Two chunks + selector + original intent/operation ID; retain restored collection count |
| Forget | Tombstone selector + zero overwrites of all four chunks + original intent/operation ID; count remains unchanged |
| Already applied Save/absent Forget | Resolution marker bound to original operation; no fabricated new data writes |

Slot zero uses `wifi.commit`, `wifi.a0/a1/b0/b1`; other slots use
`w%08x.c/a0/a1/b0/b1`. Profile identity reconciliation stays app-owned. Preserve
all original pending groups through candidate install until the exact resulting
SD checkpoint is acknowledged. Compare native/staged-view revision AND pending
frontier; same-value explicit edits and absent deletes are still intent.

### Readers and invalidation

- Files peer networking acquires namespace 6 in S
  `lib/RemoteFiles/PortablePeerFiles.cpp:209`, then uses
  `PortableNetworkSession.c:39-51,169-181`. That proxy refuses writes and reads
  default profile zero. Direct sharing explicitly uses transient credentials and
  no saved profile (`Apps/file_browser_direct.inc:1`).
- OTA/App Store reference code calls `portable_wifi_saved_network_connect`
  (`Apps/update_portable.inc:185,321-323`;
  `PortableWifiSavedNetwork.h:58-88`). It reads the original slot-zero layout and
  never saves. Exact preserved binaries remain unverified.
- Daily Paper has namespace-6 authority and a matching historical binary-source
  ledger, but its actual writer/reader call graph was not established here. It is
  a remaining activation gate, not an assumed read-only consumer.

Rebase must invalidate saved count/page/name caches and selected indices while
preserving active user drafts and captured connection requests. Existing initial
BUSY retry is insufficient once an always-usable native replica returns ordinary
values before a later card reconciliation. Do not automatically reconnect or
persist a copied request merely because the backend revision changed. All
credential copies, intents and prior-record digests stay out of logs.

## Namespace 1: checked writer families

| Origin | Exact keys | Source/caller boundary |
| --- | --- | --- |
| Explicit Settings confirmation | `time_format`, `time_zone`, `sleep_mode`, `sleep_idle`/`sleep_deep` when exposed, `alert_mode`, `desk_clock_face`, `desk_direction`, `reader_language`, `reader_flip_ui` | S `settings.inc:529,560,597,647,684,713,744`; `settings_timezone.inc:78`. Selected preserved Settings compile flags remain unverified. |
| Explicit Home Quick Controls | `brightness`, `quick_bright`, `frontlight_tone`, `alarm_volume`, `quick_volume`, `alert_dnd`, `contexts_on`, `quick_radio` | S `quick_session.c:135-157,180-240`; `quick_radios.c:68-98`; selected Home enables QuickActions, QuickRadios, frontlight tone and paper transitions. Applicability still depends on hardware/output modes. |
| Automatic Home battery crossing/rearm | `battery_edge`; on low entry `sleep_idle`, `sleep_deep`, `brightness` unless intentionally off, `quick_radio` | S `PortableLowBattery.h:36-63`; `quick_adapter.inc:60-123`; `resident_policy.inc:6-39`. Edge is consumed before preference effects, with partial-once behavior. |
| Explicit Contexts controls | `contexts_on`, `context_sources`, `context_timing` | U `Apps/contexts.c:66,97-107`; S `PortableContextsClient.h:127-136`. |
| Automatic selected Contexts app rules | `sleep_idle`, `quick_radio`, `brightness`, `alert_dnd` | U `contexts.c:150-166`; `contexts_controller.inc:48-57`; receipt mask `0x274` applies during execution as well as editing. |
| Automatic Home generic rule executor | `sleep_mode`, `sleep_idle`, `sleep_deep`, `quick_radio`, `brightness`, `alarm_volume`, `alert_dnd`, `alert_mode` as returned by a rule | S `contexts_rules_adapter.inc:24-37` and `PortableContextActions.h:5-17`. This executor does not apply the foreground editor's `0x274` mask. Existing rules therefore need broader audit. |
| Explicit BLE alias edit/clear, reference U | `bs` + address-type digit + 12 lowercase address hex digits | U `ble_sensor_names.h:9-39`, `ble_scanner.c:172-197`. Each 40-byte record binds address type+address; an empty name is a tombstone, not a KV delete. |
| Explicit BLE Scanner Enable Bluetooth, reference U | `quick_radio` | U `ble_scanner_paper.inc:48-68`; hardware mutation and checked rollback accompany the preference write. |
| Explicit Battery broadcast switch, reference U | `ble_broadcast` | U `battery_power.inc:332-342` -> S `broadcast_adapter.inc:47-58` -> `PortableBroadcastClient.h:70-87`. The old “never writes preferences” comment is inaccurate for the broadcast build. |
| Device/RTC transition state, user-triggered | `rtc_basis` | S `settings_native_time.inc:70-101` -> `PortableSetTime.c:160-244`: verified physical RTC write, native realtime seeding, then basis metadata. Not an independent portable preference. |

Additional source paths are deliberately not overclassified:

- `ctx_p0`…`ctx_p7` are one-record legacy presets with source/slot/name identity
  (`PortableContextPreferences.h:83-118`). The checked sources expose a save
  helper, but the caller search found only tests, not a selected current UI save
  caller. Home can read/apply existing presets through the fallback path
  (`PortableContextsClient.h:143-213`). Do not infer a live writer merely from the
  helper. Preserved old client equivalence remains unverified.
- `watch_face` is supported by generic face callbacks but selected Home and
  Contexts builds supply no face catalog/count; their callbacks refuse it. The
  broader Watch/older Waterfall variant remains outside proven X4 selection.
- Context rules/models are whole shared-data files (`context-rules.ctx`,
  `context-fingerprints.cfp`), not namespace-1 KV. Namespace-1 snapshotting alone
  does not back them up or synchronize their semantic dependencies.
- Native Alarms/Countdown use a read-only preference proxy (U
  `alarm_native_utc_io.h:117-127`). The old `alarm_volume` editor in
  `alarm_nova.inc:17-42` is excluded by `ALARM_NATIVE_UTC`; exact preserved flags
  must be established before relying on that exclusion.

## Provider authority: preserve exact keys

The selected `boot.json`, not examples or a generic service header, is authority.

| Provider | Namespace/key grants |
| --- | --- |
| Alarm | NS1 read `alert_mode`, `alert_dnd`, `time_zone`; NS3 read `alarm_utc_cfg`, `timer_utc_cfg`; NS4 read-write `alarm_utc_occ`, `timer_utc_occ`, `points_utc_occ`; NS5 read `points_utc_cfg`, `points_utc_meta`. Separate NS5 app-data: `points.catalog` read, `points.ledger` read-write. |
| Civil Clock X4 | NS1 read `time_zone` only. |
| HID | NS10 read-write `hid_ours`, `hid_peer`, `hid_ccc`, `hid_identity`; outside this audit's shared selection. |
| Contexts, Broadcast, other 27 providers | No bound KV grant. Contexts receives policy and returns decisions; Broadcast receives app-supplied policy. |

Alarm has **no `alarm_volume` grant**, despite a conditional source read for that
key. It cannot be silently added during persistence integration. Source evidence:
U `Services/alarm_service/service.c:109-132,229-242,498-508`;
S `Services/civil_clock/clock.c:21-38`;
U `Services/contexts/fingerprint.h:156-166` and
`Services/telemetry_broadcast/service.c:70-86`. The exact selected provider images
match internal fallback bytes; current-source equivalence of these preserved
images is not established solely by that equality.

## Required atomic boundaries and cache invalidation

These are integration requirements identified by the audit, not implemented
contracts or new rules imposed on existing product behavior.

1. **Manual value intent:** record origin and expected prior state at the action
   boundary, including same-value choices. Defaults/missing reads are not edits.
   Scalar records remain single-record operations, but their causal origin cannot
   be inferred from key name: Contexts and manual controls share many keys.
2. **Restore pairs and radio RMW:** mute needs `quick_volume` + `alarm_volume`;
   paper light-off needs `quick_bright` + `brightness`. Preserve a confirmed way
   back in one group. `quick_radio` already contains active/airplane/previous bits
   in one record; serialize whole-record RMW against both Contexts and controls.
   Frontlight/radio hardware effects need their existing checked compensation
   and retained-custody outcomes alongside, not falsely included in, KV atomicity.
3. **Policy event identity:** low-battery crossing marker plus its intended
   effects require device/session-aware event semantics. Restoring an old
   `battery_edge` must not rearm/replay an old crossing over newer manual edits.
   Existing partial-once behavior must not become retry-until-overwrite.
4. **Contexts transaction boundary:** rule evaluation/claim consumes service
   state before sequential KV effects and reports APPLIED/PARTIAL. A namespace
   batch alone cannot atomically include the separate service's claim/cursor,
   rule-file generation, timer workflow in NS3 or physical radio/display effects.
   Preserve event identity/order and unresolved partial outcomes. Do not replay
   old automatic actions after card return as if they were new user saves.
5. **RTC basis:** bind `rtc_basis` to the actual target hardware/native clock
   transition and verified epoch. A portable SD record cannot by itself authorize
   rewriting the RTC or reinterpret a clock on another device. Exclusion or an
   explicit migration protocol is required before general namespace-1 adoption.
6. **Identity aliases:** reconcile BLE names by address type+address, preserving
   explicit tombstones and expected original record. Do not interpret alias
   discovery, missing/corrupt reads, or observed defaults as user edits.
7. **Native revision invalidation:** after native rebase, notify readers at a
   bounded safe foreground boundary even if prior reads succeeded or returned
   MISSING. Invalidate/reload the following without replacing active drafts:
   - Home `desk_configuration`, basis/rule/format/direction, sleep mode and future
     retained record. Existing hydration (`paper_sparse_clock.inc:131-180,373-386`)
     retries unavailable startup reads; it is not a general rebase subscription.
   - Quick session levels/restore values, radio flags, idle timers and observed
     battery-edge state; `resident_policy.inc:34-39` already refreshes at a policy
     boundary but supplies no dataset revision handshake.
   - Contexts `loaded`, `settings_valid`, preset/event/rule state. Policy is cached
     indefinitely until invalidation (`PortableContextsClient.h:48-55,84-113`).
     Never clear a partial-event/save-failed fence by pretending a reload is an edit.
   - Broadcast `loaded`/policy, preserving its unconfirmed-save fail-closed state.
     The existing one-second refresh is not an atomic snapshot contract
     (`PortableBroadcastClient.h:53-87`).
   - Foreground paper flip and native alarm/time-format caches. Paper's retry is
     only for initial unavailability (`foreground_adapter_open.inc:27-50`,
     `adapter.c:1833-1864`); native alarm apps cache zone/format at open (U
     `alarm_native_utc_io.h:67-74,125-127`). Points/Timecard reference sources also
     cache format. Native toolbar zone reads are fresh, with a BUSY cached-display
     fallback (S `PortableNativeTimeSource.c:60-93,112-136`).
   - Alarm reconciliation and sleep tickets: refresh/restart staged reads on a
     revision change so alert mode/DND/timezone cannot come from mixed datasets.
     Keep active alarm output/custody. Civil Clock reloads timezone per read.
   - Wi-Fi profile/list/selection caches as described above.
   Retained timer-only wake must continue using its retained record; do not add
   KV/SD work there. Apply any new dataset during ordinary foreground ownership.
8. **Checked readback:** existing helpers sometimes accept IO only after exact
   fresh readback. A staging/backend cache must not “verify” against a stale
   generation. Preserve uncertainty and original pending intent on refusal,
   allocation/capacity failure, cancellation or unknown publication outcome.

## Verification and remaining gate

Example check command (paths are caller-supplied checkouts/artifact directories):

```
python3 experimental/wifi_reconcile/check_writer_inventory.py \
  --composition /path/to/composition-contexts-preflight --seed /path/to/seed \
  --system /path/to/System-6213aa7 --utilities /path/to/Utilities-fc0bdbb \
  --productivity /path/to/Productivity-0dea3d6 --negative-tests
```

Result: 26 apps, 32 providers, exact routes/grants/hashes, 72 source files and
16 evidence inputs passed; seven altered/incomplete input cases were rejected.
No target build or app execution was performed for this documentation checkpoint.

Before activation: resolve every preserved shared-capable ELF's exact provenance
and selected compile path (especially Settings, BLE Scanner, Battery, temporal
Waterfall, Game Boy and Daily Paper), update all actual writer boundaries, define
policy/RTC reconciliation, implement revision-aware readers, and run interruption
and returning-card tests on the integrated native receiver. Whole-namespace
legacy KV1 puts cannot safely be relabeled “explicit user intent.”
