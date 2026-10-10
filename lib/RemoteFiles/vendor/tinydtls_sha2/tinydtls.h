/* Minimal standalone configuration for the unmodified BSD SHA-2 source. */
#ifndef WEBDAV_TINYDTLS_SHA2_CONFIG_H
#define WEBDAV_TINYDTLS_SHA2_CONFIG_H
#define SHA2_USE_INTTYPES_H 1
#define WITH_SHA256 1
#define HAVE_ASSERT_H 1
/* Source callers validate pointers; no target assertion runtime is required. */
#ifndef NDEBUG
#define NDEBUG 1
#endif
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#endif
#endif
