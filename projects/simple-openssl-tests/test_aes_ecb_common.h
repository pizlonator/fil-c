/* test_aes_ecb_common.h -- shared machinery for the AES-ECB family tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_ecb.c); each binary defines
 * one struct ecb_kat row with its own SP 800-38A F.1 key/ciphertext constants
 * and then calls the per-cipher test routines below.  The big explanatory
 * comment (the vector provenance and the pizfix `openssl enc` cross-check
 * note) lives in test_aes_ecb_common.c.
 */

#ifndef TEST_AES_ECB_COMMON_H
#define TEST_AES_ECB_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-ECB key size: the cipher name and its SP 800-38A F.1
 * key/ciphertext hex for the shared 4-block plaintext (ECB has no IV; the
 * plaintext is family-wide and lives in the common module as an array). */
struct ecb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (all of F.1 uses the same 4-block
 * plaintext).  Declared as an array, not a pointer, so that it is an address
 * constant usable in static initializers of the per-cipher binaries. */
extern const char ecb_kat_pt_hex[];

/* Length of the shared F.1 plaintext in bytes. */
#define ECB_KAT_PTLEN 64

/* One-shot ECB encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
size_t ecb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int padding);

/* One-shot ECB decrypt; mirror image of ecb_encrypt(). */
size_t ecb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly, which is only possible if every intermediate buffer size honors
 * the EVP contract. */
size_t ecb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *in, size_t inlen,
                  unsigned char *out, size_t outcap, int enc, int padding);

/* (1) EVP_CIPHER_fetch(ciphername) one-shot encrypt == KAT, and decrypting
 * the KAT ciphertext gives back the plaintext. */
void ecb_test_fetched_kat(const struct ecb_kat *kat);

/* (2) + (3) The same KAT must also hold through the legacy static
 * EVP_aes_<n>_ecb() route (`legacy`, named in the CHECK messages via
 * `byname`) and through EVP_get_cipherbyname(byname). */
void ecb_test_alternate_routes(const struct ecb_kat *kat,
                               const EVP_CIPHER *legacy, const char *byname);

/* (4) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void ecb_test_streaming(const struct ecb_kat *kat);

/* (5) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with PKCS#7
 * padding, round-trips, and is rejected with padding disabled. */
void ecb_test_padding(const struct ecb_kat *kat);

/* (6) A zero-length no-padding ECB roundtrip is legal: the update is a no-op
 * and Final emits nothing. */
void ecb_test_zero_length(const struct ecb_kat *kat);

/* (7) A key of the wrong length must not be usable for the binary's own
 * cipher: the only public way to present a `badkeylen`-byte key to a
 * fixed-length cipher is EVP_CIPHER_CTX_set_key_length() + init, and
 * OpenSSL must reject the operation at one of those two points. */
void ecb_test_wrong_key_length(const struct ecb_kat *kat, int badkeylen);

#endif /* TEST_AES_ECB_COMMON_H */
