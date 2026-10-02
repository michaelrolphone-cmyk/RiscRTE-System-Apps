# Image Viewer

Image Viewer is the foundational file-handler app for JPEG, PNG, and BMP images opened through the RiscRTE file workflow. Upstream version **1.1.1** requires firmware **1.2.85**, builds as `image_viewer.elf`, and declares `.jpg`, `.jpeg`, `.png`, and `.bmp` as supported file types.

The app gets the source path from `T5FileOpenApi` version 1 into a 512-byte buffer. With no pending source it instructs the user to open an image from File Browser. A path is probed through `T5ImageApi` version 1; probe failure displays an error. A successful probe displays the basename and format/width/height metadata. JPEG and BMP are rendered through provider `render_fit`, whose ABI contract preserves aspect ratio, never upscales, and draws without presenting.

Current source deliberately does not call `render_fit` for PNG after a successful probe because it documents a known firmware PNG decoder fault path. Instead it displays `PNG decoder error prevented` and `Update firmware for PNG rendering`. Thus PNG participates in dispatch/probe but is intentionally not rendered by this app version.

Back, any tap, or exit closes the viewer. File handoff and image decoding/rendering are firmware-provider responsibilities. No network I/O or app-owned persistent state is present.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `f97d4875bd89054966b3f22ed0442e4ccb8b9c36`, manifest `f84588b1f89a8906c59283007dfefdb2efafcbe7`, `T5FileOpenApi.h` `4b697ae5bc8e2613959954556ceab0c48e6ac4f6`, and `T5ImageApi.h` `6b767a71810b65ab3651f13703f79e55666d5bcf`. No independent destination build/release artifact is established yet.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.1.1**; the application C source is synchronized without source edits. Source blob `f97d4875bd89054966b3f22ed0442e4ccb8b9c36`; manifest blob `6fbf0c557886e26946de0a5451719f19bb43cccf`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-image_viewer-v1.1.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-image_viewer-v1.1.1/application-image_viewer-1.1.1-xtensa-esp32s3.rte.zip) has a **4223**-byte package with SHA-256 `08a218444ffe165f612f6ed00f0128155282e7a35be636979f367ade1fb9330d`. Its embedded `image_viewer.elf` is **3108** bytes with SHA-256 `98e833a178c778ae12a4a4e142a026ad25c8642b3bed0f0657b748c52c2de8c5`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
