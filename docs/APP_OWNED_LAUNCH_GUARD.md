# Optional application-owned launch guard

Compile the shared adapter with `PORTABLE_APP_LAUNCH_GUARD` and link
`bool portable_app_before_launch(const char *destination)` from the application.
The callback runs before Home/crown/root-return handoff and QuickActions Wi-Fi
handoff, and before a catalog launch. It can preserve a pending edit, save or
capture and display a save/discard/cancel prompt. Returning false consumes that
single navigation attempt without setting failure, queuing a destination or
running handoff cleanup. The application must not recursively request launch
inside this callback. A fresh action may try again.

After an allowed guarded Home/return request, a Runtime launch refusal logs the
existing return-request diagnostic but leaves the foreground app usable. It
clears that navigation attempt and does not conflate admission refusal with a
retained provider cleanup. Successfully paused resources remain paused; the app
can resume them only through its ordinary safe lifecycle. Confirmed handoff is
terminal and cannot be replaced by repeated polling.

QuickActions may already have paused radio activity when opening its modal;
the guard protects the subsequent app handoff, not the preceding controls
presentation. Actual cleanup refusal still follows the existing failure and
retention handling. Builds without the flag keep their existing behavior. This
is a linked client contract, not a native ABI or capability expansion.

`python scripts/test_launch_guard.py` covers Home, crown, root Back and Wi-Fi
veto/retry, Runtime refusal/retry and terminal handoff across both Watch input
orientations with ASan+UBSan (24 cases). The existing124 paper control cases
also pass. The RF application's own tests cover its pending-edit controller.

Without the option, the default controls Clock ELF is byte-identical to the
2aa0cf63 baseline (SHA256
2e74292d268f311ac0ffb8c7bfd5d5b128e9efaf7094a724381d501255b094f8).
The disabled gate is preprocessed away rather than adding a helper symbol.
