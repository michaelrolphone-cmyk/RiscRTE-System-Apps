# X4 Home automatic preference qualification

Baseline: System `a4c7805a531acf53c91fee089371481db425a2e4`.
This opt-in work does not activate product selection or change source-conflict policy.

## Controller audit before changes

- Low battery consumes an observed valid crossing before the first storage read.
  Missing `battery_edge` means high. Equal persisted state makes no write.
  Entry writes and verifies the edge first, then independently attempts idle,
  deep, brightness, and radio in that order. Timer saves skip an already encoded
  equal value; brightness and radio saves do not. OFF brightness is preserved,
  including `quick_bright`. A high crossing writes only the edge. Unknown samples
  and charging alone do not rearm. Partial errors never replay the consumed edge.
- Home rules evaluate before preference access, so the service owns consumption.
  Fields are attempted in ascending rule-field order, continuing after failures.
  KEEP succeeds without writing. Low battery refuses power, idle, deep and
  brightness fields. NOTIFY can write DND and then alert mode, short-circuiting
  if DND fails. Home does **not** apply the Contexts editor mask `0x274`.
  Per-rule reports use the successful field bitmap. Workflow message/timer/launch
  claiming and execution follow the preference reports.
- Room presets select one valid identity match, release read storage, claim the
  room edge, pause, then apply preferences. Applicability and low-battery masks
  remain in the existing helper. Writes stop on first failure; volume can write
  restore metadata before volume. The report compares applied bits to the full
  selected actions, so filtered-out fields still report partial. Claimed edges
  are not replayed. Preset records and rule/model storage are not migrated here.
- Missing records use virtual defaults. No boot/default-repair writes are added.
  Equal timer/power writes are suppressed only where the original helpers do so.

## Intended transport boundary

Only explicit low-battery, Home-rule, and claimed-preset boundaries create an
AUTOMATIC operation. A scoped adapter runs the existing save helpers in their
original order, using atomic same-Core reads and fixed revision. Each existing
put is a separate copied atomic operation with a recognizable automatic intent;
only the adapter's own successful collected receipt advances its revision.
No generic legacy put gains authority, no USER intention is manufactured, and
failed or uncertain operations are not replayed. Native durability does not
claim SD acknowledgement. Foreign/conflicting source selection remains open.

## Selected implementation

`--experimental-atomic-home-automatic` requires atomic Quick, X4 idle, RF-only
Contexts and the existing sparse resident Home profile. It is default-off.
`PortablePreferenceAutomatic.h` supplies only the explicit, private automatic
scope; normal legacy KV and USER writers are unchanged. `home_automatic.inc`
keeps the capability, copied operation and terminal-custody state alive for the
whole controller boundary. No selected Home automatic write falls back to KV1.

PFA0001 is a separate 176-byte intent, with automatic origin at byte 8 and a
controller identifier at byte 11: 1 low battery, 2 Home rules, 3 Home preset,
4 foreground Contexts. Remaining fields preserve the exact key, expected
presence/value, desired value, fixed revision and CRC layout documented in the
header. It must never enter USER edit replay. The foreground identifier admits
only its editor keys; wiring that controller belongs to the Utilities change.

Existing puts remain separate transactions, intentionally preserving edge-first
publication and partial-failure behavior. Busy before acceptance retries only
the same copied mutation and revision; an accepted operation is collected once.
Back/Home cancellation never replays the consumed crossing or rule edge. Stale,
conflicting and uncertain operations fail without same-value readback being
mistaken for confirmed success. A terminal ordinary IO retains the original
helper's exact readback behavior. Timers/power still skip an already-persisted
same value; DND/radio/brightness still submit when their original helper did.

The preset hook is compiled only for the explicit profile. Default Home and
Watch retain their original helper bodies and generated behavior. The ordinary
Home rule mask and preset applicability mask remain distinct. Workflow actions,
app-data, timer namespaces and rule/model persistence are unchanged.

## Qualification

The focused host matrix exercises actual low-battery observation, Home automatic
binding, original setting encoders, claimed-preset selection and the production
Home rule loop. Normal and ASan/UBSan variants cover consume/rearm, persisted
manual overrides, invalid samples, OFF and restore metadata, same-timer skips,
unconditional same-value controls, partial writes, preset stop-on-failure and
mask filtering, rule continuation and reports, NOTIFY short-circuit, message /
timer / launch claims, fixed revisions, busy, stale, cancellation races, wrong
namespace, refusal, malformed receipts and retained custody. The API double
verifies exact copied automatic intents and denies legacy writes.

Target on/off builds pass import/export and structural ELF validation. The
reconstructed commands are in `receipts/home-automatic/target-*-command.json`.
The default-off ELF remains SHA256
`9b42c85959995290831b6ae87a3fdab8583de70fd4e3c5e567517b1d6407fb44`
and its manifest remains
`fc16120d123932426974be2f253b4e22e4082745869240dc4065c689e8c7555b`.

Two older aggregate fixtures already fail on unchanged baseline `a4c7805`:
`test_context_client.py` assumes a cached policy automatically reloads at line
93, and `test_low_battery.py` Settings adapter case 2 expects the old 25-second
value at line 50. Both failures were reproduced before and after this change;
neither was repaired in this bounded work. The low-battery policy matrix,
adapter cases 0/1, Contexts preference tests and atomic Quick regression matrix
are separately run. Host doubles and target compilation are not hardware,
SD durability, live source-selection or product-activation qualification.

Final receipts: `receipts/home-automatic/qualification.json`, `host.json`,
`sanitized.json`, `quick-regression.json`, and `quick-regression-sanitized.json`.
47 focused Home cases pass in each mode; 41 atomic Quick regression cases pass
in each mode. Selected Home ELF SHA256 is
`f2312a9f817f210125479a6ae682024569478f73ecffc8e707d66c3fd46dc732`.
The selected manifest is byte-identical to the atomic Quick manifest, so this
binding adds no capability or namespace grant. Invalid incomplete profile
selection is rejected before build. There is no selected preset legacy fallback.
