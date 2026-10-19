# Private native LoRa presentation ownership

`PORTABLE_LORA_NATIVE_UI` adds owner11 for the separately built Utilities LoRa
Messages app. It is app-local linkage, not a Runtime/T5 ABI or product selector.
`portable_lora_native_begin` succeeds only for the capability-selected RGB565
surface. Monochrome admission remains inactive. The existing display token,
frame/snapshot custody, ordered raw input and cleanup machinery remain owners
of their resources.

The five hidden hooks begin an invocation, select a hit-map generation, submit
a frame, query read-only readiness, and query the original physical origin of
one delivered eligible tap. They do not allocate a queue, read events ahead,
add provider authority or modify unrelated native owners.

Raw touch is observed before Quick's top-edge reservation. A replayed release
must have an original eligible DOWN in the same visible generation. A layout
change, previsibility contact, move or cancellation discards that witness;
release coordinates cannot fabricate a new start. The app compares that origin
with its physical action rectangle. This preserves ordinary header taps while
preventing a contact from crossing a page/history mapping change. Same-map
redraw keeps the established visibility watermark and owned gestures.

Run:

```
python scripts/test_lora_native_input.py
python scripts/test_lora_native_optout.py --output build/lora-native-optout
```

The reducer runs normally and under ASan/UBSan, including RGB admission,
monochrome bypass, delayed visibility, held gestures, Quick replay origins,
stale generations, snapshot faults, generation exhaustion and hidden symbols.
The opt-out test compares 40 preprocessed compositions against exact System
main `4b11af9e44bc7ec71a7a63d14d898b6141c616b4`, including other native owners,
Quick, paper selectors and raster snapshots. No LoRa hook is emitted with its
selector absent. End-to-end app/adapter, shared editor and target qualification
are maintained in Utilities. Software checks do not establish hardware results.

## Composed owner boundary

The publication candidate is based on System main
`c94f367f6ca3677954b580833f222ae16775c964`, retaining merged Files owner 9
and Frequency owner 10. LoRa remains owner 11.
`test_lora_native_composition.py` verifies the existing Files/Frequency private
implementations are byte-identical to that base and compares 52 LoRa-off
preprocessed compositions, including Files/Frequency together, Quick and raster
snapshots. The original 40 pre-LoRa controls remain separate and unchanged.
No Points owner 12 or Stopwatch owner 13 source is introduced.
