# Files shared plaintext input

The explicit `--shared-text-input` Files profile (1.5.34) replaces its private
Watch and paper keyboards for Rename, New Folder, and Find by name. Browser,
menus, mounted volume navigation, preview, Open, copy/move, replacement/delete
confirmation, sharing, and source handoff remain the ordinary Files controller.
The default profile and all other applications are unchanged.

## Draft and lifecycle boundaries

- A copied host request owns no Files pointers, callbacks, file handles, or
  persistent storage. Files stops polling and painting its presenter while the
  shared host owns text input. It continues bounded peer/network progress.
- Done is acted on only after confirmed host close, grant release and presenter
  resumption. Existing file-name/path validation and storage/remote operations
  remain authoritative. Invalid accepted drafts stay available for explicit Edit
  or Cancel; no mutation is replayed automatically.
- Cancel and Back discard the host edit and restore the initiating browser
  route/choice. Home closes the text session and the complete Files network,
  storage and presenter lifecycle before returning through the existing Home
  mechanism. A disconnected/expired mount invalidates its outstanding draft.
- Pending close retains the session and defers the mutation/return. Retained
  custody is terminal and forbids subsequent provider I/O. Input neutrality and
  paper contact/scroll state reset before browser input resumes.

## Backward-compatible bounded extension

The existing `ui.text-input@1` 72-byte request/result layouts, API prefix,
Home-reason suffix, and masked-password suffix are unchanged. Scene and text
provider versions are 0.4.4 and 0.1.5. A tagged, sized optional suffix provides
separately typed 128-byte request and result functions. Files requires that
suffix, keeping its existing 127-byte filename and 63-byte filter limits.
Old providers are rejected cleanly before opening an unsupported session.

The extension preserves existing non-control filename bytes losslessly. New
keyboard input remains printable ASCII. Unsupported preview glyphs become `?`
only in the bounded scene copy; the original filename/draft bytes are not
changed. The legacy 72-byte API retains its ASCII validation and behavior.

The System-private keyboard scene flag carries a bounded 71-character tail
viewport plus full byte length/limit. Text-only length changes retain the same
keyboard input epoch. Keys, including rapid identical keys, continue in order
while rendering is pending. The original scene document layout is unchanged.

## Exact target selections

- Watch: inherited RGB-only Files, installed files plus app-data export, Nearby
  mounts, disk-backed non-SD source Open, Contexts, Quick Actions, radio policy,
  alarms, motion and sleep. The already-qualified compact Watch scene profile
  remains selected. Files uses LTO and `-mno-target-align` with the same features;
  the accepted metadata-only ELF compactor preserves allocated loader content.
- X4: inherited .101 Files paper/scrolling, native-time toolbar, resident policy,
  raster snapshots, BLE broadcast, SD instance 9, shared network and alarm API2.
  The complete scene host retains Reader and telemetry APIs. Files adds one
  `ui.text-input` declaration/grant: 17 policy rows, without changing simultaneous
  native grant limits or removing any authority.

Builders produce only components. Final product recipes must retain every
baseline application/provider and grant, update the immutable X4 SD host image
paths with identical internal fallback bytes, and re-admit the complete cohort
against its exact native binary. Watch requires at least two free SPIFFS blocks.

## Qualification entrypoints

- `scripts/test_file_browser_shared_input.sh`: actual Files controller and
  adapter, real text owner, local and remote storage, RGB/paper/scrolling profiles,
  app_main acceptance/cancellation/Home and delayed close, long and non-ASCII
  drafts, validation failures, retry, mount expiry, and retained close.
- `scripts/test_file_browser_ui_parity.py`: byte-identical non-editor screens
  between default and shared-input profiles.
- `FILES_SHARED_TEXT=1 scripts/test_file_browser_source.sh`: existing non-SD
  source Open, interrupted/repeated handoff and bounded staging regression paths.
- `scripts/test_text_input_host.py`: unchanged old-client behavior and
  size/tag/function/result guards for the extended client.
- `scripts/test_text_input_runtime.py`: real unloadable text/scene/app modules and
  production Runtime/ProviderGraph in eager, demand and retained activation;
  old-client suites plus 127-byte rapid repeated input under pending rendering,
  terminal result freezing, Back/Home and close/reopen.

Normal and ASan/UBSan runs, exact target builds, complete cohort admission and
packing/readback are separate evidence. No host test executes Xtensa target
instructions or qualifies physical touch, radios, e-paper, or device resume.
