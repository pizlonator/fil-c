/* test_camellia_common.h -- shared machinery for the Camellia family tests
 * (CAMELLIA-{128,192,256} in ECB/CBC/CTR/OFB/CFB/CFB8/CFB1).
 *
 * One binary per cipher (test_camellia_{128,192,256}_{ecb,cbc,ctr,ofb,cfb,
 * cfb8,cfb1}.c); each binary defines one struct camellia_kat row with its own
 * KAT ciphertext constant and then calls the per-cipher test routines below.
 * The big explanatory comment (the RFC 3713 anchor, the derivation provenance
 * of the CLI-derived constants, and the CFB1 bit-mode route) lives in
 * test_camellia_common.c.
 */

#ifndef TEST_CAMELLIA_COMMON_H
#define TEST_CAMELLIA_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per Camellia cipher: the cipher name, the shared key/IV/plaintext
 * constants (arrays in the common module; ivhex is NULL for ECB, which takes
 * no IV), the byte length of the plaintext constant, and this cipher's KAT
 * ciphertext. */
struct camellia_kat {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *pthex;
    size_t ptlen;
    const char *cthex;
};

/* Shared KAT material (the RFC 3713 128-bit key and plaintext, extended to
 * 192/256 bits; the 32-byte stream-mode plaintext; the 64-byte CBC/CTR
 * plaintext; the stream-mode and CBC IV; the CTR initial counter).  Declared
 * as arrays, not pointers, so that they are address constants usable in the
 * static struct camellia_kat initializers of the per-cipher binaries. */
extern const char camellia_key_128_hex[];  /* the RFC 3713 128-bit key */
extern const char camellia_key_192_hex[];  /* the RFC 3713 key + 0011223344556677 */
extern const char camellia_key_256_hex[];  /* the RFC 3713 key + ...8899aabbccddeeff */
extern const char camellia_pt16_hex[];     /* the RFC 3713 16-byte plaintext */
extern const char camellia_pt32_hex[];     /* the 32-byte stream-mode plaintext */
extern const char camellia_pt64_hex[];     /* the 64-byte CBC/CTR plaintext */
extern const char camellia_iv_hex[];       /* the stream-mode and CBC IV */
extern const char camellia_ctr_iv_hex[];   /* the CTR initial counter 000102..0f */

/* Byte lengths of the shared plaintext constants. */
#define CAMELLIA_PT16_LEN 16
#define CAMELLIA_PT32_LEN 32
#define CAMELLIA_PT64_LEN 64

/* One-shot encrypt/decrypt with padding disabled; used for the ECB and
 * stream-mode KATs and roundtrips.  Returns the total output length written
 * to `out` (room for inlen + block_size per the EVP contract; the helper
 * CHECKs that). */
size_t camellia_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                      const unsigned char *iv, const unsigned char *in,
                      size_t inlen, unsigned char *out, size_t outcap, int enc);

/* CBC encrypt/decrypt with an explicit padding flag. */
size_t camellia_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *iv, const unsigned char *in,
                          size_t inlen, unsigned char *out, size_t outcap,
                          int enc, int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (last one truncated); padding disabled.  A split must
 * reproduce the one-shot result exactly. */
size_t camellia_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                       const unsigned char *iv, const unsigned char *in,
                       size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Creates a fresh context with bit-length mode enabled through the public
 * two-phase route (a bare init first, then set_flags(EVP_CIPH_FLAG_LENGTH_BITS),
 * then the init that passes key+IV).  A fresh context is used for every
 * bit-mode operation because EVP forwards only flag *changes* to the provider
 * (see test_camellia_common.c).  Returns the context; caller frees it. */
EVP_CIPHER_CTX *camellia_bitmode_ctx_new(const EVP_CIPHER *cipher,
                                         const unsigned char *key,
                                         const unsigned char *iv, int enc);

/* (1) EVP_CIPHER_fetch(ciphername): the one-shot KAT for the row, plus the
 * decrypt direction (decrypting the KAT ciphertext gives back the
 * plaintext). */
void camellia_test_kat(const struct camellia_kat *kat);

/* (2) The KAT must also hold through the legacy static route (`legacy`,
 * spelled legacy_name in the CHECK messages) and through
 * EVP_get_cipherbyname(byname); both routes encrypt the row's plaintext and
 * must produce the row's KAT ciphertext. */
void camellia_test_alternate_routes(const struct camellia_kat *kat,
                                    const EVP_CIPHER *legacy,
                                    const char *legacy_name, const char *byname);

/* (3) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void camellia_test_streaming(const struct camellia_kat *kat);

/* (4) CBC block/padding rules (the CBC binaries only): a padded roundtrip of
 * an odd length (33 -> 48 -> 33) and the padding-disabled Final failure. */
void camellia_test_cbc_padding(const struct camellia_kat *kat);

/* (5) CFB1 bit-length mode (the cfb1 binaries only): byte mode over 8 bytes
 * equals bit mode over 64 bits through the two-phase route, a 13-bit partial
 * final byte writes ceil(13/8) = 2 bytes, and the 13 bits round-trip. */
void camellia_test_cfb1_bitmode(const struct camellia_kat *kat);

#endif /* TEST_CAMELLIA_COMMON_H */
