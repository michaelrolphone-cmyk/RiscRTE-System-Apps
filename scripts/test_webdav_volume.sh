#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="${OUTPUT_DIR:-$(mktemp -d /tmp/webdav-volume.XXXXXX)}"
mkdir -p "$out"
flags=(-std=c++17 -g -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/RemoteFiles)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
"${CC:-cc}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$out/sha2.o"
"${CXX:-c++}" "${flags[@]}" lib/RemoteFiles/{WebDavCore,WebDavVolume,WebDavProperties,WebDavSession}.cpp test/remote_files/webdav_volume_test.cpp "$out/sha2.o" -o "$out/test"
"$out/test"
