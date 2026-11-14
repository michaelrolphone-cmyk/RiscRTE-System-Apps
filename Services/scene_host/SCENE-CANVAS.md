# Borrowed canvas API on ui.scene

`RiscSceneCanvasV1.h` appends a tagged accessor at a fixed offset after the full
telemetry prefix. Compact variants keep unsupported telemetry fields zero.
The descriptor still provides **only ui.scene@1**; all existing dependencies and
prefix APIs remain unchanged. Keep the admitted ui.scene grant until the borrowed
canvas session and every frame lease/receipt have been closed successfully.

Only one scene or canvas session can own physical input/display. Pause/quiesce is
not close: the other interface remains BUSY. Shared retention or Runtime identity
loss fences both interfaces; no cleanup callback is attempted after that fence.
The provider cannot quiesce/unload with a live session, lease, receipt or input
claim. Canvas cooperate/yield callbacks cannot reenter another open operation.
Canvas algorithms are the unchanged published `Services/canvas_host/*.h` sources.

Generic applications may try ui.canvas first and, only on clean absence, inspect
this optional suffix on ui.scene. A malformed accepted grant or retained result
is not permission to try another capability. Old providers without the suffix
remain honestly unavailable for canvas operations.

## Qualification

From the repository root:

```
python scripts/test_scene_canvas.py --runtime RUNTIME --output OUT
python scripts/test_scene_canvas.py --runtime RUNTIME --output OUT-SAN --sanitize
python scripts/test_scene_host.py --runtime RUNTIME --sanitize
python scripts/test_native_division.py --output DIV-SAN --sanitize
python -m unittest discover -s test/scene_build -p '*_test.py'
python scripts/build_scene_services.py --runtime RUNTIME --output TARGET --host-profile full
```

Target profiles `full`, `color-dynamic`, and `watch-compact` retain their original
feature/dependency distinctions. Each build repeats the host, checks exact bytes,
and runs the existing ELF structural validator. Private compiler division helpers
avoid unsupported stock libgcc division objects; they are hidden, not firmware
ABI exports. No import whitelist is widened. Optional generic builder archive
linking/normalization is explicit and leaves existing callers' defaults unchanged.

Host tests model physical callbacks. They do not establish physical display/input
or product Runtime execution; those are separate evidence layers.
