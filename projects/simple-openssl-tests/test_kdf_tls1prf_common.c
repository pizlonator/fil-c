/*

 * test_kdf_tls1prf_common.c -- shared implementation of the TLS1-PRF
 * MD5-SHA1 / SHA256 family tests (API in test_kdf_tls1prf_common.h; one
 * binary per digest, one struct tls1prf_family row and one struct
 * tls1prf_kat row per binary).
 *
 * The family covers the TLS PRFs through the EVP_KDF public API: the TLS
 * 1.0/1.1 PRF (RFC 2246/4346), which splits the secret into two halves and
 * XORs P_MD5(S1) against P_SHA1(S2) -- the "MD5-SHA1" combo digest, whose
 * halves share the middle byte of an odd-length secret -- and the TLS 1.2
 * PRF (RFC 5246, section 5), which runs P_<hash> with the single configured
 * digest (SHA-256 here).  TLS 1.3 replaced this PRF with the HKDF-based key
 * schedule (RFC 8446), covered separately by test_kdf_tls13kdf.  The
 * sections: the NIST-style "master secret" KATs with the seed delivered as
 * three chunks, the same KATs with a pre-concatenated seed, an odd-length
 * secret, an explicitly empty secret and the error paths.
 *
 * Known-answer vectors (verified against the authoritative pizfix OpenSSL
 * 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt digest:MD5-SHA1 \
 *       -kdfopt hexsecret:<secret> -kdfopt 'seed:master secret' \
 *       -kdfopt hexseed:<client_random> -kdfopt hexseed:<server_random> \
 *       TLS1-PRF
 *
 * and the same command under the system OpenSSL 3.0.13; the values are the
 * NIST vectors distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_tls11_prf.txt and
 * evpkdf_tls12_prf.txt)):
 *
 *   TLS 1.1, MD5-SHA1, 48-byte secret, seed = "master secret" ||
 *   client_random || server_random, 48 bytes out
 *     -> 2f6962dfbc744c4b2138bb6b3d33054c...e998f62
 *   TLS 1.2, SHA-256, same shape
 *     -> 202c88c00f84a17a2002707960478746...51288bf
 *
 */

#include "test_kdf_tls1prf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* The ASCII "master secret" label that seeds every master-secret KAT and
 * the concat-seed/odd-secret sections (copied with tls1prf_from_str, not
 * from_hex), and the 7-byte synthetic seed (01020304050607) of the
 * empty-secret and missing-input sections.  The constants are arrays (not
 * pointers) so that they are address constants usable where the per-digest
 * binaries need them (see the API comment in test_kdf_tls1prf_common.h). */
const char tls1prf_label[] = "master secret";
const char tls1prf_test_seed_hex[] = "01020304050607";

/* Working-buffer sizes (the original monolith's SECRET_CAP/SEED_CAP/
 * LABEL_CAP/OUT_CAP: the 48-byte KAT secrets and outputs are the family's
 * longest material, the 32-byte client/server randoms its longest seeds,
 * and "master secret" (13 bytes) its label). */
#define SECRET_CAP 48
#define SEED_CAP 32
#define LABEL_CAP 16
#define OUT_CAP 48

/* Copies an ASCII string into a labeled buffer; returns its length. */
size_t tls1prf_from_str(const char *s, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(s);
    if (n >= dstcap)
        test_failed("string constant larger than its destination buffer");
    memcpy(dst, s, n);
    return n;
}

/* One TLS1-PRF derive with a fresh context; up to three seed chunks.  A
 * NULL chunk pointer terminates the list.  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  TLS1-PRF is core OpenSSL 3 functionality: a
 * fetch failure fails the program (never SKIP). */
int tls1prf_derive(const char *digest, const unsigned char *secret,
                          size_t secretlen, const unsigned char *seed1,
                          size_t seed1len, const unsigned char *seed2,
                          size_t seed2len, const unsigned char *seed3,
                          size_t seed3len, unsigned char *out,
                          size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "TLS1-PRF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[7];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core TLS1-PRF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    if (secret != NULL) {
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET,
                                                      (void *)secret,
                                                      secretlen);
        params[2] = OSSL_PARAM_construct_end();
    } else {
        params[1] = OSSL_PARAM_construct_end();
    }
    ok = EVP_KDF_CTX_set_params(ctx, params) == 1;
    if (ok) {
        OSSL_PARAM seedparams[5];
        seedparams[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed1,
                                                          seed1len);
        seedparams[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed2,
                                                          seed2len);
        seedparams[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                          (void *)seed3,
                                                          seed3len);
        seedparams[3] = OSSL_PARAM_construct_end();
        ok = EVP_KDF_derive(ctx, out, outlen, seedparams) > 0;
    }
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) The NIST master-secret KAT of the calling binary's digest, with the
 * seed delivered as three chunks (label, client_random, server_random) --
 * TLS1-PRF accepts up to six seed chunks (TLSPRF_MAX_SEEDS) -- plus the
 * same KAT truncated to 16 bytes (prefix property). */
