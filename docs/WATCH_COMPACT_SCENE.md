# Explicit Watch compact scene profile

`--host-profile watch-compact` builds `scene-host` **0.4.3** with
`-DSCENE_HOST_TELEMETRY=0 -DSCENE_HOST_WATCH_240=1` and the source-controlled
`Services/scene_host/manifest-watch-compact.json`. It requires display, raw touch,
clock and presentation profile; it does not require navigation hardware.

The default remains the complete `scene-host` **0.4.2**, with telemetry enabled.
The compact profile is opt-in. Both use the shared implementation and retain
all accepted keyboard fixes. The paired `text-input-host` stays **0.1.4** and is
built from unchanged production source. The compact-color presentation profile
is also unchanged. A deployment selects exactly one scene-host implementation.

## Exact optional boundary

The optional telemetry extension and its typography/icon rendering are omitted.
The explicit 240×240 RGB565 specialization also excludes unreachable 2×
component font tables. All 1× component fonts and both keyboard font sets
remain. Other display dimensions or formats fail closed before subscription. The returned table ends at the reading-links suffix; its `struct_size`
is exactly `sizeof(risc_scene_reading_links_api_v1)`. Consequently,
`risc_scene_telemetry_get_v1` returns NULL. The compact host rejects the
telemetry document flag, the eleven telemetry node kinds (INSTRUMENT through
CODE_ROW), and telemetry icons (GAUGE through GEAR) before copying a document
or making provider calls. Clients must discover the optional telemetry API
before using those components. Unsupported telemetry is never silently drawn
as ordinary content.

Legacy scenes, navigation, shared keyboard/password controls, components,
pages, checkpoints, reading, linked reading, document icons and their fonts
remain available. Legacy CHIP controls, component fonts, paper keyboard fonts,
remain compiled; the default full source still supports both component densities. No function-section garbage
collection, LTO or blanket symbol stripping is used.

## Reproduction

Build with the Xtensa ESP32-S3 GCC 8.4.0 esp-2021r2-patch5 toolchain:

```sh
export NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc
python scripts/build_text_input_services.py \
  --runtime /path/to/RiscRTE-at-1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70 \
  --host-profile watch-compact --output /new/output/directory
```

The optional telemetry glyph file is not required for a compact target build.
Tests that compare full and compact profiles require its reproducible generation
(`Pillow==12.3.0`, `python scripts/generate_telemetry_fonts.py`). Each build records
its profile, optional API availability, compile defines, source and SDK hashes,
compiler, raw artifact hash and manifest. The explicit compact manifest changes
package version together with artifact functionality.

Run the preserved suites with `--host-profile watch-compact`:

```sh
python scripts/test_scene_host.py --runtime "$SDK" --host-profile watch-compact
python scripts/test_scene_host.py --runtime "$SDK" --host-profile watch-compact --sanitize
python scripts/test_scene_components.py --runtime "$SDK" --host-profile watch-compact
python scripts/test_scene_compact_profile.py --runtime "$SDK" --output /new/profile-proof
python scripts/test_scene_telemetry.py --runtime "$SDK"
python scripts/test_text_input_runtime.py --runtime "$WATCH_RUNTIME" \
  --host-profile watch-compact --sanitize --output /new/runtime-proof
```

`SDK` is Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70`;
`WATCH_RUNTIME` is production Runtime `267aab6d9382ce4921a1536fab1620749b2f5301`.
The touch-only Runtime qualification removes the navigation provider entirely
and uses actual touch header actions for Back/Home and an actual keyboard touch
for a pending-frame edit. Only the three navigation-callback fault modes are
inapplicable. Password burst/repeated-key accept and cancel cases run twice
through app unload/reopen, across all four provider activation policies.

## Size and verification limits

The raw compact scene ELF is 246,456 bytes. The existing Watch
`loader-metadata-v3` compactor reduces it to 159,128 bytes, versus 297,836 bytes
for the full current presenter, a saving of 138,708 bytes. It preserves allocated
sections, program headers, imports/exports, retained symbols and relocation
semantics and retains the original ELF. The text host is 12,044 raw / 7,740
compacted bytes. This repository does not replace the device compactor or change
its removal policy.

See `receipts/watch-compact-scene-043-20261012.json` for artifacts and proof.
Hardware display, touch timing and actual-device flashing remain untested.
The device product repository owns SPIFFS composition, loader admission and
remaining free-block validation; source qualification alone is not a flash claim.
