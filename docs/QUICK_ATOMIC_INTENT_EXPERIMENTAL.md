# Experimental X4 Home/Quick preference intents

Default off: `build_paper_clock.py --experimental-atomic-quick` requires the
X4 sparse resident Home, Quick radios, paper transitions and frontlight tone.
It adds only an experimental `storage.key-value.atomic@1` namespace-1 requirement.
No product manifest, Runtime selection, Watch, firmware delivery, GitHub or
physical device is changed.

## Explicit controller boundary

`quick_foreground` captures the Core revision before loading a modal draft.
`quick_apply` binds the explicit action to that revision and verifies its old
canonical preference matches the displayed base. This also protects a touch
whose down/preview/up coalesces to one commit before the first apply. Sliders
keep the same captured revision throughout preview and release. Only this
modal's own successful collected receipt advances the baseline for the next
independent action. An external revision change requires reopening; no rebase,
retry against a new revision, source conflict selection or merge is defined.

Each ordered input reduction copies its pending action and values into a bounded
32-entry FIFO before another sample can OR flags into it. Distinct controls and
commits keep their order; only adjacent same-contact previews coalesce. Queued
DND/radio/brightness operations publish as separate explicit intents, with
revision advancement only from prior successful receipts. Overflow rejects
new events, keeps already captured actions, and shows a save error until the
modal is reopened. Low-battery and alarm interruption clear obsolete queued
UI actions rather than turning them into automatic intentions.

Selected user paths use explicit session/radio callbacks, never legacy KV1
puts: brightness, frontlight_tone, alert_dnd, contexts_on and quick_radio.
Brightness OFF submits brightness and quick_bright together. quick_bright is
subordinate restore metadata, cannot be an independent USER intent, and is
validated against the captured nonzero brightness or same-revision prior
restore byte. Each group has exactly one unchanged 176-byte PFI0001 primary
intent. The provider copies all values and the intent before accepting a token.

Preview, torch and hardware restoration remain non-persistent. Radio control
still attempts hardware first; failed or cancelled persistence restores only
hardware. It does not emit a compensating preference intent. A cancellation
that loses the publication race follows the successful collected receipt.
Uncertain custody retains staging, token and invocation before later I/O.
Back/Home can request cancellation while the copied operation is pending;
new Quick edits are suppressed until collection and a neutral input boundary.
Finalization checks the atomic owner before unrelated teardown.

## Activation gates that remain open

This is not a production activation. Shared automatic contexts/rule/preset and
low-battery setters are unchanged and do not manufacture USER intents. Their
legacy namespace-1 writes must be fenced by the selected Runtime; they need
separate, explicitly resolved automatic-origin operation semantics before
namespace-1 production activation. Preserve low-battery consumed-crossing and
partial-once semantics, and resolve the Home/editor rule-mask distinction.
Automatic/rollback/boot origins remain rejected by the USER helper. No SD
backup acknowledgement is claimed by a native durable receipt. Source-conflict
selection and cross-device reconciliation remain unresolved.

## Qualification

`scripts/test_quick_atomic.py` executes production controller staging,
quick_actions input reduction, quick_session and quick_radios against a copied
scheduled provider double. Cases cover all explicit keys, OFF metadata,
same-value saves, previews/torch, coalesced/stale/repeated gestures, copied
payloads, Busy, cancellation races, ordinary errors, invalid endpoints,
malformed receipts, dirty-token refusals, failed release and retained close.
This is host qualification, not a physical-device test or product-selected
Runtime end-to-end test. `receipts/quick-atomic-20261012.json` records completed
checks and the exact default-off baseline comparison.

Final selected qualification runs 41 cases normally and under ASan+UBSan,
including actual reducer DND/Wi-Fi/Bluetooth/Contexts/brightness actions queued
through one delayed apply window and bounded-queue overflow. The shared Settings
helper regression passes 47 actual controller cases under ASan+UBSan. Existing
Quick actions (108 core/session/adapter executions), radio, tone-input and
frontlight-session regressions pass. Xtensa compile, ELF structure and
imports/exports validate; the default-off Home ELF and manifest remain exactly
byte-identical to a5bd60f.

The older sparse Home tone-startup fixture timed out on both the selected and
unchanged flag-off .94 profiles after excluding unsupported unrelated fixture
features. That broader startup/wake matrix is not claimed as passed. Full
product-selected Runtime routing, independent automatic-origin policy,
source-conflict handling, and physical-device qualification remain gates.
