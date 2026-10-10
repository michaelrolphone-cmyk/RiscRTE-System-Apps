#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/tcp-listener-service.XXXXXX)}"
mkdir -p "$output"
flags=(-std=c++17 -g -Wall -Wextra -Werror -Ilib/PortableApps/include)
if [[ "${SANITIZE:-0}" == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
fi
"${CXX:-c++}" "${flags[@]}" test/native_apps/tcp_listener_service_test.cpp -o "$output/tcp-listener-service-test"
for scenario in lifecycle validation recoverable overflow \
    listen-retained listen-context listen-error-handle listen-zero accept-retained accept-zero duplicate-native \
    close-retained close-io close-context quiesce-client quiesce-listener \
    read-context read-retained zero-success oversize-success wouldblock-bytes write-eof unknown-status; do
    "$output/tcp-listener-service-test" "$scenario"
done
