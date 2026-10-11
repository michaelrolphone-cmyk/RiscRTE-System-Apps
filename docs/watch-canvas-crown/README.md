# Watch canvas crown exit compatibility branch

Base: `63532e4611749acad2060d0437a883b7c4ac8fd5`, the published complete Watch canvas launcher source used in the delivered Watch ancestry. This branch adds persistent, read-only native crown cancellation to that adapter rather than replacing it with the newer shared X4-era adapter. Its bounded target build is for the existing Watch application contract and 23-entry catalogue.

The only production behavior changes are the native exit check before/after the outer app poll and while an already-open Quick Controls modal waits for input. Cancellation discards pending modal actions, restores confirmed brightness/torch and the saved frame using the existing cleanup, and returns an exit request without queuing an app. Runtime owns acknowledgement after complete cleanup, its recovery deadline, and emergency reset.

The Runtime SDK/header helper is byte-bound to published Runtime `3b90cd11cf0ff4e8116bb54d2d9eb9c7c67a148b`. The ABI is append-only and the helper checks the physical table size before reading the suffix. Tests cover a physically short old table and a missing callback.

Unchanged shared features include the existing Watch NOVA rendering, local navigation and sleep, alarm/cue lifecycle, Quick Controls, BLE broadcast, motion wake, automatic low battery, Contexts capture/workflows/face catalogue, and catalogue/launcher behavior. Their existing compile flags remain product inputs. This does not port newer X4 shared-keyboard, resident-shell, paper UI, or shared-settings presentation changes. The product integrator must preserve or rebuild each app from explicitly recorded source and prove complete-store capacity/admission.

Software tests and representative target sizes are recorded in `software-qualification.json`. Aggregate historical tests still contain unrelated stale catalogue/version and sleep-policy expectations; those have not been suppressed. The focused workflow qualifies its stated tests and target spot check, not every legacy workflow or physical hardware.

An exit arriving inside an already-running alarm modal, a blocked provider callback, or uncertain legacy cleanup still relies on Runtime's native deadline fallback. No software build demonstrates physical button timing, non-maskable recovery, or hardware qualification.
