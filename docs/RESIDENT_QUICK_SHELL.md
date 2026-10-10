# X4 resident Quick Actions

The opt-in resident deployment links Quick Actions into `default.elf` once.
Converted foregrounds contain only gesture arbitration and explicit checkpoints;
their model, stack and grants remain live while the host presents the sheet.
Watch and non-resident selections retain their existing implementations.

## Build selection

- Default: `--resident-shell-host --resident-runtime-sdk <Runtime/sdk/app>`,
  together with the selected sparse Home, Quick Actions and completed-display
  snapshot profile. Only the host receives Quick Actions sources and assets.
- Foregrounds: `--resident-shell-client --resident-runtime-sdk <Runtime/sdk/app>`
  with their existing native custody, tagged alarm and touch-scrolling profiles.
  Do not pass Quick Actions, Quick radios, paper sheet transitions or an
  independent idle/sleep implementation to these builds.
- USB: its standalone builder accepts the same client flags plus
  `--tagged-alarm-utilities`. It keeps exporting media only in its own ELF.
- `--wake-light-restore` is an explicit sparse Home selection for the separately
  implemented deep-GPIO-wake restoration of persisted frontlight intent.

Every selected ELF exports the frozen `risc_resident_app_descriptor_v1` literal.
The owner boot policy must separately select the host and every foreground path.
Runtime and resident-table sizes and roles are checked; the invocation getter is
bound only from app entry. Build receipts record the selected role and canonical
Runtime/resident header digests.

## Ownership and navigation

The foreground calls checkpoints only after releasing its frame and settling
presentation and app/provider activity. Guarded editors or media owners may
refuse entry into controls until their existing navigation guard permits clean
return. The frozen API has no cancelable host exit or edit-state negotiation.

The host copies the completed child image through the tagged display snapshot
extension. Snapshot refusal causes no host focus or presentation change. During
controls, the host owns navigation focus and a fresh raw-touch subscription.
It restores the exact copied image, settles presentation, closes its subscription
and relinquishes focus before returning to the same child stack. A neutral gate
prevents controls input from becoming an app tap. Child damage history is
invalidated for the next app-owned frame. The host reloads saved orientation at
overlay entry and clean child return, so a setting changed while it was suspended
also applies to the shared sheet and the next Home frame.

The admitted alarm output descriptor selects sound controls, without opening an
audio provider for detection. Visual-only hardware omits both volume and Silent.
The resident sheet uses rounded tiles and unpadded hours in the saved 12/24-hour
format. The existing automatic low-battery policy is unchanged; the reference's
manual Low Power and Clean Refresh tiles are not new actions in this change.

USB/Wi-Fi navigation from a child sheet queues a target and requests clean child
return. The host runs the target only after that child is fully cleaned up.
Home in a converted child returns to the existing host instead of launching a
second default. Home input in the host remains in that host. A child sleep
checkpoint likewise exits first; only Home may invoke its sleep implementation.

A retained result seals both invocations. The adapter then changes local guard
state only: no subsequent provider, focus, rendering or cleanup calls are made.

## Verification

`test_resident_shell.py` builds and dynamically loads separate real ELF shared
objects containing the production adapter. It checks repeated drag/open/close,
exact copied-image and model preservation, focus/subscription cleanup, queued
USB, clean child Home, refused snapshots and launch admission, app edit refusal,
sleep ordering, sound/visual descriptor paths, truncated/wrong ABI rejection,
and terminal retention including focus failure. Symbol and literal inspection
ensures the foreground ELF contains no Quick renderer or sheet title.

Run it normally and with `--sanitize`, providing the canonical Runtime app SDK
and display snapshot SDK directories. These are deterministic software checks,
not physical panel-latency or hardware qualification. Actual Xtensa builds and
loader/import/export checks are required separately for the selected cohort.
