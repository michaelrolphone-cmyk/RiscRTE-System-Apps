# Paper Space: the external default GUI entry

Paper Space is a new ordinary `default.elf` application, version **1.0.0**, for
minimal RiscRTE. This PR implements the connected Home entry slice. It does **not**
replace the existing Reader GUI, establish feature parity, or change a device's
boot selection. Reader firmware remains active while its remaining GUI is migrated.

## What owns the UI now

For this app, the ELF owns Home rendering, original Reader font pixels, menu
geometry, selection, pagination, neutral-input gating, and launch decisions.
Only the existing `risc_runtime_get_api` and ordinary libc functions are imported.
There is no `ActivityManager`, `GfxRenderer`, `NativeUiBridge`, or firmware-renderer
callback behind the ELF. The existing canonical `display.output@1`,
`input.navigation@1`, and optional `input.touch.raw@1` contracts are consumed
unchanged; no new kernel ABI or app framework is introduced.

Home preserves the Reader's list-based Apps/Settings ordering, inverted selected
row, wrapping Up/Down navigation and Confirm-on-release. The actual Reader
Ubuntu 12 ASCII glyph pixels and metrics are subsetted into the renamed
**Paper Space Text 12**, with the original Ubuntu Font Licence and source custody.
Full Reader themes, covers, text shaping and navigation routes are not claimed.
Touch is optional by explicit build/manifest profile; a completed stationary tap
selects and opens a row. The visible Select/Up/Down footer controls also work by touch,
so all eight configured entries remain reachable across pages without buttons. Held entry, event gaps and canceled contacts cannot launch.
When touch and navigation coexist the app claims foreground touch, then restores
it before handoff, including rollback after a failed foreground claim. Raw touch and display coordinate extents must match.

The deployment links up to eight explicit menu entries. The example contains
Apps (`springboard.elf`) and Settings (`settings.elf`). These names are **not**
evidence that compatible apps are installed. This slice performs no package
inventory scan and does not create app grants. Child apps need their own manifests,
capability grants, provider compatibility and installed bytes. Existing portable
Springboard/Settings only support RGB565; they do not yet make this example an
X4 MONO1 deployment. No placeholder Recents/reader/OPDS row is presented as working.

## Real boot and lifetime

Use the existing RiscRTE `boot.json` `default_app` selector and app-capability policy.
The builder emits a selection **fragment**, deliberately omitting physical board,
driver and wiring assumptions. Instance 0 means uniquely admitted provider, as in
the existing runtime; select exact nonzero physical instances in a real multi-provider
profile. Merge the fragment with a separately validated board/dependency graph.

App-local frames, touch subscriptions and foreground claims are closed before
queueing a child. A successful request returns immediately. The runtime finalizes
and unmaps Paper Space before mapping the child. Child return or load failure maps
a fresh Paper Space with selection zero. Nothing is kept by pointer across ELF
lifetimes. Missing/corrupt default fails once through the existing runtime error
path; there is no hidden firmware GUI fallback or restart loop. Child-load failures
use the runtime's existing diagnostic because its ABI has no launch-result channel.

MONO1 uses the canonical MSB-first, 1=black convention. RGB565 uses true black/white
pixels, not a format reinterpretation. Frames validate dimensions, stride, storage
size and format, honor safe insets, and clip rendering. Presentation has both a
20-second deadline and an iteration bound with real runtime yields. The application
owns no task, timer, callback or hardware register. A stopped/faulted display exits
the entry invocation; a new runtime invocation starts cleanly. Provider quiescence
and any required retention remain the real runtime's responsibility.

## Build and test

```sh
python scripts/build_paper_space.py
python scripts/build_paper_space.py --input touch --output-dir dist/paper-space-touch
python scripts/build_paper_space.py --input both --catalog my-home.json
python -m unittest discover -s tests -p test_paper_space.py -v
bash scripts/test_paper_space.sh /path/to/pinned/RiscRTE
```

The dedicated builder is separate from the historical 18-app Reader migration
inventory. It builds the new minimal-runtime ABI rather than pretending this app
is an unchanged legacy T5 package. It emits the ordinary ELF, source manifest,
selection fragment, immutable-input hashes, compiler/import/export evidence and
license notices. It does not update a live app catalog or install/publish anything.
All three target input profiles enforce 32-bit table layouts, a narrow dynamic
symbol allowlist, and the actual repository ELF structural validator.

The runtime test requires RiscRTE commit
`a3d23da9cdc1b3a66c6429f29781856fa7fc8f75` (PR #2, based on minimal-runtime PR #1)
and rejects modified runtime sources. It compiles the actual Runtime, board parser,
ProviderGraph and module loader with the actual Paper Space application shared
objects; only physical display/input providers are modeled. It proves the actual
unmap-before-child sequence, no inherited child grants, fresh default pixel/selection
state, missing/corrupt children/defaults, partial initialization, failed presentation,
bounded waits, held input/gaps, navigation wrapping, touch cleanup and restart.
Padded/odd-address frames and multiple e-paper-sized viewports are covered under
UBSan. Saved rasters are test output, not physical panel evidence.

## Remaining Reader migration (not completed here)

Pinned source: [Reader e58ac310](https://github.com/michaelrolphone-cmyk/T5S3-Reader/tree/e58ac3106e5569a1271d28ab3a2f87907129a601).
Its tree is `b1dfbe371d0dd429cfa2a1e4ef221e36c5139c33`.

| Function | Current Reader ownership | Required app/provider migration |
| --- | --- | --- |
| Home pins, recents and covers | `src/activities/home/HomeActivity.cpp`, `src/RecentBooksStore.*`, `src/components/HomeReadingCard.*` | Bounded storage/intent access, app-owned state and cover lifecycle. No fabricated list or broad bootfs-as-storage shortcut |
| Book routes and readers | `src/activities/ActivityManager.cpp`, `src/activities/reader/`, `lib/Epub/`, `lib/Xtc/` | App-owned reader activities, data access, parsing/layout and safe unload of resources |
| Renderer, themes and fonts | `src/main.cpp`, `lib/GfxRenderer/`, `src/components/themes/`, `src/SdCardFontSystem.*` | Move complete UI rendering/theme/font ownership, preserve rotation/grayscale/caches and dismantle global renderer references |
| Settings and state | `src/CrossPointSettings.*`, `src/CrossPointState.*`, `src/native/NativeSettingsBridge.cpp`, `src/activities/settings/` | Portable model/persistence and real service calls; no firmware settings-screen proxy |
| Installed apps / file routing | `src/native/NativeAppHost.cpp`, `src/activities/util/InstalledAppActivity.cpp`, `src/native/InstalledAppPath.*` | Shared admitted inventory/intents and explicit lifecycle integration; no recursive firmware GUI launcher |
| Startup, shutdown and sleep UI | `src/main.cpp`, `src/activities/boot_sleep/`, `src/components/StartupScreen.*` | Separate generic lifecycle from product screens; preserve storage/provider ownership and independent sleep work |
| Runtime device deployment | Reader panel/input/storage provider profiles and RiscRTE board manifests | Validate actual installed providers and complete dependency/grant graph before any firmware cutover |

The existing X4/T5 display, storage and battery architecture is untouched. No
firmware startup change, Reader GUI removal, merge, release, device access or flash
is part of this entry slice. Host and Xtensa builds do not qualify physical panels,
input devices or the still-incomplete feature migration.
