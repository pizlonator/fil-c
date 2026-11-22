/*

 * test_kdf_pkcs12kdf_common.c -- shared implementation of the PKCS12KDF
 * family tests (API in test_kdf_pkcs12kdf_common.h; one binary per digest,
 * one struct pkcs12_family row and one KAT row set per binary:
 * test_kdf_pkcs12kdf_sha1.c and test_kdf_pkcs12kdf_sha256.c).
 *
 * The family covers the PKCS#12 KDF (RFC 7292 appendix B) through the
 * EVP_KDF public API: the three material types (id 1 = key, 2 = IV, 3 = MAC
 * secret), both the SHA-256 and the SHA-1 digest, iteration-count
 * sensitivity and the mandatory pass/salt error paths.
 *
 * There are no published RFC vectors for this KDF, so the constants below
 * were derived with the authoritative pizfix OpenSSL 3.6.5 (and
 * cross-checked against the system OpenSSL 3.0.13; the two agree on every
 * value):
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt digest:sha256 \
 *       -kdfopt hexpass:70617373776f7264 -kdfopt hexsalt:73616c74 \
 *       -kdfopt iter:2048 -kdfopt id:1 PKCS12KDF
 *
 * (pass = "password", salt = "salt"; the hex* prefixes make the CLI decode
 * the octet-string parameter values from hex).
 *
 */

#include "test_kdf_pkcs12kdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* pass = "password" (70617373776f7264), salt = "salt" (73616c74).  The
 * constants are arrays (not pointers) so that they are address constants
 * usable in the per-digest binaries' static struct initializers. */
const char pkcs12_pass_hex[] = "70617373776f7264";
const char pkcs12_salt_hex[] = "73616c74";

/* Working-buffer sizes (the original monolith's PASS_CAP/SALT_CAP/DK_CAP:
 * pass = "password" is 8 bytes, salt = "salt" is 4 bytes, and every
 * derivation produces the full 32 bytes). */
#define PASS_CAP 8
#define SALT_CAP 4
#define DK_CAP 32

/* One PKCS12KDF derive with a fresh context (always the "PKCS12KDF" KDF).
 * pass or salt may be NULL to exercise the "missing pass/salt" error paths
 * (the omitted parameter is simply not set at all).  Returns 1 on success,
 * 0 on failure.  PKCS12KDF is core OpenSSL 3 functionality: a fetch failure
 * fails the program (never SKIP). */
int pkcs12_derive(const char *digest, const unsigned char *pass,
                  size_t passlen, const unsigned char *salt,
                  size_t saltlen, unsigned long iter, int id,
                  unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "PKCS12KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core PKCS12KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    if (pass != NULL)
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                      (void *)pass, passlen);
    else
        params[1] = OSSL_PARAM_construct_end();
    if (salt != NULL) {
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      (void *)salt, saltlen);
        params[3] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
        params[4] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS12_ID, &id);
        params[5] = OSSL_PARAM_construct_end();
    } else {
        params[2] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
        params[3] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS12_ID, &id);
        params[4] = OSSL_PARAM_construct_end();
    }
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) The KAT rows of the calling binary's digest: two digests x three
 * material ids in the monolith, plus (for SHA-256) an iteration count of
 * 1.  All 32 bytes of output. */
void pkcs12_test_kats(const struct pkcs12_family *fam,
                      const struct pkcs12_kat *kats, size_t nkats)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen, wantlen, k;
    char msg[128];

    passlen = from_hex(pkcs12_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(pkcs12_salt_hex, salt, SALT_CAP);

    for (k = 0; k < nkats; k++) {
        const struct pkcs12_kat *kat = &kats[k];
        wantlen = from_hex(kat->dkhex, want, DK_CAP);
        snprintf(msg, sizeof(msg), "PKCS12KDF derive failed (%s)",
                 fam->digest);
        CHECK(pkcs12_derive(fam->digest, pass, passlen, salt, saltlen,
                            kat->iter, kat->id, dk, DK_CAP), msg);
        snprintf(msg, sizeof(msg), "%s id=%d iter=%lu does not match the KAT",
                 fam->digest, kat->id, kat->iter);
        expect_equal(msg, dk, DK_CAP, want, wantlen);
    }

    free_poison_allocations();
}

/* (2) The three material ids (key / IV / MAC secret) must all be derived
 * from the same pass and salt yet produce different output. */
void pkcs12_test_ids_differ(const struct pkcs12_family *fam)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk[3];
    size_t passlen, saltlen, i, j;
    char msg[128];

    passlen = from_hex(pkcs12_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(pkcs12_salt_hex, salt, SALT_CAP);
    for (i = 0; i < 3; i++) {
        dk[i] = ALLOCATE_OUTPUT(DK_CAP);
        snprintf(msg, sizeof(msg),
                 "PKCS12KDF derive failed for one of the ids (%s)",
                 fam->digest);
        CHECK(pkcs12_derive(fam->digest, pass, passlen, salt, saltlen, 2048,
                            (int)(i + 1), dk[i], DK_CAP), msg);
    }
    unpoison_all();
    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++) {
            snprintf(msg, sizeof(msg),
                     "two PKCS12KDF material ids produced identical output "
                     "(%s)", fam->digest);
            CHECK(memcmp(dk[i], dk[j], DK_CAP) != 0, msg);
        }
    repoison_all();

    free_poison_allocations();
}

/* (3) The pass and the salt are both mandatory: omitting either must fail. */
void pkcs12_test_missing_inputs(const struct pkcs12_family *fam)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen;
    char msg[128];

    passlen = from_hex(pkcs12_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(pkcs12_salt_hex, salt, SALT_CAP);

    snprintf(msg, sizeof(msg), "PKCS12KDF accepted a missing pass (%s)",
             fam->digest);
    CHECK(!pkcs12_derive(fam->digest, NULL, 0, salt, saltlen, 2048, 1,
                         dk, DK_CAP), msg);
    snprintf(msg, sizeof(msg), "PKCS12KDF accepted a missing salt (%s)",
             fam->digest);
    CHECK(!pkcs12_derive(fam->digest, pass, passlen, NULL, 0, 2048, 1,
                         dk, DK_CAP), msg);

    free_poison_allocations();
}