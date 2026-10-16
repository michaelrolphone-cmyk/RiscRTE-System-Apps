#!/bin/sh
set -eu
BASE=/tmp/x4-settings-producer-20261016
TOOL=/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf
SYSTEM_SOURCE="$1" CATALOG_LAST= NATIVE_APP_CC="$TOOL-gcc" python "$BASE/reproduce.py" --settings-profile x4-native-time --native-time-runtime-repo /workspace/shared/watch-core-driver-digest-20261014 --resident-shell-client --resident-runtime-sdk "${RESIDENT_SDK:-/workspace/shared/x4-t023-repair-20261014/runtime/sdk/app}" --resident-policy --alarm-client --tagged-alarm-utilities "$BASE/utilities-history" --home-desk-lock --touch-scrolling --settings-list-scrolling --ble-broadcast --home-app default.elf --return-app springboard.elf --raster-snapshot --unpadded-hours --experimental-atomic-settings --experimental-settings-availability --output-dir "$2"
"$TOOL-objcopy" --discard-all --remove-section=.xt.prop --remove-section=.xt.lit "$2/settings.elf" "$2/settings-discard.elf"
wc -c "$2/settings-discard.elf"
sha256sum "$2/settings-discard.elf"
