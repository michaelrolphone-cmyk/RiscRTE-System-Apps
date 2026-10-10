# Cached software framebuffers

The X4 Home 0.4.4 and Springboard 1.7.30 builds use the linked
`PortableRasterLayer.h` extension with `PORTABLE_RASTER_SNAPSHOT`.

Input capture, model reduction, software painting and physical presentation have
separate ownership. A reusable app framebuffer contains the next image; display
acquisition occurs only to copy a completed image, after the previous token has
completed. An unsent completed LOW_LATENCY image can be replaced while the
previous display submission is busy. Explicit clean images are not replaced.
The provider never receives a buffer that the UI subsequently mutates.

Layer capture records ordinary drawing calls once, then paints them in bounded
slices into an app-owned surface. END makes the pixels immutable. Subsequent
frames blit a logical source rectangle at a new destination. Rotation, flip,
viewport clipping, packed-bit alignment and destination stride padding are
preserved. Layer replacement creates a new generation; recorded commands retain
the old generation until replay or discard. No Runtime or provider ABI changes.

Springboard caches unselected page contents, its time header and one selected
app tile. Selection changes do not invalidate all twelve page icons. One missing
page is warmed on an idle pass; caches invalidate on catalog names, visual
metadata, time, selection, highlight or orientation as appropriate. Quick Actions
caches the full sheet and borrows its immutable modal underlay until the closing
drain. Position changes only change the blit destination. Control values,
capabilities, time, battery and orientation invalidate sheet content. Home caches
its complete scene so a transition-only frame does not repaint fonts or Points.

At native 800x480 MONO1 each surface costs 48,000 pixel bytes. Springboard uses
one surface per compiled catalog page plus header and selection surfaces; its
page count is bounded by the deployment catalog. Home adds one scene surface and
the modal adds one sheet surface. Command slabs and the working framebuffer are
reused and released at adapter teardown. Allocation failure retains complete
immediate rendering compatibility rather than dropping drawing commands.

Validation for X4 0.1.66 includes 1,728 complete-buffer layer comparisons with
translation, source crops, viewport clips, rotation and flip; zero allocations
and two commands per warm compositor fixture frame; retained-generation and
BUSY immutability/replacement checks; production Springboard gestures and
catalog changes; Home endpoint equality; Quick brightness/tone/open-sheet pixel
equality against immediate rendering; asynchronous sleep and terminal-custody
checks. Synthetic CPU-cost tests keep ordered touch and navigation delivery
within their existing bounds. Host results do not establish hardware FPS.
