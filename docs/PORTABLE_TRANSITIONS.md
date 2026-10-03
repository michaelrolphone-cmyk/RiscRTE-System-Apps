# Optional retained-frame RGB565 transition

This is shared application/client code, with no firmware UI and no new runtime
ABI. `PORTABLE_RETAINED_RGB565_HANDOFF` is disabled by default. It is a deployment
contract, not a property of the generic `display.output@1` acquire call.

A deployment may enable it only when all of the following are verified from its
exact provider, application and lifecycle source pins:

1. The outgoing application requests this launcher only after its last frame
   reports COMPLETE. It does not acquire, modify or release an unsubmitted frame
   after that completion. Failed presentations cannot request this handoff.
2. The provider remains resident between applications and owns its framebuffer.
   An incoming acquire returns the same RGB565 bytes as the last completed
   presentation, unchanged across application grant revocation and ELF unload.
   Initialization zeroes the buffer, but zero-initialization is not evidence of
   a completed frame and does not qualify a standalone launcher entry.
3. The configured default is the outgoing Clock; no independent launch entry
   may enter the opted-in launcher without the completed-frame precondition.
   A different provider or entry route must build without this opt-in.
4. The incoming launcher copies the acquired frame into its own allocation
   before clearing or drawing it. No outgoing heap pointer, callback, frame
   token or framebuffer lease survives the application handoff.

The paired Watch deployment's source-owned panel has a static 240×240 RGB565
buffer. Its boot graph keeps selected drivers resident for the application loop.
Its Clock requests the launcher only between completed presentations. The Watch
packager must enforce these exact inputs before enabling the macro. A standalone
portable development build remains off unless this contract is explicitly
selected; selecting it against an arbitrary provider is unsupported.

Only the compact fast-display presentation uses the transition. Format/stride
validation remains mandatory before any access. Allocation failure, dimensions
above 320×320, slow/static display capabilities
use ordinary direct rendering. The borrowed frame is copied while the incoming
lease is held; only owned allocations participate after submit.

A 180 ms elapsed-time blend increases outgoing box-blur radius as its opacity
falls and decreases incoming radius as opacity rises. The timer starts on the
first submit, after allocation and the initial render, so that first frame is
byte-exact outgoing content. Incoming radius is rounded up, remaining nonzero
until alpha256; at that endpoint the incoming render is unchanged and sharp,
and outgoing contribution is exactly zero in that same frame. The normal frame
cadence presents the endpoint on the next eligible frame; this is not a promise
of an exact physical 180 ms duration. Rendering and touch polling use the normal application loop;
there is no blocking animation loop in the launcher. The current held contact
is adopted as drag-only before its first render, and its release cannot launch.

Two independently owned allocations hold the outgoing image and blur scratch.
Each is width×height×2 bytes (115,200 bytes at240×240), bounded to320×320 and
released at completion or fini/error cleanup. They do not require or enable
partial-damage support. The Watch build also sets `PORTABLE_FORCE_FULL_FRAMES`, disabling that separate
cache even if a future provider advertises partial damage. Every submit remains
full frame, including all 240 rows on the Watch. On other providers, an existing optional previous-frame cache is
separate and is bypassed during the transition. Crown Back
can exit during the transition; child launch waits for it to finish.

These are software contracts and model checks, not measured physical frame rate
or atomic/vsync presentation. No DSP performance claim or benchmark is made.

## Interaction and inexpensive rendering changes

The inherited gesture stays drag-only until it is released; current position is
sampled before the first render. A tap on the current center or its first hex
ring centers and opens in one gesture. Its 160 ms cubic ease reaches the exact
center without the old final-frame spring jump. A farther icon first centers
with the existing spring model, and a subsequent tap opens. A new contact or
cancellation stops a pending open. Pending launches wait for the incoming
transition to finish, while Back remains available throughout.

The compact renderer clears directly to black once, skips alpha blending for
fully opaque pixels, and skips masking the already fully visible center. These
preserve rendered pixels and avoid redundant work; they are not measured
on-device frame-rate claims.

## Verification

- `python scripts/test_portable_transition.py` runs an independent pixel oracle
  over 12,125 frames plus 182 invalid-buffer cases with ASan/UBSan. Production
  adapter fixtures cover padded strides, exact first/final images, forced full
  frames, delayed presentation input sampling, held touch, timer wraparound,
  allocation failure, submit/status failure and interrupted cleanup. The fixture
  counts every transition allocation and free.
- `python scripts/test_springboard_nova.py` also builds the actual application
  with the retained handoff enabled, covering normal, inherited, cancelled,
  repeated and delayed input flows, alongside motion and all 19 tap layers.
- CI builds the opt-in target ELF with the pinned compiler. Host checks and ELF
  validation do not prove physical panel appearance, performance or startup.

On ptrace-based hosts where LeakSanitizer cannot run, `ASAN_OPTIONS=detect_leaks=0`
keeps AddressSanitizer and UBSan enabled; the allocation-count fixture still
checks transition ownership. Standard CI uses its ordinary leak checking.

## Explicit return and lower-latency handoff

Deployment may set `PORTABLE_RETURN_APP` to a named .elf destination. A successful input poll queues it only for an explicit root exit; nested Settings Back remains inside Settings. Errors, health termination and ordinary app launches do not synthesize a return.

`PORTABLE_HANDOFF_EAGER_MS=60` opts into a faster retained-image transition. The outgoing completed image is already visible. Initial incoming drawing time counts toward the60ms phase and the first transfer already contains incoming content, avoiding the prior extra full-frame alpha-zero retransmission. The ordinary180ms exact-first-submission behavior remains the default for existing consumers. Actual target load/SPI/render latency is additional and is not inferred from a host clock model.
