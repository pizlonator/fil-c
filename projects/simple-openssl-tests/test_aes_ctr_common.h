/* test_aes_ctr_common.h -- shared machinery for the AES-CTR family tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_ctr.c); each binary defines
 * one struct ctr_kat row with its own SP 800-38A F.5 key/ciphertext constants
 * (plus its counter-wrap ciphertext for the shared IV ff..fd) and then calls
 * the per-cipher test routines below.  The big explanatory comment (the
 * vector provenance and the counter-carry explanation) lives in
 * test_aes_ctr_common.c.
 */

#ifndef TEST_AES_CTR_COMMON_H
#define TEST_AES_CTR_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-CTR key size: the cipher name, its SP 800-38A F.5
 * key/ciphertext hex for the shared 4-block plaintext and IV f0f1..feff, and
 * its counter-wrap ciphertext for the shared IV ff..fd (the plaintext, the
 * F.5 IV, and the carry IV are family-wide and live in the common module as
 * arrays). */
struct ctr_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;     /* F.5 row (IV f0f1..feff) */
    const char *carryhex;  /* counter-wrap row (IV ff..fd) */
};

/* Shared SP 800-38A KAT material.  Declared as arrays, not pointers, so that
 * they are address constants usable in static initializers of the per-cipher
 * binaries. */
extern const char ctr_kat_pt_hex[];     /* the shared 64-byte F.5 plaintext */
extern const char ctr_kat_iv_hex[];     /* IV f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff */
extern const char ctr_carry_iv_hex[];   /* IV fffffffffffffffffffffffffffffffd */

/* Length of the shared F.5 plaintext in bytes. */
#define CTR_KAT_PTLEN 64

/* One-shot CTR encrypt/decrypt (CTR is symmetric; `enc` selects the route).
 * Returns the total output length written to `out`, which must have room for
 * inlen + block_size bytes per the EVP output-buffer contract. */
size_t ctr_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly. */
size_t ctr_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and decrypt roundtrip. */
void ctr_test_fetched_kat(const struct ctr_kat *kat);

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions (CTR keeps the partial keystream block
 * in the context, so every split point must be transparent). */
void ctr_test_streaming(const struct ctr_kat *kat);

/* (3) Counter carry across the 128-bit boundary: with the shared IV ff..fd
 * the fourth 16-byte block is enciphered with counter 0x0000...0000.  The
 * cipher's carry ciphertext is kat->carryhex, checked one-shot and split. */
void ctr_test_counter_carry(const struct ctr_kat *kat);

/* (4) Partial final block: CTR is a stream mode, so a 37-byte message (two
 * blocks plus five bytes) round-trips, in one shot and in place. */
void ctr_test_partial_final_block(const struct ctr_kat *kat);

/* (5) In-place operation: CTR must accept out == in for both directions. */
void ctr_test_in_place(const struct ctr_kat *kat);

/* (6) Two fresh contexts with the same key and IV produce identical
 * keystreams; the partial-block counter position (EVP_CIPHER_CTX_get_num)
 * tracks how much of the current keystream block was consumed. */
void ctr_test_keystream_and_num(const struct ctr_kat *kat);

#endif /* TEST_AES_CTR_COMMON_H */
