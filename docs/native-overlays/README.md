# Explicit native overlay app successors

Base: System b8e34df8b33096663d92b469dff225d130217354, tree d0525431eec60eef2c8e25028e3cc60ccab0f498.

Settings 1.4.2-dev1 and Wi-Fi 1.4.1-dev1 explicitly add PORTABLE_QUICK_NATIVE and PORTABLE_ALARM_NATIVE to the complete previously released 16/18-capability target recipes. All source lists, namespace bindings, external dependency pins, atomic persistence, availability, navigation/sleep/motion, radio, catalog, and terminal-custody features remain selected. Default/selector-off builders remain independent. Shared adapter production code is unchanged by this successor checkpoint.

The exact previous public controls are Settings 1.4.1-dev1 at 3050182821b7c4352a5f953c38bb1d32369c67be and Wi-Fi 1.4.0-dev1 at 9d2c55b32e88c0b12b1fa16200f399164767f5d1. Frozen public command, build-record, manifest and memory receipts are rehashed against current GitHub release asset metadata in released-controls.json. Existing tags and assets are not replaced. Both successor tags/releases were absent on 2026-10-17 at 22:07 UTC.

Evidence-origin correction: an earlier Settings 1.4.0 observation came from a historical local worktree. It was not the current main recipe or current release. Current main already preserves the released Settings 1.4.1 input/presentation fixes. Of 113 released System source inputs, 111 match this base; shared adapter.c contains independently qualified Wi-Fi/Battery guards and alarm.inc contains the qualified paper coordinate projection guard. No restoration/cherry-pick of old Settings code is appropriate.

The comparison runner checks that each target command adds exactly the two overlay selectors apart from its distinct version; capabilities, public imports/exports and external pins stay identical. It records exact ELF bytes and static executable/data allocation deltas using pinned Runtime admission evidence. The original 1.4.1 release, rather than any locally rebuilt 1.4.1 file, is the Settings memory baseline.

This is an incremental source checkpoint. Existing app contract tests pass locally; target builds and new actual-app overlay composition witnesses remain required before integration. The current workflows retain prior full controller, captured-input, ownership, idle, atomic/availability, bridge and selector-off suites. Native overlay lifecycle provider witnesses are being added without changing production input/lifecycle behavior.

Physical-device memory fit and hardware operation remain unqualified. Structural ELF admission is not an allocator/heap capacity measurement. Full linked-loader configuration, concurrent Runtime/provider/cached-image memory, fragmentation, app stacks and dynamic frame/Quick/sleep allocations remain deployment qualification requirements. This does not replace any product firmware or historical package.
