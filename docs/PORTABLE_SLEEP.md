# Optional retained application idle sleep

Deployments may compile `PORTABLE_APP_SLEEP_LOCAL` and supply the bounded
`PortableAppSleep.h` hook. The shared client tracks actual touch/crown activity
and invokes it after 60 seconds of idle, with no display lease and its touch
subscription closed. The caller stack, editor drafts and capability grants stay
live. A normal return reopens touch, resets navigation and restarts the interval;
the waking contact/crown is not delivered as an app action. Failure ends polling.
The runtime's native retention barrier must run before fini/unload on unsafe sleep.
No settings namespace, board identity, GPIO or policy service is added here.

The Watch deployment uses Light for five minutes, then terminal Deep only on a
timer wake with no PMU key/IRQ pending. Apps resume directly from Light. Deep
restarts the default Clock. Its manual saved Clock choices are explicitly labeled
Clock-only; other app idle uses Hybrid. Missing/invalid mode records default Hybrid,
while previously valid manual Light/Deep records remain valid and are never rewritten.

Tests exercise drag-neutral idle, held contact, repeated sleep, refusal cooldown,
frame ownership, uint32 time wrap and real nested Settings editor loops.
