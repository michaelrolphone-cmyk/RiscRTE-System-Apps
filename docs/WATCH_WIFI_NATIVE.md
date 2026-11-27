# Native safe-content Wi-Fi presentation

Explicit `scripts/build_portable_wifi.py --watch-native-ui` selects version
1.1.41 and `PORTABLE_WIFI_NATIVE_PRESENTATION`. It enables the existing Nova
native primitives and safe-content adapter, not the legacy 240px Nova layout.
The receipt adds `watch_native_ui: true` and `safe_content_native_raster: true`;
all PortableApps sources and original font notices are already hashed/copied by
the owning builder. Omitted profiles retain their old version and definitions.
Rotation, legacy Nova, paper, resident, shared text and native-time combinations
are rejected. Product integration must opt in separately; this flag grants no
providers and performs no device action.

`PortableWifiNativeLayout.h` is a pure content-local layout/hit contract for
240..2048px dimensions. The actual provider supplies the safe dimensions; the
adapter applies its physical safe origin once. Header Back, adaptive paginated
rows, entry/message, footer Prev/Select/Next and all 35 local keyboard keys share
half-open painted rectangles. Gutters are inert. At minimum240 two rows fit;
328 content gives three rows. Other shapes use the same rules. The keyboard
retains 32 character positions, three printable ASCII pages, and IDs32/33/34
for Page/Delete/Done. Smaller cells at minimum dimensions are not a claim of
physical touch usability. Glyphs use original licensed Orbitron14 and
Rajdhani15/11 faces, without enlarged intermediate bitmap rendering.

The controller uses the same hit dispatcher for native Back, footer and keys;
the old fixed240 footer remains compiled only into omitted presentation paths.
Credential namespace6, local masks/editor, Done-to-draft, explicit Save,
Forget/cancel, status uncertainty and all radio/lifecycle custody stay owned by
the existing controller. No provider/service or automatic reconnect was added.

## Owning proof

Run `python scripts/test_wifi_native.py --output <directory>`. It runs normal
and ASan/UBSan pure geometry, production native renderer and real
controller+adapter fixtures. Dimensions include minimum240, safe328,
odd327x411, landscape502x410,640x480 and generated layouts. Checks cover
half-open edge/gutter hits, every row/key, actual raw taps across three ASCII
pages, eight-character password entry, native Back/footer, Done versus Save,
masked entry, deletion and suspend/resume drafts. All providers are synthetic;
no network, device, or real credential is accessed.

The renderer fixture checks unaligned/strided storage guards, every individual
13px physical row/column slice, whole-frame parity, long SSID/status and empty
scan/error/keyboard states, and exports model raster PPMs. Safe328 at origin69
uses a466 physical frame; exported captures suppress guard sentinel pixels
outside safe content and the circular aperture. These are model captures, not
panel photographs.

A latent generic native primitive defect was exposed by combining safe insets
and snapshot bands: primitive coordinates were content-local while bands were
physical. `nova_native.inc` and `raster_snapshot_replay.inc` now add the safe
origin when clipping/computing spans. A per-band write guard fails with the
prior primitive and passes after correction; whole-final-image parity alone
would miss this. Existing native RF replay tests without safe insets remain
unchanged and pass. The sealed b7 target receipts and exact adapter preprocessor macro sets were
checked: Alarms/Countdown select native canvas and safe insets but do not select
snapshot replay; Files selects safe insets without native canvas; Settings,
Wi-Fi and Home do not select the intersection. No sealed public recipe enables
all three macros. The change is therefore a latent opt-in replay correction,
not an observed public panel defect. Every changed executable statement is
inside the snapshot branch; no published artifact was rewritten.

Run the existing portable Wi-Fi, workflow, persistence, manifest,
adapter-deferral, scan-latency and affected paper/scrolling suites alongside
product execution qualification. Physical RF association, power, panel and
actual touch results remain unverified.

The legacy admission fixture now conditionally links current Runtime stream
sources and binds its explicit host metadata source when available. The old
scan-latency harness now shares one verified-identical radio SDK header identity
with Runtime, avoiding duplicate pragma-once typedefs; AP-only SDK entry points
fail on invocation because these tests exercise station operation only. These
are owning test-assembly repairs, not production policy changes.

The prior-lease latency fixture also distinguishes current Runtime promotion
admission from app-exit safety: a healthy station blocks exit while allowing
provider-storage work and promotion admission. The fixture asserts all three
current outcomes explicitly; production custody policy is unchanged.

The revised prior-lease assertion qualifies the supplied current Runtime source,
not arbitrary historical revisions. The latency receipt binds its exact
`runtime_commit` and Runtime/CpuPort hashes; old promotion semantics are not
silently accepted as equivalent.
