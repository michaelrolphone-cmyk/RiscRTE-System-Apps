#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/files-peer-ui.XXXXXX)}";mkdir -p "$output"
flags=(-g -O1 -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/NativeApps/include -Ilib/RemoteFiles -Ilib/UserFiles -I../runtime/src -I../runtime/sdk/app -DPORTABLE_FILE_SHARING_SELECTED_VOLUME)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
objects=()
for source in WebDavCore WebDavVolume WebDavProperties WebDavSession WebDavDigest WebDavSharing PortableFileSharing WebDavClient WebDavRemoteVolume PortablePeerFiles;do
 "${CXX:-c++}" -std=c++17 "${flags[@]}" -c "lib/RemoteFiles/$source.cpp" -o "$output/$source.o";objects+=("$output/$source.o")
done
for source in lib/UserFiles/UserFilesVolume.cpp test/remote_files/peer_ui_backend.cpp;do
 object="$output/$(basename "$source").o";"${CXX:-c++}" -std=c++17 "${flags[@]}" -Wno-misleading-indentation -c "$source" -o "$object";objects+=("$object")
done
for source in lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c lib/PortableApps/src/PortableNetworkSession.c lib/PortableApps/src/PortableFileSetup.c;do
 object="$output/$(basename "$source").o";"${CC:-cc}" -std=c11 "${flags[@]}" -c "$source" -o "$object";objects+=("$object")
done
for profile in rgb paper paper-scroll;do
 selected=();if [[ "$profile" == paper* ]];then selected+=(-DFILE_BROWSER_PAPER_PROFILE);fi
 if [[ "$profile" == paper-scroll ]];then selected+=(-DPORTABLE_TOUCH_SCROLL);fi
 "${CC:-cc}" -std=c11 "${flags[@]}" ${selected[@]+"${selected[@]}"} -Wno-unused-function -c test/native_apps/file_browser_mount_test.c -o "$output/$profile.o"
 "${CXX:-c++}" "${flags[@]}" "$output/$profile.o" "${objects[@]}" -o "$output/$profile"
 for scenario in actions failures watch-export;do mkdir -p "$output/$profile-$scenario";"$output/$profile" "$scenario" "$output/$profile-$scenario";done
done
