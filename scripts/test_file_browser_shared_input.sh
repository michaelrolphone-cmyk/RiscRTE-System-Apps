#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/files-peer-ui.XXXXXX)}";mkdir -p "$output"
flags=(-g -O1 -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/NativeApps/include -Ilib/RemoteFiles -Ilib/UserFiles -I../runtime/src -I../runtime/sdk/app -DPORTABLE_FILE_SHARING_SELECTED_VOLUME)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -no-pie);fi
if [[ "${FILES_RESIDENT:-0}" == 1 ]];then flags+=(-DFILE_POLICY_RESIDENT);fi
objects=()
if [[ "${FILES_POLICY:-0}" == 1 ]];then
 utilities="${UTILITIES_ROOT:?Set UTILITIES_ROOT for policy fixtures}"
 flags+=(-I"$utilities/Apps" -I"$utilities/lib/Alarm/include" -I"$utilities/lib/Contexts/include")
 for source in quick_actions quick_render quick_session quick_radios;do
  "${CC:-cc}" -std=c11 "${flags[@]}" -c "lib/PortableApps/src/$source.c" -o "$output/$source.o";objects+=("$output/$source.o")
 done
fi
for source in WebDavCore WebDavVolume WebDavProperties WebDavSession WebDavDigest WebDavSharing PortableFileSharing WebDavClient WebDavRemoteVolume PortablePeerFiles;do
 "${CXX:-c++}" -std=c++17 "${flags[@]}" -c "lib/RemoteFiles/$source.cpp" -o "$output/$source.o";objects+=("$output/$source.o")
done
for source in lib/UserFiles/UserFilesVolume.cpp test/remote_files/peer_ui_backend.cpp;do
 object="$output/$(basename "$source").o";"${CXX:-c++}" -std=c++17 "${flags[@]}" -Wno-misleading-indentation -c "$source" -o "$object";objects+=("$object")
done
for source in lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c lib/PortableApps/src/PortableNetworkSession.c lib/PortableApps/src/PortableFileSetup.c;do
 object="$output/$(basename "$source").o";"${CC:-cc}" -std=c11 "${flags[@]}" -c "$source" -o "$object";objects+=("$object")
done
"${CC:-cc}" -std=c11 "${flags[@]}" -Isdk/app -I../runtime/sdk/driver -IServices/text_input -Dt5_driver_get=files_text_driver_get -c Services/text_input/host.c -o "$output/text-host.o"
objects+=("$output/text-host.o")
profiles="${FILES_PROFILES:-rgb paper paper-scroll}"
for profile in $profiles;do
 selected=();if [[ "$profile" == paper* ]];then selected+=(-DFILE_BROWSER_PAPER_PROFILE);fi
 if [[ "$profile" == paper-scroll ]];then selected+=(-DPORTABLE_TOUCH_SCROLL);fi
 "${CC:-cc}" -std=c11 "${flags[@]}" "${selected[@]}" -Isdk/app -I../runtime/sdk/driver -Wno-unused-function -c "test/native_apps/${FILES_TEST_SOURCE:-file_browser_shared_input_test.c}" -o "$output/$profile.o"
 "${CXX:-c++}" "${flags[@]}" "$output/$profile.o" "${objects[@]}" -o "$output/$profile"
 for scenario in ${FILES_SCENARIOS:-entry-accept entry-cancel entry-home entry-pending entry-home-pending local-rename local-nonascii local-filter local-mkdir local-cancel local-home local-invalid local-long local-unavailable remote-rename remote-actions remote-mkdir remote-cancel remote-home remote-invalid remote-long remote-unavailable remote-expiry local-retained};do mkdir -p "$output/$profile-$scenario";if [[ "${FILES_POLICY:-0}" == 1 ]];then "$output/$profile" "$scenario";else "$output/$profile" "$scenario" "$output/$profile-$scenario";fi;done
done
