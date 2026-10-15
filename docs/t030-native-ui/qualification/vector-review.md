# Independent native-vector qualification

Run from the System repository:

```sh
python3 scripts/test_springboard_native_vectors.py
```

The runner compiles the exact production scan converter and embedded outline
corpus with a clipped alpha sink. It runs normal and ASan/UBSan variants and
writes commands, logs, stack-usage output and `build/t030-native-vectors/results.json`.
The geometry CI job runs this command and retains that directory.

The recorded corpus has 168 glyphs. Both variants pass:

- 12,096 draws: eight widths from 1 to 128, square/one-pixel/128-pixel heights,
  repeated-output equality and exact negative-edge clipping.
- Two analytic nonzero-winding cases: opposite nested contours leave an exact
  50×50 hole; equal winding fills the same area.
- Nine rejection cases, each leaving the destination untouched: invalid source
  extent, unknown command, truncated point, overlong varint, excessive width or
  height, source-coordinate overflow, transformed-coordinate overflow and more
  than 512 flattened edges.
- Maximum observed edge count 432/512, active intersections 12, and original
  corpus coordinate-mapping product 8,684,544 at the admitted dimensions.

The source bounds quadratic/cubic subdivision at depth 8, at most nine live
curve frames. To inspect the actual linked Xtensa stack frames as well:

```sh
python3 scripts/test_springboard_native_vectors.py \
  --target-debug-elf /path/to/springboard.debug.elf \
  --target-objdump /path/to/xtensa-esp32s3-elf-objdump
```

The profiled vector candidate's linked LTO prologues use 64 bytes per quadratic
or cubic frame, at most 576 bytes of recursive curve frames; line uses 48 bytes
and draw 144 bytes. `vector-review.json` binds this receipt to its debug ELF
hash. It predates the final adapter brightness correction; rerun the command
against the final debug ELF rather than treating this as final package proof.
Function frame sizes are not a complete task-stack high-water measurement.

Separate controlled full-feature replay measured the four-row checkpoint
change, with every final partial group retained: at 410×502, Contexts capture
calls fell from 261,852 to 138,955 over 240 frames (46.9%); at 240×240, 129,025
to 58,211 (54.9%). Pixel hashes, controller-state hashes and other operation
counts were identical. These are callback counts, not on-device timing or FPS.
Full adapter custody, failure retention, input replay and RGB565 integration
remain covered by their separate suites.
