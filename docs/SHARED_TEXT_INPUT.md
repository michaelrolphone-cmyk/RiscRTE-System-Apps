# One shared plain-text keyboard

This implementation is a System-owned, separately loaded `ui.text-input@1`
provider. Points and BLE naming acquire that one capability. They do not link a
keyboard renderer, key layout, HID decoder or private keyboard fallback.

The recovered earlier `PortableTextInput*.h` draft was app-local code copied
into each application. That draft is not the implementation delivered here.

## Composition and reuse

- `text-input-host` owns the copied text, editing session, character insertion,
  capacity limit, cancellation and optional hardware keyboard subscription.
- The existing PR88 `scene-host` owns all rendering, touch hit testing,
  navigation, presentation profiles and asynchronous frame/token custody.
- A private System keyboard scene node extends that presenter. There is no
  second text-input renderer, and no new Runtime UI implementation.
- `RiscTextEntryV1.h` is the application contract. The original PR60 scene SDK
  headers are copied unchanged with source provenance. This prototype does not
  add an email, URL, password or numeric format.
- Current text storage is bounded printable ASCII, up to 71 bytes. Each app
  supplies its own smaller capacity. App validation and durable save remain
  outside the host.

Select exactly one text-host manifest. The base manifest needs only `ui.scene`;
it is suitable for compositions with no USB subsystem. The alternative USB
manifest also requires `usb.hid.keyboard`. Both use the same ELF and provider
identity. Selecting a provider is not evidence that a keyboard is attached:
only the keyboard snapshot determines hardware preference. Attach hides the
onscreen grid; detach restores the same grid without changing the text. Input
held across activation, attach/detach, queue gaps or transient poll failures is
ignored until neutral. Ctrl/Alt/GUI combinations are not interpreted as text.

The four shared pages preserve the supplied uppercase/numeric topology and add
lowercase and remaining printable characters. Hardware-mode Back cancels and
Confirm accepts. Enter and Escape have the corresponding HID behavior.

The existing experimental `input.text` driver is a low-level transport event
stream, with no attached-keyboard snapshot or modal onscreen presenter. It is
not overwritten or relabeled by this service. The session contract deliberately
uses the distinct `RiscTextEntryV1.h` name/types; a compile regression includes
both ABIs together. This first host backend uses the existing raw HID snapshot
capability so hardware preference can be determined reliably.

## Lifetime and safety

The host and scene presenter check native Runtime liveness immediately after
each dependency callback, including apparent successes. Callback-induced native
retention is terminal before the next provider call or cleanup.

Requests and results are copied. The host retains no application buffer,
callback, code address or display command. Tokens must not be saved across
close, grant release or application unload. Only one text session is active.

The application handoff helper drains its pending frame, releases its mutable
frame, parks its input and claims no host graphics. While the host owns the
modal, the app polls the host and yields. It resumes its own presenter only after
host close succeeds and its grant is released, with input neutrality reset.

`close` returning AGAIN means keep the exact session/grant and retry only that
cleanup after yielding. RETAINED is a one-way fence: no more service I/O or
application/host unmapping. The provider refuses quiescence while active or
uncertain. Display submit consumes the mutable frame; the presenter retains only
the asynchronous presentation token afterward. This also corrects PR88's prior
release-after-submit mistake.

The client permits its existing alarm service to step only at host-confirmed
settled frames. An alarm cancels the text modal, closes it fully, then restores
normal app alarm handling; the keyboard service contains no alarm policy.

The real Runtime tests distinguish correct clean client teardown from a caller
that returns without closing: a clean client unloads normally; an explicitly
retained client and its providers stay mapped. With eager boot pins, a caller
that simply returns can unload because no app pointers were retained, but the
unclosed host refuses graph shutdown and Runtime requires restart. With demand
or unpromoted demand-retained activation, attempted grant teardown instead
retains the unclean app invocation and provider graph. Explicitly armed
demand-retained activation pins the host and has the eager orphan outcome. There is no
claim of automatic abandoned-session reclamation in the opaque Runtime ABI.

## Verification and integration boundary

Scripts:

- `test_text_input_host.py`: copied requests, validation, hardware attach/detach,
  held-key neutrality, capacity, keyboard gaps/errors, cancel, pending close and
  retained no-further-I/O behavior.
- `test_scene_host.py`: existing Alarms presenter plus shared keyboard, glyphs,
  stale/pending/superseded frame, touch cancellation and profile coverage.
- `test_text_input_runtime.py`: production Runtime, provider graph and loader;
  actual unloadable app, scene/text providers, hardware doubles, repeated fresh
  app invocations, pending close and retained graph behavior: 23 modes for each
  of eager, demand, unpromoted demand-retained and explicitly armed
  demand-retained activation (92 executions per build).
- `build_text_input_services.py`: Xtensa GCC8.4 builds and strict ELF validation
  for text host, scene host and both external profiles.

The working source is based on current Home System `9d32d82`. The PR88 presenter
files are composed onto it without replacing current Home, resident-sleep,
fast-interactive or native SDMMC changes. Product boot policy/provider-capacity
selection, cohort binding and physical Watch/X4 testing remain separate; see
`SHARED_TEXT_INPUT_INTEGRATION.md` for the exact edges and remaining migration
scope. No
product image, hardware operation, release, push or PR update is performed here.
