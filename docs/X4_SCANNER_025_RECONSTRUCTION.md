# Exact deployed X4 Scanner 0.2.25 adapter

System 5b28172e979b5f008da19e9a00a3b98e35937974 plus broadcast_adapter.inc, PortableBroadcastClient.h, PortablePreferenceAtomic.h, PortablePreferenceRollback.h and RiscKeyValueAtomicV1.h from a3504d4f8f342d9d78383978e8b70032d0097c23 reproduces the deployed Scanner ELF when paired with Utilities b0b32919a12d7fd12326091f2c6eb0bcfad5b4ad, SDK 1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70 and the recovered X4 display SDK.

Exact compact ELF SHA-256: a21d03b37168feb9f1dd7ae1d7c7620c6d4ab67960bc65db7189d0f5a9431b8d (160144 bytes). Independently recovered from the delivered X4 .103 SD overlay; its manifest matches the immutable .109 selection exactly. This is a reconstructed component source checkpoint, not a claim to have recovered the original .103 build receipt or original mixed-tree commit.

The newer complete System union adds raster-layer code absent from this deployed binary. Retaining the exact older renderer prevents an unrelated UI implementation change. No product image, native firmware, settings layout or other app is modified here.
