# T030 native-resolution application coverage review

Read-only audit, 14 October 2026. No production files, Workboard records, devices or releases were changed. The active Springboard replacement remains a separate implementation task.

## Findings

- The official 0.1.43 factory image contains **27 application manifests and 26 internal application ELFs**. LoRa Messages is an admitted external-media application, with its ELF expected on SD. All **128 store files** were extracted in memory and verified against the official evidence ZIP's SHA-256 inventory.
- The reviewed shared application architecture still needs substantial native layout/raster work: **18 portable applications**, Home, the hidden diagnostic, and the external-media LoRa application retain fixed-coordinate or scaled-raster dependencies. Springboard has a native replacement in progress; this audit does not qualify that replacement.
- **Lists and Contexts already have the right semantic application boundary**: they declare nodes and actions, while a shared scene provider handles geometry. The reviewed scene host derives dimensions from display capabilities and draws directly into the provider surface. Exact shipped provider/profile/text-input qualification and font quality remain open.
- **Model Viewer has a native-sized canvas path**. Its viewer branch derives canvas dimensions from the capability, including rotation. The same canvas library has a fixed-resolution game branch; that branch must not be confused with the Viewer path.
- **Clock is a route alias to Home**, without an independent renderer. **USB SD Transfer needs its exact shipped color presentation source closure recovered** before assigning migration coverage.
- No full application is marked newly qualified at 240×240, 410×502, or arbitrary capability sizes by this source-only audit. Existing source-level native paths are distinguished from tested released behavior.

## Exact release baseline

- Factory image: `Waveshare-0.1.43-Catalog-Acquire-factory-erases-data-0x0.bin`
- Factory SHA-256: `1a67657a58254e26f99f674e71a5d9742fb023633e16b7e412b4a0f13a73754a`
- Product source: `9db48cef5bdb11206ff7f3601178cc2f32d326b8`
- Runtime source: `a2c2c9fa692b0b36f9750d44fac807c9149b9434`
- Release System source: `326396614def41f4a3b11c8c193c3ac1bbb75ab0`
- Released Springboard ELF SHA-256: `f0dd72760ece4c9102aa316281663ddd4d90eeabc54ca39f326766ce521f05a7`
- The release says application payloads were unchanged from 0.1.41. That is not a native UI qualification claim.

The `store-snapshot.json` file includes each actual manifest, version, capability requirements, boot policy, ELF size and hash. The `inventory.json` file assigns every one of those 27 applications a migration/qualification status.

## Application coverage

