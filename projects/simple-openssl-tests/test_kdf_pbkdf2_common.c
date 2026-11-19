/*

 * test_kdf_pbkdf2_common.c -- shared implementation of the PBKDF2 family
 * tests (API in test_kdf_pbkdf2_common.h; one binary per digest, one struct
 * pbkdf2_family row and one KAT row set per binary: test_kdf_pbkdf2_sha1.c
 * and test_kdf_pbkdf2_sha256.c).
 *
 * The family covers PBKDF2-HMAC through the EVP_KDF public API: RFC 6070
 * known-answer vectors, passwords crossing the HMAC block size, and the SP
 * 800-132 lower-bound checks behind the "pkcs5" parameter.
 *
 * Known-answer vectors (every hex value verified against the authoritative
 * pizfix OpenSSL 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:sha1 \
 *       -kdfopt hexpass:<pass> -kdfopt hexsalt:<salt> -kdfopt iter:<c> PBKDF2
 *
 * and the same command under the system OpenSSL 3.0.13; the SHA-256 values
 * additionally match OpenSSL's own test data
 * test/recipes/30-test_evp_data/evpkdf_pbkdf2.txt):
 *
 *   RFC 6070, P="password"  S="salt"                             SHA-1
 *     c=1     -> 0c60c80f961f0e71f3a9b524af6012062fe037a6
 *     c=2     -> ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957
 *     c=4096  -> 4b007901b765489abead49d926f721d065a429c1
 *   RFC 6070, P="passwordPASSWORDpassword" (24 bytes),
 *             S="saltSALTsaltSALTsaltSALTsaltSALTsalt" (36 bytes), SHA-1
 *     c=4096, dkLen=25 -> 3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038
 *
 * The c=16777216 RFC 6070 vector is deliberately not run: it costs minutes.
 *
 * Distribution decisions (carried out of the monolith): the KAT rows are
 * digest-specific and moved to their own binaries (the four RFC 6070 rows
 * for test_kdf_pbkdf2_sha1.c; the four CLI/test-data rows -- including the
 * 70-byte block-size-crossing password -- for test_kdf_pbkdf2_sha256.c),
 * while the SP 800-132 "pkcs5" bounds generalize and run in every binary
 * for its own digest, with the digest name parameterized into the CHECK
 * messages.  Where the monolith verified a pkcs5 value for one digest only,
 * the value comparison follows the constant: the SHA-256 binary compares
 * its CLI-verified pkcs5=0 value (25eb86ac...) and the well-known pkcs5=1
 * c=1 prefix (120fb6cf..., the first 16 bytes of its c=1 KAT), and the
 * SHA-1 binary compares the pkcs5=1 c=1 derive against the first 16 bytes
 * of its own RFC 6070 c=1 KAT (the same prefix construction the monolith
 * used for SHA-256; pkcs5 only toggles the SP 800-132 checks, never the
 * derivation) -- but it has no verified pkcs5=0 value, so there only the
 * compliant-derive acceptance is checked.
 */

#include "test_kdf_pbkdf2_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* The RFC 6070 passwords and salts (P = "password" 8 bytes and
 * "passwordPASSWORDpassword" 24 bytes; S = "salt" 4 bytes and
 * "saltSALTsaltSALTsaltSALTsaltSALTsalt" 36 bytes), plus the 16-byte
 * (000102..0f) and 15-byte (000102..0e) pattern salts the pkcs5 bounds
 * section feeds.  The constants are arrays (not pointers) so that they are
 * address constants usable in the per-digest binaries' static struct
 * initializers. */
const char pbkdf2_pass_hex[] = "70617373776f7264";
const char pbkdf2_long_pass_hex[] =
    "70617373776f726450415353574f524470617373776f7264";
const char pbkdf2_salt_hex[] = "73616c74";
const char pbkdf2_long_salt_hex[] =
    "73616c7453414c5473616c7453414c5473616c7453414c5473616c7453414c54"
    "73616c74";
const char pbkdf2_salt16_hex[] = "000102030405060708090a0b0c0d0e0f";
const char pbkdf2_salt15_hex[] = "000102030405060708090a0b0c0d0e";

/* Working-buffer sizes (the original monolith's PASS_CAP/SALT_CAP/DK_CAP:
 * the 70-byte block-size-crossing password, the 36-byte long salt and the
 * 64-byte output ceiling are the family's largest buffers). */
#define PASS_CAP 70
#define SALT_CAP 36
#define DK_CAP 64

/* One PBKDF2 derive with a fresh context (always the "PBKDF2" KDF).
 * Returns 1 on success, 0 if EVP_KDF_derive failed.  PBKDF2 is core
 * OpenSSL 3 functionality, so a fetch failure fails the program (never
 * SKIP). */
int pbkdf2_derive(const char *digest, unsigned char *pass, size_t passlen,
                  unsigned char *salt, size_t saltlen, unsigned long iter,
                  int pkcs5, unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "PBKDF2", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok;
    int i5 = pkcs5;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core PBKDF2 KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                  pass, passlen);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  salt, saltlen);
    params[3] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
    params[4] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS5, &i5);
    params[5] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) Every KAT row of the calling binary's digest through the default
 * (pkcs5=1, SP 800-132 checks off) path. */
