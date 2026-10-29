# Shared application capability contracts — source reconciliation

Status: working design under Rhizome `a60738b8-3a44-4502-af30-6e8ad56948be` (13 points). This document distinguishes existing contracts from additions still requiring design/implementation qualification. It does not turn findings into new user restrictions.

## Scope and evidence

The first-pass inventory contains 93 app occurrences in 33 groups and 151 provider occurrences across four products. Actual packaged application manifests declare 57 capability/API pairs. The inventory keeps 18 source/catalog extras and 12 uninstalled Badge roles visible; absence from a particular image does not remove them from scope. Existing migrations retain standing approval. Forty-one unresolved original-build bindings are findings, not a universal prerequisite for other work.

`EXISTING-SOURCE-CONTRACTS.json` pins every reviewed header to exact bytes and the selected published Runtime/System/Utilities inputs. These are current integration inputs, not an assertion that their branches cannot have newer heads. `APP-CAPABILITY-CONTRACT-COVERAGE.json` preserves each required/optional declaration separately. Declared optional use is not evidence that a feature was exercised.

## Reuse before adding interfaces

| Concern | Existing contract | Boundary to preserve |
|---|---|---|
| Capability acquisition | Runtime API1 `acquire`/`release` and `risc_runtime_capability_v1` | Explicit manifest and deployment grants; table version/size checked; invocation-local borrowed interfaces. No implicit hardware discovery in an app. |
| Semantic UI | `ui.scene@1`, components, lifecycle/checkpoint suffixes | Apps submit model/content/action IDs. Presenter owns geometry, typography, pagination, physical hit regions and asynchronous display custody. |
| Text/document/telemetry presentation | Existing text entry, copied page/reading/links/telemetry suffixes | Preserve exact document semantics, normalized plot gaps, text editing and copied-payload ownership. Do not replace established APIs just to introduce a new umbrella API. |
| Navigation/input | `input.navigation@1`; `input.touch.raw@1` below presentation adapters | Navigation intents and logical canvas input may cross the app boundary; transport, rotation, controller sampling and physical contact mapping stay with providers/adapters. Snapshot recovery after a touch gap is existing semantics, not a new polling mandate. |
| Shared controls and policy | Runtime resident shell plus scene resident bridge | Default/host owns shared controls; foreground app keeps its model. Scene quiescence precedes controls/policy handoff. A nonresident product does not become integrated merely because its app ELF admits successfully. |
| Headless Alarms | `alarm.control@1`, tagged scheduler/editor contracts and published headless owner | Model/control work has no GUI dependency. GUI is an optional consumer, not a required scheduler. Preserve exact retry commands and edit-custody fences. |
| Time, preferences and storage | Existing civil-time, preference, KV, file/volume/AppData contracts | Reuse existing authority and persistence semantics; storage choice and clock basis are not device-specific app branches. Private versus shared data authorities remain distinct. |
| Custom engines | Utilities `lib/CanvasApp`, Model Viewer/Hollow Trail and their imported engine history | Preserve engine features, logical frame snapshots, slow-display/input tests and retained-custody behavior. The current adapter is app-linked, not a separately acquired generic canvas service. |

The current semantic scene ABI has fixed capacities and suffix layout rules. These are facts of that ABI, not newly proposed universal limits. Capability design should expose or negotiate real provider capacity where needed, without inventing a small common denominator or changing existing ABI structures in place.

## Required and optional capability semantics

The existing task asks for optional UI/input/device-service contracts, not that every capability is optional for every operation. Keep the distinction at the feature or operation boundary:

