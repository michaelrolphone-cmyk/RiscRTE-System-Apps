# Experimental settings availability UI

Default off. No product manifest, deployment catalog, boot grant, Watch profile,
or hardware has been changed by this work.

## Selection and authority

Select `--experimental-settings-availability` explicitly, in addition to each
existing X4 atomic profile. The deployment must grant
`storage.key-value.availability@1` with the exact namespace:

- Settings and Home: namespace 1
- Wi-Fi: namespace 6

The new read-only status capability grants no keys or write authority. It is
independent of `storage.key-value.atomic@1` and does not pin an editor draft.
The copied `RiscKeyValueAvailabilityV1.h` is the exact Runtime contract from
its nonblocking native-settings lifecycle. Existing builds do not request it.
Home additionally requires the atomic automatic-policy profile, which already
requires sparse X4 Home, resident host, Quick controls, and checked idle policy.

## User-visible behavior

Unavailable or conflicting storage leaves temporary RAM defaults visible.
There is no default write, legacy writer fallback, automatic source merge,
reset, or declaration that an SD backup is confirmed. Native availability only
means the Runtime has published its authoritative local source.

Settings can edit a RAM draft. Saves, including the physical clock/basis
operation, are refused until authority is available. Back, Home, and both
Cancel routes ask Keep or Discard. Failed or cancelled saves retain the draft;
a submitted attempt is never silently recaptured/replayed. A temporary draft
cannot become a write against a newly recovered source. Discard it explicitly
and use the Settings storage row to Reload.

Wi-Fi can connect using temporary RAM credentials without saving them. Save
and Forget are refused when unavailable. Its data grant and draft survive
shared keyboard suspension. Explicit discard/reload closes the namespace-6
data grant, then observes status and reacquires before loading saved networks.

Home shows unavailable, pending or conflict status in its existing notice
region and Quick sheet. Persistent Quick controls are read-only in those
states, including queued previews; navigation, torch, clean refresh and the
existing checked USB route keep their normal behavior. Automatic Home writes
are refused. A newly authoritative generation is read as a complete
configuration at the ordinary clean foreground boundary. The retained TIMER
path neither polls status nor reloads settings.

## Reconciliation boundary

Runtime may collect a source privately while an editor remains open. Pending
status is not authority. Every relevant namespace data grant must leave its
owner boundary before Runtime can publish that source. Status grants do not
count as data grants and never authorize releasing somebody else's custody.

Settings and Wi-Fi provide explicit clean reload controls. Home's ordinary
reads are transient, and its atomic grant is owned only by a Quick modal or
bounded automatic operation. No draft is overwritten when a generation moves.

A boot-pinned provider can still hold a bound namespace-1 dependency. The alarm
service, for example, retains its bound KV dependency for its initialized
lifetime; whether it blocks namespace 1 depends on the deployment's explicit
namespace authorization. A foreground reload cannot release this dependency.
If Pending remains after resolving other drafts, the smallest honest fallback
is a clean Runtime graph restart, after all edits are saved or explicitly
discarded. The UI says so; it does not force provider release or reboot.

Conflicting sources remain untouched. This UI does not choose which dataset
to replace; such a source-selection action needs a separate explicit decision.

## Qualification

Receipts are in `receipts/settings-availability-20261012`,
`receipts/home-settings-availability-20261012`, and
`receipts/wifi-settings-availability-20261012.json`. They record exact source
hashes, target build inputs and bounded host/controller tests. Selected target
ELFs pass structural and import/export validation. Unselected/default artifacts
contain no availability requirement or capability string. AddressSanitizer and
UndefinedBehaviorSanitizer are active; LeakSanitizer is unavailable under the
executor's ptrace environment. Rendered Settings and Home screens were checked.
These are source/host/target-build results, not hardware qualification.
