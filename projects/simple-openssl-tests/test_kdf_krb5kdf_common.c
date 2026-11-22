/*

 * test_kdf_krb5kdf_common.c -- shared implementation of the KRB5KDF (RFC
 * 3961 "DK", encryption-key derivation) family tests (API in
 * test_kdf_krb5kdf_common.h; one binary per cipher, one struct krb5_family
 * row and one KAT row set per binary: test_kdf_krb5kdf_aes_128_cbc.c and
 * test_kdf_krb5kdf_aes_256_cbc.c).
 *
 * The family covers the Kerberos 5 RFC 3961 "DK" (encryption key) KDF
 * through the EVP_KDF public API: the RFC 3961-derived AES KATs, the
 * n-fold boundary case of a block-size constant, and the error paths.
 *
 * Spec note: the original task brief called for a "digest" parameter, but
 * the KDF is cipher-based: the provider takes "cipher" (AES-128-CBC /
 * AES-256-CBC for the aes128-cts-hmac-sha1-96 / aes256-cts-hmac-sha1-96
 * enctypes), "key" (the input key) and "constant" (the key-derivation
 * constant).  The RFC 3961 DES-EDE3-CBC vectors are not covered because DES
 * lives in the legacy provider, which these programs do not load.
 *
 * Known-answer vectors (Kerberos working-group / RFC 3961 n-fold test
 * values as distributed in OpenSSL's own test data
 * test/recipes/30-test_evp_data/evpkdf_krb5.txt; every value re-verified
 * with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt cipher:AES-128-CBC \
 *       -kdfopt hexkey:<key> -kdfopt hexconstant:<constant> KRB5KDF
 *
 * and the same command under the system OpenSSL 3.0.13; the AES-256-CBC
 * row with -keylen 32 and cipher:AES-256-CBC):
 *
 *   AES-128-CBC, key=42263c6e89f4fc28b8df68ee09799f15,
 *       constant=0000000299 -> 34280a382bc92769b2da2f9ef066854b
 *   AES-128-CBC, constant=00000002aa -> 5b14fc4e250e14ddf9dccf1af6674f53
 *   AES-128-CBC, constant=0000000255 -> 4ed31063621684f09ae8d89991af3e8f
 *   AES-128-CBC, full-block constant (see the blocksize-constant section)
 *       -> b6838592d1101aa673277d35a505b052
 *   AES-256-CBC, key=fe697b52bc0d3ce14432ba036a92e65bbb52280990a2fa2788399
 *       8d72af30161, constant=0000000299
 *       -> bfab388bdcb238e9f9c98d6a878304f04d30c82556375ac507a7a852790f4674
 *
 * (the rows live in the per-cipher binaries).
 *
 * The KDF's output blocks chain by plain output feedback: block 0 is the
 * encryption of the n-fold'ed constant -- for a full block-size constant
 * the n-fold is the identity, so block 0 is the plain zero-IV encryption
 * of the constant under the key, which the preserved AES-128-CBC
 * full-block KAT b6838592... equals exactly -- and every further block
 * encrypts the previous one, which the preserved AES-256-CBC KAT's second
 * block equals.  The blocksize-constant section uses this to verify the
 * AES-256-CBC path relatively, computing the reference in-test instead of
 * inventing a vector.
 *
 */

#include "test_kdf_krb5kdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* The RFC 3961-derived AES-128 and AES-256 input keys (from OpenSSL's krb5
 * test data): the AES-128-CBC binary's KAT rows, blocksize-constant section
 * and negatives derive over the 16-byte key, the AES-256-CBC binary's over
 * the 32-byte one.  The full cipher-block-size constant (a 16-byte constant
 * takes the n-fold identity path) and the 17-byte constant that exceeds the
 * block size are the blocksize-constant and constant-length sections'
 * shared material.  The constants are arrays (not pointers) so that they
 * are address constants usable in the per-cipher binaries' static struct
 * initializers. */
const char krb5_key128_hex[] = "42263c6e89f4fc28b8df68ee09799f15";
const char krb5_key256_hex[] =
    "fe697b52bc0d3ce14432ba036a92e65bbb52280990a2fa27883998d72af30161";
const char krb5_blocksize_constant_hex[] = "000102030405060708090a0b0c0d0e0f";
const char krb5_long_constant_hex[] =
    "000102030405060708090a0b0c0d0e0f10";

/* Working-buffer sizes for the shared KAT material and outputs (the
 * original monolith's KEY_CAP/CONST_CAP/OUT_CAP: the 32-byte AES-256 key
 * and output and the 17-byte over-long constant are the family's longest
 * material). */
