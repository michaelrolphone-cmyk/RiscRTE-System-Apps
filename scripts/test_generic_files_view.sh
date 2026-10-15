#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="${OUTPUT_DIR:-$(mktemp -d /tmp/generic-files-view.XXXXXX)}"
mkdir -p "$out"
printf '#include "PortableFilesVolume.h"\nint main(void){return 0;}\n' > "$out/header.c"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Ilib/PortableApps/include "$out/header.c" -o "$out/c-header"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Ilib/PortableApps/include "$out/header.c" -o "$out/cpp-header"
for mode in normal sanitized;do
 flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -Ilib/PortableApps/include)
 if [[ "$mode" == sanitized ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -no-pie);fi
 "${CXX:-c++}" "${flags[@]}" test/remote_files/generic_files_view_test.cpp -o "$out/$mode"
 ASAN_OPTIONS=detect_leaks=0 "$out/$mode"
done
