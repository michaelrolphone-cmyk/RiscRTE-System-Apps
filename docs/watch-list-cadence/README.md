# Watch list cadence investigation

This is an isolated diagnostic checkpoint based on the compact Watch canvas adapter with crown hook, System `6ea07eb`. It does not alter the 1.1.6 recovery composition or change production behavior.

`python scripts/probe_watch_list_cadence.py` drives the real Settings controller/view/adapter with a wall-clock drag and configurable synthetic display-completion delay. All cases start after the initial frame is complete. The fixture retains lifecycle, grant-release and no-storage-write assertions.

The existing `present()` waits for the token to complete. While waiting it captures touch samples, but logical Settings scroll reduction cannot resume until the call returns. A 400 ms drag generates 19 scroll updates with a zero-cost display, 10 at 40 ms latency and 5 at 80 ms latency. Logical poll stalls reach 60 and 100 ms respectively. With a 240 ms display delay, an 80 ms drag advances only 30 pixels of the requested 120-pixel travel because movement sampled during presentation is not reduced before release.

These values are deterministic software measurements, not actual Watch frame rates or hardware latency. No visual effects, animation duration, input thresholds or UI features have been removed. A correction must keep logical input and wall-clock animation independent of display completion, preserve the latest complete presentation state without replaying stale frames, and retain existing crown/modal/alarm/cleanup custody behavior. Merely increasing a sampling rate or dropping visual work would not meet that requirement.
