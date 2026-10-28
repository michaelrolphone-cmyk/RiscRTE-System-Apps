# Independent staged sleep timers

`--sleep-settings --sleep-timer-settings` explicitly exposes both maintained
Idle sleep timer and Deep sleep timer controllers without adding sleep, battery,
radio or other hardware grants. Select `--default-sleep-mode hybrid` for the
shared Light-first workflow. The second timer is the interval already asleep
before Deep, not an alternate idle timeout. Existing saved mode and independent
namespace-1 `sleep_idle` and `sleep_deep` records retain precedence and encoding.

`--idle-timer-settings` remains the exact single-idle-row profile. The flags are
mutually exclusive. Neither flag may be used without sleep Settings or in
incompatible desk/native-paper profiles. Native-time-touch is supported; new
selection reports Settings1.4.1 and both editable timer booleans in its receipt.
No controller persistence/retention logic is replaced. Cancel does not write;
ordinary write failure remains unconfirmed and rereads, not a success claim.

Owning qualification: `scripts/test_settings_sleep_defaults.py` checks profile,
mode and fallback matrix, invalid combinations and sixteen actual-controller
normal/ASan/UBSan builds. Both rows exercise fresh reads, explicit Save, cancel,
write refusal, malformed/missing/unreadable fallback, independent keys and bounds.
`python -m unittest discover -s tests -p '*settings*'` preserves builder tests.
Actual selected Runtime retained-custody behavior and physical display/touch are
separate product qualification; the compact controller double is not that proof.

The native-time-touch profile keeps its established centered 240-pixel content
presentation, positioned by capability dimensions/safe insets; it is not claimed
as a new full-panel Settings redesign. Timer adjustment and action buttons share
drawn/hit geometry constants. The owning geometry matrix drives both timer editors
at240x240, round466x466 with68-pixel safe insets in both touch orientations, and
400x480 with30-pixel insets. Captures are actual software RGB565 frames, not photos.

Explicit mode/timer Save first completes a frame marked `Saving - not confirmed`
while custody is known. Readback-confirmed commits alone return Saved. Ordinary
IO with successful independent readback may confirm an already persisted write;
failed verification remains unconfirmed. A terminal custody result leaves the
already-presented pending frame and makes no later provider/render/cleanup calls.
`test_settings_timer_retained.py` tests the actual custody proxy/controller for
mode, idle and Deep keys in normal/sanitized lanes. Actual Runtime/product and
physical validation remain separate from these provider-double owning tests.

Save is deferred while the pending redraw is dirty or its presentation is still
in flight. The normal input/presentation loop advances it; no nested polling or
extra provider call is used after custody loss. Until completion the chosen draft
is frozen (additional Save/adjust taps do not change it), while Back/Cancel still
cancels without a write. The owning async fixture delays every frame, checks the
matching pending frame/value completed before the sole write, and covers repeat
Save/adjust input, cancellation, callback failure and failed-present retention.
