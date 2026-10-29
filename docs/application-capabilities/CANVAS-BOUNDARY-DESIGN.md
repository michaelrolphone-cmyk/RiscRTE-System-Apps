# Provider-owned custom canvas: version-one contract design

This is the missing boundary identified by the existing capability-contract task, not a claim of deployed implementation. Reuse the CanvasApp frame/input/custody implementation and its engine tests; retain scene page and telemetry interfaces for their existing consumers. The intended migration is additive.

## Why a capability boundary rather than another renderer

Current CanvasApp already separates a logical working image from queued display pixels. Its app-linked adapter chooses physical MONO/RGB565 representation, rotation, letterboxing, contact mapping and presentation timing. Those functions belong on the presentation-provider side for one app ELF across device products. Engine logic, source image content, simulation, model history and semantic actions remain app-owned.

A generic canvas capability can carry the existing behavior without requiring an app to learn a device class, physical panel format, waveform, GPIO, DMA queue or screen orientation. It does not replace semantic scenes with a raster-only interface or force custom visualizations into a fixed semantic component list.

## Versioned surface profile

The proposed capability name is `ui.canvas`, API1. It is separate from existing `ui.scene@1`; this avoids changing its table layout or requiring a new canvas consumer to implement every historical page/reading/telemetry suffix. A provider may expose both over the same presenter/resource owner. No implicit Runtime UI implementation is introduced.

