#!/usr/bin/env bash
# Host-only composition of the real pinned Runtime, provider and consumer.
set -euo pipefail
system="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${MOUSE_RUNTIME_SOURCE:?Set MOUSE_RUNTIME_SOURCE to the pinned Runtime checkout}"
reader="${MOUSE_READER_SOURCE:?Set MOUSE_READER_SOURCE to the pinned Reader checkout}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
# Recheck standalone invocation too; never mutate these exact upstream sources.
[[ "$(git -C "$runtime" rev-parse HEAD)" == 6a3b3d46f5b2857eacc6e9863799dec1f466b036 ]]
[[ "$(git -C "$reader" rev-parse HEAD)" == 59177418ea746c7605b7b5a5aec501e70bdae80f ]]
[[ -z "$(git -C "$runtime" status --porcelain)" && -z "$(git -C "$reader" status --porcelain)" ]]
mkdir -p "$build/sdk" "$build/mouse"
cp -R "$system/lib/PortableApps/include/." "$build/sdk/"
cp "$runtime/sdk/app/RiscRuntimeV1.h" "$runtime/sdk/app/RiscStreamClientV1.h" "$build/sdk/"
cp "$reader/Drivers/usb_hid_mouse/manifest.json" "$build/mouse/manifest.json"
san=(-g -O1)
runtime_opt=()
if [[ "${SANITIZE:-0}" == 1 ]]; then
 san+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
 # The pinned ArduinoJson empty iterator triggers GCC's maybe-uninitialized
 # diagnostic with ASan/UBSan at -O1. Keep upstream bytes and -Werror intact;
 # instrument Runtime at -O0, while consumer/provider C stays instrumented -O1.
 runtime_opt=(-O0)
fi
cc="${CC:-cc}"; cxx="${CXX:-c++}"
flags=("${san[@]}" -std=c11 -Wall -Wextra -Werror -fPIC -fvisibility=hidden -shared
 -I"$build/sdk" -I"$reader/sdk/driver" -I"$system/test/native_apps")
set -x
"$cc" "${flags[@]}" -DMOUSE_RUNTIME_HID_WITNESS "$system/test/native_apps/mouse_runtime_dependency_witness.c" -o "$build/hid.elf"
"$cc" "${san[@]}" -std=c11 -Wall -Wextra -Werror -I"$reader/sdk/driver" -I"$system/test/native_apps" -DMOUSE_RUNTIME_CLOCK_WITNESS -c "$system/test/native_apps/mouse_runtime_dependency_witness.c" -o "$build/clock.o"
"$cc" "${flags[@]}" "$reader/Drivers/usb_hid_mouse/driver.c" -o "$build/mouse/driver.elf"
"$cc" "${flags[@]}" -DPORTABLE_USB_MOUSE "$system/test/native_apps/mouse_runtime_consumer_app.c" \
 "$system/lib/PortableApps/src/PortableMouseConsumer.c" -o "$build/default.elf"
"$cxx" "${san[@]}" "${runtime_opt[@]}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -no-pie -rdynamic -DRISC_STREAM_HOST_TESTING \
 -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" \
 -I"$runtime/lib/ArduinoJson/src" -I"$runtime/test/drivers/stubs" -I"$runtime/test" -I"$system/test/native_apps" \
 "$runtime/src/bootstrap/Json.cpp" "$runtime/src/bootstrap/Board.cpp" "$runtime/src/bootstrap/Runtime.cpp" \
 "$runtime/src/runtime/streams/ProviderQueueHost.cpp" "$runtime/src/runtime/streams/AppStreamSessions.cpp" \
 "$runtime/src/runtime/drivers/ProviderGraphV2.cpp" "$runtime/src/runtime/drivers/ProviderModuleV2.cpp" \
 "$system/test/native_apps/mouse_runtime_integration_test.cpp" "$build/clock.o" -ldl -o "$build/test"
for activation in eager demand; do
 for mode in normal connected-close close-retained unregistered-clock; do
  "$build/test" "$build" "$mode" "$activation"
 done
done
