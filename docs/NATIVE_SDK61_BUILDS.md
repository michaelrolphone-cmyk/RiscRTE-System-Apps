# Native SDK 6.1 shared application builds

The ordinary application and capability-service builders run with the
ESP-IDF 6.1 compiler without installing Arduino, PlatformIO, an Arduino SDK
package, or building firmware. Export the SDK toolchain before invoking the
existing commands:

```sh
. "$IDF_PATH/export.sh"
python scripts/build_all_apps.py
python scripts/build_portable_settings.py
python scripts/build_portable_wifi.py --alarm-client --navigation --full-frames --wifi-instance 15
python scripts/build_portable_file_browser.py --alarm-client --navigation
python scripts/build_portable_springboard.py
python scripts/build_portable_updates.py --services-only --rtc-utc-offset-seconds 28800
python scripts/build_entropy_service.py
python scripts/build_tcp_listener_service.py
```

The tested SDK is ESP-IDF commit
`fff9895c82d744c7237be8847347bdd1b07c6643`, with
`esp-15.2.0_20251204` GCC 15.2.0. The independent
`native-sdk61.yml` workflow installs only that official compiler archive,
verified against the SHA-256 in the pinned SDK's `tools/tools.json`.

## Compiler selection and target safety

`scripts/native_toolchain.py` supplies one selection rule to the ordinary
app, portable app, service, shared Watch source, and scene builders:

1. Explicit `--cc` (where provided), then `NATIVE_APP_CC`.
2. The exported `xtensa-esp32s3-elf-gcc` target frontend on PATH.
3. The same-installation ESP32-S3 frontend beside an exported unified compiler.
4. An existing legacy tool-only package under `PLATFORMIO_CORE_DIR`.

An invalid explicit override fails instead of silently selecting a different
compiler. A unified compiler override is accepted only by selecting its
same-installation S3 frontend. A bare `xtensa-esp-elf-gcc` defaults to a generic
big-endian Xtensa configuration; it is never invoked directly. The target
frontend also chooses the matching S3 libgcc multilib. Symlink names are kept.

Legacy GCC 8.4 remains usable through an explicit compiler override or the
existing package fallback. The release-byte-parity workflow and exact historical
cohort/replay qualification scripts keep their pinned legacy compiler contract.
A GCC15 development build is not evidence of equality with a GCC8 release,
nor does it publish a new package or change product composition.

## Public ABI audit

The current shared Apps, Services, PortableApps and RemoteFiles sources do not
pass native `time_t`, `struct timespec`, `struct stat`, `struct timeval`,
`struct tm`, `jmp_buf`, or libc reentrancy structures through ordinary imports.
Time, storage, network and lifecycle interfaces use the existing versioned,
fixed-width capability SDK. The generated artifacts retain their existing
import/export checks and real ELF structural validator.

The Files item count now casts its uint32 value to the existing `%u` argument
type. GCC15's target uint32 typedef is unsigned long; the explicit 32-bit cast
retains the displayed value and permits `-Werror=format` builds without
weakening warnings or changing app behavior.

ArduinoJson, where consumed by the installed-app provider from Runtime, is a
standalone header-only JSON dependency and does not require Arduino runtime.
GCC15's C++ freestanding subset omits the allocation declarations it needs;
that provider selects hosted library declarations while still linking with
`-nostdlib -nostartfiles` and enforcing its unchanged ordinary-import ceiling.
Explicit JSON array lifetimes and integer status conversions retain strict
GCC15 warnings without changing selection or provisioning behavior.

## Local verification

- All 18 inventory application ELFs: GCC15 build, manifest and loader checks.
- Portable Settings, Wi-Fi, Files, Springboard, software-update providers,
  entropy, TCP listener and USB SD Transfer: GCC15 ELF imports/exports and
  loader checks.
- Internal-package and installed-app providers: GCC15 links and exact import
  audits against the native Runtime SDK headers.
- 129 Python source/builder contract tests.
- Files paper host fixture: raster, 513-item paging, filtering, preview, errors,
  and touch lifecycle under ASan/UBSan.
- Installed-app service, installation/download/provisioning, media and feed
  host fixtures under ASan/UBSan, including interruption/restart boundaries.
- GCC8 rebuild of all 18 inventory apps and release parity checker: 14/18
  byte-identical; all mandatory unchanged-version checks pass. The other four
  are already newer development versions.
- Portable Files also rebuilt and loader-validated with GCC8.

LeakSanitizer is unsupported by the local ptrace-based execution sandbox, so
host sanitizer tests were repeated with `ASAN_OPTIONS=detect_leaks=0`; ASan
and UBSan remained enabled. No device execution, install, catalog publication
or hardware qualification is implied. New remote CI has not run until this
source is published.
