#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/webdav-core.XXXXXX)}"
mkdir -p "$output"
flags=(-std=c++17 -g -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/RemoteFiles)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CC:-cc}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$output/sha2.o"
"${CXX:-c++}" "${flags[@]}" lib/RemoteFiles/WebDavCore.cpp lib/RemoteFiles/WebDavVolume.cpp lib/RemoteFiles/WebDavProperties.cpp test/remote_files/webdav_core_test.cpp "$output/sha2.o" -o "$output/webdav-core-test"
"$output/webdav-core-test"

"${CXX:-c++}" "${flags[@]}" lib/RemoteFiles/WebDavCore.cpp lib/RemoteFiles/WebDavVolume.cpp lib/RemoteFiles/WebDavProperties.cpp lib/RemoteFiles/WebDavSession.cpp test/remote_files/webdav_session_test.cpp "$output/sha2.o" -o "$output/webdav-session-test"
"$output/webdav-session-test"
