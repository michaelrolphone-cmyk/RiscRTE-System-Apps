# Portable Springboard client

Springboard remains `Apps/springboard.c`; there is no Watch fork. Version 1.3.3
includes the Reader ec0c09991f8f7babc69b3da0681b0b3f1e99c3ab slow-display
selection fix (1.3.2) and adds a compact static grid for smaller displays. The
existing large-screen renderer and animation remain available on their prior
interfaces. Home editing is offered only with actual storage support.

`lib/PortableApps` is a client library linked into existing application ELFs.
It implements the subset of existing T5 drawing, input, UI and battery getters
used by Springboard and the shared Utilities Battery app through canonical
RiscRTE runtime grants: display.output@1, input.touch.raw@1 and board.battery@1.
These T5 getters are hidden local symbols, not additional runtime exports or a
competing SDK. Original ABI headers are pinned byte-for-byte in SOURCES.json;
the new Portable headers are private library interfaces. Upstream licenses are
retained under the repository license. No hardware register, bus or pin appears
in the adapter.

The deployment links a bounded catalog of at most 16 explicit apps. It does not
pretend to discover installed files. Runtime boot policy must grant each app's
own required capabilities. A successful launch request returns through the
existing app lifecycle; there is no recursive loader or retained app state.
Unavailable storage/video returns no interface, and editing is disabled. Drawing
uses bounded RGB565 surfaces (200..1024 pixels per dimension), clipped primitives,
original compact glyphs and two simple symbols. Presentation yields and has both
an elapsed-time and iteration bound. Unsupported display formats fail closed.

Touch uses the canonical copied-event/snapshot interface. Each invocation needs
neutral input before arming; event gaps, failed polls, multiple contacts and
contact replacement discard gesture state. A completed stationary tap activates
an item. Upper-left is Back; Battery's upper-right is Update. Battery rows scroll
using the bottom Previous/Next controls. Subscriptions, held frames and grants are released on return and on
partial initialization failure. No driver/task pointer survives unload.

`python scripts/test_portable_apps.py --utilities /path/to/RiscRTE-Utilities`
executes the production apps and adapter under UBSan. Existing app fixtures and
target builders remain intact. Watch deployment builds link this library against
the real shared sources and separately validate target imports/exports. Runtime
and physical peripheral integration are distinct evidence, not implied by these
service-model tests.

Source audit records remain historical. Springboard is now an intentional
external development delta; do not overwrite it when synchronizing Reader.
Published-byte checks still require identical bytes for unchanged published
versions; strictly newer versions are reported as development builds, never
reported byte-identical to an older publication. Springboard: 1.3.1 -> 1.3.3.
