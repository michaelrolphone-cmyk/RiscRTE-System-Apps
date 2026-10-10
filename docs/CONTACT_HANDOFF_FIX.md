# Contact and modal ownership correction

RF application integration exposed two shared-client defects: a vetoed Home
could return the previous Nova contact snapshot, and tentative top-edge input
could suspend radio work before the controls gesture committed.

The adapter now clears presentation contact snapshots at each poll boundary
and after a consumed handoff attempt. Terminal handoffs also expose no stale
contact. Saved QuickActions replay samples remain separate and unchanged.
While a top-edge gesture is only reserved, the app sees a neutral presentation
sample with no synthetic release. A rejected gesture replays its original down
and current sample; the Clock swipe and normal header taps still work.

QuickActions defers radio/storage/modal ownership for TOP_PENDING until the
sheet is visible. Both Watch and paper use the correction. This does not relax
any actual cleanup failure or retention handling.

36 real-adapter guard/contact/reservation scenarios, 124 existing paper control
cases and 104 Watch QuickActions integration cases pass under normal C and
ASan+UBSan. The RF application owner separately tests real editor/capture paths.
This correction changes shared client behavior and is separate from the original
opt-in launch guard, whose disabled build was byte-identical at3c881ef9.
