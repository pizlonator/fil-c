/* test_aes_cbc_common.h -- shared machinery for the AES-CBC family tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_cbc.c); each binary defines
 * one struct cbc_kat row with its own SP 800-38A F.2 key/ciphertext constants
 * and then calls the per-cipher test routines below.  The big explanatory
 * comment (the vector provenance and the pizfix `openssl enc` cross-check
 * note) lives in test_aes_cbc_common.c.
 */

#ifndef TEST_AES_CBC_COMMON_H
#define TEST_AES_CBC_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-CBC key size: the cipher name and its SP 800-38A F.2
 * key/ciphertext hex for the shared 4-block plaintext (the plaintext and the
 * IV are family-wide and live in the common module as arrays). */
struct cbc_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (all of F.2 uses the same 4-block plaintext
 * and the same IV 000102..0f).  Declared as arrays, not pointers, so that
 * they are address constants usable in static initializers of the per-cipher
 * binaries. */
extern const char cbc_kat_pt_hex[];   /* the shared 64-byte F.2 plaintext */
extern const char cbc_kat_iv_hex[];   /* IV 000102..0f */

/* Length of the shared F.2 plaintext in bytes. */
#define CBC_KAT_PTLEN 64

/* One-shot CBC encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
size_t cbc_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap,
                   int padding);

/* One-shot CBC decrypt; mirror image of cbc_encrypt(). */
size_t cbc_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap,
                   int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly, which is only possible if every intermediate buffer size honors
 * the EVP contract. */
size_t cbc_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap,
                  int enc, int padding);

/* (1) EVP_CIPHER_fetch(ciphername) one-shot encrypt == KAT, and decrypting
 * the KAT ciphertext gives back the plaintext. */
void cbc_test_fetched_kat(const struct cbc_kat *kat);

/* (2) + (3) The same KAT must also hold through the legacy static
 * EVP_aes_<n>_cbc() route (`legacy`, named in the CHECK messages via
 * `byname`) and through EVP_get_cipherbyname(byname). */
void cbc_test_alternate_routes(const struct cbc_kat *kat,
                               const EVP_CIPHER *legacy, const char *byname);

/* (5) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void cbc_test_streaming(const struct cbc_kat *kat);

/* (6) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with
 * PKCS#5/#7 padding, round-trips, and is rejected with padding disabled. */
void cbc_test_padding(const struct cbc_kat *kat);

/* (7) Output-buffer contract: a single 16-byte update into a buffer of
 * exactly inl + block_size bytes succeeds and produces exactly that many
 * bytes (update 16 + final 16, the latter being the full pad block). */
void cbc_test_output_buffer_contract(const struct cbc_kat *kat);

#endif /* TEST_AES_CBC_COMMON_H */