- A headless alarm operation can proceed without UI. An interactive editor needs an admitted presentation route; lack of one does not imply the underlying alarm service stopped.
- An optional radio, file source or sensor can be absent while other functions remain usable. An operation that requires that resource must report its actual unavailability; it must not fabricate success or silently discard persisted data.
- Runtime `acquire` currently returns a boolean. A false result alone does **not** distinguish absent hardware, denied policy, incompatible provider or another acquisition failure. Do not invent a reason code in the app. Use richer provider diagnostics only where the existing contract supplies them.
- Optional table suffixes are discovered through version/size/tag/function admission before reading them. Unknown or unsupported functionality remains unavailable; a short table is not dereferenced speculatively.
- Absence, transient busy/pending work, invalid requests, stale handles and retained/uncertain custody are distinct existing states. They are not interchangeable with an empty model or successful completion.
- No-device-name and no-backend branches are existing migration criteria. Responsive layout arithmetic or a game engine's own logical scene dimensions are not automatically forbidden fixed-device branches. The app may describe its logical content; the provider owns adaptation to the actual display/input device.

## Ownership and lifecycle

Runtime grants remain invocation-scoped. A copied document, frame or event must not leave an untracked app pointer/callback in a longer-lived provider. Existing scene APIs copy documents before returning. Physical display surfaces are borrowed resources with separate submit/completion custody; releasing an app grant does not magically prove a queued frame completed.

Use the existing settled-work checkpoint and resident handoff semantics. Do not place headless service work into every UI render/yield path. Busy/deferred service work is not permission to stop rendering or consume all CPU time. Preserve owner-selected scheduling and meaningful frame/input progress.

For shared controls, preserve the existing scene activity/input-busy signals and host checkpoint. The host renders the controls, not every client. Current temporary Badge per-app controls are recorded compatibility debt, not universal architecture completion or justification to remove functioning controls before host integration.

Existing RETAINED states mean uncertain custody and a terminal invocation fence. This document does not introduce extra checks on every rendering instruction. Check at actual admission, ownership and lifecycle boundaries already defined by the APIs. Ordinary transient refusal must remain retryable without resetting a model or replaying side effects.

## Custom canvas: reuse and the actual gap

The selected System SDK already supports copied MONO1 document pages and semantic plots. `display.output@1` exposes physical formats, surface dimensions and presentation tokens. Utilities `CanvasApp` already provides a logical working/snapshot frame and input adaptation for imported engines, but its current implementation performs format/rotation/output mapping and some device/profile-dependent selection inside the app-linked adapter.

The missing boundary is a provider-owned, device-independent custom surface contract reusable by those engines and other custom visualizations. This is not evidence that no canvas code exists. The first reuse candidate is the existing CanvasApp implementation and tests, alongside scene page/telemetry contracts; do not rewrite the engines or delete their features.

The contract design must cover logical dimensions/content, presentation/input mapping, copied or explicitly leased pixel/command payloads, asynchronous completion, logical contact/navigation events, backpressure and cancellation/retention. A canonical logical image representation is different from selecting a physical panel format. Color and multi-contact features must not be lost by forcing all consumers through the existing MONO1 page path. Custom rendered content must coexist with default-owned shared controls and policy handoff.

No new canvas ABI is declared implemented here. Before defining a new versioned table, reconcile current source-index results and the existing CanvasApp source/test lineage, then specify precisely which behavior is reused and which capability boundary is new. Proposed capacities, enum values and ownership rules need executable examples, not arbitrary constants inferred from one device.

## Coverage and qualification examples

Use the inventoried applications as tests of a generalized contract, not a finite whitelist:

1. Headless Alarms with no scene provider, current mixed scheduler and edit contention: no GUI dependency; pending/retained states do not become successful writes.
2. Lists/Settings semantic navigation on rectangular and round presentation: model/actions unchanged, provider owns geometry, controls and hit regions.
3. Model Viewer and Hollow Trail with slow asynchronous display, multiple contacts and logical content dimensions: engine/features retained; no mutation of queued pixels.
4. File Browser/Reader with optional volume, private storage and file-open handoff: preserve each storage authority and unavailable-feature explanation.
5. Radio/audio/telemetry apps with optional devices: unavailable functions are disclosed without removing unrelated features or requiring one hardware backend in app code.
6. Same application artifact across product bindings: compare full ELF identity separately from provider differences, packaging metadata and device qualification.

Existing tests and delivered source are reused first. New qualification should target missing contract behavior. No new full-product acceptance or hardware gate is silently added to the contract-definition task, and software interface tests must not be reported as device acceptance.
