# Masked keyboard capture comparison

`../../scripts/test_keyboard_capture_runtime.py` executes the real masked
`ui.text-input` owner, the shared scene presenter, the selected production GT911
provider, and the production Runtime/Graph. Only the physical controller,
GPIO/I2C/synchronization boundaries, clock and display transport are fixtures.

Example, from the System repository:

```sh
python scripts/test_keyboard_capture_runtime.py \
  --runtime /path/to/Runtime \
  --gt911 /path/to/X4 \
  --reader /path/to/Reader \
  --baseline /path/to/delivered-System \
  --candidate /path/to/candidate-System \
  --output /path/to/results
```

Add `--sanitize` for ASan/UBSan. Paths must point to separate, frozen source
checkouts. The runner hashes its production and fixture inputs and refuses a
qualification receipt if any listed source changes while the tests run.
It never accesses hardware, credentials, networks, or published artifacts.
The Reader checkout supplies only the two named SDK headers used by the
existing strict GT911 transport fixture.

## Matrix and assertions

- 600 physical taps at 200-ms spacing (nominal 60-wpm key cadence)
- 180 physical taps at 133-ms spacing (approximately 90-wpm burst cadence)
- Repeated Q and alternating Q/W, with 20-ms and 100-ms contact durations
- A real 64-byte-capacity masked text session. Every 60th tap is an explicit
  on-screen CLEAR, avoiding password-buffer saturation. Thus the 600-tap case
  has 590 character taps and 10 CLEAR taps; the 180-tap case has 177 character
  taps and 3 CLEAR taps. No ABI or ownership shortcut expands the buffer
- A 2,300-ms modeled display completion delay and four modeled 2-ms transport
  slices serviced through Runtime/Graph
- Explicit virtual pixel costs of 500 ns (control) and 10,000 ns (stress)

Both baseline and candidate must preserve every tap and key value in the
500-ns control. The candidate must also preserve all counts and exact value
order in the stress cases, including CLEAR. The baseline stress records loss
instead of aborting at its first missed key. At least one baseline stress case
must reproduce loss, so a comparison against an already-fixed baseline is not
silently called a reproduction.

The controller model scans every 5 ms. A changed snapshot latches READY until
STATUS is acknowledged. Output records physical DOWN/UP counts, acknowledged
pressed/neutral report counts, logical text actions, maximum polling gap and
maximum modeled release-to-action latency when exact ordering is verified.
These boundaries distinguish missed physical reports from later semantic
rejection. Baseline logical counts may additionally reflect later capacity
saturation if a CLEAR tap was among the missed inputs.

## Limits

The virtual costs are stress parameters, not measured ESP32 execution times.
The model's latching behavior is an explicit assumption; this is not a physical
GT911 measurement. The actual panel driver and SPI transport are not run here.
All timing numbers are simulated. The proof demonstrates a software scheduling
vulnerability and its regression protection, not the cause of a particular
reported hardware symptom. It does not establish a repeated-character-only
fault: the stressed baseline can lose both repeated and different characters.
