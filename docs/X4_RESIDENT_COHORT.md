# Initial X4 resident application cohort

This recipe builds `default.elf`, `springboard.elf` and `settings.elf` for the
resident Runtime contract. It produces development artifacts and a proposed
policy report, not an installable store or product image. The delivered .43
image is the inventory authority: all 21 applications remain recorded, and the
Springboard catalog keeps its 19 launchable entries. Nothing is removed to make
the initial three-app subset appear complete.

Run with the existing pinned compiler:

```sh
python3 scripts/build_x4_resident_cohort.py \
  --system /workspace/shared/x4-resident-shell-system \
  --runtime /workspace/shared/runtime-resident-catalog-union-0185 \
  --utilities /workspace/shared/points-catalog-storage-custody \
  --productivity /workspace/shared/points-catalog-productivity \
  --product /workspace/shared/x4-resident-desk-product-040 \
  --sdk /workspace/shared/x4-provider-sdk-recovered \
  --baseline /workspace/shared/x4-043-image \
  --compiler /workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
  --output /workspace/shared/x4-resident-initial-cohort
```

`examples/x4-resident-cohort.json` freezes source commits, SDK hashes, delivered
manifest identities and binary custody receipts. The command rejects changed
inputs. `commands.json` records each exact builder invocation. Target receipts,
the symbol proof, inventory, proposed bindings and remaining blockers are
collected in `cohort-receipt.json` and `policy-proposal.json`.

The default keeps sparse Home, the copied Points face and timer refresh,
landscape desk lock, GPIO wake-light intent, shared Quick Actions, USB action,
radio controls, telemetry and the selected idle helper. Springboard and Settings
keep native time, their touch controllers, saved preferences and telemetry.
Springboard selects the exact .43 3-column × 4-row implementation from
`5e33fec83c08a3e6692d198bd229002daa1dc1e7`: horizontal finger-tracked paging,
dots-only footer and saved 12/24-hour unpadded-hour text. Its older formatter in
`springboard_paper.inc` is in the unselected branch. Settings keeps the selected
scrolling lists, Home desk-lock controls and explicit `--unpadded-hours`
selection. Converted children contain the
checkpoint client, never the Quick Actions rendering sources.

## Product policy changes required

The initial development selection is:

```json
{"api":1,"host":"default.elf","foreground":["springboard.elf","settings.elf"]}
```

It belongs under `boot.json.resident_shell`, with `default_app` still
`default.elf`. Each converted ELF must export the 16-byte
`risc_resident_app_descriptor_v1`: API 1, role 1 for default or role 2 for a
foreground, reserved 0. No new manifest role field or resident capability grant
is accepted. Runtime checks both the explicit boot membership and ELF role.

Replace the rebuilt manifests and reconcile their exact grants; do not retain
undeclared child sleep/radio grants. The generated policy report preserves
delivered instance bindings and all untouched app rows. Keep retained-wake and
provider-promotion authority with the host. A full product must preserve its
17-row app policy configuration, 512-byte retained payload and large metadata
capacity for the complete 21-app inventory. Runtime/provider whole-store
admission and physical qualification remain separate gates.

The proposed three-app selection is deliberately not emitted as `boot.json`.
The full Springboard catalog contains apps not yet converted. Enabling this
subset in the delivered product would deny their launches.

## Remaining conversion work

System's File Browser, Wi-Fi, OTA, App Store and USB builders already accept
explicit resident client selection; rebuild and qualify them with the final
cohort before adding their paths to `foreground`. Preserve storage/file-open,
update and USB transfer custody instead of introducing per-app sheet sources.

Utilities' current `build_native_broadcast.py`, `build_native_utc_utilities.py`
and `build_native_utc_alarm_apps.py` still force Quick rendering and old export
sets. Their explicit resident profile must select the current shared adapter,
stage the resident SDK, export its descriptor and remove only the shared sheet
sources. Preserve native alarm editors, launch guards, HID/scanner/RF controls,
telemetry exclusions and each app's storage bindings. The selected catalog
service pin is independent from these application-builder changes.

Productivity's `build_points_native_utc.py` and `build_timecard_native_time.py`
likewise need explicit resident flags, SDK/descriptor exports and dependency
receipt reconciliation. Keep the authoritative catalog/editor and civil
Timecard storage models. The selected Contexts editor comes from its separate
delivered Utilities lineage, recorded in the recipe, and must not disappear
because it is absent from the catalog-service checkout.

Two policy features also need reconciliation before full parity: the delivered
Home's Contexts observation ownership, and automatic idle/low-battery behavior
while a resident child is active. The current client builder rejects the old
independent sleep helper, while host POLL dispatch does not implement an
equivalent child policy. Do not claim those features are preserved merely
because the host has its own idle policy.

Builder versions remain their existing development identities. No release
reservation is reassigned. In particular, Settings currently emits 1.3.18,
below delivered 1.3.20; successor version allocation and stamping belongs to
the product integration before packaging.

## Accepted GameBoy boundary

The .43 GameBoy 1.3.20 binary stays accepted and parked. The recipe reads only
its delivered manifest/custody metadata; it neither opens nor builds/tests the
binary. Its catalog entry and `.gb`/`.gbc` associations remain inventoried.

Runtime .85 cannot launch this legacy ELF while a resident host is active.
Host `request_launch` is denied; child launches and `file.open` accept only
admitted foregrounds, and foregrounds require the descriptor. Omitting GameBoy
from that list denies launch; adding it does not create a legacy exception.
Handler enumeration would still advertise its admitted associations even when
the request is denied. Therefore the delivered legacy boot remains unchanged
until a separately qualified compatibility handoff exists. There is no shared
Quick Actions overlay inside the accepted legacy GameBoy ELF.
