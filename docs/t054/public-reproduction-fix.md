# T054 public reproduction and delivered launcher integration

The inherited reproduction script compiled the current checkout while labelling it as the pinned baseline and comparing its digest to baseline Settings 4ee26705175a325f62af18e6dc98edc9e28aa51b9bf239bc946d8908cbce1385. T054 changes shared Quick code, so that assertion was no longer valid.

The script now materializes immutable baseline System Git objects and retains all original baseline hash, size, source, import and export assertions. Separately it builds the current default and atomic Settings/Springboard profiles against a reviewed T054 fixture. Each profile builds twice with different runtime inventories; both binary and transitive-input digests must match. CI cannot refresh expectations. The explicit local --record-current option only refreshes the new fixture, never the baseline fixture.

The delivered T030 launcher branch is merged, retaining its ordered raw-input queue, native-height geometry, raster/drag fixes, catalog and optional-service lifecycle. Atomic waits consume the delivered sample to avoid stalling the ordered input queue. The 68-case actual Settings/Quick matrix passes ASan/UBSan on both sizes and rotations, including continuous contact suppression and acceptance of a new tap after release. Existing catalog tests pass; all 10 actual Contexts catalog launch cases pass; actual Springboard drag renders 240 frames at both 240x240 and 410x502 under ASan/UBSan. Timings are host-only.

Public GCC 8.4 reproduction passed all six profiles (two immutable baseline, four current). This is source/component qualification; it does not claim a completed firmware release or hardware qualification.
