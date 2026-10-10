#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/entropy-service.XXXXXX)}"
mkdir -p "$output"
flags=(-std=c++17 -g -Wall -Wextra -Werror -Ilib/PortableApps/include)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CXX:-c++}" "${flags[@]}" test/native_apps/entropy_service_test.cpp -o "$output/entropy-service-test"
for mode in healthy unavailable invalid-native context retained unknown reentry overflow bad-dependency; do "$output/entropy-service-test" "$mode"; done
