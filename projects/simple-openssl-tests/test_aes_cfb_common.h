/* test_aes_cfb_common.h -- shared machinery for the AES-CFB(128) family
 * tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_cfb.c); each binary defines
 * one struct cfb_kat row with its own SP 800-38A F.3 CFB128 key/ciphertext
 * constants and then calls the per-cipher test routines below.  The big
 * explanatory comment (the vector provenance, the verification commands, and
 * the shift-register note) lives in test_aes_cfb_common.c.
 */

#ifndef TEST_AES_CFB_COMMON_H
#define TEST_AES_CFB_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-CFB128 key size: the cipher name ("AES-<n>-CFB") and its
 * SP 800-38A F.3 CFB128 key/ciphertext hex for the shared 4-block plaintext
 * (the plaintext and the IV are family-wide and live in the common module as
 * arrays).  The F.3 .Encrypt vectors are the KAT constants; the .Decrypt
 * examples are covered by decrypting those ciphertexts, since CFB decryption
 * uses the forward cipher. */
struct cfb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (all of F.3 uses the same 4-block plaintext
 * and the same IV 000102..0f).  Declared as arrays, not pointers, so that
 * they are address constants usable in static initializers of the per-cipher
 * binaries. */
extern const char cfb_kat_pt_hex[];   /* the shared 64-byte F.3 plaintext */
extern const char cfb_kat_iv_hex[];   /* IV 000102..0f */

/* Length of the shared F.3 plaintext in bytes. */
#define CFB_KAT_PTLEN 64

/* One-shot CFB128 encrypt/decrypt.  Returns the total output length written
 * to `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t cfb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly. */
size_t cfb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) EVP_CIPHER_fetch one-shot KATs in both directions: encrypting the
 * plaintext matches the F.3 CFB128 .Encrypt vectors, and decrypting those
 * ciphertexts through the decrypt route (which uses the forward cipher)
 * gives back the plaintext. */
void cfb_test_fetched_kat(const struct cfb_kat *kat);

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions. */
void cfb_test_streaming(const struct cfb_kat *kat);

/* (3) num-state continuity: CFB128's shift register position is observable
 * through EVP_CIPHER_CTX_get_num() and must survive zero-length updates.
 * After 5 bytes the position is 5; after 11 more it wraps to 0; a further 3
 * bytes move it to 3 again. */
void cfb_test_num_continuity(const struct cfb_kat *kat);

/* (4) The same continuity holds on the decrypt route, and a 37-byte message
 * round-trips through split updates in both directions. */
void cfb_test_decrypt_num_and_roundtrip(const struct cfb_kat *kat);

#endif /* TEST_AES_CFB_COMMON_H */
