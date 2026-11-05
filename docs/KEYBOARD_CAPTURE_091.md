# Shared keyboard corrective checkpoint

This candidate keeps keyboard input, logical text and immutable display frames
separate. Password entry gains an explicit Show/Hide control and starts masked;
the initial letter-case selection is unchanged.

## Capturing short releases

The previous presenter already avoided waiting for display completion, but its
fixed eight-row monochrome raster batch could delay the next physical sample
under expensive CPU work. A missed neutral report can merge separate presses.
Keyboard raster work now checks its clock between logical rows and returns to
input capture after a two-millisecond budget. One row may overrun that budget;
this is cooperative scheduling, not a preemptive timing guarantee. Other scene
batch sizes are unchanged. Off-band rounded shapes skip work without changing
their pixels. No key value is used for deduplication.

An independent defect involved reused hardware contact IDs: a released press
waiting behind an earlier held press was incorrectly treated as a duplicate
active contact. Released logical presses now remain in an ordered bounded
32-entry queue while only live contacts participate in hardware-ID matching.
The target memory inventory must include that enlarged queue.

## Password visibility

The shared text owner keeps the true draft. Only its copied presentation value
changes between masked and explicitly revealed text. Hide cancels and wipes an
owned unsubmitted revealed raster; a submitted display token still follows its
normal completion/custody path. Done, Back and Home re-mask, and the next session
starts masked. Close clears both the logical and frozen text copies. Credentials
are never diagnostic fields, test inputs use synthetic text, and no public ABI
structure or capability grant is changed.

## Qualification and limits

The delivered X4 .90 scene-host0.3.6 was rebuilt from published source e372ded:
all13 allocated ELF sections match its selected image
fd8543dcb48d02f08cfcda144b0038d457f59b9cc5bf670caeee746f92af3ff7.
The underlying masked text service, real Runtime/ProviderGraph, presenter and
GT911 register path are exercised together by the corrective timing tests.
Simulated expensive pixel work reproduces missed captures in that baseline;
the adaptive candidate preserves repeated and alternating keys. Those costs
and panel delays are explicit software loads, not measured device timings.

Before final delivery, rerun the combined source tests, target build, product
ELF admission and actual immutable-SD/fallback selection checks. Hardware
verification remains required. The deployment pair is scene-host0.3.7 and
text-input-host0.1.4, with matching SD images and internal fallbacks. This work
does not activate the unfinished shared-settings persistence experiment.

## Current source integration, 2026-10-12

The preserved corrective checkpoint `15f8dfb152dbe85c1d499902aa2878a75562b892`
was reapplied locally to the combined Files/OBD System source
`49752a02746b38afbcc403bb92189be5a8356c9d`. The paired versions are now
scene-host **0.4.2** and text-input-host **0.1.4**. The original 0.3.7 scene
version above describes the historical .91 checkpoint, not this integration.
All production changes merged without behavioral conflicts; the scene manifest
was advanced past the current 0.4.1 instead of reverting its lineage.

Completed local checks:

- 194 scene/profile cases in normal mode and 194 with ASan/UBSan.
- Text-owner, hardware-keyboard, visibility, navigation, failed teardown and
  private-storage checks in normal mode and ASan/UBSan.
- Real Runtime/Graph text sessions on Watch and X4 profiles, including eager,
  demand, retained and armed-retained activation. Runtime
  `6377d498a47b59dac8ad85d930983fffc6c8baa5` passed under ASan/UBSan.
- 75 current-base raster/cadence checks, generic component/reading/checkpoint
  checks, and normal/sanitized OBD telemetry regression checks.
- Sustained masked-input comparisons with current Runtime: all 6,240 candidate
  taps retained per normal and sanitized run, including repeated keys and
  ordered CLEAR actions. The historical baseline reproduced capture loss under
  the explicit software stress model. These are modeled timings, not hardware
  latency measurements.
- Current-Runtime target builds, structural ELF validation and complete Xtensa
  relocation checks at eight allocator alignments: 38,856 relocation-site checks
  for scene and 1,056 for text, with exact section/redzone checks.
- Password previews inspected with packaged Watch and X4 profiles. The X4
  packaged rotation remains 270 and existing grid geometry is unchanged.

Compared with the frozen .98 store, scene's total allocated ELF sections grow
4,028 bytes, including 1,728 bytes of BSS for the ordered contact queue (now
2,048 bytes for 32 entries). Text's allocated sections grow 268 bytes with no
BSS growth. These are static inventories, not measured live heap headroom.

This remains a local source checkpoint. The earlier denied publication has
not been retried or rerouted. No product image, SD selection, boot packing,
accepted release, remote branch or release asset was changed here. Final
product admission/selection, publication authorization and physical testing
remain separate outstanding steps.
