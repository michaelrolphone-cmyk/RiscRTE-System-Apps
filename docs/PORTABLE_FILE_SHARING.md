# Foreground file sharing

Files has an explicit `--webdav-sharing` profile, version 1.5.19. The default builder does not add network services or enable sharing. The profile preserves the existing declared handlers, storage selection, RGB/paper presentation, resident lifecycle, scrolling and optional telemetry selections.

The user chooses Share over Wi-Fi and Start HTTP. A saved default profile (slot zero of the existing Wi-Fi namespace6 schema) supplies the connection. A healthy existing connection is borrowed and left running; only a connection operation accepted by Files is cancelled on exit. The helper never calls synchronous startup as a fallback. Quick Actions' persisted radio policy is checked before startup. No profile is created or rewritten by Files.

The endpoint is displayed only after a real copied IPv4 address and listener are ready. Sessions last five minutes and use a fresh sixteen-character random password with RFC7616 SHA-256 Digest authentication. The UI explicitly describes HTTP file contents as unencrypted. Sharing never starts at boot or because a remote device discovers it. Credentials are not logged and are wiped on stop, expiry or terminal owner loss.

Only the boot-configured `storage.app-data.export` map is exposed. It projects existing AppData files with their existing revisions, quotas and atomic replacement semantics. There is no whole-bootfs export, NVS/bond export, duplicate mirror or inferred filesystem root. The browser closes its prior volume grant before the sharing client borrows the export. Existing GET/HEAD/OPTIONS/PROPFIND and conditional PUT behavior is documented in `lib/RemoteFiles`; unsupported mutations remain unavailable.

Automatic idle is inhibited while startup, serving or cleanup is active. Manual Home/Back, Quick launch and sleep retain their requested action until checked cleanup finishes. HTTP sockets and export grants close before cancellation of the owned Wi-Fi operation. Pending cleanup is cooperative; uncertain terminal ownership fences all later provider I/O, draws and freeing. Background Contexts and telemetry pause before network admission and resume through their existing normal policy after sharing ends. Service leases exclude worker SDK operations from storage/HTTP work.

## Build selection and authority

Pass `--webdav-sharing` with explicit nonzero, distinct `--sharing-export-instance`, `--sharing-tcp-instance` and `--sharing-entropy-instance` values matching the boot graph. The build receipt records the selected IDs, namespace1 radio policy and namespace6 profile access, HTTP choice, expiration and exact required grants. No boot authority is added by the builder itself.

The complete selected X4 resident Files profile, including SD, handlers, scrolling, telemetry and raster snapshots, builds with fifteen policy grants before BLE setup is added. RGB and paper app targets pass the real ELF/import checks. These are component builds; a full Watch image, combined radio memory budget and physical Wi-Fi/display behavior require product qualification.

## Verification

`bash scripts/test_file_browser_sharing.sh` compiles the actual Files controller and shared adapter with the real saved-profile helper, sharing client and HTTP implementation. Its synthetic capability backends cover twelve scenes in both RGB and paper: explicit start, cancellation before setup, disabled/busy/terminal policy, missing profile, borrowed and owned connection, deferred service lease, pending network close, terminal TCP close, finite expiry and secret erasure. `SANITIZE=1` runs ASan/UBSan. Terminal scenes deliberately retain owned memory; use `ASAN_OPTIONS=detect_leaks=0`.

`python scripts/test_wifi_adapter_deferral.py` checks the actual shared adapter's separate sharing mode and existing Wi-Fi mode, including pending Home/Quick/sleep, idle inhibition, checked fini, and terminal no-I/O boundaries. Its lifecycle hooks are copied fixtures; the Files suite above checks the real controller/helper ordering. `scripts/test_file_sharing_client.sh` separately checks real Digest authentication against an independent OpenSSL response calculation, authenticated file reads and backend failures.

BLE session setup is a separate generic provider and is not activated by this HTTP-only checkpoint. Its app presentation and native coexistence qualification are in progress. There has been no hardware installation or product release from these component tests.
