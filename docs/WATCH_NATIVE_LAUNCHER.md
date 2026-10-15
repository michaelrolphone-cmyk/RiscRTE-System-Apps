# Capability-sized watch launcher

Springboard's Nova path now accepts supported RGB565 display dimensions, including 410×502. The app scales its hex layout and hit testing from the reported width and centers it in the reported height. Existing motion, frame cadence, launch handling and input-while-present behavior remain shared across devices.

The native-height profile merges the complete licensed portable glyph inventory with the existing higher-resolution Watch rasters. Pixel-center bilinear coverage avoids nearest-neighbor blocks. Files has explicit folder metadata; the touchpad glyph is included. The visible 23-app delivered inventory is tested for complete glyph coverage. No substitute glyph is used for a missing icon.

Circle interiors bypass square roots; mask processing skips the fully opaque interior. This reduces rendering work without changing the motion model. The production launcher suite covers 240×240, rotation, empty/small catalogs, asynchronous display delays and the full 23-app 410×502 catalog. UBSan, caption ASan/UBSan, 19,000 randomized trajectories, and bounded frame/input assertions pass. Physical responsiveness still requires device testing.
