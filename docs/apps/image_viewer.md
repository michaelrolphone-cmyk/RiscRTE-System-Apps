# Image Viewer

Image Viewer is the foundational file-handler app for JPEG, PNG, and BMP images opened through the RiscRTE file workflow. Upstream version **1.1.0** requires firmware **1.2.85**, builds as `image_viewer.elf`, and declares `.jpg`, `.jpeg`, `.png`, and `.bmp` as supported file types.

The app gets the source path from `T5FileOpenApi` version 1 into a 512-byte buffer. With no pending source it instructs the user to open an image from File Browser. A path is probed through `T5ImageApi` version 1; probe failure displays an error. A successful probe displays the basename and format/width/height metadata. JPEG and BMP are rendered through provider `render_fit`, whose ABI contract preserves aspect ratio, never upscales, and draws without presenting.

Current source deliberately does not call `render_fit` for PNG after a successful probe because it documents a known firmware PNG decoder fault path. Instead it displays `PNG decoder error prevented` and `Update firmware for PNG rendering`. Thus PNG participates in dispatch/probe but is intentionally not rendered by this app version.

Back, any tap, or exit closes the viewer. File handoff and image decoding/rendering are firmware-provider responsibilities. No network I/O or app-owned persistent state is present.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `f97d4875bd89054966b3f22ed0442e4ccb8b9c36`, manifest `f84588b1f89a8906c59283007dfefdb2efafcbe7`, `T5FileOpenApi.h` `4b697ae5bc8e2613959954556ceab0c48e6ac4f6`, and `T5ImageApi.h` `6b767a71810b65ab3651f13703f79e55666d5bcf`. No independent destination build/release artifact is established yet.
