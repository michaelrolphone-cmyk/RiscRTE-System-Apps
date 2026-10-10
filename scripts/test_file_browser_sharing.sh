#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-/tmp/files-sharing-ui-proof}";mkdir -p "$output"
flags=(-g -O1 -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/NativeApps/include -Ilib/RemoteFiles)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
for source in WebDavCore WebDavProperties WebDavSession WebDavDigest WebDavSharing PortableFileSharing;do
 "${CXX:-c++}" -std=c++17 "${flags[@]}" -c "lib/RemoteFiles/$source.cpp" -o "$output/$source.o"
done
"${CC:-cc}" -std=c11 "${flags[@]}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$output/sha2.o"
"${CC:-cc}" -std=c11 "${flags[@]}" -c lib/PortableApps/src/PortableNetworkSession.c -o "$output/network.o"
extra=();scenes=(back-off cancel-prepare policy-off policy-busy policy-context empty borrowed owned end-pending lease-busy tcp-terminal expiry)
if [[ "${FILE_SETUP_TEST:-0}" == 1 ]];then
 flags+=(-DFILE_SETUP_TEST)
 "${CC:-cc}" -std=c11 "${flags[@]}" -c lib/PortableApps/src/PortableFileSetup.c -o "$output/setup.o"
 extra+=("$output/setup.o")
 scenes=(ble-pair ble-reject ble-stale ble-close-retry ble-back-retry ble-owner-loss ble-expiry ble-cancel-start)
fi
for profile in rgb paper;do
 selected=();if [[ "$profile" == paper ]];then selected+=(-DFILE_BROWSER_PAPER_PROFILE);fi
 "${CC:-cc}" -std=c11 "${flags[@]}" "${selected[@]}" -c test/native_apps/file_browser_sharing_test.c -o "$output/fixture-$profile.o"
 "${CXX:-c++}" "${flags[@]}" "$output/fixture-$profile.o" "$output"/{WebDavCore,WebDavProperties,WebDavSession,WebDavDigest,WebDavSharing,PortableFileSharing,sha2,network}.o "${extra[@]}" -o "$output/test-$profile"
 for scene in "${scenes[@]}";do
  mkdir -p "$output/$profile-$scene"
  "$output/test-$profile" "$scene" "$output/$profile-$scene"
 done
done
