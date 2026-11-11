Monocypher 4.0.2, upstream LoupVaillant/Monocypher tag 4.0.2.
Unmodified monocypher.c/h; dual BSD-2-Clause / CC0 terms are retained in source headers.
https://github.com/LoupVaillant/Monocypher/tree/4.0.2
SHA256 monocypher.c: 02174117935699d418443c75a558a287deb06ef8cf7c1adced61d9047d2f323d
SHA256 monocypher.h: fcaf6ed771358bb4f40fba016f6518ae86ec02b1b877d2cc35ad92d3a26fd7b3
Uses X25519 and RFC8439 ChaCha20-Poly1305 with a fresh ephemeral key per enrollment;
HKDF-SHA256 is composed from the existing pinned SHA256 primitive.