void tls1prf_test_kat_multi_seed(const struct tls1prf_family *fam,
                                 const struct tls1prf_kat *kat)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *cr = ALLOCATE_SALT(SEED_CAP);
    unsigned char *sr = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, labellen, crlen, srlen, wantlen;
    char msg[128];

    secretlen = from_hex(kat->secrethex, secret, SECRET_CAP);
    labellen = tls1prf_from_str(kat->label, label, LABEL_CAP);
    crlen = from_hex(kat->crhex, cr, SEED_CAP);
    srlen = from_hex(kat->srhex, sr, SEED_CAP);

    snprintf(msg, sizeof(msg), "%s (%s) master-secret derive failed",
             fam->tls_version, fam->digest_prose);
    CHECK(tls1prf_derive(fam->digest, secret, secretlen, label, labellen,
                         cr, crlen, sr, srlen, out, OUT_CAP), msg);
    wantlen = from_hex(kat->outhex, want, OUT_CAP);
    snprintf(msg, sizeof(msg), "%s %s output does not match the KAT",
             fam->tls_version, fam->digest_prose);
    expect_equal(msg, out, OUT_CAP, want, wantlen);

    /* The same KAT truncated to 16 bytes (prefix property). */
    snprintf(msg, sizeof(msg), "%s 16-byte derive failed", fam->tls_version);
    CHECK(tls1prf_derive(fam->digest, secret, secretlen, label, labellen,
                         cr, crlen, sr, srlen, out, 16), msg);
    snprintf(msg, sizeof(msg), "%s 16-byte output is not the KAT prefix",
             fam->tls_version);
    expect_equal(msg, out, 16, want, 16);

    free_poison_allocations();
}

/* (2) Seed chunks may also be pre-concatenated: one seed with the label and
 * both randoms glued together must give the identical output. */
void tls1prf_test_concat_seed(const struct tls1prf_family *fam,
                              const struct tls1prf_kat *kat)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *concat = ALLOCATE_SALT(LABEL_CAP + 2 * SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, concatlen, wantlen;
    char msg[128];

    secretlen = from_hex(kat->secrethex, secret, SECRET_CAP);
    concatlen = tls1prf_from_str(kat->label, concat, LABEL_CAP + 2 * SEED_CAP);
    concatlen += from_hex(kat->crhex, concat + concatlen, 2 * SEED_CAP);
    concatlen += from_hex(kat->srhex, concat + concatlen, 2 * SEED_CAP);
    wantlen = from_hex(kat->outhex, want, OUT_CAP);

    snprintf(msg, sizeof(msg),
             "TLS1-PRF derive with a single concatenated seed failed (%s)",
             fam->digest);
    CHECK(tls1prf_derive(fam->digest, secret, secretlen, concat, concatlen,
                         NULL, 0, NULL, 0, out, OUT_CAP), msg);
    snprintf(msg, sizeof(msg),
             "three-seed and concatenated-seed outputs differ (%s)",
             fam->digest);
    expect_equal(msg, out, OUT_CAP, want, wantlen);

    free_poison_allocations();
}

/* (3) An odd-length secret (TLS <= 1.1 splits it into two halves sharing
 * the middle byte) still derives deterministically.  MD5-SHA1-only: the
 * verified odd-secret output constant is a TLS 1.1 value. */
void tls1prf_test_odd_secret(const struct tls1prf_family *fam,
                             const struct tls1prf_kat *kat,
                             const char *outhex)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *label = ALLOCATE_SALT(LABEL_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen, labellen, wantlen;

    /* Drop the last byte of the 48-byte TLS 1.1 secret -> 47 bytes. */
    secretlen = from_hex(kat->secrethex, secret, SECRET_CAP) - 1;
    labellen = tls1prf_from_str(kat->label, label, LABEL_CAP);
    wantlen = from_hex(outhex, want, OUT_CAP);
    CHECK(tls1prf_derive(fam->digest, secret, secretlen, label, labellen,
                         NULL, 0, NULL, 0, out, 32),
          "derive with a 47-byte (odd) secret failed");
    expect_equal("odd-length secret output does not match the KAT",
                 out, 32, want, wantlen);

    free_poison_allocations();
}

/* (4) An explicitly empty secret derives deterministically.  The
 * empty-secret value is an OpenSSL edge case: an explicitly set zero-length
 * secret is accepted (and both the 3.0.13 and 3.6.5 CLI agree on the
 * result), while omitting the secret entirely is rejected -- see (5)
 * tls1prf_test_missing_inputs().  SHA-256-only: the verified empty-secret
 * output constant is a SHA-256 value over the shared 7-byte synthetic
 * seed. */
void tls1prf_test_empty_secret(const struct tls1prf_family *fam,
                               const char *outhex)
{
    unsigned char *empty = ALLOCATE_KEY(1);
    unsigned char *seed = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t seedlen, wantlen;

    seedlen = from_hex(tls1prf_test_seed_hex, seed, SEED_CAP);
    wantlen = from_hex(outhex, want, OUT_CAP);
    CHECK(tls1prf_derive(fam->digest, empty, 0, seed, seedlen,
                         NULL, 0, NULL, 0, out, 16),
          "derive with an explicitly empty secret failed");
    expect_equal("empty-secret output does not match the KAT",
                 out, 16, want, wantlen);

    free_poison_allocations();
}

/* (5) Omitting the secret or the seed must fail.  Digest-independent
 * provider checks, so every binary runs this section for its own digest;
 * the stand-in secret is the binary's own KAT secret (its value is
 * irrelevant to the must-fail assertions). */
void tls1prf_test_missing_inputs(const struct tls1prf_family *fam,
                                 const char *secrethex)
{
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *seed = ALLOCATE_SALT(SEED_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t secretlen = from_hex(secrethex, secret, SECRET_CAP);
    size_t seedlen = from_hex(tls1prf_test_seed_hex, seed, SEED_CAP);
    char msg[128];

    snprintf(msg, sizeof(msg), "TLS1-PRF accepted a missing secret (%s)",
             fam->digest);
    CHECK(!tls1prf_derive(fam->digest, NULL, 0, seed, seedlen,
                          NULL, 0, NULL, 0, out, 16), msg);
    snprintf(msg, sizeof(msg), "TLS1-PRF accepted a missing seed (%s)",
             fam->digest);
    CHECK(!tls1prf_derive(fam->digest, secret, secretlen, NULL, 0,
                          NULL, 0, NULL, 0, out, 16), msg);

    free_poison_allocations();
}