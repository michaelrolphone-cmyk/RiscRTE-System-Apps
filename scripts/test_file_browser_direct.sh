#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="$(mktemp -d)";trap 'rm -rf "$output"' EXIT
flags=(-g -O1 -Wall -Wextra -Werror -Wno-unused-function -Ilib/PortableApps/include -Ilib/NativeApps/include -Ilib/RemoteFiles -DPORTABLE_FILE_SHARING_SELECTED_VOLUME)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
objects=()
for source in WebDavCore WebDavVolume WebDavProperties WebDavSession WebDavDigest WebDavSharing PortableFileSharing;do
 "${CXX:-c++}" -std=c++17 "${flags[@]}" -c "lib/RemoteFiles/$source.cpp" -o "$output/$source.o";objects+=("$output/$source.o")
done
for source in lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c lib/PortableApps/src/PortableNetworkSession.c lib/PortableApps/src/PortableFileSetup.c;do
 object="$output/$(basename "$source").o";"${CC:-cc}" -std=c11 "${flags[@]}" -c "$source" -o "$object";objects+=("$object")
done
for profile in rgb paper;do
 selected=();if [[ "$profile" == paper ]];then selected+=(-DFILE_BROWSER_PAPER_PROFILE);fi
 "${CC:-cc}" -std=c11 "${flags[@]}" "${selected[@]}" -c test/native_apps/file_browser_direct_test.c -o "$output/$profile.o"
 "${CXX:-c++}" "${flags[@]}" "$output/$profile.o" "${objects[@]}" -o "$output/$profile"
 for scenario in complete initial-on refuse cancel expiry active-expiry no-peer radio-fail entropy-busy $(seq -f 'stop-%g' 0 35);do "$output/$profile" "$scenario";done
done
