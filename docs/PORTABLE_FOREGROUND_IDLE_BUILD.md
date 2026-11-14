# Opt-in foreground returning Light build

This is a development build selection for ordinary foreground apps. It does not
install an app, choose physical provider instances, change a boot profile, or
qualify hardware behavior. With every `--foreground-idle-*` flag omitted, the
existing builder pins, definitions, sources and include paths are unchanged.

## Shared selection

The Settings, Springboard and Wi-Fi builders accept:

- `--foreground-idle-helper-dir PATH`: explicitly select returning foreground
  Light and link `foreground_light_sleep.c` and `foreground_idle_preferences.c`
  from that directory.
- `--foreground-idle-runtime-dir PATH`: a local canonical Runtime checkout whose
  HEAD is exactly `273bdbccf65647a803ab0586a418f8283732baec`. The builder reads
  immutable committed SDK/license bytes, not mutable checkout header copies.
- `--foreground-idle-drivers-sdk-dir PATH`: current Drivers `sdk/driver` capability
  headers, including display power, generic power sleep and power-key drain.
- `--foreground-idle-fallback-seconds N`: missing-key fallback, 5–3600 seconds.
  This opt-in candidate defaults to 15 if the flag is omitted. It is a build
  input, not a new product policy. A valid confirmed namespace-1 `sleep_idle`
  value overrides it. Invalid or unavailable preferences defer automatic idle.

All three paths must be provided together. The seconds flag alone is an error.
`--navigation` is also required: its live adapter grant keeps the filtered
power-key dependency alive after the temporary helper key grant is released.
The deployment must wire navigation to that filtered power-key provider; this
builder does not choose or grant its physical instance.
The feature cannot be combined with a resident host/client or the separate X4
idle helper. It supplies these definitions:

```
-DPORTABLE_FOREGROUND_IDLE
-DPORTABLE_APP_SLEEP_LOCAL
-DPORTABLE_NATIVE_CUSTODY_FENCE
-DPORTABLE_FOREGROUND_IDLE_FALLBACK_SECONDS=<N>u
```

The `--default-sleep-mode` Settings option remains independent. In particular,
Home's selected Deep mode never changes ordinary foreground suspension from
Light to Deep or Hybrid. No existing preference is rewritten by this selection.

## One staged SDK root

`portable_foreground_idle_build.configure(...)` is the reusable recipe. It returns
`(include_root, definitions, additional_sources)` and stores its receipt on the
argument namespace. Utilities loads this exact module from its explicitly pinned
System checkout rather than duplicating the recipe.

The builder stages the selected System PortableApps include tree and sibling
time tree into `foreground-idle-sdk`. Headers are overlaid in this order:

1. Drivers `sdk/driver`.
2. Canonical Runtime `sdk/driver`.
3. Canonical Runtime `sdk/hardware`.
4. Canonical Runtime `sdk/app`.

Runtime therefore owns every overlapping ABI declaration. Drivers supplies its
new capability headers absent from Runtime. The compiler gets this one SDK root
plus the existing non-SDK NativeApps interface directory. This is intentionally
not a stack of `-I` overlays: a quoted include from a Portable header must not
silently bind its bundled stale neighbor. No bundled SDK file is edited.

## Capability declarations and namespace custody

The app manifest adds `power.sleep@1`, `display.power@1`, `input.power-key@1` and
`storage.key-value@1` if absent. Physical providers and their grants must be
selected separately by the deployment integration owner.

The preference helper performs `get` only on exact namespace 1 and releases that
grant before ordinary app work. The capability itself still exposes write and
delete methods; it is not an inherently read-only grant. Receipts preserve
Stopwatch's private namespace 2 and Wi-Fi's private namespace 6, adding namespace
1 rather than replacing the private binding or selecting wildcard instance 0.

## Evidence and host consumption

The app build record contains `foreground_idle`, full `compile_command` and
`build_defines`. `compile_inputs` exposes the complete source list and hashes,
include directories and definitions directly for host consumers. Its feature receipt captures the exact Runtime pin, helper
sources and hashes, builder hash, source repository states, each staged header's
origin and hash, and time-tree hashes. `licenses/foreground-idle/SOURCES.json`
also stores the feature receipt and accompanying Runtime license.

A host integration should consume the recorded actual source list, definitions
and staged headers from this recipe/record. It should not rebuild a hand-written
second list. The ordinary manifest declares capability names and versions only;
no capability declaration is proof that a physical grant or sleep path works.

## Focused checks

```
python scripts/test_foreground_idle_build.py
python scripts/test_settings_sleep_defaults.py
```

These host checks cover selection/no-op boundaries, validation, canonical quoted
include resolution, source immutability, fallback bounds, additive namespaces,
and independent Home Deep preference selection. Target build verification still
requires the selected SDKs and ESP32-S3 compiler; hardware qualification is
explicitly outside these builder tests.

The selected Runtime advances from 7c6b0ac to 273bdbc for product-sized AppData
mount geometry. All app/driver/hardware SDK headers are byte-identical across
that change; native time, sleep and capability lifecycle sources are unchanged.
Exact HEAD admission remains enforced.

## Canonical SDK versus native implementation

The SDK pin remains273bdbccf65647a803ab0586a418f8283732baec. A clean descendant
Runtime checkout is accepted only when the entire `sdk` Git tree and `LICENSE`
blob equal that canonical revision. All staged headers still come from immutable
canonical Git objects, not working files. Receipts separately record canonical
SDK commit, actual checkout commit, compared object IDs and cleanliness. Native
implementation equivalence is not asserted. Dirty, unrelated, modified-SDK and
modified-license checkouts are rejected. Historical omitted profiles are unchanged.
