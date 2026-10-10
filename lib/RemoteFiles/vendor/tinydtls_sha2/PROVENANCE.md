# SHA-256 source provenance

sha2.c and sha2.h are byte-for-byte copies of the Aaron D. Gifford SHA-2
implementation vendored in the installed official PlatformIO espressif32
platform's espidf-coap-server/libcoap/tinydtls example:
platforms/espressif32/examples/espidf-coap-server/components/libcoap/ext/tinydtls/sha2/.

Original source identifies Aaron D. Gifford, copyright 2000-2001, with BSD
3-clause redistribution terms retained in both files and LICENSE.txt. SHA-256
is selected by the local minimal tinydtls.h configuration. SHA-384/512 and the
DTLS protocol are not enabled. Required dependencies are C standard fixed-width
integer types, size_t, memcpy, memset and disabled assertions. There is no TLS,
network, entropy, allocation or operating-system dependency.

Upstream project: https://github.com/eclipse/tinydtls/tree/main/sha2
Platform source: https://github.com/platformio/platform-espressif32

Copied source SHA-256 checksums:
- sha2.c: 1635a91007f60c9bfc8c42dde1ed7d4f6134240bf0289583b51bfd614d49ebc5
- sha2.h: a473ab617b9cb37d9b13dfc95e07a1dc3eeecdbe4afe488c5426dac721286c7b
