# Pinned X4 typed Clock integration fixture

The Nova7 paper-clock workflow still checks out the production X4 client at
[`c8a4f951abccd2e88660468958621612c82083c3`](https://github.com/michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO/tree/c8a4f951abccd2e88660468958621612c82083c3).
Neither that pin nor `minimal/apps/portable_sleep.c` is changed or patched by
this fixture. The independent X4 typed-sleep failure matrix also continues to
run directly from that checkout.

System now owns the current-app integration fixture
`test/native_apps/x4_desk_clock_typed_app_test.c`, adapted from X4's
`minimal/test/desk_clock_typed_app_test.c` at the same commit (MIT). The original
file's SHA-256 is
`4b702d6c6944c2648fa34de754ed2ad205d5ec6df2348b1c6de5d6591c14c3fb`.
The original runner is `minimal/test/run_desk_clock_typed_app_test.sh` at that
commit. The fixture retains its real System `paper_clock_test.c` support code,
fake providers, callbacks, assertions, and scenario semantics.

The compatibility edits are limited to:

- A designated initializer for the fake Runtime API. All previously initialized
  members retain the same callbacks; optional append-only suffixes are NULL.
  Positional initialization failed `-Werror=missing-field-initializers` when the
  current System Runtime header added `confirm_boot` and later suffixes.
- The X4 power header is found through the supplied X4 checkout's include path,
  instead of a path relative to the old fixture location.

`scripts/test_x4_desk_clock_typed_app.py` compiles the unchanged pinned X4
production source with the current System Clock, adapter, faces, and optionally
QuickActions/radios. It overlays only the five canonical Reader driver headers
used by the original runner. Runtime headers come from the explicit Runtime
checkout. No production dependency is taken from an unpinned local copy.

Each normal and ASan/UBSan run preserves both radio profiles and all 45 fresh
processes per profile:

1. `terminal`, `refused`, `retained`, `key-retained`, `touch-retained`,
   `touch-refused`, `sd-refused`, `wifi-retained`, `stage-refused`, `alarm-due`,
   and `held` each begin with their own absent state file.
2. 32 more `terminal` processes replay the original terminal record and exact
   physical pixels. No in-memory app state crosses processes.
3. `seed-retained` and `gpio` read the resulting terminal record, preserving
   retained frame custody and GPIO non-seeding checks.

The runner reports 180 fresh processes total. It retains strict warnings,
sanitizer failure behavior, all original assertions, and bounds each process
to 20 seconds. It does not suppress compiler warnings or broaden production
behavior to accommodate a test.

Reproduce the workflow integration with:

```sh
python scripts/test_x4_desk_clock_typed_app.py \
  --x4 .desk-deps/x4 --runtime .desk-deps/runtime \
  --sdk .desk-deps/reader/sdk/driver
```

The associated workflow pins are Reader
`aac8c06d3221139084acd0cfc64f7b0ba194a97a` and Runtime
`3c39aa7ac50ecd7f6da0296a62a2d7af066f82d1`.
