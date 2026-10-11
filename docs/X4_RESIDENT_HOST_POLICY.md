# Resident Home and foreground policy

This is the successor to the frozen three-app recipe in
`X4_RESIDENT_COHORT.md`. All selections are explicit; existing Watch and
non-resident builds keep their previous paths.

## Home Contexts

`build_paper_clock.py --contexts-rf-only` restores the delivered RF-only Home
client: `contexts.service@1`, preferences namespace 1, source mask 2, settled
observations, applicable room presets and the one-shot `waterfall.elf` model
owner rendezvous. It does not add an audio source, fabricate observations or
open the owner's model storage. The recovered implementation is pinned by the
frozen .29/.43 default receipt and System commit
`ce76cbe5e43cb8e7929223d87da3ac56226228ac`.

The host pauses and closes its borrowed Contexts grant before child launch.
It queues the owner through `run_foreground`, and rearms recovery after a clean
return/refusal. An unfinished owner export becomes an explicit unsupported
result. A legacy owner uses the same clean host handoff described below.

The sparse TIMER path never opens Contexts or its settings. Contexts adds no
retained fields: the copied Points/desk schema remains 3 and 408 bytes. The
selected Home manifest has 16 distinct requirements and needs 17 policy rows
because namespace-1 and namespace-5 KV grants are separate bindings. Live grant
limits stay at 16; no provider or policy bound is raised by this change.

## Legacy handoff

`--resident-legacy-handoff` selects the qualified Runtime .86 contract or a
compatible successor. It requires `RISC_RESIDENT_HANDOFF=3` and
`RISC_RESIDENT_NO_PENDING=4` without changing the API1 table layout.

The fresh host registers its callbacks and queries `run_foreground(NULL)`
after sparse foreground startup and before drawing Home. NO_PENDING means
ordinary Home. A saved resident/file-open continuation runs as the child.
HANDOFF means immediate host `app_main` return: no preference read, focus or
touch restoration, or Home repaint. Runtime performs ordinary host cleanup
before entering the separately admitted legacy target.

GameBoy remains the accepted delivered legacy binary. It has no resident
client and no shared overlay while it runs. Its original manifest, .gb/.gbc
associations and binary custody remain untouched.

## Foreground power policy

`--resident-policy` requires the explicit Runtime .89 policy handshake. The
host also requires the existing typed X4 idle helper and low-battery profile.
Clients link only the lifecycle protocol and gesture arbitration, with no
Quick Actions renderer or independent sleep helper.

POLL carries only three bounded bits: ACTIVITY=1, INHIBIT_IDLE=2 and
INHIBIT_POLICY=4. Activity remains pending until a successful POLL. BUSY never
consumes activity or pending permission. Ordinary POLL keeps child focus and
touch subscriptions intact.

A successful, non-inhibited POLL may return POLICY_REQUEST=4. At a later safe
boundary the child checks frames, borrowed buffers, app operations, capture
and queued/held input again. It pauses services and closes touch/focus before
entering POLICY=7 with zero flags. Runtime requires invocation-bound pending
permission. BUSY preserves permission; successful POLICY/EXIT consumes it;
a successful inhibited POLL cancels it. Unknown bits and invalid combinations
are rejected before policy execution.

Continuous capture and USB ownership defer POLL locally because native exit
barriers still apply. They keep activity pending; clean capture release starts
a fresh inactivity interval. Inhibition flags never bypass native ownership
checks. An editor draft does not need to resolve its launch guard for POLICY:
the child stack and model remain live across reversible Light.

The host reads confirmed preferences and runs the existing once-per-crossing
low-battery policy. The checked product helper remains responsible for Light
resource preparation, storage refusal, alarm tickets and resume. Its six
transient grants retain their existing physical bindings. Ordinary refusal
returns to the same child; uncertain custody fences both invocations without
provider restoration or memory cleanup. After a safe POLICY return the child
reopens touch/focus and gates inherited input until neutral.

Explicit SLEEP still requests clean child return first. The host cannot enter
deep sleep with a native child stack alive. No request flag adds deep-sleep
authority.

## Qualification and remaining integration

The tests load real host/client ELF modules, and the .89 integration fixture
also uses production Runtime/Graph admission and separate provider modules.
Context client/preset, native adapter, owner rendezvous, sparse expiry and Home
render/controller fixtures cover the restored lifecycle. Target receipts record
all selected flags and canonical SDK/source hashes. Hardware qualification and
full application conversion are separate outstanding gates.

Keep all 21 delivered app policies and the 19-entry Springboard catalog. For
an intermediate development cohort, admit the rebuilt Springboard/Settings as
foreground and preserve every unconverted application in an explicit disjoint
legacy list. This retains launch reachability, but shared overlays are available
only in converted foregrounds. Complete the selected Utilities/Productivity and
remaining System conversions before claiming the whole cohort uses one shared
shell. Keep release version allocation with product integration.

The frozen development recipe selects System
`3b0a250e0f1d016d3831aa9012ab89a1aa2e35d5` and Runtime
`83cc1f8f661fb39c5028ce669887629de9423bf7`. Run it from this recipe checkout:

```sh
python3 scripts/build_x4_resident_cohort.py \
  --spec examples/x4-resident-policy-cohort.json \
  --system /workspace/shared/x4-resident-policy-qualified-source \
  --runtime /workspace/shared/runtime-resident-policy-0189 \
  --utilities /workspace/shared/points-catalog-storage-custody \
  --productivity /workspace/shared/points-catalog-productivity \
  --product /workspace/shared/x4-resident-desk-product-040 \
  --sdk /workspace/shared/x4-provider-sdk-recovered \
  --baseline /workspace/shared/x4-043-image \
  --compiler /workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
  --output /workspace/shared/x4-resident-policy-qualified-target
```

This leaves the earlier `/workspace/shared/x4-resident-initial-cohort` receipt
and targets intact. The new recipe preserves all 21 manifests/policies and
records 18 explicit legacy paths in its development proposal. It still emits
no installable boot/store image.
