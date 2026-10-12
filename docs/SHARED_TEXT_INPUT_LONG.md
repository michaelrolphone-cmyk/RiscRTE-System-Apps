# Long shared plaintext input

The optional long suffix on `ui.text-input@1` adds a 256-byte copied request and
result text buffer. Capacity includes the trailing NUL: a LoRa message uses
capacity 241 for at most 240 printable ASCII bytes. Empty drafts are valid;
control and non-ASCII bytes are rejected. This does not add multiline editing,
radio policy, persistence, or an application keyboard.

## Client contract

Include `PortableTextInputLongClient.h` and retain the existing
`portable_text_client` session object:

- `portable_text_client_begin_long(client, runtime, label, initial, capacity)`
- `portable_text_client_poll_long(client, risc_text_entry_state_v1_long *)`
- `portable_text_client_close(client)`

The helper validates the sized/tagged/versioned `risc_text_entry_api_v1_long`
suffix and both long entrypoints before suspending the app presenter. Unsupported
providers return `RISC_TEXT_ENTRY_UNAVAILABLE` without opening or truncating a
draft. The direct contract uses `risc_text_entry_request_v1_long`,
`risc_text_entry_state_v1_long`, `open_long`, and `poll_long`.

Calls preserve the established copied-buffer ownership, grant, liveness,
Home-reason, bounded attention and close/retry/retained behavior. Accepted data
must be copied by the caller and acted on only after successful close. Polling
an old 72- or 128-byte result while a larger-capacity session is active returns
`RISC_TEXT_ENTRY_INVALID` before consuming input or writing the result.

## Compatibility and presentation

The original 72-byte layouts, 128-byte extension, and Home/masked/extended suffix
offsets are unchanged. Legacy ASCII and masked-password semantics remain the
same. The separate 128-byte filename extension continues preserving non-control
filename bytes. The new long plaintext path does not permit masked requests.

Text host 0.1.6 increases its internal copied text storage by 128 bytes. Both full
and compact scene host 0.4.5 accept the same existing private keyboard viewport
encoding up to 255 bytes. The scene document ABI and 71-character tail viewport
are unchanged; the packed count carries the full length and capacity limit.
Existing 127-byte scenes remain valid. An older scene host rejects unsupported
long sessions cleanly. Keyboard text revisions keep their input epoch, so
repeated keys continue to be processed while a display frame is still pending.

## Qualification

Use the existing test scripts with an appropriate Runtime source checkout:

- `test_text_input_host.py --decoupled-attention`: old behavior, frozen
  pre-extension header compatibility, ABI sizes/offsets, suffix rejection,
  malformed input/results, 240/255-byte limits, USB and onscreen repeated input,
  Home/Back, pending close, and reopen into old clients.
- `test_text_input_runtime.py --scene-compact`: actual unloadable app/text/scene
  modules with production Runtime/ProviderGraph. Long 240-byte flows and all old
  modes run under eager, demand, demand-retained, and armed retained activation.
  `--long-only` selects the new Done/Back/Home flows.
- `test_scene_host.py`: existing scene lifecycle, password privacy, keyboard and
  rendering checks plus 72/127/128/240/255-byte viewport validation boundaries.
  Generate the existing telemetry fonts first for the full host.
- `test_file_browser_shared_input.sh` and `test_wifi_workflow.py`: existing Files
  long/non-ASCII local/remote workflows and password-client application behavior.

Run normal and `--sanitize` (or `SANITIZE=1` for the shell suite) variants.
These are host checks, not physical touchscreen or radio qualification. Native
component builds and final product composition remain separate steps.
