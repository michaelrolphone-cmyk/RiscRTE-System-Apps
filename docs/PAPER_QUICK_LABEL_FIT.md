# Complete native paper Quick labels

This component follows the separately qualified paper endpoint correction in
System `1d61e92e5aeeb4c2df0db6be49a38f94176eb06e`. It keeps native glyph heights,
font assets, text origins and allocated slots. It does not change input,
actions, modal custody or app selection.

## Measured cause and bounded correction

The nonresident paper sheet projects a 480-unit text slot into the display's
native width. Its shared text renderer uses fixed native font metrics. Before
this correction, it stopped before an entire glyph whose advance would exceed
the projected slot. A complete time or availability state could therefore
lose its last character:

* At 410×700, `20:59` needs 100 pixels inside a 95-pixel slot.
* `UNAVAILABLE` needs 110 pixels inside a 107-pixel slot.
* At the admitted minimum width of 400 pixels, `QUICK ACTIONS` needs 252 pixels
  inside a 250-pixel slot.

The existing private `PAPER_TEXT_FIT` contract fits the complete glyph spans
horizontally while retaining their height. The nonresident Quick wrapper now
requests it only when measured advance exceeds the native slot. The shared
renderer makes that existing path available when Quick is compiled, without
changing the separate `PAPER_TEXT_LARGE` selection. No slot is widened into
neighboring tile chrome and no critical text is replaced or truncated.

Paper eligibility remains capability-selected: retaining MONO1 portrait,
400–600 pixels wide, height at least 600 and greater than width. The resident
reference-mask sheet uses a different text path and is unchanged.

## Compatibility and evidence

The dedicated `scripts/test_paper_label_fit.py` runner executes the production
Clock/adapter with provider inputs and compares complete labels against an
independent font-bitmap oracle. Its evidence distinguishes full-label raster
coverage from mere nonempty text. Reference-size compatibility, native slot
containment, neighboring chrome, snapshot/direct output, input actions,
background restoration and presentation ownership are separate checks.

The existing paper coordinate suite remains the reference for primitive/input
boundaries and resident row coverage. Exact 480×800 controls must remain
unchanged. Non-Quick Clock text compiled alongside Quick is also checked,
because the renderer's feature guard covers the whole translation unit.

Host evidence alone is insufficient for target admission. Affected real
recipes are rebuilt with the existing compiler and unchanged ELF validator;
no import allowlist or loader policy is relaxed. Qualification receipts and
final captures are recorded with the published checkpoint.

This is a source component, with no hardware acceptance claim. Paper alarm
horizontal hit strips and native-height flag composition remain hypotheses
until an admitted recipe reproduces a disagreement. The separately stopped
shared idle/lifecycle correction is outside this component.

## Qualified checkpoint

The final local source-bound run passes 133 provider cases with 2,546 complete
label masks, 290 exact frame comparisons and 4,406,682 chrome/white-neighbor
pixel comparisons. Five known-clipped baseline controls fail the new oracle as
expected. The existing coordinate suite passes 175 cases and 30 comparisons;
the original paper suite passes 124 cases plus two 59-check Home runs.
Wi-Fi, Clock, Files and published-main Settings recipes pass their unchanged
target admission checks.

`paper-label-fit/summary.json` binds the exact source and retained artifacts.
The losslessly compressed provider and coordinate receipts include compiler
source closure, executable digests and actual input/ownership outcomes. Their
local pre-commit HEAD is the byte-identical published baseline tree; dirty-diff
and source hashes bind the correction. Six selected final captures are kept
alongside the receipts. The maintained CI runner retains its complete capture
matrix. Local reproduction scripts record their original paths and commands;
they are evidence scripts, not universal environment installers.
