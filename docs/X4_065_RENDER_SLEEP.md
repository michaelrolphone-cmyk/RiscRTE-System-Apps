# X4 0.1.65 rendering and sleep fixes

The deployed asynchronous renderer exposed two assumptions in resident Light
sleep. `present()` could return with a valid outstanding token; the overlay
and restore paths treated it as a fatal retention condition before turning the
frontlight off. Both paths now drain the submitted image before proceeding.
The physical settling loop also collected input without reducing it after the
input/render split. It now dispatches ordered input at its cancellation
checkpoints, including the final sample before sleep.

Home's reference time, labels, dial, battery shapes and dock icons now record
immutable glyph/bitmap/shape operations instead of per-scanline fill commands.
The payload captures highlight geometry and points only to constant generated
assets. Replay clips work to its row band. Desk rendering and allocation-failure
fallbacks keep the original immediate drawing path. One representative Home
scene records 212 operations instead of 4,413 spans, below the 4,096-command
compatibility threshold; 96 scale/highlight/band combinations have identical
complete pixels under ASan/UBSan.

Springboard advertises contact, snap animation and pending launch state through
a private linked-app hook. Resident policy work waits for the animation tail,
then for the final frame, rather than taking the focus/storage boundary after
finger-up. Input, model updates, software rendering and panel progress continue
through their existing decoupled paths. No Runtime ABI changes are needed.

Build identities: Home 0.4.3, Springboard 1.7.29, scene-host 0.2.1. The latest
shared Lists component corrections are included from PR 93 at eec748ca081fe59346be03180600c65013cc7298.

Validation includes the deployed snapshot option, delayed display completion,
94 resident sleep scenarios (normal and sanitized), 13 actual Runtime/Graph
policy cases, seven Springboard motion cases in both modes, Home pixel equality,
and native Xtensa builds. The Springboard fixture records the submitted image's
position, not a controller position that can advance during deferred replay.
The Home transition fixture permits expired intermediate fades while requiring
the complete final Home image. Hardware responsiveness and ghosting still
require device testing; synthetic times are not device FPS measurements.
