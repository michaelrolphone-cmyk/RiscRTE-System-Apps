# Shared Wi-Fi product gate

This is offline qualification tooling, not a rollout or physical test claim.
The selected published Wi-Fi app is internal in Watch, Wave and X4. The X4
scene provider also has a retained SD source and internal fallback.

## Verified inputs

Published inventory passed in System-Apps run 42001924476, job 127159009399:
https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/42001924476

- Watch: Wi-Fi1.1.32, 148744 bytes, SHA256
  9d44fa73b99dad6fc2c58b82cd75838ae917d4a9caae19eca56a7b1e677c18b6.
- Wave: Wi-Fi1.1.34, 169460 bytes, SHA256
  25a5b8c1f36f4c9ea4edf7a6bcae5b890264a646bbbd2bca37e8c153c5233247.
- X4: Wi-Fi1.1.30, 260116 bytes, SHA256
  b48f179a82666079dd27f6d045dca5f7c0a1ad76a6ceddc8d590ea2a88fa8662.
- Watch/Wave display name Wi-Fi, icon solid:f1eb; X4 omits both.
- Text provider0.1.8 and profile files remain untouched.
- Watch lacks atomic grants and the pinned Watch Runtime fixture does not enable
  atomic storage. Wave has atomic namespaces6 and1, availability6. X4 has
  atomic6 and availability6.
- Watch/Wave omit resident_shell. X4 lists wifi_settings.elf as foreground.

These are blockers, not permission to remove old features, install a fabricated
storage provider, add test-only Runtime feature defines, or invent a shell policy.

## Gate behavior

All predecessor grants must remain declared and retained, in order. Only a
uniquely resolved ui.scene grant can be appended. Every required capability
must already have authority. Foreground shell policy must already be present.
A smaller candidate manifest therefore fails until explicit feature/lifecycle
migration is implemented and reviewed. Merely declaring unused legacy grants
does not prove behavioral parity; app/lifecycle tests are additionally required.

Only the selected Wi-Fi ELF/manifest, scene ELF/manifest, scene SD successor and
their exact boot rows can change. Scene requirement/provision schemas and all
bindings remain unchanged. X4's old addressed scene image and internal fallback
are retained. Providers require source/hash/connectivity receipts and exact
font-license files. Generated extra profiles are deliberately not installed.

Qualification compares the candidate with constrained reassembly, preserves all
other files, roundtrips SPIFFS/envelopes/SD ZIP, validates the selected Runtime ELF
loader, and invokes unchanged pinned product Runtime compilation templates in
normal and ASan/UBSan modes. It never adds atomic support or capacity defines.
The helper marks success only after every actual Runtime check passes.
No target instructions or hardware are executed. The previously installed
cohort is used as preserved input, not newly certified.

## Proposed CI steps

Keep this gate separate from the native candidate smoke tests. A blocked gate
must fail; do not use continue-on-error to label a product qualified.

Check out Runtime712ecc3f894dba99d9c9638941778a82f5856b97, PinnedSystem
710b200021b983ea55eb86a6fbddbbf5ce10a984 and Bootfs
ab9ff33975680c5bb8d9a2e7ed346d0300732a4e. Build from the exact same clean
System-Apps commit as the script checkout. Then run:

    python scripts/test_shared_wifi_products.py
    python scripts/build_wifi_manager.py --runtime Runtime --output build/wifi-manager
    python scripts/build_scene_services.py --runtime Runtime --output build/providers/watch --host-profile watch-compact
    python scripts/build_scene_services.py --runtime Runtime --output build/providers/wave --host-profile color-dynamic
    python scripts/build_scene_services.py --runtime Runtime --output build/providers/x4 --host-profile full
    python scripts/inspect_shared_wifi_baseline.py --system PinnedSystem --bootfs-tools Bootfs/scripts --output build/baseline
    python scripts/assemble_shared_wifi_products.py --system PinnedSystem --bootfs Bootfs --baseline build/baseline --baseline-lock docs/shared-wifi/baseline.json --shared build/wifi-manager --providers build/providers --output build/candidate
    python scripts/qualify_shared_wifi_products.py --runtime Runtime --system PinnedSystem --bootfs Bootfs --baseline build/baseline --baseline-lock docs/shared-wifi/baseline.json --shared build/wifi-manager --providers build/providers --candidate build/candidate --output build/qualification

Upload build receipts, all provider licenses, inventories, compiler commands,
admission logs and qualification receipts even on failure. Publish no product
candidate while parity or real Runtime admission remains blocked.

Implementation adapted from Utilities a6c33d75d616d56c9fe1889c8ae234c5079c4e5f
shared SDR product tooling; Wi-Fi uses its own measured internal layout and
strict unmigrated authority rules.
