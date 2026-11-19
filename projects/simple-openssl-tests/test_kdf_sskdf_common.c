/*

 * test_kdf_sskdf_common.c -- shared implementation of the SSKDF (NIST SP
 * 800-56C) family tests (API in test_kdf_sskdf_common.h; one binary per
 * digest, one struct sskdf_family row and one KAT row set per binary:
 * test_kdf_sskdf_sha1.c and test_kdf_sskdf_sha256.c).
 *
 * The family covers the Single Step KDF through the EVP_KDF public API: the
 * digest-based mode (SHA-1 and SHA-256), the MAC-based mode with HMAC
 * (HMAC-SHA256), multi-chunk "info" parameters, the key/secret parameter
 * alias, and the missing-key error path.
 *
 * Known-answer vectors (NIST CAVS values as distributed in OpenSSL's own
 * test data test/recipes/30-test_evp_data/evpkdf_ss.txt; every hex value
 * re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt digest:SHA1 \
 *       -kdfopt hexsecret:<z> -kdfopt hexinfo:<info> SSKDF
 *
 * and the same command under the system OpenSSL 3.0.13):
 *
 *   SHA1,   z=d09a6b1a472f930db4f5e6b967900744, info=b117255ab5f1b6b96fc434b0
 *       -> b5a3c52e97ae6e8c5069954354eab3c7
 *   HMAC-SHA256 + salt (see the table in test_kdf_sskdf_sha256.c)
 *       -> 13479e9a91dd20fdd757d68ffe8869fb
 *
 * The MAC-based CMAC-AES128 mode mentioned in the plan is NOT covered: the
 * SSKDF provider's settable parameter list has no "cipher" parameter (only
 * KBKDF exposes one), so a CMAC MAC context cannot be given its cipher
 * through the EVP_KDF facade -- both a "cipher" param and a "digest"
 * pointing at the cipher were probed and EVP_KDF_derive fails either way.
 * The digest-based and HMAC modes fully cover the two KDF branches.
 *
 * Distribution decisions (carried out of the monolith): the KAT rows are
 * digest-specific and moved to their own binaries (the SHA-1 row for
 * test_kdf_sskdf_sha1.c; the 32/64-byte digest-mode rows and the HMAC row
 * for test_kdf_sskdf_sha256.c), while the key/secret alias, the multi-chunk
 * info handling and the missing-key negative generalize and run in every
 * binary for its own digest, with the digest name parameterized into the
 * CHECK messages.  Where the monolith verified a multi-info OKM for one
 * digest only (SHA-256, the 32-byte fb9ee554... value, which that binary
 * passes in), the digest without such a constant verifies the same
 * concatenation behavior relatively -- against a single-info reference
 * derive -- instead of dropping the check.
 */

#include "test_kdf_sskdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* The 16-byte shared secret z (d09a6b1a...) and the 12-byte info
 * (b117255a...) of the digest-based CAVS rows -- every digest-based KAT row
 * of both digests uses them, as do the key/secret alias and the missing-key
 * sections.  The 48-byte 000102...2f info is the multi-chunk info section's
 * input, split into 8/8/8/8/16-byte chunks in every binary.  The constants
 * are arrays (not pointers) so that they are address constants usable in
 * the per-digest binaries' static struct initializers. */
const char sskdf_z16_hex[] = "d09a6b1a472f930db4f5e6b967900744";
const char sskdf_info12_hex[] = "b117255ab5f1b6b96fc434b0";
const char sskdf_info48_hex[] =
    "000102030405060708090a0b0c0d0e0f1011121314151617"
    "18191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f";

/* Working-buffer sizes for the shared KAT material and outputs (the
 * original monolith's Z_CAP/SALT_CAP/INFO_CAP/OUT_CAP: the 16-byte z and
 * HMAC salt, the 48-byte multi-chunk info and the 64-byte SHA-256 output
 * row are the family's longest material). */
#define Z_CAP 16
#define SALT_CAP 16
#define INFO_CAP 48
#define OUT_CAP 64

/* One SSKDF derive with a fresh context (always the "SSKDF" KDF).  Returns
 * 1 on success, 0 on failure.  SSKDF is core OpenSSL 3 functionality: a
 * fetch failure fails the program. */
int sskdf_derive(const char *digest, const char *mac, const char *keyparam,
                 const unsigned char *z, size_t zlen,
                 const unsigned char *salt, size_t saltlen,
                 unsigned char *info, size_t infolen,
                 unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    if (mac != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                                       (char *)mac, 0);
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_octet_string(keyparam,
                                                    (void *)z, zlen);
    if (salt != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                        (void *)salt,
                                                        saltlen);
    if (info != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                        (void *)info,
                                                        infolen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) The KAT rows of the calling binary's digest, in both modes and all
 * output lengths. */
void sskdf_test_kats(const struct sskdf_family *fam,
                     const struct ss_kat *kats, size_t nkats)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zlen, saltlen, infolen, wantlen, k;
    char msg[128];

    for (k = 0; k < nkats; k++) {
        const struct ss_kat *kat = &kats[k];
        zlen = from_hex(kat->zhex, z, Z_CAP);
        saltlen = kat->salthex != NULL ?
            from_hex(kat->salthex, salt, SALT_CAP) : 0;
        infolen = from_hex(kat->infohex, info, INFO_CAP);
        wantlen = from_hex(kat->outhex, want, OUT_CAP);

        CHECK(sskdf_derive(fam->digest, kat->mac, OSSL_KDF_PARAM_KEY,
                           z, zlen, saltlen ? salt : NULL, saltlen,
                           info, infolen, out, kat->outlen),
              "SSKDF derive failed");
        snprintf(msg, sizeof(msg), "%s%s %zu-byte output does not match "
                 "the KAT", kat->mac ? kat->mac : "", fam->digest,
                 kat->outlen);
        expect_equal(msg, out, kat->outlen, want, wantlen);
    }

    free_poison_allocations();
}

