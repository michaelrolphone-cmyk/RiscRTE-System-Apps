# Watch storage migration prerequisites

No firmware, grants, namespace routing, source data or Home implementation is
changed by this audit. The read-only storage-authority census lists every
declared application and provider storage authority by namespace. A KV grant
is a potential writer, not proof of writes; missing native writers remain
explicitly unverified. Instance zero is unresolved, never treated as ns1.

## Source layout evidence and limits

RiscRTE-T-Watch-S3 profiles/native-idf61.cmake selects the expanded AppData
partition table, native file-source fencing and NativeAppDataMount. It does
not enable the atomic/settings worker/lifecycle stack. The Runtime expanded
CSV has AppData at 0x270000, size0x80000, matching NativeSettingsBackend's
checked mount requirements. This is source layout compatibility only.
It is not an installed firmware identity, writable capacity, migration
reservation or proof that all coexisting writers are excluded.

The published Bringup companion stores say to retain their exact baseline
native firmware. Pin that release chain and verify actual native receipt,
profile, partition bytes and mounted identity before proposing a successor.
Do not use factory images, erase, format or repartition to make a source fit.

## Preserving sequence

1. Inventory boot-granted KV/preferences/atomic authority and provider-bound
   key access across all namespaces using the hash-verified published store.
   Audit native registrations and actual writer implementations separately.
2. Keep ns1 routing unchanged until every existing writer, including Home and
   Settings, is proven compatible. Their separate owned work is a dependency;
   do not duplicate it or infer publication from local qualification.
3. Evaluate the smaller ns6-only candidate with exact wifi_settings writer
   admission. Existing native lifecycle namespace mask can restrict routing,
   but still allocates controls for both namespaces; measure actual full-link
   resources rather than claiming a single-namespace allocation reduction.
4. Reuse checked native legacy inventory/import and receipt recovery. Preserve
   all original rte/kvj/kvd records, shadowed bytes, source fingerprints and
   archive obligations. Missing alone is never fresh-empty authority. Unknown,
   corrupt, anchored/ambiguous or conflicting sources remain unavailable.
5. Prove storage admission/reservation across every shared AppData writer,
   staged replacement and reboot recovery. A matching partition size, observed
   free bytes or mount lease is not a durable reservation. Insufficient space
   must reject cleanly, preserving all old data and peers, without deletion,
   resizing or formatting. The outstanding shared-storage replacement-budget
   work remains a dependency, not an assertion this migration can bypass.
6. Only after native source, capacity and custody qualification may an actual
   product firmware profile enable broker/worker/lifecycle and exact atomic6
   and availability6 authority. Derive host admission commands from that real
   profile; never add fixture-only feature switches to pass.
7. Require saved-profile read/edit/reboot preservation, checked import retry,
   interruption recovery, unknown/oversized source and insufficient-space
   refusals, foreign writer exclusion, pending cancellation/collection,
   retained custody, namespace isolation and default-off regression.

Native source references:
- RiscRTE/src/ports/esp32s3/NativeSettingsBackend.cpp
- RiscRTE/src/ports/esp32s3/NativeSettingsLifecycle.cpp
- RiscRTE/experimental/native_replica/NVS_LEGACY_SOURCES.md
- RiscRTE/docs/ATOMIC_KEY_VALUE_RUNTIME_EXPERIMENTAL.md

Product source reference:
https://github.com/michaelrolphone-cmyk/RiscRTE-T-Watch-S3/blob/main/profiles/native-idf61.cmake

These are design prerequisites. No actual saved user data has been inspected
or migration/capacity approval established by this census.
