# Application-owned radio sessions

`PORTABLE_RADIO_SESSION` is an opt-in, application-local lifecycle contract.
The new app implements `portable_radio_suspend()` and
`portable_radio_services_safe()` from `PortableRadioSession.h`.

- Suspend closes only app-owned operations, is idempotent, and never resumes RF.
- A failed close or capability release retains the app invocation and grants.
- The adapter suspends before radio quick controls load/apply, due alarms/cues,
  sleep preparation, return handoff, error teardown, and module finalization.
- The safe probe is memory-only. It rejects uncertain cleanup before normal
  storage-backed service reconciliation.
- Dismissing an overlay or waking leaves the operation stopped. A fresh app
  action is required to resume it.

The flag does not change builds which do not select it. Existing audio and Wi-Fi
ownership hooks remain independent. No Runtime ABI or provider functionality is
introduced. App versions which first select this flag must be versioned in their
owner repository.

Run `python scripts/test_portable_radio.py` for the same real-adapter ownership
failure matrix used by audio, plus radio-control ordering. It runs 58 ordinary /
ASan / UBSan executions without a physical controller. Hardware remains untested.
