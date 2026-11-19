/* test_aes_cfb8_common.h -- shared machinery for the AES-CFB8 family tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_cfb8.c); each binary defines
 * one struct cfb8_kat row with its own SP 800-38A F.3 CFB8 key/ciphertext
 * constants and then calls the per-cipher test routines below.  The big
 * explanatory comment (the vector provenance, the byte-granular continuity
 * property, and the implementation note) lives in test_aes_cfb8_common.c.
 */

#ifndef TEST_AES_CFB8_COMMON_H
#define TEST_AES_CFB8_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-CFB8 key size: the cipher name and its SP 800-38A F.3
 * CFB8 key/ciphertext hex for the shared 4-block plaintext (the plaintext
 * and the IV are family-wide and live in the common module as arrays). */
struct cfb8_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (all of F.3 uses the same 4-block plaintext
 * and the same IV 000102..0f).  Declared as arrays, not pointers, so that
 * they are address constants usable in static initializers of the per-cipher
 * binaries. */
extern const char cfb8_kat_pt_hex[];   /* the shared 64-byte F.3 plaintext */
extern const char cfb8_kat_iv_hex[];   /* IV 000102..0f */

/* Length of the shared F.3 plaintext in bytes. */
#define CFB8_KAT_PTLEN 64

/* One-shot CFB8 encrypt/decrypt.  Returns the total output length written
 * to `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t cfb8_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly. */
size_t cfb8_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) EVP_CIPHER_fetch one-shot KATs in both directions: CFB8 decryption
 * uses the forward cipher, so decrypting the KAT ciphertext reproduces the
 * plaintext. */
void cfb8_test_fetched_kat(const struct cfb8_kat *kat);

/* (2) The key CFB8 property: byte-at-a-time updates must be identical to one
 * big update, in both directions. */
void cfb8_test_byte_at_a_time(const struct cfb8_kat *kat);

/* (3) Mixed chunk sizes ({1,15,16,17,31}) must also reproduce the one-shot
 * result, in both directions. */
void cfb8_test_streaming(const struct cfb8_kat *kat);

/* (4) A zero-byte update is a no-op that must not disturb the shifted-IV
 * state: 5 + 0 + 11 bytes == one 16-byte update, and a 37-byte message
 * round-trips through split updates. */
void cfb8_test_zero_update_and_roundtrip(const struct cfb8_kat *kat);

#endif /* TEST_AES_CFB8_COMMON_H */
