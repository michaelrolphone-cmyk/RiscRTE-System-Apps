#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/file-sharing-client.XXXXXX)}"
mkdir -p "$output"
flags=(-g -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/RemoteFiles -Itest/remote_files)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CC:-cc}" -std=c11 "${flags[@]}" -c lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c -o "$output/sha2.o"
"${CXX:-c++}" -std=c++17 "${flags[@]}" lib/RemoteFiles/WebDav{Core,Properties,Session,Digest,Sharing}.cpp lib/RemoteFiles/PortableFileSharing.cpp test/remote_files/file_sharing_client_test.cpp "$output/sha2.o" -lcrypto -o "$output/file-sharing-client-test"
for mode in oom transport-choice cancel-prepare expiry-prepare missing busy unavailable release-fault owner-loss entropy-terminal read cancel-client close-fault expiry clock-back restart; do "$output/file-sharing-client-test" "$mode"; done
