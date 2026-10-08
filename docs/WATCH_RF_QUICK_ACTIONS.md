# RF foreground ownership during QuickActions

This focused correction starts from accepted Watch System source f146d82d.
A top-edge press only reserves a possible QuickActions gesture. Until the pull
commits, the shared adapter must not suspend the foreground receiver, load
controls preferences or copy a modal background. A plain tab/gear tap replays to
the app with RF still active. A committed pull still pauses RF, and dismissal
does not restart it.

`PORTABLE_APP_LAUNCH_GUARD` optionally binds the private client callback
`bool portable_app_before_launch(const char *destination)`. The selected app may
veto a return or QuickActions Wi-Fi handoff while a draft, capture window or save
is pending. A veto consumes only that attempt and leaves the app interactive.
A refused Runtime launch also remains retryable when the guard is selected.
Unselected applications retain their existing return-failure behavior. The
callback does not change provider or Runtime ABI or grant additional authority.

The RF Watch profile selects this callback only for Waterfall and assigns new
deployment versions to its rebuilt apps. Accepted Watch1.0.7 binaries and frozen
release inputs are unchanged. This branch contains no X4 display/app additions.

`scripts/test_quick_actions.py` exercises the production shared controller,
preferences and adapter in both orientations, normally and with ASan/UBSan.
The new foreground-radio fixture verifies ordinary tap replay, committed modal
suspension, no implicit resume, and guarded Wi-Fi refusal/retry. The Utilities
RF controller suite additionally exercises this adapter with real Watch touch,
RF pending data and its production save/discard UI.
