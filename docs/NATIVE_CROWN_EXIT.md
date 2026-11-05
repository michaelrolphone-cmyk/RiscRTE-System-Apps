# Native crown exit consumer checkpoint

Requires Runtime host-control suffix and board-selected native recovery, source
97b780888663be667582e21039dd1345b9eda6e8 (plus emergency parent 84ac1c4).
The portable adapter tests persistent native exit before modal, keyboard and
app navigation branches, requesting normal cleanup. The scene host prioritizes
suspend over queued touch, keyboard and rendering work. Neither consumer can
acknowledge the native request: only Runtime's completed teardown does so.
A nonresponsive app or incomplete cleanup reaches the native 3000-ms reset.
Home receives no native short-exit request and keeps its existing sleep policy.

Run `python3 scripts/test_native_crown_adapter.py --utilities PATH` for actual
Springboard/Battery+adapter execution with UBSan. Run shared scene component and
Contexts production tests with the matching Runtime/Utilities sources. The new
header is copied byte-for-byte from the exact Runtime source in SOURCES.json;
the historical Runtime prefix is unchanged. No full firmware or hardware
qualification is claimed. Rebuild every adapter-linked app and the scene host
in the final cohort; source changes alone do not alter installed ELFs.