#define KEY_CAP 32
#define CONST_CAP 17
#define OUT_CAP 32

/* One KRB5KDF derive with a fresh context (always the "KRB5KDF" KDF).
 * Returns 1 on success, 0 on failure.  KRB5KDF is core OpenSSL 3
 * functionality: a fetch failure fails the program. */
int krb5kdf_derive(const char *cipher, const unsigned char *key,
                   size_t keylen, const unsigned char *constant,
                   size_t constantlen, unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "KRB5KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[5];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KRB5KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    if (cipher != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER,
                                                       (char *)cipher, 0);
    if (key != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                        (void *)key, keylen);
    if (constant != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_CONSTANT,
                                                        (void *)constant,
                                                        constantlen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* One zero-IV single-block encryption with the given cipher and key (the
 * reference step of the relative blocksize-constant check below: with
 * padding disabled and a zero IV, one update of one block is the plain
 * block encryption E(key, in)).  The key length follows from the cipher;
 * the key buffer is the caller's KEY_CAP working buffer.  Returns 1 on
 * success, 0 on failure. */
static int krb5kdf_block_encrypt(const char *cipher, const unsigned char *key,
                                 const unsigned char *in, unsigned char *out,
                                 size_t blocksize)
{
    EVP_CIPHER *ciph = EVP_CIPHER_fetch(NULL, cipher, NULL);
    EVP_CIPHER_CTX *ctx;
    unsigned char zeroiv[EVP_MAX_BLOCK_LENGTH];
    unsigned char finalbuf[EVP_MAX_BLOCK_LENGTH];
    int len, flen, ok = 0;

    memset(zeroiv, 0, sizeof(zeroiv));
    if (ciph != NULL && (ctx = EVP_CIPHER_CTX_new()) != NULL) {
        if (EVP_EncryptInit_ex(ctx, ciph, NULL, key, zeroiv) > 0 &&
            EVP_CIPHER_CTX_set_padding(ctx, 0) > 0 &&
            EVP_EncryptUpdate(ctx, out, &len, in, (int)blocksize) > 0 &&
            EVP_EncryptFinal_ex(ctx, finalbuf, &flen) > 0)
            ok = 1;
        EVP_CIPHER_CTX_free(ctx);
    }
    EVP_CIPHER_free(ciph);
    return ok;
}

/* (1) The KAT rows of the calling binary's cipher. */
void krb5kdf_test_kats(const struct krb5_family *fam,
                       const struct krb5_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen, constantlen, wantlen, k;
    char msg[128];

    for (k = 0; k < nkats; k++) {
        const struct krb5_kat *kat = &kats[k];
        keylen = from_hex(kat->keyhex, key, KEY_CAP);
        constantlen = from_hex(kat->constanthex, constant, CONST_CAP);
        wantlen = from_hex(kat->outhex, want, OUT_CAP);

        CHECK(krb5kdf_derive(fam->cipher, key, keylen, constant, constantlen,
                             out, kat->outlen),
              "KRB5KDF derive failed");
        snprintf(msg, sizeof(msg), "%s constant %s does not match the KAT",
                 fam->cipher, kat->constanthex);
        expect_equal(msg, out, kat->outlen, want, wantlen);
    }

    free_poison_allocations();
}

/* (2) The constant may be up to the cipher block size; a full block-size
 * constant takes the n-fold identity path (see the header for the two
 * verification routes).  The output length is the cipher's key length
 * (value CLI-verified on both the 3.6.5 and the 3.0.13 builds for the
 * AES-128-CBC KAT). */
void krb5kdf_test_blocksize_constant(const struct krb5_family *fam,
                                     const char *keyhex, const char *outhex)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *block = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(keyhex, key, KEY_CAP);
    size_t constantlen = from_hex(krb5_blocksize_constant_hex, constant,
                                  CONST_CAP);
    size_t wantlen, off;
    char msg[128];

    if (outhex != NULL) {
        /* This cipher has a verified full-block-constant KAT (the
         * monolith's AES-128-CBC value). */
        wantlen = from_hex(outhex, want, OUT_CAP);
    } else {
        /* No verified full-block-constant vector for this cipher: verify
         * the same behavior relatively, without inventing a vector.  The
         * output must equal the plain zero-IV encryption chain of the
         * constant under the key -- block 0 encrypts the constant, every
         * further block encrypts the previous one -- so build that chain
         * in-test as the reference.  The output length is the cipher key
         * length, a whole number of blocks. */
        CHECK(fam->blocksize != 0 && keylen % fam->blocksize == 0 &&
              keylen / fam->blocksize > 0,
              "the key length is not a positive multiple of the block size");
        memcpy(block, constant, constantlen);
        for (off = 0; off < keylen; off += fam->blocksize) {
            snprintf(msg, sizeof(msg),
                     "the block-encryption reference step failed (%s)",
                     fam->cipher);
            CHECK(krb5kdf_block_encrypt(fam->cipher, key, block, want + off,
                                        fam->blocksize), msg);
            memcpy(block, want + off, fam->blocksize);
        }
        wantlen = keylen;
    }

    CHECK(krb5kdf_derive(fam->cipher, key, keylen, constant, constantlen,
                         out, keylen),
          "KRB5KDF derive with a block-size constant failed");
    if (outhex != NULL)
        snprintf(msg, sizeof(msg),
                 "block-size constant output does not match the KAT (%s)",
                 fam->cipher);
    else
        snprintf(msg, sizeof(msg),
                 "block-size constant output does not match the "
                 "encryption-chain reference (%s)", fam->cipher);
    expect_equal(msg, out, keylen, want, wantlen);

    free_poison_allocations();
}

