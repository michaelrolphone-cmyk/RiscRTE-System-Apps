#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root="${OUTPUT_DIR:-$(mktemp -d /tmp/t052-files.XXXXXX)}"
for suite in direct webdav peer mount resident scopes source; do
 case "$suite" in
 direct) script=test_file_browser_direct;; webdav) script=test_webdav_peer;; peer) script=test_file_browser_peer;; mount) script=test_file_browser_mount;; resident) script=test_file_browser_peer_resident;; scopes) script=test_file_browser_local_scopes;; source) script=test_file_browser_source;; esac
 mkdir -p "$root/$suite"
 OUTPUT_DIR="$root/$suite" bash "scripts/$script.sh" > "$root/$suite.log" 2>&1
 printf '%s PASS\n' "$suite"
done
