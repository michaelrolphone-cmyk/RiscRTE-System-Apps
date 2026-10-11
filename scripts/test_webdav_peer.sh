#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="${OUTPUT_DIR:-$(mktemp -d /tmp/webdav-peer.XXXXXX)}"
flags=(-g -Wall -Wextra -Werror -Wno-misleading-indentation -Ilib/PortableApps/include -Ilib/RemoteFiles -Ilib/UserFiles -I../runtime/src -I../runtime/sdk/app)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
"${CC:-cc}" -std=c11 "${flags[@]}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$out/hash.o"
"${CXX:-c++}" -std=c++17 "${flags[@]}" lib/RemoteFiles/WebDav{RemoteVolume,Client,Core,Volume,Properties,Session,Digest,Sharing}.cpp lib/RemoteFiles/PortablePeerFiles.cpp lib/UserFiles/UserFilesVolume.cpp test/remote_files/webdav_peer_test.cpp "$out/hash.o" -o "$out/peer"
"$out/peer"
