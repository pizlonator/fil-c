/* test_kdf_krb5kdf_common.h -- shared machinery for the KRB5KDF (RFC 3961
 * "DK", encryption-key derivation) family tests (EVP_KDF "KRB5KDF").
 *
 * One binary per cipher (test_kdf_krb5kdf_aes_128_cbc.c and
 * test_kdf_krb5kdf_aes_256_cbc.c); each binary defines one struct
 * krb5_family row (its OSSL_KDF_PARAM_CIPHER value and the cipher's block
 * size) and its own KAT rows, then calls the per-cipher test routines
 * below.  The big explanatory comment (the RFC 3961 background, the
 * n-fold/derive-key commentary, the vector provenance and the CLI
 * verification commands) lives in test_kdf_krb5kdf_common.c.
 */

#ifndef TEST_KDF_KRB5KDF_COMMON_H
#define TEST_KDF_KRB5KDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per cipher: the OSSL_KDF_PARAM_CIPHER value passed to the
 * "KRB5KDF" KDF and the cipher's block size, which caps the key-derivation
 * constant's length.  (The KDF's mandated output length is the cipher's KEY
 * length -- 16 for AES-128-CBC and 32 for AES-256-CBC -- i.e. the KAT rows'
 * outlen; the block size is 16 for both family ciphers.) */
struct krb5_family {
    const char *cipher;
    size_t blocksize;
};

/* One KAT row: the key hex (the RFC 3961-derived input key of the calling
 * binary's cipher, shared via the common module), the key-derivation
 * constant hex, the requested output length and the expected output. */
struct krb5_kat {
    const char *keyhex;
    const char *constanthex;
    size_t outlen;
    const char *outhex;
};

/* Shared KAT material: the RFC 3961-derived AES-128 and AES-256 input keys
 * (every KAT row of both ciphers, and every blocksize-constant and negative
 * section, derives over one of them), the full cipher-block-size constant
 * that takes the n-fold identity path, and the 17-byte constant that
 * exceeds the block size.  Declared as arrays, not pointers, so that they
 * are address constants usable in the per-cipher binaries' static struct
 * initializers. */
extern const char krb5_key128_hex[];
extern const char krb5_key256_hex[];
extern const char krb5_blocksize_constant_hex[];
extern const char krb5_long_constant_hex[];

/* One KRB5KDF derive with a fresh context.  cipher/key/constant may be
 * NULL to exercise the "missing ..." error paths.  Returns 1 on success,
 * 0 on failure.  KRB5KDF is core OpenSSL 3 functionality: a fetch failure
 * fails the program (never SKIP). */
int krb5kdf_derive(const char *cipher, const unsigned char *key,
                   size_t keylen, const unsigned char *constant,
                   size_t constantlen, unsigned char *out, size_t outlen);

/* (1) The KAT rows of the calling binary's cipher. */
void krb5kdf_test_kats(const struct krb5_family *fam,
                       const struct krb5_kat *kats, size_t nkats);

/* (2) The constant may be up to the cipher block size; a full block-size
 * constant takes the n-fold identity path, and the output length is the
 * cipher's key length.  keyhex is the binary's input key.  outhex != NULL
 * compares the derive against that verified full-block-constant KAT (the
 * AES-128-CBC binary's 16-byte value).  outhex == NULL verifies the same
 * behavior relatively, without inventing a vector: the output must equal
 * the plain zero-IV encryption chain of the constant under the key (block
 * 0 encrypts the constant, every further block encrypts the previous one),
 * computed in-test; the preserved full-block AES-128 KAT and the AES-256
 * KAT's second block both corroborate that chaining (see
 * test_kdf_krb5kdf_common.c). */
void krb5kdf_test_blocksize_constant(const struct krb5_family *fam,
                                     const char *keyhex, const char *outhex);

/* (3a) The output length must equal the cipher key length: a 24-byte
 * request (wrong for both family ciphers, whose keys are 16 and 32 bytes)
 * must fail. */
void krb5kdf_test_negative_output_length(const struct krb5_family *fam,
                                         const struct krb5_kat *kat);

/* (3b) An explicitly-set zero-length constant: rejected from OpenSSL 3.1
 * on ("invalid constant length"); 3.0.x only rejects an absent constant
 * and accepts the empty one, so the check is adaptive (a note, not a
 * failure, on the old behavior).  The monolith's gate ran inside its
 * negative section, which this split keeps in the AES-128-CBC binary. */
void krb5kdf_test_negative_empty_constant(const struct krb5_family *fam,
                                          const struct krb5_kat *kat);

/* (3c) The constant may not exceed the cipher block size (the block size
 * is 16 for both family ciphers, so the 17-byte constant must fail for
 * both). */
void krb5kdf_test_negative_constant_too_long(const struct krb5_family *fam,
                                             const struct krb5_kat *kat);

/* (3d) The cipher, the key and the constant are all mandatory (kat
 * supplies the working key and constant). */
void krb5kdf_test_negative_mandatory(const struct krb5_family *fam,
                                     const struct krb5_kat *kat);

#endif /* TEST_KDF_KRB5KDF_COMMON_H */