A session describes content intent, an optional engine-native logical extent, and whether content should fit, fill or use a provider-selected responsive extent. Engine-native extent is a property of content (for example an imported game's original coordinate system), not a device-resolution branch. The provider returns the actual logical extent, visible/content rectangles, and a geometry revision. The app does not branch on physical device shape; providers map surfaces and input consistently.

Logical image encodings describe the app's content, not the panel backend. Existing monochrome and color engines must be accommodated without losing features. The implementation should reuse existing encoding/conversion helpers and expose an extensible logical-format descriptor rather than silently reinterpret a physical display enum. Unsupported content encoding is explicit; format adaptation belongs in the shared provider/adapter, not per-device app code. The candidate table below assigns logical MONO1 and RGBA8 encodings independently of physical display enums; migration fixtures must qualify their feature preservation.

## Operations and ownership

- **Open:** acquire an invocation-owned session and copied geometry/capacity information. Failure creates no live session. Required-versus-optional presentation is decided by the app feature, not by adding a GUI dependency to its model.
- **Acquire logical frame:** return an explicit writable lease for a declared logical image. It is not a physical display surface. Report actual admissible dimensions/bytes and backpressure, not a fabricated universal quota. An unsuccessful acquisition leaves no writable resource.
- **Submit:** consumes the writable lease into immutable presentation custody and returns a session-scoped receipt. Version one submits a whole logical frame; it has no damage-region argument. Providers may internally derive/reuse damage without changing ownership; explicit client damage requires a later tagged extension. An unsuccessful non-consuming result leaves the lease owned by the caller and says so; an ambiguous/retained result never invites a blind second submit.
- **Poll receipt:** distinguish queued/active, completed, superseded, recoverable failed and uncertain-retained outcomes. Completion or a documented consuming supersession releases presentation ownership; it does not authorize stale writes to the old lease.
- **Cancel/release unsubmitted frame:** relinquish only an owned writable lease. Cancelling queued work requires explicit receipt-level outcome, not memory release masquerading as cancellation.
- **Input:** copied logical-coordinate contacts and semantic navigation/action events use the same geometry revision as displayed content. A gap invalidates derived contact state and requires a current snapshot, as in existing touch semantics. The provider owns rotation, clipping, source arbitration and physical thresholds.
- **Quiesce/resume:** establish no writable or presenting frame and reconciled input before existing resident shared-controls/policy handoff. After resume OK, the caller queries geometry and redraws; version one does not assume retained display contents. Ordinary busy is retryable; retained custody fences the invocation.
- **Close:** succeeds only when ownership is settled; no app pointer, callback, frame or subscription survives a successful close or normal invocation teardown. Reuse existing Runtime terminal-retention semantics when custody cannot be established.

These are transaction boundaries, not per-pixel validation calls. A caller fills its logical image locally; a reusable adapter batches geometry/conversion work. Do not add a health query, synchronous service dispatch or full-frame duplicate copy to every pixel or input event. Optimize reuse/damage/zero-copy only where the explicit lease and immutable queued-frame semantics remain true.

## State examples that constrain implementation correctness

1. Writable frame → submit accepted → queued → complete: app loses write authority at submit; service owns queue until terminal receipt.
2. Writable frame → submit refused before consumption: app retains the same lease and can retry or cancel, without regenerating side effects.
3. Queued frame superseded by a newer one: receipt states precisely which resource was consumed/released; no abandoned allocation or writable alias.
4. Input gap or geometry change during motion: reset/reconcile contacts against the provider's snapshot/revision; do not turn either Y direction into one directional drift.
5. Shared controls requested while presentation is busy: defer host handoff until settled, preserving app model and input history; do not render another copy of the controls in the app.
6. No canvas provider: headless features continue where their own required services exist; an interactive-only operation reports presentation unavailable, not fabricated rendering success.

## Existing implementation reuse and open ABI details

Reuse Utilities CanvasApp logical working/snapshot separation, retained cleanup, slow-display/input fixtures, imported Model Viewer/Hollow Trail engines and feature-preservation tests. Reuse System scene resident/checkpoint logic for host handoff and provider display/touch contracts below the boundary. Do not move game/renderer business logic into a hardware provider.

The candidate table and executable ownership model below define encoding, geometry, input and receipt structures. They remain specification artifacts pending review and production integration, not a claim that the canvas service is implemented. Existing approved per-app migrations remain usable work; only those specifically needing the missing canvas boundary depend on its implementation.

## Concrete candidate ABI and custody rules

`proposed/RiscCanvasV1.h` now supplies a separately labeled candidate C table. It is a definition artifact, not a production provider or migration. Logical MONO1 and RGBA8 are content encodings; new encodings/tables may be added without changing existing meanings. No provider must silently discard color, alpha, contacts or engine state. Unsupported content is an explicit result; capability acquisition alone never promises every content encoding.

`begin_frame` yields a provider-owned writable lease and explicit byte extent/stride. The caller writes only that extent while owning the lease. The lease records its geometry revision internally; a geometry change makes submission STALE until the caller cancels and reacquires. There is no extra revision argument on submit. `submit` OK consumes the lease atomically and returns one stable receipt; the caller must stop accessing the pointer before returning to app work. All ordinary non-OK submit results leave the lease with the caller, including geometry STALE. RETAINED is the exception: no further pointer access or normal operations are permitted, and the invocation follows existing retained-custody policy. `cancel_frame` OK consumes an unsubmitted lease; refusal leaves it unchanged. Unknown/stale tokens do not authorize resource release.

QUEUED/PRESENTING are nonterminal. COMPLETE/SUPERSEDED/FAILED prove that the provider has settled all backing-frame ownership, not just that it stopped displaying an image. CUSTODY_RETAINED is terminal only for normal client operations; it does not assert resources were released. Terminal receipts remain queryable until explicit `forget_receipt`; forgetting a pending or retained receipt is refused without changing custody. This avoids inventing an arbitrary retention count or time window and allows real provider backpressure.

`quiesce` OK proves no writable lease, pending presentation, or unresolved custody remains, and blocks new frame submission until resume. AGAIN/BUSY mean not settled; they do not surrender model state. The provider drains or settles its work without requiring the app to release unknown queued resources. The app must explicitly cancel its own writable leases. Existing resident-shell ownership handles the actual shared-controls handoff after this fence; canvas does not add a second host or copy of controls. `close` OK invalidates the session and all settled receipts; AGAIN leaves it live, and RETAINED fences further normal access. Quiesce alone is not a power-sleep command.

Output structs carry caller-supplied struct_size. Providers inspect size before writing, initialize the returned prefix on success, and leave output/ownership unchanged on ordinary refusal, except contacts AGAIN explicitly returns required count (with no partial contact records and unchanged snapshot sequence). Tokens are nonzero and cannot be reused while observable in their session. No persistence of pointers/tokens is allowed. Zero requested dimensions are valid only for responsive layout; stride/extent multiplication must be overflow-checked at frame admission, not per pixel.

Input is copied logical content input. Events carry geometry revision; callers discard/reconcile motion against a newer geometry or an input gap using contacts snapshot. Snapshot size-query/fill uses AGAIN on growth without partial output. A successful snapshot's sequence identifies the event cut; later events have greater sequence. Contacts have stable ids within a gesture; CANCEL is explicit. Coordinates can fall outside the visible rectangle during captured drags, preserving gesture semantics. Navigation is normalized action data; custom game input is not constrained to the device's physical button layout.

The serialized caller model makes size-query/fill races limited to provider input arrival, not concurrent app calls. No hard display-class, resolution, contact-count, queue-count, frame-rate or per-pixel health-check constraint is introduced by these definitions. Real provider limits return retryable backpressure or explicit unsupported admission, with requested features preserved.

## Definition evidence and limits

`qualify_contract_examples.py` rebuilds and runs nineteen ownership/input/admission examples in normal and ASan/UBSan configurations. The model exercises consumed versus refused leases, stale geometry, immutable queued snapshot backing, pending receipt refusal, settled/superseded handoff, retained fences, contacts growth with no partial output, and input sequence/geometry reconciliation. Existing semantic-scene optional-table tests are recorded separately. These checks detect contradictions in the proposed protocol examples; they do not prove an actual provider implements them, test rendering performance, or replace the existing CanvasApp feature/slow-display suites. Production adapter extraction and identical app-ELF migration remain existing implementation work.

Navigation payload reuses `risc_input_navigation_frame_v1` and its existing buttons/pressed/released semantics rather than new canvas-specific physical controls. Each navigation event is a copied sample at its sequence; contact fields are unused for that event. Unknown future event kinds must be ignored without changing ownership. Input-gap recovery clears derived navigation edges as well as contact motion before adopting the snapshot and subsequent samples.
