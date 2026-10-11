# Shared-settings writer audit and activation gates

Initial source audit of published System Apps
`c564dd665072db141fc42d55be3be52511750719`. This document adds no runtime rule,
capability, ABI, product selection or code behavior. The experimental Wi-Fi
model and staging layer are not an audit of every bundled writer.

## Namespace 6: explicit Wi-Fi editing

`Apps/wifi_settings_portable.inc` calls `portable_wifi_profile_save` for profile
Save and `portable_wifi_profile_forget` for Forget. It also retains the separately
compiled legacy single-record save/forget path. `PortableWifiProfiles.h` stages
profile count, credential chunks and selector through the existing KV1 codec.
The experimental Stage can produce one bounded complete operation, but the live
UI does not use it yet. Both compiled paths need an explicit selection and test;
a new native backend alone cannot make old sequential puts an atomic intent.

The native result must distinguish local durability, pending SD persistence,
conflict, capacity, uncertain write and retained custody. An uncertain native
write must be reloaded/inspected, not blindly replayed. A successful staged RAM
resolution is not a native save or SD acknowledgement. No intent, record digest,
SSID or credential chunk belongs in a diagnostic log.

## Namespace 1: different sources of writes

These sources must not all be labeled as explicit user edits merely because
KV1 `put` was called:

- Manual Settings controls: `lib/PortableApps/src/settings.inc` and
  `settings_timezone.inc` save sleep timers/mode, time format, alert mode, desk
  direction/face, reader preference and timezone from UI choice handlers.
- Quick Controls: `quick_session.c` saves brightness, tone, volume, restore
  values, Contexts enable and DND; `quick_radios.c` saves radio policy. Several
  UI actions require multiple related values, such as restore volume plus
  current volume. Future explicit intent batching must preserve those relations.
- Automatic low-battery policy: `include/PortableLowBattery.h` observes a
  battery crossing, persists `battery_edge`, then updates sleep timers,
  brightness and radio flags. It deliberately avoids replaying an old crossing
  over later manual changes. This is a policy event, not a user preference edit.
- Automatic Contexts policy: `include/PortableContextsClient.h`,
  `include/PortableContextActions.h`, `src/contexts_policy.inc` and
  `src/contexts_rules_adapter.inc` apply claimed room/rule events. Existing code
  consumes the event before preference writes and reports partial failure.
  Merely retrying those old puts after SD return could defeat that behavior.
  A namespace batch alone does not atomically include the separate Contexts
  service's event claim; that cross-component boundary needs an explicit design.
- RTC basis: `include/PortableRtcBasis.h` and `src/PortableSetTime.c` persist
  basis/transition information associated with setting the hardware clock.
  These bytes cannot be classified as ordinary independent preference values
  without checking the clock transition, target device and recovery semantics.
- Context presets/rules and broadcast enable also have save helpers in
  `PortableContextPreferences.h`, `PortableContextRules.h` and
  `PortableBroadcastClient.h`; their callers and actual selected profile still
  need complete classification.

This is an initial source inventory, not proof that all selected app/provider
binaries are audited. Utilities-owned clients, bound provider key maps and
legacy binaries must be checked against the eventual exact product inventory.

## Required integration work

1. Preserve app-owned semantics. Assign explicit operation origins and expected
   preconditions; do not infer intent by comparing values or by intercepting
   arbitrary legacy `put`. Decide policy-event and clock-state reconciliation
   separately from manual preferences. Runtime remains schema-blind.
2. Specify and qualify a copied, namespace-bound native interface for atomic
   local intent, held rebase views and finish. Do not alter old KV1/KV2 layouts,
   hand an incompatible v3 table to old code, or expand exact-key provider grants.
3. Validate first adoption and migration. The new RTENSD1 reader proves content,
   not authority to replace a native dataset. Missing, corrupt, legacy format,
   unknown native base, foreign lineage and an intentionally new card need
   distinct decisions. Preserve old native rows/journals and SD data until the
   replacement is durable. Initial dataset creation remains unimplemented.
4. Keep ordinary native reads usable without SD. Restore/reconcile on bounded
   worker paths and expose truthful progress/retry states. After native values
   change, consumers must invalidate stale cached defaults/preferences using a
   revision signal. Launch readiness remains an in-memory check scoped to the
   namespaces actually needed; no launch-path SD scan, checksum or revalidation.
5. Complete credential hygiene in the real worker/package/provider copies and
   AppData verifier. Current experimental wiping does not cover every production
   scratch buffer. Retained resource custody must remain latched until reset.
6. Measure native image/pending-group space, recovery headroom, peak RAM, worker
   latency and flash wear on the selected target. Logical large-profile tests
   do not establish that their data fits the current native byte budget.
7. Qualify live app saves, reloads, interruptions, policy events, connection
   behavior, card removal/USB export, format migration and device power cuts.
   Do not enable shared namespaces using old unaudited client binaries.

All of this remains separate from X4 .90, whose shared-settings selection stays
absent. Its general/Wi-Fi values remain native-only, and full initial flashing
still erases them. Software prototype roundtrip tests must not be described as
an enabled or hardware-qualified persistence feature.

## Checked X4 selection inventory

The follow-up [writer inventory](WRITER_INVENTORY.md) and its
[machine-readable evidence](WRITER_INVENTORY.json) cover all 26 apps/32 providers
in the actual .90 preflight selection, Utilities fc0bdbb, exact provider grants,
writer origins, atomic groups and cache invalidation. The accompanying read-only
checker verifies selected bytes and source inputs. Preserved-binary provenance
gaps remain explicit; this does not enable shared settings or qualify hardware.
