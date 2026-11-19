/* test_aes_siv_common.h -- shared machinery for the AES-SIV family tests.

 * One binary per cipher (test_aes_{128,192,256}_siv.c); each binary defines
 * its RFC 5297 A.1/A.2 struct siv_kat rows (its own doubled key and its own
 * expected synthetic IV / ciphertext) and then calls the per-cipher test
 * routines below.  The big explanatory comments (the SIV rules, the S2V
 * size-boundary rationale, the empty-payload version difference, and the
 * vector-derivation provenance) live in test_aes_siv_common.c.
 */

#ifndef TEST_AES_SIV_COMMON_H
#define TEST_AES_SIV_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One RFC 5297 A.1/A.2 tuple (or a derived tuple in the same format): the
 * label used in the check messages, the cipher under test, its key (twice
 * the underlying cipher key size), and the expected synthetic IV and
 * ciphertext. */
struct siv_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *sivhex;
    const char *cthex;
};

/* Shared RFC 5297 A.1/A.2 input material: the ADs, the nonce, and the
 * plaintexts are cipher-independent (only the keys and the expected
 * SIV/ciphertext differ per row).  Declared as arrays, not pointers, so
 * that they are address constants usable in static initializers. */
extern const char siv_a1_ad_hex[];
extern const char siv_a1_pt_hex[];
extern const char siv_a2_ad1_hex[];
extern const char siv_a2_ad2_hex[];
extern const char siv_a2_nonce_hex[];
extern const char siv_a2_pt_hex[];

/* SIV encrypt: key (2x), AAD updates, ONE payload update, Final, GET_TAG.
 * The tag (synthetic IV) is returned via `siv`; the ciphertext via `out`
 * (which needs room for inlen bytes; SIV emits none extra). */
void siv_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char * const *ads, const size_t *adlens,
                 size_t nads, const unsigned char *pt, size_t ptlen,
                 unsigned char *out, size_t outcap, size_t *outlen,
                 unsigned char *siv);

/* SIV decrypt with the synthetic IV set before the payload update. */
void siv_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *siv,
                 const unsigned char * const *ads, const size_t *adlens,
                 size_t nads, const unsigned char *ct, size_t ctlen,
                 unsigned char *out, size_t outcap, size_t *outlen);

/* (1) The row's RFC 5297 A.1 tuple (single AD) through the one-shot
 * helpers, including the decrypt-back-to-plaintext check. */
void siv_test_kat_a1(const struct siv_kat *kat);

/* (1) The row's RFC 5297 A.2 tuple (two ADs and the nonce as the last AAD
 * update -- multiple AAD updates before data), likewise with the decrypt
 * roundtrip. */
void siv_test_kat_a2(const struct siv_kat *kat);

/* (2) Roundtrips for the S2V boundary sizes {1,15,16,17,1024} for
 * ciphername (the sizes straddle the S2V <16 / >=16 branches; the empty
 * size is section (2b) below).  keylen is the doubled key length (32/48/64),
 * CHECK()ed against the fetched cipher. */
void siv_test_roundtrip_sizes(const char *ciphername, size_t keylen);

/* (2b) The empty-payload case, with the 3.0.x/3.6.x behavioral difference
 * asserted explicitly (see test_aes_siv_common.c). */
void siv_test_empty_payload(const char *ciphername, size_t keylen);

/* (3) A wrong synthetic IV must fail DecryptFinal (and the output must be
 * cleansed by the implementation). */
void siv_test_wrong_siv(const char *ciphername, size_t keylen);

/* (4) A context performs exactly ONE crypto operation per init: a second
 * payload update on the same context must fail (siv128.c crypto_ok). */
void siv_test_second_payload_fails(const char *ciphername, size_t keylen);

/* (5) The doubled key length is enforced: requesting a 1x key length at
 * init must fail.  (A full-size key buffer is passed so the failure comes
 * from the length parameter, not from a short buffer.) */
void siv_test_wrong_key_length(const char *ciphername, size_t keylen);

#endif /* TEST_AES_SIV_COMMON_H */