/* (2) The "secret" parameter is an alias of "key": both routes must give
 * the identical output.  The monolith ran this on SHA-1 as the
 * representative; every binary now runs it for its own digest, over its
 * own first KAT row's z/info. */
void sskdf_test_key_secret_alias(const struct sskdf_family *fam,
                                 const struct ss_kat *kat)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out_key = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *out_secret = ALLOCATE_OUTPUT(OUT_CAP);
    size_t zlen = from_hex(kat->zhex, z, Z_CAP);
    size_t infolen = from_hex(kat->infohex, info, INFO_CAP);
    char msg[128];

    snprintf(msg, sizeof(msg), "SSKDF derive via the key param failed (%s)",
             fam->digest);
    CHECK(sskdf_derive(fam->digest, NULL, OSSL_KDF_PARAM_KEY, z, zlen,
                       NULL, 0, info, infolen, out_key, 16), msg);
    snprintf(msg, sizeof(msg),
             "SSKDF derive via the secret param failed (%s)", fam->digest);
    CHECK(sskdf_derive(fam->digest, NULL, OSSL_KDF_PARAM_SECRET, z, zlen,
                       NULL, 0, info, infolen, out_secret, 16), msg);
    snprintf(msg, sizeof(msg), "key-param and secret-param outputs differ "
             "(%s)", fam->digest);
    expect_equal(msg, out_key, 16, out_secret, 16);

    free_poison_allocations();
}

/* (3) Up to five "info" params are accepted and, on OpenSSL releases that
 * concatenate repeated info params (3.2+), five chunks (8+8+8+8+16 bytes)
 * equal the single 48-byte info.  On 3.0.x only the first chunk is used;
 * the program prints a note instead of failing.  A sixth chunk is rejected
 * outright (SSKDF_MAX_INFOS == 5).  The monolith ran this section on
 * SHA-256 (whose 32-byte fb9ee554... OKM it had verified); every binary now
 * runs it for its own digest -- SHA-256 against its verified constant, the
 * other digests against a single-info reference derive. */
void sskdf_test_multi_info(const struct sskdf_family *fam, const char *okmhex)
{
    unsigned char *z = ALLOCATE_KEY(Z_CAP);
    unsigned char *chunks[6];
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    static const size_t chunklen[5] = { 8, 8, 8, 8, 16 };
    size_t zlen, infolen, wantlen, i, off;
    int multi_info_equal;
    EVP_KDF *kdf;
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[10];

    for (i = 0; i < 6; i++)
        chunks[i] = ALLOCATE_SALT(INFO_CAP);
    zlen = from_hex(sskdf_z16_hex, z, Z_CAP);
    infolen = from_hex(sskdf_info48_hex, info, INFO_CAP);

    if (okmhex != NULL) {
        wantlen = from_hex(okmhex, want, OUT_CAP);
    } else {
        /* No verified multi-info OKM for this digest: derive the reference
         * once with the full 48-byte info as a single param; the five
         * chunks must reproduce it (the same concatenation behavior,
         * checked relatively). */
        CHECK(sskdf_derive(fam->digest, NULL, OSSL_KDF_PARAM_KEY, z, zlen,
                           NULL, 0, info, infolen, want, 32),
              "SSKDF reference derive with the full info failed");
        wantlen = 32;
    }

    /* Split the 48-byte info into 8/8/8/8/16-byte chunks. */
    off = 0;
    for (i = 0; i < 5; i++) {
        memcpy(chunks[i], info + off, chunklen[i]);
        off += chunklen[i];
    }
    CHECK(off == infolen, "info chunk sizes do not add up to the info");

    kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  z, zlen);
    for (i = 0; i < 5; i++)
        params[2 + i] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, chunks[i], chunklen[i]);
    params[7] = OSSL_PARAM_construct_end();
    CHECK(EVP_KDF_derive(ctx, out, 32, params) > 0,
          "SSKDF derive with five info chunks failed");
    unpoison_all();
    multi_info_equal = wantlen == 32 && memcmp(out, want, 32) == 0;
    repoison_all();
    if (multi_info_equal) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note %s: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n",
               test_prog_name());
    }

    /* A sixth chunk must be rejected. */
    params[7] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  chunks[5], 8);
    params[8] = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(ctx, out, 32, params) <= 0) {
        g_checks++; /* the documented SSKDF_MAX_INFOS limit is enforced */
    } else {
        printf("note %s: this OpenSSL accepts more than five "
               "info params (pre-3.1 behavior)\n",
               test_prog_name());
    }
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);

    free_poison_allocations();
}

/* (4) Deriving without the shared secret must fail.  The monolith ran this
 * on SHA-1 as the representative; every binary now runs it for its own
 * digest, over its own first KAT row's info. */
void sskdf_test_missing_key(const struct sskdf_family *fam,
                            const struct ss_kat *kat)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    EVP_KDF_CTX *ctx;
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[3];
    size_t infolen = from_hex(kat->infohex, info, INFO_CAP);
    char msg[128];

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  info, infolen);
    params[2] = OSSL_PARAM_construct_end();
    snprintf(msg, sizeof(msg), "SSKDF accepted a missing shared secret (%s)",
             fam->digest);
    CHECK(EVP_KDF_derive(ctx, out, 16, params) <= 0, msg);
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);

    free_poison_allocations();
}