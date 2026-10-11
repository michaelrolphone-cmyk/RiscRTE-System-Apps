# Paper Quick Controls backlight

The explicitly selected `PORTABLE_PAPER_TRANSITIONS` paper sheet has an ON/OFF
button beside BACKLIGHT, separate from its 10–100% slider. OFF commits the
existing namespace-1 `brightness` preference as zero after first confirming the
previous nonzero level in `quick_bright`. ON restores that level; an absent saved
level uses the existing 40% default. Invalid values remain unconfirmed.

No grants or SDK contracts are added. Watch-selected controls retain their
existing layout, brightness floor and preference behavior. Builds without the
paper transitions flag retain the previous paper layout too.

The sheet synchronizes its loaded preference with the display capability before
showing a confirmed level. Its value and ON/OFF appearance use the latest
successful hardware application, independently of pending gesture proposals.
Preference writes require readback; OFF first saves the restoration level.
Rejected saves restore the previous level. Failed display calls invalidate the
displayed state, and failed capability release preserves native custody without
performing the pending display change.

Validation:

- `python3 scripts/test_paper_backlight.py`: production gesture/session tests,
  normal and ASan/UBSan. Covers OFF/ON/restoration, zero, modal and invocation
  persistence, rapid pending input, slider cancel, corrupt restore preference,
  failed save/readback/set/release and the legacy Watch session suite.
- `python3 scripts/test_quick_actions.py`: 108 existing shared core/session and
  real Watch adapter executions.
- `python3 scripts/test_paper_quick_actions.py`: 124 existing modal/Home cases.
- `python3 scripts/test_paper_quick_motion.py`: native and portrait motion,
  normal and ASan/UBSan, intermediate frames and exact background restoration.
- `scripts/test_core_paper_motion.py`: actual Settings pending-token interaction
  covers OFF, ON and rapid level changes with stored, applied and rendered
  brightness agreement; token settling continues to sample input.

All measurements here use deterministic provider fixtures. Physical hardware
brightness and panel timing are not qualified by these tests.

Selected interactive paper frames request `LOW_LATENCY`, including full-frame
UI paints. The nonselected MONO1 and Watch intent policy is unchanged. Locked
desk-clock quality/clean intents are separately owned by its clock controller.
