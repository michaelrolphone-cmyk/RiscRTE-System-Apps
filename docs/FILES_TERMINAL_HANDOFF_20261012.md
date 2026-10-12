# Files 1.5.32: completed file-open handoff

With the delivered `PORTABLE_FILE_SETUP` and `PORTABLE_FILE_PEER` combination,
`fbx_dispatch` successfully requests the selected file handler, closes Files,
and sets `fb_terminal`. The real `app_main` then drains its last display frame.
That drain checks the setup service. Previously the service treated every
false owner predicate as failure, including a clean terminal app with setup
never started. The adapter consequently retained the invocation after a
successful file-open request, before the host could unload Files.

The setup service now accepts a terminal checkpoint only when the adapter is
not retained, sharing is OFF, and setup is uninitialized, OFF or UNAVAILABLE.
It performs no provider work in this completed state. Live/pending setup,
cleanup-only ownership, and genuine retained states retain their existing
failure fences.

The regression drives the actual Files `app_main`, ordered touch input, file
handler request, close, terminal drain and module finalization. It uses the
real setup helper and peer/sharing components with deterministic providers.
The baseline fails specifically at `!retains` after the earlier assertions
prove one successful handler request, terminal state and no peer/network
ownership. Local setup-never-started, setup-closed and setup-unavailable cases
run on RGB, paper, paper-scroll and paper-scroll with raster snapshots.
Existing peer transfer/upload/collision/cancel/expiry/unavailable/owned cases
also check their terminal drain on the three original profiles. The manually
stepped legacy peer-frame fixture is not qualified for deferred raster;
the new snapshot profile runs the actual application-loop handoffs instead.

This is host evidence for the observed pre-Reader retention signature, not a
physical X4 boot or complete Reader-launch test. The final target must keep
the deployed 16-grant mounted Files profile documented in
`FILES_SCROLL_POLICY_20261012.md`, at the same candidate version 1.5.32.
