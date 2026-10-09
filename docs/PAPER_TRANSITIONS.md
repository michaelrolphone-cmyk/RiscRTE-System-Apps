# Paper transition checkpoint

`PORTABLE_PAPER_TRANSITIONS` opts a paper build into the shared Quick Controls
pull-down animation. The existing Watch path and unselected paper builds retain
their behavior. This option is not yet selected by the product builder.

The sheet tracks the vertical drag over the copied, completed foreground image.
Release commits opening after 32 logical pixels; an upward drag of 64 pixels
closes an open sheet. Intermediate sheet positions cannot activate controls.
The shared animation clock advances toward the current target, and the current
image is restored after dismissal. Alarm cancellation still takes immediate
precedence. No extra frame queue or app-to-app buffer is introduced.

The production Clock, adapter and Quick Controls are exercised by
`python scripts/test_paper_quick_motion.py`. The provider fixture covers portrait
and native landscape output, both normally and with ASan/UBSan. It verifies
multiple distinct intermediate images, exact final background restoration,
and no leaked grants, frames, settings writes or app launches. Captured PBM/PNG
frames are in `build/paper-quick-motion` and were visually inspected.

This checkpoint covers the pull-down renderer and synchronous provider fixture.
Slow asynchronous completion, alarm/navigation interruption and repeated slider
input require additional combined qualification before product selection.
Home-to-Springboard crossfade is separate work and is not implemented here.
Host frame counts do not establish the physical panel's refresh rate.