| Application | Reviewed rendering path | Required next scope |
| --- | --- | --- |
| Springboard | Native replacement in progress; .43 is the previous logical renderer | Complete native renderer/controller/cadence first; preserve catalog and shared controls |
| Home / default | 240-wide logical surface, completed canvas copied through nearest-neighbor viewport scaling | Native face layouts, text/assets, transitions and retained app-return frames |
| Clock | Return-to-default route; matching exact .43 receipt | Validate handoff to native Home; no separate renderer |
| Settings | Extra native-height space but 240-wide pixel/text transform | Native settings geometry and all modal/footer hit regions |
| Alarms, Countdown | Fixed watch editor controls through NOVA | Native editors, alarm/volume actions and interruption flows |
| Battery | Shared portable list/power presentation | Native rows, telemetry/unavailable states and shared chrome |
| Calculator | Reviewed NOVA branch requires 240×240; fixed keys | Native keypad/menu layout and hit testing |
| Stopwatch | Some capability-aware placement, shared legacy raster | Native timing presentation and persistent timing semantics |
| Frequency Generator | Some capability-aware placement, shared legacy raster | Native controls; retain tone/audio ownership |
| BLE Buttons, BLE Touchpad | Fixed watch/paper contact geometry in reviewed HID controller | Native controls and raw-contact mapping; retain pairing/multi-touch/session behavior |
| BLE Scanner | Portable NOVA with application-owned touch chrome | Native rows/detail/editor; retain radio session behavior |
| Wi-Fi | Portable list/editor and shared NOVA raster | Native layout; retain storage availability, credentials flow and retry states |
| App Store, Firmware Update | Shared update presentation with fixed watch coordinates | Native progress/choices; retain bank/cohort/cancellation behavior |
| Files | Portable browser chrome/rows plus shared raster | Native list/editor/sharing presentation; retain all mounted-media and file-source paths |
| Audio Spectrum | Fixed plot/control raster; reviewed plot width 228 | Native plot density, controls and editor without changing DSP/storage/session semantics |
| Waterfall | Fixed plot width 228, dimension-bound history/control arrays | Native plot/history geometry; retain RF capture and cleanup behavior |
| Points in Time | Fixed watch picker/editor hit rectangles | Native geometry for every scheduling mode and shared-input flow |
| Timecard | Centered 240×240 controller, fixed header/footer bounds | Native week/day/editor and hit testing; preserve records |
| Lists | Semantic `ui.scene` nodes | Qualify shared scene/profile/text-input closure; do not duplicate renderer in app |
| Contexts | Semantic `ui.scene` nodes | Same scene qualification; preserve rule/model/service behavior |
| Model Viewer | Capability-sized viewer canvas and 1:1 output after rotation | Qualify exact Viewer build, fonts, padding/rotation and raster/input behavior |
| USB SD Transfer | Shipped color source closure not established by located paper source | Recover exact 0.1.11 color recipe, then audit native presentation |
| Hardware diagnostic / bringup | Hidden fixed 240×240 diagnostic with viewport wrapper | Audit exact diagnostic pin; include in all-app migration without aliasing it to Home |
| LoRa Messages | Manifest only internally; fixed 240 controller in reviewed source | Verify external SD payload; migrate same shared app while retaining unavailable-media/radio states |

These are source-lineage findings. In particular, a fixed-size guard in an older reviewed source is **not** a claim that the shipped .43 controller takes that exact branch. The exact build closure must settle that question before implementation.

## Critical common dependencies

### Native-height is not native-resolution rasterization

At the released System pin:

- `WatchLayout.h` fixes logical width to 240 and computes logical height as `ceil(physical_height * 240 / physical_width)`. Thus 410×502 becomes 240×294 logical layout.
- `watch_native_layout.inc` maps each already-selected logical pixel to a physical rectangle. Larger physical frame allocation does not supply extra glyph or curve detail.
- `nova_ui.inc` reads frozen glyph masks and feeds the legacy pixel expansion path. App labels, icons and control primitives need a coherent native-size contract.
- Settings explicitly benefits from extra logical height but remains 240-wide. Existing native-height regression tests validate that previous contract, not the replacement requirement.
- Home's reviewed `viewport.c` allocates a 240-wide canvas and, on submit, maps the finished canvas to the physical surface. This is the completed-view scaling path that must be removed.

### Quick Controls blocks a broad full-application geometry claim

Quick Controls uses a `uint16_t pixels[240]` row and samples logical x/y from physical coordinates. `pqa_set_height` requires a logical height of at least 240. The adapter also checks physical dimensions between 160 and 1024 and, in native-height mode, requires logical height at least 240.

A 480×320 display computes a 240×160 logical layout and is rejected by the full application even if the new standalone Springboard renderer/controller accepts it. Native Springboard fixtures across dimensions do not prove full-app admission until this shared dependency is migrated. The correct next work is capability-based controls and checked resource limits, without device-specific application branches.

### Preserve async surface and lifecycle semantics

The native-size asynchronous canvas correctly validates surface dimensions, padded stride, capacity, token ownership and copied rows. Native layout must preserve those semantics. Shared controls, alarm overlays, retained app transitions, text input, cancellation and failed-provider retention need native coordinates and native underlay restoration together. No font/layout migration justifies removing service grants or feature definitions.

