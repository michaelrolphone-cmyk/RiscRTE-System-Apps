# X4 Wi-Fi shared editor policy

The deployed 1.1.28 editor stops the ordinary app input loop. That also stops
resident idle/battery policy, while idle readiness rejects every active editor.
The explicitly selected 1.1.29 profile enables shared-host checkpoints, polls
policy only after settled frames and quiet input, pauses the host before policy,
and resumes the same draft. The shared keyboard remains the sole input consumer.

Scanning, scan-result ownership, joining, connected links, pending radio requests,
radio cleanup and outstanding atomic storage work continue to inhibit policy.
The active loop still calls wifi_tick before editor progression. Home/runtime exit
cancels an unaccepted result and waits for shared-host close and async radio drain.
Pause AGAIN/BUSY remain cooperative; malformed enable/resume results retain custody.
The existing availability keep/discard guard remains authoritative for RAM drafts.
Only explicit Save/Connect paths may persist credentials; modal completion itself
only updates the RAM draft. Password entries retain the masked shared-host request.

## Source reconciliation

`evidence/x4-wifi-1128-baseline.json` records the recovered immutable checkpoint.
All deployed ELF sections are byte-identical except four compiler-generated symbol
names in .strtab. The source checkpoint restores the pre-b40ca2a settings Runtime
copy behavior solely to reproduce that predecessor. Candidate source retains the
newer bounded copy extent and optional native callbacks. No binary patch is used.
The deployed feature selection retains atomic namespace 6, availability, broadcast,
shared text, scrolling, native time, resident policy, alarms and navigation.
Manifest requirement sets are unchanged; product assembly preserves deployed grant
bindings and ordering, including the existing redundant legacy namespace-6 grant.

## Qualification

`build_x4_wifi_policy.py` selects this X4-only profile. `reproduce_x4_wifi_1128.py`
rebuilds the predecessor with the recovered System checkout. Both take pinned
Runtime and Utilities directories and output directories; use Espressif GCC
8.4.0 / esp-2021r2-patch5. Neither compacts or patches the resulting ELF.

`test_x4_wifi_modal_policy.py` runs 25 cases against both legacy storage and the
actual atomic/availability controller selection. It links the production app,
adapter and checkpoint client, with deterministic text-provider and resident
boundary tables. The resident boundary verifies paused raster/input ownership.
Normal and UBSan runs pass locally. Existing controller/view (392 cases), workflow
(56), atomic (23), availability (17 normal + 17 paper), credential and saved-profile
suites also pass. Linux CI adds ASan and exact repeated target builds.
MacOS requires only an unused-function warning exception for unchanged vendored
SHA-256 source. Physical sleep/wake, low-battery and radio qualification is pending.
No native Runtime, provider host, hardware driver or existing release is changed.
