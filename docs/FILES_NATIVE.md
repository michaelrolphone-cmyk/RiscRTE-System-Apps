# Files native color UI

`build_portable_file_browser.py --native-ui` selects an app-local, capability-sized
color renderer. It adds `PORTABLE_FILES_NATIVE_UI`, `PORTABLE_NATIVE_CANVAS` and
`PORTABLE_ASYNC_RGB565`. The default, existing width-based Watch presentation and
paper renderer remain unchanged. This source qualification does not change a
firmware recipe, product store, published version or hardware qualification.

## Controller and layout

The native renderer uses physical capability dimensions, supporting dimensions
at least 160×160. It lays out a header, bounded list area and footer rather than
stretching the 240-coordinate canvas. Lists use one to six rows as height allows;
labels use retained 16/20-pixel faces with explicit ellipsis and compact 10-pixel
text where a small control or narrow body requires it. Legacy keyboard pages and
all 35 key actions remain available. Shared text entry keeps the existing draft,
mutation, checked close, retention and provider-custody controller.

Local and mounted files use the same existing navigation and operations. Native
list swipes retain bounded page transitions. Preview pagination adds a viewport
inside each existing provider read: every text byte or hex pair remains reachable
before advancing the storage offset. Long detail and sharing text can be paged.
Sharing still shows the unencrypted-HTTP warning; direct/LAN selection, temporary
credentials, expiry, setup and Nearby actions use their existing controllers.

Bluetooth comparison digits are rendered from a captured pairing generation.
Existing setup visibility and stale-generation checks remain authoritative.
Native contacts additionally require owner9's actually visible screen generation,
neutral input and a fresh contact. Changed file/row/session context invalidates an
old target. Same-map repaint preserves queued contacts and held releases. Leaving
a target cancels its tap; list dragging does not become a release activation.
Text-host handoff resets native input identity without clearing an outstanding
frame or borrowing its provider ownership.

Four private hidden hooks select Files ownership. Runtime/T5 ABI, provider
contracts, shared Nova transforms and physical-navigation ownership are unchanged.
The raster snapshot path carries owner9's generation through the existing queue.

## Reproduce

- `python3 scripts/test_files_native_input.py`
- `python3 scripts/test_files_native_optout.py`
- `python3 scripts/test_files_native_ui.py --output /tmp/files-normal`
- Add `--sanitize`, `--snapshot`, or both for the other profiles.

The native matrix uses the actual app, adapter and raster plus synthetic storage,
network and display providers. It covers 160×160, 240×240, 320×180, 410×502 and
502×410, immediate and 80 ms presentation, real contact routing, stale layout and
row replacement, same-map repaint, list swipe, text/hex byte coverage, comparison,
remote preview, destination and cleanup. Each profile also runs existing shared
text, BLE setup, source-handoff and direct-sharing/retention scenarios at each size.
These are host/controller checks, not physical touch, radio or display evidence.

Feature-off checks require exact preprocessed token parity against System
67dda0e5a7e09ee3eaff8fe52f35386c8ec8893d for 40 adapter compositions and six Files
controller compositions. The target job builds and validates a full-featured
qualification ELF with sharing, Nearby, shared text and source handoff. Firmware
integration and its inherited service/grant composition remain a separate step.
