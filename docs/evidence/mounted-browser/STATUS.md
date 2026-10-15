# Mounted Files browser candidate, 2026-10-12

Files 1.5.31 reuses the existing Reader-derived portable Files controller and its RGB/paper/touch-scroll presentation. Connection setup mounts the authenticated peer and returns to ordinary Files rows. There is no separate remote file-list or transfer-action panel.

## Implemented

- Root/nested/empty directories, naturally sorted cached rows, parent, root Back to local storage, volume switching, explicit disconnect, failure/Refresh, and expiry cleanup.
- Existing Preview, Rename, Move, Copy, Delete, New-folder and declared-handler Open actions route to the mounted backend. Remote work is queued and progressed in bounded foreground ticks.
- Generations distinguish queued, running and completed requests even when a native service lease is BUSY. Cancel drains request custody before reuse. Disconnect invalidates remote copy sources rather than passing them to a local volume.
- Short local-storage and handler-discovery service scopes cover both JOIN and connected sessions. Pending lease ends never replay completed callbacks. Local and remote handler launch wait for peer shutdown.
- X4 Open materializes the selected file into admitted primary SD storage and dispatches using the existing file.open broker. Existing local files are preserved; bounded alternate names retain the file-type extension.
- Exact Watch fixed exports use their atomic writer and snapshot reader, including 512-byte I/O limits, catalog admission, exclusive creation, explicit revision-checked remote replacement, commit-unknown reporting and retained cleanup. Generic SD keeps staged/sync/rename publication.
- Server capabilities derive from actual callbacks/Allow headers. Unsupported operations report unavailability. COPY/MOVE/DELETE preflight protected descendants before mutation.

## Source custody

- Final functional source tested/built locally: cbf8c5d50b70068db8bd47f331ac7167c79c6ed0. Earlier full mounted-browser qualification remains recorded at c94bb08e3654d2dea3c3ef24a4fd6d5aae7fb1c1.
- Published independent backend: 061795323ac9f7869d5fa2b1f6d69e86286fd741, branch work/remote-browser-backend-20261012. Its exact tree matches local edbadd32b13864c3bd02a795caa4d6847cc91266. No remote CI runs existed at handoff; the former peer-panel test needs this UI integration.
- This combined branch includes separately blocked alarm-lease source 1fccf281e2242a3c31ad8b0dcebc81c011f05b17 (local cherry-pick bb61944). The alarm GitHub create_tree and evidence-backed retry were denied. No combined publication or alternate publication route is permitted until that approval is resolved.
- Delivered reproduction baseline remains Watch 1.1.10 / X4 .92, System 1aaf53e23ca9651a5cf9d1debe44941c9d7d0e58. Later Watch wake-only builds preserve those Files bytes.

## Qualification and limits

Complete qualification receipts are stored with the private recovery archive. Target preflight is a client ELF build/load/import check, not a flashable image or hardware test. No combined source, product recipe, or firmware has been published or delivered.

Remaining functional limits:

1. Generic declared-handler Open on Watch needs a versioned non-SD broker/receiver handoff. Watch has writable bounded app-data, but its frozen file.open contract only accepts /sd paths. Current UI refuses a remote Open without a writable admitted handler destination. Do not disguise app-data as SD or grant arbitrary namespaces.
2. Watch file operations remain limited to its eight admitted export paths. It has no generic directory creation, rename or deletion authority. New mapped records and explicit revision replacements are supported.
3. Ordinary Copy remains exclusive until the normal explicit replacement confirmation. Generic SD replacement now uses HEAD followed by If-Match: * and the existing staged replace_file callback; Watch keeps its returned exact revision. Unsupported replacement callbacks fail without a delete-and-retry workaround. Remote-to-local copies and same-volume rename/move/copy still reject existing destinations as the original browser did.
4. The original Reader browser at both the port source 34d8e694 and current 45cf61ac already declined recursive folder Copy, cross-volume Move and existing-destination overwrite. The common browser retains those folder/move limits; confirmed remote upload replacement is an addition. Remote directory responses are bounded to 64 children / 32 KiB XML; oversized or malformed results fail explicitly with no partial rows.
5. Physical sharing/browser/handler behavior is unqualified. Combined GitHub CI and whole-image admission wait for publication and product composition.

An additional legacy test_touch_scroll_files.py actions fixture failed identically on the original b953a82 source and the service-scope checkpoint under the available native SDK. This is not counted as passing or attributed to this change. The new actual mounted touch-scroll cases and ordinary local operations/paper suites are independently qualified.

## Confirmed replacement qualification

The paused WIP note is superseded by functional checkpoint cbf8c5d50b70068db8bd47f331ac7167c79c6ed0.

- Explicit generic replacement, default exclusive-create reset, real Watch exact revisions/stale rejection, failed HEAD and malformed/weak/empty ETag refusal.
- Ten cancellation boundaries, including acknowledged PUT with pending local read-handle close; completed writes remain completed.
- Destination removed between HEAD/PUT stays absent. Write/sync failures preserve old bytes; both pre- and post-publication replace_file failures report unknown outcome without retry/deletion. Transport uncertainty, temporary source unavailability and retained owner/socket custody are covered.
- Actual common UI: decline emits no request, confirmation changes the bytes, a later Copy needs fresh confirmation, and queued cancellation preserves the file. RGB/paper/paper-scroll normal and UBSan pass.
- Actual mounted navigation/actions/Watch export, peer controller/protocol and local service boundaries pass normal and UBSan. Fresh Watch/X4 target ELFs pass loader/import checks with unchanged 22/16 policy rows. Exact source and artifact hashes are in the private recovery receipt; no whole-image or hardware qualification is claimed.
