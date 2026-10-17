#!/bin/sh
set -eu
: "${SYSTEM_SOURCE:?System reconstruction checkout}"
: "${NATIVE_TIME_RUNTIME_REPO:?Runtime repository containing exact native-time SDK object}"
: "${RESIDENT_RUNTIME_REPO:?Runtime a91edf3 checkout}"
: "${UTILITIES_REPO:?Utilities e801723 checkout}"
: "${NATIVE_APP_CC:?Xtensa ESP32-S3 GCC8.4 executable}"
: "${OUTPUT:?Build output directory}"
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export SYSTEM_SOURCE NATIVE_APP_CC
CATALOG_LAST= python "$HERE/reproduce.py" --settings-profile x4-native-time --native-time-runtime-repo "$NATIVE_TIME_RUNTIME_REPO" --resident-shell-client --resident-runtime-sdk "$RESIDENT_RUNTIME_REPO/sdk/app" --resident-policy --alarm-client --tagged-alarm-utilities "$UTILITIES_REPO" --home-desk-lock --touch-scrolling --settings-list-scrolling --ble-broadcast --home-app default.elf --return-app springboard.elf --raster-snapshot --unpadded-hours --experimental-atomic-settings --experimental-settings-availability --output-dir "$OUTPUT"
"${NATIVE_APP_CC%-gcc}-objcopy" --discard-all --remove-section=.xt.prop --remove-section=.xt.lit "$OUTPUT/settings.elf" "$OUTPUT/settings-discard.elf"
sha256sum "$OUTPUT/settings.elf" "$OUTPUT/settings-discard.elf"
EXPECTED=1c4914ed12749cb076019dd255c40939d73a6be10f1eda069c4e5d68b48148b7
printf '%s  %s\n' "$EXPECTED" "$OUTPUT/settings-discard.elf" | sha256sum -c -
if [ -n "${RETAINED_ELF:-}" ]; then cmp "$RETAINED_ELF" "$OUTPUT/settings-discard.elf"; fi