### Existing native paths still need font and closure review

The scene host draws at capability dimensions, supports a reviewed 160..2048 bound and uses profile/font metrics. Some glyphs are 5×7 bitmaps drawn at integer scales. Model Viewer chooses native dimensions within a reviewed 160..1200 bound and uses a monochrome working/snapshot raster before 1:1 output. Neither path is a reduced completed view, but both require native font/icon quality and exact-package qualification.

## Source and pin attribution

The report records full source/dependency pins from available retained provenance, plus explicit qualification of their relationship to .43. It does not silently substitute whichever repository checkout is newest.

- The released Springboard System pin is established by .43 release metadata.
- The Clock route receipt at Watch `ec768c7fae6fdb484fef28c6ec87f129b0a7f585`, `apps/clock/return_to_default.c`, matches the actual .43 ELF hash.
- Common Utilities source lineage is available at `fc0bdbbbc600ce891e60c44d091fce0586ade7d8`.
- Older specialized source receipts identify Spectrum/RF, Scanner, Files, Lists, LoRa, Points, Timecard and Wi-Fi implementations and their dependent source pins. Their recorded ELF hashes differ from the .43 payloads, so these are lineage records, not exact current build proofs.
- The current product history includes T054's atomic-preference rebuild of applications and later startup/UI replacements. Reusing earlier source/flags without carrying those changes would risk a functional regression. `scripts/t054/assemble.py` explicitly replaces namespace-1 writer binaries and adds `storage.key-value.atomic` requirements.
- The .38 Home source record identifies Watch `87b17a368b32abe50a21787242f94616b2b26983`; the current .43 inherited build closure was not freshly reproduced in this audit.
- USB SD Transfer 0.1.11 is actually shipped; the located `Apps/usb_sd_transfer.c` paper-reference presentation is insufficient to bind the shipped RGB color path.
- Exact source closure gaps mean “not established in this bounded audit,” not unavailable implementation source. Existing recovery/reproduction evidence remains intact.

## Recommended implementation sequence and proof gates

1. Finish the active Springboard-only native renderer/controller and cadence work. Keep its release claims scoped to the tested app paths and supported geometry envelope.
2. Migrate shared Quick/overlay/coordinate/font dependencies, retaining all lifecycle and persistence behavior. Test full-app opening across landscape as well as the two required display sizes.
3. Bind the exact released source closure for each remaining app, including the T054 preference changes and later feature fixes. Replace native geometry in related app groups without introducing product-specific branches.
4. Migrate Home and its faces/transitions, the remaining portable controller groups and optional-media LoRa. Qualify already-semantic Lists/Contexts and native Viewer at their shared presenter boundaries.
5. Before any all-app completion claim, test 240×240, 410×502, representative odd/padded and landscape surfaces, checked minimum/maximum sizes, touch/rotation agreement, fonts/icons at actual output sizes, clipping/scroll/footer layout, repeated entry and modal interruption. Include async busy/failure/retention and app return. Existing tests are reference material; none were rerun here.
6. Rebuild with unchanged feature closure, pin all source/compiler/normalization/compaction inputs, run target ELF/load/admission checks, and verify candidate payload hashes against the intended app changes. Physical performance and visual/hardware qualification remain separate evidence.

## Deliverables

- `coverage.json`: concise machine-readable status and source-pin summary for all 27 apps
- `inventory.json`: detailed app rows, source pins, capability dependencies and status/qualification fields
- `store-snapshot.json`: actual .43 store manifests and boot policies, bound to official image and file hashes
- `source-evidence.json`: hashed source files and line-level evidence; released System evidence uses immutable git revision reads
- `official-store-SHA256SUMS.txt`: official evidence inventory used for independent comparison
- `build_inventory.py`: read-only inventory generator; writes only this evidence directory

The source-only qualification flags intentionally remain false. A later renderer/test/packaging report can supply that evidence without rewriting the provenance of this audit.
