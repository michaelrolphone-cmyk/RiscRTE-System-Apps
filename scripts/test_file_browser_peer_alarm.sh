#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/files-peer-alarm.XXXXXX)}";mkdir -p "$output"
runtime="${RUNTIME_ROOT:-../runtime}"
flags=(-g -O1 -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/NativeApps/include -Ilib/RemoteFiles -Ilib/UserFiles -I"$runtime/src" -I"$runtime/sdk/app" -DPORTABLE_FILE_SHARING_SELECTED_VOLUME)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
objects=()
for source in WebDavCore WebDavVolume WebDavProperties WebDavSession WebDavDigest WebDavSharing PortableFileSharing WebDavClient WebDavRemoteVolume PortablePeerFiles;do
 "${CXX:-c++}" -std=c++17 "${flags[@]}" -c "lib/RemoteFiles/$source.cpp" -o "$output/$source.o";objects+=("$output/$source.o")
done
for source in lib/UserFiles/UserFilesVolume.cpp test/remote_files/peer_resident_backend.cpp;do
 object="$output/$(basename "$source").o";"${CXX:-c++}" -std=c++17 "${flags[@]}" -Wno-misleading-indentation -c "$source" -o "$object";objects+=("$object")
done
for source in lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c lib/PortableApps/src/PortableNetworkSession.c lib/PortableApps/src/PortableFileSetup.c;do
 object="$output/$(basename "$source").o";"${CC:-cc}" -std=c11 "${flags[@]}" -c "$source" -o "$object";objects+=("$object")
done
utilities="${UTILITIES_ROOT:-../utilities}"
cc_flags=("${flags[@]}" -I"$utilities/lib/Alarm/include" -I"$utilities/test/native_apps" -I"$runtime/sdk/driver")
"${CC:-cc}" -std=c11 "${cc_flags[@]}" -Wno-unused-function -c test/native_apps/file_browser_peer_alarm_provider.c -o "$output/provider.o"
objects+=("$output/provider.o")
for source in PortableTimeZone PortableTimeZoneCatalog;do
 "${CC:-cc}" -std=c11 "${cc_flags[@]}" -c "lib/PortableApps/src/$source.c" -o "$output/$source.o";objects+=("$output/$source.o")
done
for profile in rgb paper;do
 selected=();if [[ "$profile" == paper ]];then selected+=(-DFILE_BROWSER_PAPER_PROFILE);fi
 "${CC:-cc}" -std=c11 "${cc_flags[@]}" "${selected[@]}" -Wno-unused-function -c test/native_apps/file_browser_peer_alarm_test.c -o "$output/$profile.o"
 "${CXX:-c++}" "${flags[@]}" "$output/$profile.o" "${objects[@]}" -o "$output/$profile"
 for scenario in ${SCENARIO:-guarded connected borrowed lease-end retained-begin retained-end};do "$output/$profile" "$scenario";done
done
