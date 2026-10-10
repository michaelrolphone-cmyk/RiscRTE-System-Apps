# Token-bound sleep overlay settling

Home 0.3.26 is a separate, optional successor to the exact delivered 0.3.25.
Select `--display-settled-sdk` with the canonical provider SDK and resident policy
host. Other app versions and ordinary build selections remain unchanged.

The overlay previously waited only for the low-latency presentation token. The
product's following panel power preparation cancelled resident settling, so a
logically complete Sleeping image could be faint when sleep started.

The new owner preserves the accepted overlay token, explicitly requests final settling only
for that sleep image, and queries the optional
settled-status suffix. It yields normally and temporarily owns touch/navigation
while waiting, with a final input sample at completion so sub-16 ms fresh input
cannot fall between the periodic sample and power preparation. It returns sleep-UI READY only for that token's completed settling.
Fresh contact or navigation cancels sleep, releases input ownership and restores
the exact foreground image; physical Home also requests a clean child exit.
The product helper retains ownership of all power operations and wake restoration.

There is no fixed sleep or 2.3-second controller constant in Home. Different
provider durations complete as reported. A 10-second/10,000-poll ceiling handles
stalled or reversed clocks. Invalid, failed or superseded-token status and any
uncertain callback retain silently, with no further provider calls, logging,
redraw or cleanup. The legacy absent suffix preserves its original contract;
a present malformed suffix refuses the overlay before drawing.

The qualification matrix uses the actual Home adapter/overlay, input ownership
and product idle helper over deterministic providers. It covers short and long
settling, repeat sleep/wake, touch/Home cancellation, refusal, terminal retention,
newer tokens, timeout/stalled/backward clocks, input failure and cleanup retention.
Separate actual-panel tests qualify the suffix and waveform/power state machine.
These are software proofs, not physical panel-contrast measurements.