void pbkdf2_test_kats(const struct pbkdf2_family *fam,
                      const struct pbkdf2_kat *kats, size_t nkats)
{
    size_t k;
    for (k = 0; k < nkats; k++) {
        const struct pbkdf2_kat *kat = &kats[k];
        unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
        unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
        unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
        unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
        size_t passlen, saltlen, wantlen;
        char msg[128];

        passlen = from_hex(kat->passhex, pass, PASS_CAP);
        saltlen = from_hex(kat->salthex, salt, SALT_CAP);
        wantlen = from_hex(kat->dkhex, want, DK_CAP);

        CHECK(pbkdf2_derive(fam->digest, pass, passlen, salt, saltlen,
                            kat->iter, 1, dk, kat->dklen),
              "PBKDF2 derive failed");
        snprintf(msg, sizeof(msg),
                 "%s c=%lu dkLen=%zu does not match the KAT",
                 fam->digest, kat->iter, kat->dklen);
        expect_equal(msg, dk, kat->dklen, want, wantlen);

        free_poison_allocations();
    }
}

/* (2) SP 800-132 lower bounds behind "pkcs5"=0: iteration count >= 1000,
 * salt >= 128 bits, derived key >= 112 bits.  With pkcs5=1 (the default)
 * the same weak inputs are accepted.  On 3.0.x the bound is enforced while
 * the params are set (inside derive); on 3.1+ during derive itself -- the
 * observable behavior (EVP_KDF_derive fails) is the same.  The monolith
 * ran this section on SHA-256 (whose pkcs5 values it had verified); every
 * binary now runs it for its own digest. */
void pbkdf2_test_pkcs5_bounds(const struct pbkdf2_family *fam,
                              const char *strong_hex, const char *weak_hex)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *salt16 = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want_strong = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want_weak = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen, wantlen;
    char msg[128];

    passlen = from_hex(pbkdf2_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(pbkdf2_salt16_hex, salt16, SALT_CAP);
    if (strong_hex != NULL)
        wantlen = from_hex(strong_hex, want_strong, DK_CAP);

    /* pkcs5=0 with valid inputs works. */
    snprintf(msg, sizeof(msg), "pkcs5=0 rejected a compliant derive (%s)",
             fam->digest);
    CHECK(pbkdf2_derive(fam->digest, pass, passlen, salt16, saltlen, 1000, 0,
                        dk, 16), msg);
    if (strong_hex != NULL) {
        snprintf(msg, sizeof(msg),
                 "pkcs5=0 derive does not match the CLI-verified value (%s)",
                 fam->digest);
        expect_equal(msg, dk, 16, want_strong, wantlen);
    }

    /* pkcs5=0 forbids iteration counts below 1000... */
    snprintf(msg, sizeof(msg),
             "pkcs5=0 accepted an iteration count of 1 (%s)", fam->digest);
    CHECK(!pbkdf2_derive(fam->digest, pass, passlen, salt16, saltlen, 1, 0,
                         dk, 16), msg);
    /* ...salts shorter than 128 bits... */
    saltlen = from_hex(pbkdf2_salt15_hex, salt, SALT_CAP);
    snprintf(msg, sizeof(msg), "pkcs5=0 accepted a 15-byte salt (%s)",
             fam->digest);
    CHECK(!pbkdf2_derive(fam->digest, pass, passlen, salt, saltlen, 1000, 0,
                         dk, 16), msg);
    /* ...and keys shorter than 112 bits. */
    snprintf(msg, sizeof(msg),
             "pkcs5=0 accepted an 8-byte derived key (%s)", fam->digest);
    CHECK(!pbkdf2_derive(fam->digest, pass, passlen, salt16, 16, 1000, 0,
                         dk, 8), msg);

    /* pkcs5=1 accepts all of the above. */
    snprintf(msg, sizeof(msg),
             "pkcs5=1 rejected an iteration count of 1 (%s)", fam->digest);
    CHECK(pbkdf2_derive(fam->digest, pass, passlen, salt16, 16, 1, 1,
                        dk, 16), msg);
    saltlen = from_hex(pbkdf2_salt_hex, salt, SALT_CAP); /* "salt", 4 bytes */
    snprintf(msg, sizeof(msg), "pkcs5=1 rejected a 4-byte salt (%s)",
             fam->digest);
    CHECK(pbkdf2_derive(fam->digest, pass, passlen, salt, saltlen, 1, 1,
                        dk, 16), msg);
    /* Where a c=1 value prefix was verified for the digest, the c=1 derive
     * must reproduce it (the monolith's well-known PBKDF2-HMAC-SHA256
     * prefix construction). */
    if (weak_hex != NULL) {
        wantlen = from_hex(weak_hex, want_weak, DK_CAP);
        snprintf(msg, sizeof(msg),
                 "pkcs5=1 c=1 derive is not the well-known PBKDF2-HMAC-%s "
                 "prefix", fam->digest);
        expect_equal(msg, dk, 16, want_weak, wantlen);
    }

    free_poison_allocations();
}