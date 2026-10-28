# Advertised round Settings layout

The selected round opt-in uses display.output `RISC_DISPLAY_INFO_ROUND` bit6.
The ABI struct is unchanged. Drivers8800cb702428032796891656b465b955f8ea75f6
owns this additive declaration and explicitly advertises it only in the round
Waveshare1.75c profile. Safe insets remain the legacy rectangular safe region.
Rectangular/square resolution or inset patterns are never treated as shape.

Round Settings uses physical content coordinates, native retained-family font
rasters, and circle chords intersected over each complete control/text band.
Drawn action bounds and hit bounds share adjustment. No circle pixel clipping
conceals incomplete controls or text. Fonts use an available native raster bank
and reduce only when a complete heading cannot fit its actual chord. Timezone
names retain complete wrapping, page controls, and Save/Cancel/Back identity.

Three-row density is chosen from the retained status font height and available
status/action clearance, not a device-name breakpoint. Smaller round surfaces
use36-reference-unit rows; larger surfaces40. Native actions retain44px minimum
height. Footer allocation, focus reveal, list hit limits, and timezone page/foot
hit bands use the same geometry. Existing sliced-frame receipt/epoch bindings,
drafts, persistence, navigation, sleep modes, and settings remain unchanged.

Owning tests exercise round300/328/330/400/466/480 physical bounds, all basic
editor scenes and common control corners, actual draft navigation, and sanitized
466. Integer chord proofs include odd/even and rectangular backing rasters.
The existing rectangular240 raster is byte-identical across baseline/selector-off
and native240 profiles. Round sizes below300 are not part of this qualification.
Physical-device acceptance and end-to-end scrolling latency remain pending.
