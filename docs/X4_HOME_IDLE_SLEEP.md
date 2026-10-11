# Home idle and resident Sleeping overlay

The selected X4 resident Home uses its saved idle timeout to enter the same
landscape desk-clock owner as an explicit lock. It no longer takes the generic
Light helper while leaving the portrait Home frozen. Refusal restores Home;
terminal retained custody permits no repaint or provider cleanup.

A settled resident foreground retains its application stack and receives a
centered Sleeping card over a copy of its completed frame. The product idle
helper owns preparation, sleep, native resume and grant release. Only after
confirmed cleanup does the host restore the original frame. A later retained
outcome can leave the Sleeping image physically visible; repainting after that
boundary would be unsafe.

The optional `portable_idle_sleep_ui` callbacks are a local app/helper contract,
not a Runtime or provider ABI. The no-UI entry point remains available. Build the
selected host with the matching product helper that implements
`portable_app_idle_sleep_with_ui`. Resident Home reserves version 0.3.22. This
source checkpoint does not assemble a product image or alter a native source pin.

The overlay refreshes the current reader orientation, including a change made
inside resident Settings. Display failures while drawing/restoring the card
install the silent custody fence before any diagnostic timestamp or log call.
The sparse desk-clock retention entry point follows the same rule after native
sleep-entry or restore retention.

## Verification

- `scripts/test_home_idle.py`: actual Home, adapter, persisted 5/60/180-second
  timers, desk rendering and product sleep hook; both directions, manual lock,
  refused sleep, terminal retention and three subsequent retained-minute wakes,
  normally and under ASan/UBSan with stage logging enabled.
- `scripts/test_resident_idle.py`: actual host adapter and product helper with
  production stage logging; repeated sleeps, orientation changes after host
  startup, delayed display completion, refusals, and retained acquisition,
  submission and presentation on both overlay and restore frames. The untouched
  background and the restored child frame are compared byte for byte.
- Product `minimal/test/run_idle_alarm_sleep_test.py`: actual tagged alarm
  service with the product helper, including UI refusal/retention and retained
  native/provider cleanup.
- Existing resident-policy and resident-shell host fixtures exercise the real
  Runtime/Graph or separate ELF boundary. The policy fixture uses a synthetic
  sleep helper and does not replace the helper integration suite above.
- Existing `scripts/test_shared_quick_home.py` verifies manual lock with the
  shared drawer open and closed, in both desk directions. The older full
  `test_home_desk_lock.py` harness does not support the current resident cohort;
  no passing full retained-minute matrix is claimed here.

These are host/provider-double and Xtensa build checks. No physical sleep,
power, e-ink timing, SDMMC sleep/resume or complete product cohort is qualified
by this source change. The newer delivered X4 0.1.49 image preserves the 0.1.47
applications; future integration must preserve that newer native composition.
