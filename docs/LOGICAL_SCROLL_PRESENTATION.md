# Logical scroll state and presentation

The selected portable Settings, Time Zone, Wi-Fi, File Browser, and Springboard
controllers consume current logical geometry and identity. A busy display may
coalesce visual frames but cannot make a valid input depend on a completed
image. Both fast displays and slow paper use this rule.

## Reachable controller paths

- Settings root and Time/Date lists: `sp_poll` snapshots the current list offset
  at DOWN. Page changes and navigation still cancel the active gesture.
- Time Zone: `stz_poll` configures the current list and snapshots its logical
  offset, including a page that has not yet been drawn.
- Wi-Fi: touch uses the current prepared view identity/offset; Confirm resolves
  the current selection without requiring a highlighted frame.
- File Browser: DOWN pins the current logical row and its bounded cached item
  identity. Generation checks, directory inventory checks, release-time stat,
  and exact handler identity checks remain. Confirm resolves the selected row
  and current cache immediately, even if the selection is outside the last
  displayed viewport.
- Springboard: touch snapshots current catalog identity and horizontal offset.
  Actual animation still consumes a stop-tap; a busy display alone does not.
  Touch/Confirm request the current target without waiting for a highlighted
  frame. The legacy paper controller follows the same admission rule.

Frame readiness remains a presentation concern. Submitted/completed snapshots
are retained for existing raster diagnostics, not used to admit interaction.
The adapter still owns safe frame drain and release at app handoff/teardown.
The tests distinguish the time of Springboard's logical launch request from
completion of the adapter's ownership handoff.

Fine-grained raw MOVE input also exposed a Springboard axis-lock issue:
PortableTouch marks motion at 6 pixels, while horizontal page intent requires
8 pixels. The page controller now waits for its existing 8-pixel threshold
before latching the axis. A small move is still not eligible to become a tap.

## Verification

Run from an isolated source workspace:

```
python scripts/test_logical_scroll_latency.py \
  --sdk /path/to/verified/sdk/include \
  --output-dir /tmp/logical-scroll-latency
```

The runner compiles production controllers and adapters with strict warnings,
then executes ordinary and ASan/UBSan variants. The provider doubles supply a
sequenced raw DOWN/MOVE/UP queue and edge-only navigation. Display completion is
simulated at 0, 17, and 2300 ms. List layouts cover 480×800 and 400×600; both paper
Springboard controllers are covered at 480×800.

The matrix covers unpainted page/selection touch, Confirm, fine-grained drag,
post-release momentum or page animation, and a File Browser item changing
between DOWN and release. It verifies immutable in-flight pixels, complete raw
edge consumption, logical outcomes before the first 2300 ms frame completes,
coalesced presentation, and clean ownership teardown. The long-latency cases
submit one frame while still applying the scripted logical actions.

The drag cases require the adapter to use current controller time for idle
animation passes, while retaining original raw-event time for queued edges.
Otherwise a stationary snapshot whose timestamp remains at the last event
freezes momentum. This adapter fix is integrated separately.

The prior snapshot-only fixtures are not silently treated as a passing raw-event
suite: they publish changing snapshots without corresponding sequenced edges,
which the current reducer correctly rejects. The new matrix uses real provider
stream semantics. Existing completion-highlight admission assertions describe
the replaced behavior, not requirements.

These are host-simulated controller/raster checks. No hardware timing, radio
operation, device flashing, or publication is qualified by them.
