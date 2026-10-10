# Shared X4 Quick Actions reference and behavior

The selected resident default owns the supplied Orbitron/Rajdhani Quick Actions sheet, its assets and all control execution. Foreground ELFs retain their live model and stack while the host takes input and draws over a copied completed image. The source adds no renderer to a foreground app. Ordinary Watch profiles keep their existing UI and default versions.

The native paper layout uses the supplied SVG geometry, exact font files and reference icon paths. Static reference labels are shaped with a private font configuration and stored as one-bit masks; dynamic time and percentages use subsets of those fonts. The title, FRONTLIGHT label, battery label and sun icon match the thresholded reference crops exactly. The build receipt records the source SVG, generator, assets and original font hashes. Font licenses accompany the host artifact. The generator is `scripts/generate_quick_reference_assets.py` and the supplied source is `test/native_apps/fixtures/quick-refined-reference.svg`.

Frontlight level and the FRONTLIGHT tile use the existing confirmed brightness/restore preferences. Toggling or dragging keeps the drawer open. CLEAN REFRESH appears only when display.output advertises CLEAN_PRESENT; it submits the current full sheet with CLEAN intent, bypasses unchanged/partial-damage suppression, and waits for completion while retaining input service. A failed or unconfirmed transfer follows the native custody fence. It does not change a radio or persistence policy.

Physical Home while the drawer covers a child now closes the sheet and requests the child's clean return before neutralizing input. A closed child drawer retains the same Home behavior. Home's crown remains latched after its own drawer dismissal, so the existing selected desk-lock path runs. Neither case adds a new drawer button mapping. USB still queues the dedicated app through the host and waits for child cleanup before launching it.

## Content-dependent reference differences

- X4 has no admitted sound output. VOLUME and SILENT are omitted and the remaining tiles move up. Sound-capable profiles retain both controls without starting an output provider merely to discover support.
- LOW POWER is absent. No manual low-power mode operation or behavior is assigned to that tile. Existing automatic low-battery and idle policies retain their assigned behavior.
- USB SD TRANSFER is an added full-width row on the no-audio layout. With sound hardware it occupies the reference's lower-left grid position; CLEAN REFRESH remains at lower right.
- Time, level, battery, radio states and disabled-control labels reflect confirmed live/persisted data. The sample's selected SILENT tile and 40% volume cannot both represent this product's mute state; the real control follows its stored volume.
- A bottom swipe-close hint also reports an unconfirmed setting change. One-bit dynamic font rasterization has different edge hinting from the antialiased SVG renderer.

## Qualification and identity

`test_shared_quick_reference.py` executes actual Paper Clock, Files and USB controllers/adapters as separate ELFs through production Runtime/Graph. It covers Home/child controls, physical child Home with the drawer closed/open, persisted frontlight toggle and level, CLEAN completion and capability omission, USB clean launch/return, and terminal display failures. Synthetic admitted peripheral APIs provide repeatable timing and failure injection. Completed frame captures are saved by the fixture.

`test_shared_quick_home.py` separately exercises selected sparse Home, the retained desk renderer, both desk directions, and the actual pinned product sleep hook for crown entry with the drawer closed/open. Its Runtime/power callbacks are strict fixtures; actual Runtime routing is covered by the first suite. Existing resident, Quick state/session, native/portrait motion, backlight and paper modal regressions remain part of the receipt. A pre-existing motion fixture expected old CLEAN/QUALITY intents; the clean 9f9393e baseline reproduced that assertion, and the fixture now checks the selected LOW_LATENCY transition contract.

Final resident-only reservations are Home 0.3.18, Springboard 1.7.21 and Settings 1.3.22. Fresh checks of 89 live branch heads, 16 tags, their version-source trees and exact-version PR searches found no claims. The Watch owner confirmed its Clock 0.10.10, Springboard 1.7.20 and Settings 1.3.21 reservations remain distinct. Other System client reservations stay Files 1.5.15, Wi-Fi 1.1.18, OTA/App Store 1.2.8 and USB 0.1.3.

The earlier eight-app checkpoint remains frozen. The successor's exact source, target, command, SDK, visual and behavior receipts are under `/workspace/shared/x4-quick-reference-proof` and its final target directory. No USB transport/provider, Runtime or GameBoy source is changed or rebuilt by this unit. Hardware behavior, the Windows USB hang and serial restoration remain unqualified.
