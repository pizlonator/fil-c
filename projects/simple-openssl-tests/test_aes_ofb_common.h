/* test_aes_ofb_common.h -- shared machinery for the AES-OFB (OFB-128) family
 * tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_ofb.c); each binary defines
 * one struct ofb_kat row with its own SP 800-38A F.4 key/ciphertext constants
 * and then calls the per-cipher test routines below.  The big explanatory
 * comment (the vector provenance and the OFB-symmetry note) lives in
 * test_aes_ofb_common.c.
 */

#ifndef TEST_AES_OFB_COMMON_H
#define TEST_AES_OFB_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-OFB key size: the cipher name and its SP 800-38A F.4
 * key/ciphertext hex for the shared 4-block plaintext (the plaintext and the
 * IV are family-wide and live in the common module as arrays). */
struct ofb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (all of F.4 uses the same 4-block plaintext
 * and the same IV 000102..0f).  Declared as arrays, not pointers, so that
 * they are address constants usable in static initializers of the per-cipher
 * binaries. */
extern const char ofb_kat_pt_hex[];   /* the shared 64-byte F.4 plaintext */
extern const char ofb_kat_iv_hex[];   /* IV 000102..0f */

/* Length of the shared F.4 plaintext in bytes. */
#define OFB_KAT_PTLEN 64

/* One-shot OFB encrypt/decrypt (`enc` selects the route; the keystream is
 * the same either way).  Returns the total output length written to `out`,
 * which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t ofb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Returns
 * the total output length.  Splits must reproduce the one-shot result
 * exactly. */
size_t ofb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and -- because OFB uses the
 * same keystream in both directions -- decrypting the KAT ciphertext through
 * the decrypt route gives back the plaintext. */
void ofb_test_fetched_kat(const struct ofb_kat *kat);

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions; the keystream state must carry across
 * update boundaries. */
void ofb_test_streaming(const struct ofb_kat *kat);

/* (3) The decryption route applies the same keystream as the encryption
 * route: decrypt(encrypt(x)) == x and, because C = P XOR KS implies
 * C XOR KS == P, encrypt(encrypt(x)) == x as well. */
void ofb_test_decrypt_is_encrypt(const struct ofb_kat *kat);

/* (4) A zero-length update is a no-op that must not disturb the keystream
 * state: 5 bytes + 0 bytes + 11 bytes must equal a single 16-byte update. */
void ofb_test_zero_byte_update_noop(const struct ofb_kat *kat);

/* (5) A 37-byte message (partial final block) round-trips through the
 * streaming route in both directions. */
void ofb_test_partial_final_block(const struct ofb_kat *kat);

#endif /* TEST_AES_OFB_COMMON_H */
