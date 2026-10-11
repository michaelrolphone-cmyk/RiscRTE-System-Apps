# Portrait orientation follow-on

The initial portrait presentation profile selected scene display rotation 90.
That rendered the keyboard 180 degrees away from the installed X4 application's
orientation while the raw portrait touch coordinates still selected logical
keys. Logical-touch-only tests did not catch the visible inversion.

The corrected `scene-profile-portrait-monochrome` package is 0.1.1. Shared builder
`PROFILE_FLAGS` selects scene display rotation 270 and leaves touch rotation 0:

- Installed app adapter 90: logical(x,y) → physical(y,479-x).
- Shared scene 270: logical(x,y) → physical(y,479-x).
- Shared scene 90 (old): logical(x,y) → physical(799-y,x).
- X4 GT911 raw coordinates already occupy 480x800 portrait space. A physical
  landscape point(px,py) corresponds to raw(479-py,px); no extra touch rotation.

The fix changes only the selected presentation policy. It does not reverse the
scene engine's general rotation semantics, alter the keyboard layout, or change
the compact Watch profile 0/0. Host/ABI/render custody implementations are unchanged.

`test_scene_host.py` now imports the same profile flags as the target builder,
compiles the actual profile provider, and checks Watch and physical 800x480 X4
frames against an independent installed-panel coordinate reference. Checks cover
asymmetric glyph pixels, key borders and all four keyboard pages through raw
coordinates, plus the existing 128 presenter/profile positive cases in total.
A separately compiled old 90 profile must fail the physical raster assertion.
Both normal and ASan/UBSan runs pass. These are software framebuffer/touch-double
tests, not physical device qualification.

This is an isolated source follow-on to 6bc3692f. It does not rewrite the earlier
recovery archive, build a target ELF, replace X4 .50 bytes, compose a product,
perform hardware actions or publish anything. Consumers should use this successor
source and rebuild the selected portrait provider in their separately qualified
integration unit; the earlier portrait 0.1.0 artifact is superseded for X4 use.