/* (3a) The output length must equal the cipher key length (the negative
 * header of the monolith's error-path section).  24 is wrong for both
 * family ciphers, whose keys are 16 and 32 bytes. */
void krb5kdf_test_negative_output_length(const struct krb5_family *fam,
                                         const struct krb5_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(kat->keyhex, key, KEY_CAP);
    size_t constantlen = from_hex(kat->constanthex, constant, CONST_CAP);
    char msg[128];

    snprintf(msg, sizeof(msg), "KRB5KDF accepted a 24-byte output for %s",
             fam->cipher);
    CHECK(!krb5kdf_derive(fam->cipher, key, keylen, constant, constantlen,
                          out, 24), msg);

    free_poison_allocations();
}

/* (3b) An explicitly-set zero-length constant is rejected from OpenSSL 3.1
 * on ("invalid constant length"); 3.0.x only rejects an absent constant
 * and accepts the empty one, so this check is adaptive (the monolith's
 * gate, kept with its negative section in the AES-128-CBC binary). */
void krb5kdf_test_negative_empty_constant(const struct krb5_family *fam,
                                          const struct krb5_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(kat->keyhex, key, KEY_CAP);

    /* The constant param is present but explicitly empty (length zero),
     * which 3.0.x accepts. */
    if (!krb5kdf_derive(fam->cipher, key, keylen, constant, 0, out,
                        kat->outlen)) {
        g_checks++;
    } else {
        printf("note %s: this OpenSSL accepts an explicitly "
               "empty constant (pre-3.1 behavior)\n",
               test_prog_name());
    }

    free_poison_allocations();
}

/* (3c) The constant may not exceed the cipher block size (the
 * "at most the block size" half of the monolith's constant-length rule;
 * the block size is 16 for both family ciphers). */
void krb5kdf_test_negative_constant_too_long(const struct krb5_family *fam,
                                             const struct krb5_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *longconst = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(kat->keyhex, key, KEY_CAP);
    size_t longlen = from_hex(krb5_long_constant_hex, longconst, CONST_CAP);
    char msg[128];

    snprintf(msg, sizeof(msg),
             "KRB5KDF accepted a constant longer than the block size (%s)",
             fam->cipher);
    CHECK(!krb5kdf_derive(fam->cipher, key, keylen, longconst, longlen,
                          out, kat->outlen), msg);

    free_poison_allocations();
}

/* (3d) The cipher, the key and the constant are all mandatory (the tail of
 * the monolith's error-path section; kat supplies the working key and
 * constant). */
void krb5kdf_test_negative_mandatory(const struct krb5_family *fam,
                                     const struct krb5_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *constant = ALLOCATE_SALT(CONST_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(kat->keyhex, key, KEY_CAP);
    size_t constantlen = from_hex(kat->constanthex, constant, CONST_CAP);
    char msg[128];

    snprintf(msg, sizeof(msg), "KRB5KDF accepted a missing cipher (%s)",
             fam->cipher);
    CHECK(!krb5kdf_derive(NULL, key, keylen, constant, constantlen,
                          out, kat->outlen), msg);
    snprintf(msg, sizeof(msg), "KRB5KDF accepted a missing key (%s)",
             fam->cipher);
    CHECK(!krb5kdf_derive(fam->cipher, NULL, 0, constant, constantlen,
                          out, kat->outlen), msg);
    snprintf(msg, sizeof(msg), "KRB5KDF accepted a missing constant (%s)",
             fam->cipher);
    CHECK(!krb5kdf_derive(fam->cipher, key, keylen, NULL, 0,
                          out, kat->outlen), msg);

    free_poison_allocations();
}
