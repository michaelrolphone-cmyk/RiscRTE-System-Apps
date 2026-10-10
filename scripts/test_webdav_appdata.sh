#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
runtime="${1:?Pass matching Runtime source}"
output="${OUTPUT_DIR:-$(mktemp -d /tmp/webdav-appdata.XXXXXX)}"
mkdir -p "$output"
cmp "$runtime/sdk/app/RiscAppDataExportV1.h" lib/PortableApps/include/RiscAppDataExportV1.h
flags=(-std=c++17 -g -Wall -Wextra -Werror -Ilib/RemoteFiles -I"$runtime/sdk/app" -I"$runtime/src")
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CXX:-c++}" "${flags[@]}" lib/RemoteFiles/WebDavCore.cpp lib/RemoteFiles/WebDavProperties.cpp lib/RemoteFiles/WebDavSession.cpp "$runtime/src/runtime/storage/AppDataFiles.cpp" test/remote_files/webdav_appdata_integration.cpp -Wl,--wrap=close -o "$output/webdav-appdata-test"
for mode in read write create scope auth propfind race retained; do
 stage=$(mktemp -d "$output/$mode.XXXXXX")
 "$output/webdav-appdata-test" "$stage" "$mode"
done
