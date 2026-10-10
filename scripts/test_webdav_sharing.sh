#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/webdav-sharing.XXXXXX)}"
mkdir -p "$output"
flags=(-g -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/RemoteFiles)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CC:-cc}" -std=c11 "${flags[@]}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$output/sha2.o"
"${CXX:-c++}" -std=c++17 "${flags[@]}" lib/RemoteFiles/WebDav{Core,Properties,Session,Digest,Sharing}.cpp Services/tcp_listener/service.cpp test/remote_files/webdav_sharing_test.cpp "$output/sha2.o" -lcrypto -o "$output/webdav-sharing-test"
for mode in read write unauthorized tamper cancel expire auth-expire auth-cleared close-fault storage-retained; do "$output/webdav-sharing-test" "$mode"; done
