# Springboard 1.4.4: complete distinct app glyphs

This version carries the shared verified icon subset and explicit Nova build
profile. The approved Springboard layout, movement, sizing, color assignment and
clock display are unchanged. Alarms has a real bell, Countdown an hourglass,
Points a calendar-day and Calculator its own calculator. No icon is reused in
the delivered nine-app catalog. The Watch deployment must consume the same
catalog registry when building its ordinary app catalog.

[Actual settled launcher](screens/launcher-after.png) · [Before](screens/before.png).
The images execute normal app/adapter source with fake peripherals, not a
substitute renderer, target-ELF execution or physical qualification.

`test_nova_launcher.py` verifies all nine genuine glyphs produce pixels, all IDs
are distinct, and the normal complete launcher draws and cleans up under normal
and ASan/UBSan builds. The original Springboard suite and19,000 randomized motion
trajectories still pass. Pinned GCC8.4 Nova Springboard ELF structure/import/export
checks pass. No face replacement, merge, release or device operation.
