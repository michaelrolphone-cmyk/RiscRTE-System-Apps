# Touch and Quick rendering cost

The shared Quick adapter used to clear a newly acquired display surface before
copying the completed foreground image over every visible byte. The clear could
never reach the display. Quick now uses the same surface acquisition and failure
handling without that discarded paint. Ordinary clear operations still paint
their requested color.

On an X4 800 × 480 MONO1 surface this removes 384,000 pixel visits per restoration;
the required 48,000-byte image copy remains. On a Watch 240 × 240 RGB565 surface it
removes 57,600 pixel visits per background copy. There are no new production
counters, checks, caches, IRQ assumptions, input filters, or provider calls.

## Deterministic production measurements

`scripts/test_touch_dispatch_cost.py` builds both the immutable System baseline
`ba280f3c1db89f3bf42647bfdea8c205ad55e9ce` and the current source. Host-only GCC
function-entry instrumentation observes actual adapter calls. The fixtures use
the production touch adapter, Quick controller, session, renderer, native-time
source and timezone helpers with deterministic provider tables.

Each paper case opens Quick using raw top-edge input, then delivers a stationary
contact, moves, a DND tap, or cancelled multi-contact input, and closes Quick with
a fresh touch. Counts cover 45 samples and exclude foreground initialization.

| Case | Touch poll / next / snapshot | KV reads | Native reads | Paper renders | Presents including initial image | Discarded restore pixel visits, before → after |
| --- | --- | --- | --- | --- | --- | --- |
| Stationary contact | 45 / 45 / 45 | 6 | 1 | 1 | 3 | 384,000 → 0 |
| Moving contact | 45 / 45 / 45 | 6 | 1 | 1 | 3 | 384,000 → 0 |
| DND tap | 45 / 45 / 45 | 7 | 1 | 2 | 4 | 384,000 → 0 |
| Cancelled contact | 45 / 45 / 45 | 6 | 1 | 1 | 3 | 384,000 → 0 |

Storage and time reads are session/telemetry work, not repeated per stationary or
moving sample. The DND tap still performs its single save and alarm refresh. The
existing paper dirty-state policy already suppresses unchanged panel rendering.
An additional retained-acquisition case proves that a surface returned alongside
a custody fence is neither overwritten nor cleaned up.

The 13 existing Watch integration scenarios perform 982 background copies in
total. Their 56,563,200 discarded clear pixel visits become zero. Animation,
render calls, input outcomes and every submitted image are preserved. This is
removed operation count, not a measured hardware latency or battery improvement.

The companion X4 provider test `minimal/test/gt911_test.c` measures the shipped
driver independently: an idle poll performs one I2C transaction; a ready held or
moving single-contact report performs status + point + ACK (three); release and
Home-only reports perform two. Repeated identical coordinates emit no duplicate
MOVE events. Polling, event draining, snapshots, ACK handling, bounded queues and
ownership checks remain unchanged.

## Verification

- 36 before/after cases: five paper and thirteen Watch cases, normal + ASan/UBSan.
  Every submitted frame is compared byte-for-byte; provider and event outcomes
  match. Instrumentation is confined to the host test binary.
- 124 paper Quick/Home cases; 104 Watch adapter cases plus core/session tests.
- 116 native-toolbar cases, 11 compile refusals and two pinned Xtensa links.
- 72 native System app cases plus six native-profile Xtensa app builds and loader,
  import/export and SDK checks.
- 956 native Settings interaction cases.
- Retained RGB565 handoff, interrupted cleanup, delayed transfer and eager return
  checks; 12,125 renderer reference frames with stride and canary checks.
- X4 GT911: 77 normal and 77 ASan/UBSan cases; unchanged provider target compiles
  and passes import/export and relocation checks.

Reproduce the cost comparison with:

```sh
python3 scripts/test_touch_dispatch_cost.py --runtime /path/to/runtime-30dcec5
```

The source change is separate from the already assembled product BIN. No
hardware test, flashing, publication, package installation or product-version
change is included. Target artifacts built during qualification are development
artifacts; publication and cohort staging remain separate work.
