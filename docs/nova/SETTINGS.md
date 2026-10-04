# Settings 1.2.4: retain Nova, enlarge the controls

Settings remains the visual authority. The opt-in Nova utility profile enlarges
Back, Save, Cancel, +/- and every selection button to at least44 pixels high,
with44-pixel rows. The three-choice selectors use two columns plus a wide third
choice, retaining explicit Save/Cancel and room for errors. Hour editing in12h
mode has an explicit AM/PM button and preserves the chosen half-day while the
hour wheel wraps12→1.24h editing continues to use0–23. Date/time persistence,
DST ambiguity/gap handling, uncertain readback and namespace1 are unchanged.

[Before](screens/before.png) · [Actual after pages](screens/settings-after.png).
These images execute production source with modeled peripherals, not the final
Xtensa ELF or a physical Watch.

`test_nova_settings.py` passes272 production-controller executions over normal
and ASan/UBSan builds in both touch orientations. It covers real drag-to-Hour,
+/-/AMPM, nested Back/Cancel with no RTC writes,12h midnight/noon and24h display,
44px edge bounds, the38 alert-setting scenarios, nine combined same-invocation
scenarios and19 time-format scenarios. Existing legacy portable tests remain
required and pass. Target build: `build_portable_settings.py --nova-ui` with the
same explicit deployment feature flags. No release, merge or device write.
