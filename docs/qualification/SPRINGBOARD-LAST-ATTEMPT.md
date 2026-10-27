# Optional Springboard last-attempt diagnostics

`build_portable_springboard.py --last-attempt-checkpoint` selects bounded pre-call
RAM checkpoints using the existing Runtime ABI. It requires the native custody
fence and remains disabled for every historical profile unless explicitly selected.
It does not change app control flow, provider results, cleanup, display or timing
policy. A false Boolean, denied checkpoint, local failure or retention permanently
stops further diagnostic writes in that invocation. Missing old Runtime support is
optional unavailability, not an app failure.

The copied text says **last-attempt**, not first cause. This is deliberately not
live logging after an uncertain callback. Runtime derives app/invocation identity;
no pointers survive the write. Native checkpoint replay uses the existing read-only
`diag` line command while the same boot remains alive. The generic fatal log does
not automatically print it. Reset/power loss clears it. There is no claim of serial
recovery if the native console is unavailable, and no device command was issued.

Focused unit tests cover false callbacks, terminal touch, old ABI, denied writes,
and retention. `tests/test_last_attempt_runtime.py --runtime CHECKOUT [--sanitize]`
loads an actual host app through actual Runtime and verifies copied checkpoint
survival through normal and retained return, optional unavailable backend, and no
post-false writes. Target Springboard ELF import/export/layout validation is a
separate build check; these are not physical-device acceptance or a bug fix.
