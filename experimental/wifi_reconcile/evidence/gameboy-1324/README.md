# Selected Game Boy namespace-1 audit

Audited 2026-10-11. Read-only product/source audit with a fresh isolated reproduction. No selected binary, production source, or packaging recipe was changed.

## Result

The exact selected X4 0.1.90 Game Boy 1.3.24 is source-reproduced and is a **namespace-1 reader only**. Its complete shared-settings key inventory is `time_zone`, `time_format`, and `brightness`, each read at application launch. It performs no namespace-1 writes or default-repair writes. Selected-byte uncertainty is closed by byte-for-byte reproduction, not merely inferred from nearby source or strings.

Selected file: `/workspace/shared/x4-shared-settings-090/composition-contexts-preflight/SD/Apps/gameboy/gameboy.elf`.

- Selected and freshly compacted ELF: 164,604 bytes, SHA-256 `4e3f218fdee9c8344ca38ac040c337bebfd72ad76fc67832df077e999c7ea794`.
- Fresh uncompacted ELF: 256,432 bytes, SHA-256 `fa9942510091b77a4ee092f4ff5c517ccf74b639c6e4f94b549acd7e025d990b`.
- Source: `a579dedd2a76956c7dcd654434cff9a556beb37c`, clean checkout from the preserved `gameboy.bundle`.
- Repository: https://github.com/michaelrolphone-cmyk/T5S3-GameBoy/tree/a579dedd2a76956c7dcd654434cff9a556beb37c
- Runtime build dependency: `274bc66f193cbe29018d2a85c9400cb0dce8aacc`, clean checkout from `runtime.bundle`.
- Toolchain: official Xtensa ESP32-S3 GCC 8.4.0, crosstool-NG `esp-2021r2-patch5`; objcopy 2.35.1.20201223.
- Exact transformation: `objcopy --remove-section=.xt.lit --remove-section=.xt.prop --discard-all`.

The historical X4 .55 `build-custody.json` records precisely this before/after hash transformation under `compactions.gameboy`. Reproduction here independently confirms it. The source is an explicitly identified 1.3.24 reconstruction of the recovered original UI; it does not claim to be missing 1.3.23 revision `e429db588040c19d6ca152066c36a32a564afa0a`.

## Exact inputs and fresh checks

Source seal: `/workspace/shared/gameboy-1324-source-seal`.

- Game Boy Git history: `gameboy.bundle`; dependency Git history: `runtime.bundle`.
- SDK: `inputs/sdk`.
- System timezone implementation: `inputs/system/lib/PortableApps/src/PortableTimeZone.c`, `PortableTimeZoneCatalog.c`, and `PortableTimeZonePreference.c`.
- The build receipt records every source, generated source and compiled dependency hash. All source maps, staged-source maps and the 69 compiled dependency content hashes match the earlier sealed target receipt.
- `fresh-target/build.json` is the new compilation receipt; `fresh-build.log` records successful native ELF structural validation and the ELF layout audit (4 mapped sections and 2,834 verified relative relocations).
- `gameboy.compacted.elf` passed `cmp` against the exact selected ELF.
- `evidence.json` records machine-readable custody, selected boot-grant entry, file/section hashes and equality assertions.
- `replay.sh COMPILER_PATH NEW_OUTPUT_DIRECTORY` repeats the fresh source build and exact compaction without altering the product inputs.

The fresh compilation used `gameboy/capability/build.py` with the seal SDK/System source, and the pinned Runtime checkout. No saved ELF/object was used as a compilation input.

## Namespace binding and complete call graph

The selected composition's `store/boot.json` entry with manifest `gameboy.json` selects `Apps/gameboy/gameboy.elf` on volume 9 and has exactly one `storage.key-value@1` grant, `instance_id: 1`. `capability/backend.cpp:335` calls `rt->acquire(name, 1, 0, &grant)` for each capability. Instance 0 means the unique authorized provider; it is not a request for KV namespace 0. The sole authorized KV provider here is namespace 1. `backend.cpp:336` stores that table in the private `preferences` pointer.

Launch path: exported `app_main` -> private `begin_application` -> the three preference loads below. All repository references to the actual `preferences` table are confined to `capability/backend.cpp`; it is not handed to the game, UI, storage implementation or another module.

1. `backend.cpp:345` -> `portable_timezone_preference_load` -> KV `get("time_zone", capacity=44)`.
   - Reader implementation: sealed System `PortableTimeZonePreference.c:30-49`.
   - 44-byte `TZ` version-1 record, XOR checksum, canonical catalog ID and zero padding.
   - Missing, unavailable or malformed data selects the helper's UTC fallback. The result is resolved into the private timezone rule at line 346.
   - Never writes. The companion save function is compiled in its source translation unit but discarded by section GC and absent from the final symbol table.
2. `backend.cpp:347` -> inline `portable_time_format_load` -> KV `get("time_format", capacity=4)`.
   - Header: sealed SDK `PortableTimeFormat.h:20-33`.
   - Four-byte encoding `0x54, 1, mode, mode XOR 0xa5`; mode 0 is 12-hour, 1 is 24-hour.
   - Missing, invalid or unavailable data selects 12-hour. No writes.
3. `backend.cpp:348` -> inline `pqa_preference_load(..., "brightness", 40, 0, ...)` -> KV `get("brightness", capacity=1)`.
   - Header: sealed SDK `PortableQuickPreferences.h:15-27`.
   - One byte, accepted values 0 through 100. Missing means virtual 40%; malformed/unavailable data leaves the initialized 40% launch state unchanged.
   - The generic validation checks that `kv->put` is non-null, but it never invokes it. This is not a writer path.

All three loads occur once per launch. There is no shared-settings refresh loop or change subscription in this selected build. `cap_clock_text` uses the cached timezone/time-format state.

## Apparent writer paths ruled out

- `night_light_set_brightness_tenths` calls `display->set_brightness`, changing the current display level only. `night_light_shutdown` restores the cached launch level. These functions never call KV.
- Upstream `src/night_light.cpp` contains Arduino Preferences writes, but that file is not a compilation input of `capability/build.py`. This target uses the backend replacements.
- The helper headers expose `pqa_preference_save`, `pqa_dnd_save`, and `portable_time_format_save`; none is used by the compiled Game Boy source or linked into the ELF.
- `portable_timezone_preference_save` is likewise absent from the linked ELF.
- Emulator globals named `preferences_cgb_speed` and `preferences_ppu_timing` are private game-core variables, not key/value API usage.
- ROM/session save/load operations use `storage.volume`, not namespace-1 KV.

The original unstripped-equivalent build symbol table and disassembly are saved as `fresh-symbols.txt` and `fresh-disassembly.txt`. Relevant addresses: `begin_application` 0x19ba0 (size 0x595), `portable_timezone_preference_load` 0x1334c, load implementation 0x131f8, and `app_main` 0x1a484. The inline format and brightness reads are in `begin_application`; the latter checks the put field at offset 16 for non-null but calls only the get field at offset 12.

## Scope limits

This closes the exact selected-byte provenance and namespace-1 caller inventory. It does not establish a new migration, a shared-settings refresh design, hardware behavior, or execution of the Xtensa instructions on a device. No new hardware test or behavioral test suite was run for this read-only audit. The fresh target compiler, structural validator, ELF layout audit, compaction equality, and complete source/call-path review are the new verification evidence.
