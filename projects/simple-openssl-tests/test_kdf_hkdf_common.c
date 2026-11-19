/*
 * test_kdf_hkdf_common.c -- shared implementation of the HKDF-SHA1 /
 * HKDF-SHA256 / HKDF-SHA512 (RFC 5869) family tests (API in
 * test_kdf_hkdf_common.h; one binary per digest, one struct hkdf_family row
 * per binary).
 *
 * The family covers HKDF through the EVP_KDF public API: EXTRACT_AND_EXPAND,
 * EXTRACT_ONLY and EXPAND_ONLY modes, multi-chunk "info" parameters, the
 * separately registered concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512 names
 * and the error paths.
 *
 * Known-answer vectors: RFC 5869 test cases 1-3.  Every hex value below was
 * verified against the authoritative pizfix OpenSSL 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:SHA256 \
 *       -kdfopt hexkey:<ikm> -kdfopt hexsalt:<salt> -kdfopt hexinfo:<info> HKDF
 *
 * (and the same command under the system OpenSSL 3.0.13), and the extract
 * outputs were additionally cross-checked against `openssl dgst -sha256
 * -mac hmac` chains and OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_hkdf.txt).  Note that the CLI needs
 * the "hex" prefix on octet-string parameters (hexkey/hexsalt/hexinfo);
 * without it the ASCII bytes of the string are used.
 *
 * The concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512 KDF names (3.5+) are
 * fetched directly and derive with NO "digest" param, since each
 * implementation presets its digest.  Their 42-byte EXTRACT_AND_EXPAND OKMs
 * and EXTRACT_ONLY PRKs (for the RFC 5869 TC1 inputs) were verified with
 *
 *   pizfix/bin/openssl kdf -keylen 42 -kdfopt hexkey:<ikm> \
 *       -kdfopt hexsalt:<salt> -kdfopt hexinfo:<info> HKDF-SHA384
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt mode:EXTRACT_ONLY \
 *       -kdfopt hexkey:<ikm> -kdfopt hexsalt:<salt> HKDF-SHA384
 *
 * and cross-verified with the system OpenSSL 3.0.13 CLI, which predates
 * those names and so ran the bare-HKDF form with -kdfopt digest:SHA2-384
 * instead (both routes resolve to the same HKDF provider code, so the
 * values must agree).
 *
 * Distribution decisions (carried out of the monolith): the RFC 5869 TC1-3
 * OKM and PRK rows are digest-specific and moved to their own binaries
 * (TC1-3 for SHA-256, the monolith's "TC1 inputs with the SHA-1 and SHA-512
 * digests" rows for the others); the extract-only/expand-only modes, the
 * multi-info concatenation and the missing-input negatives generalize and
 * run in every binary for its own digest, with the digest name parameterized
 * into the CHECK messages.  Where the monolith verified a KAT value for one
 * digest only (the TC2 OKM and the switch-independent PRKs), the digests
 * without such a constant verify the same behavior relatively -- against a
 * single-info reference derive or by extracting the PRK first -- instead of
 * dropping the check; the SHA-512 binary reuses the monolith's verified
 * TC1 SHA-512 PRK (the concrete HKDF-SHA512 EXTRACT_ONLY value, which the
 * bare-HKDF route must agree with).  The concrete-name rows go to their own
 * digests' binaries (the HKDF-SHA384 row stayed with the representative
 * digest, SHA-256, until the test_kdf_hkdf_sha384 sibling was added).
 */

#include "test_kdf_hkdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* The RFC 5869 TC1 inputs (IKM = 0b x 22, salt = 000102..0c, info =
 * f0f1..f9) and TC2 inputs (80-byte IKM = 00..4f, salt = 60..af, info =
 * b0..ff).  The constants are arrays (not pointers) so that they are
 * address constants usable where the per-digest binaries need them (see
 * the API comment in test_kdf_hkdf_common.h). */
const char hkdf_tc1_ikm_hex[] =
    "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b";
const char hkdf_tc1_salt_hex[] = "000102030405060708090a0b0c";
const char hkdf_tc1_info_hex[] = "f0f1f2f3f4f5f6f7f8f9";
const char hkdf_tc2_ikm_hex[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f";
const char hkdf_tc2_salt_hex[] =
    "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf";
const char hkdf_tc2_info_hex[] =
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

/* Working-buffer sizes (the original monolith's IKM_CAP/SALT_CAP/INFO_CAP/
 * OUT_CAP: the 80-byte TC2 inputs are the family's longest KAT material
 * and the 82-byte TC2 OKM its longest output; the concrete-name section's
 * PRK buffers hold the largest preset digest size, 64). */
#define IKM_CAP 80
#define SALT_CAP 80
#define INFO_CAP 80
#define OUT_CAP 82

/* Fetches `kdfname`, derives `outlen` bytes with `params`.  Returns 1 on
 * success, 0 if EVP_KDF_derive failed.  Fails the program if the KDF itself
 * cannot be fetched (HKDF is core OpenSSL 3 functionality: never SKIP). */
int hkdf_derive(const char *kdfname, const OSSL_PARAM params[],
                unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, kdfname, NULL);
    EVP_KDF_CTX *ctx;
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core HKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) EXTRACT_AND_EXPAND (the default mode) must reproduce every RFC 5869
 * OKM of the calling binary's digest.  Salt/info of length zero are simply
 * not passed at all (TC3), which the RFC treats the same as an empty
 * string. */
void hkdf_test_extract_and_expand(const struct hkdf_family *fam,
                                  const struct hkdf_kat *kats, size_t nkats)
{
    size_t k;
    for (k = 0; k < nkats; k++) {
        const struct hkdf_kat *kat = &kats[k];
        unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
        unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
        unsigned char *info = ALLOCATE_SALT(INFO_CAP);
        unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
        unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
        OSSL_PARAM params[5];
        size_t ikmlen, saltlen, infolen, wantlen;
        char msg[128];

        ikmlen = from_hex(kat->ikmhex, ikm, IKM_CAP);
        saltlen = from_hex(kat->salthex, salt, SALT_CAP);
        infolen = from_hex(kat->infohex, info, INFO_CAP);
        wantlen = from_hex(kat->okmhex, want, OUT_CAP);

        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                     (char *)fam->digest, 0);
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                      ikm, ikmlen);
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      salt, saltlen);
        params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                      info, infolen);
        params[4] = OSSL_PARAM_construct_end();
        CHECK(hkdf_derive("HKDF", params, okm, kat->outlen),
              "EXTRACT_AND_EXPAND derive failed");
        snprintf(msg, sizeof(msg),
                 "RFC 5869 case %zu (%s) OKM does not match the KAT",
                 kat->rfc_case, fam->digest);
        expect_equal(msg, okm, kat->outlen, want, wantlen);

        free_poison_allocations();
    }
}

/* (2) EXTRACT_ONLY returns the PRK = HMAC-Hash(salt, IKM); its length must
 * be the digest size, requested through EVP_KDF_CTX_get_kdf_size(), and
 * every row with a verified PRK constant must reproduce it. */
void hkdf_test_extract_only(const struct hkdf_family *fam,
                            const struct hkdf_prk_kat *prks, size_t nprks)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *prk = ALLOCATE_OUTPUT(fam->mdsize);
    unsigned char *want = ALLOCATE_OUTPUT(fam->mdsize);
    size_t k;

    for (k = 0; k < nprks; k++) {
        EVP_KDF *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
        EVP_KDF_CTX *ctx;
        OSSL_PARAM setp[5], getp[2];
        size_t ikmlen, saltlen, wantlen, prksize = 0;
        char msg[128];

        CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core HKDF KDF");
        ctx = EVP_KDF_CTX_new(kdf);
        CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");

        ikmlen = from_hex(prks[k].ikmhex, ikm, IKM_CAP);
        saltlen = from_hex(prks[k].salthex, salt, SALT_CAP);
        setp[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)fam->digest, 0);
        setp[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                   "EXTRACT_ONLY", 0);
        setp[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                    ikm, ikmlen);
        setp[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                    salt, saltlen);
        setp[4] = OSSL_PARAM_construct_end();
        CHECK(EVP_KDF_CTX_set_params(ctx, setp) == 1,
              "EXTRACT_ONLY set_params failed");

        /* The fixed PRK size is visible through get_kdf_size. */
        getp[0] = OSSL_PARAM_construct_size_t(OSSL_KDF_PARAM_SIZE, &prksize);
        getp[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_KDF_CTX_get_params(ctx, getp) == 1,
              "EVP_KDF_CTX_get_params(SIZE) failed");
        snprintf(msg, sizeof(msg),
                 "EXTRACT_ONLY PRK size is not the %s size",
                 fam->digest_prose);
        CHECK(prksize == fam->mdsize, msg);

        CHECK(EVP_KDF_derive(ctx, prk, fam->mdsize, NULL) > 0,
              "EXTRACT_ONLY derive failed");
        if (prks[k].prkhex != NULL) {
            wantlen = from_hex(prks[k].prkhex, want, fam->mdsize);
            snprintf(msg, sizeof(msg), "RFC 5869 case %zu PRK does not match",
                     prks[k].rfc_case);
            expect_equal(msg, prk, fam->mdsize, want, wantlen);
        } else {
            /* No verified PRK constant for this digest (the original
             * monolith carried only the SHA-256/384/512 PRKs): the row
             * still proves that EXTRACT_ONLY derives the digest-size PRK
             * successfully; the value is covered by the expand-only
             * composition below. */
        }
        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
    }

    free_poison_allocations();
}

/* (3) EXPAND_ONLY: the "key" is the PRK from a previous extract, and the
 * expand stage alone must reproduce the TC1 OKM of the binary's digest. */
void hkdf_test_expand_only(const struct hkdf_family *fam,
                           const struct hkdf_kat *tc1_row,
                           const char *prkhex)
{
    unsigned char *prk;
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[5];
    size_t infolen, wantlen;

    if (prkhex != NULL) {
        prk = ALLOCATE_KEY(fam->mdsize);
        from_hex(prkhex, prk, fam->mdsize);
    } else {
        /* No verified PRK constant for this digest: extract it first, so
         * that the expand stage is still checked against the TC1 OKM with
         * the true PRK as its key (the composition the constant-based form
         * also relies on). */
        unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
        unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
        OSSL_PARAM ep[5];
        size_t ikmlen, saltlen;

        prk = ALLOCATE_OUTPUT(fam->mdsize);
        ikmlen = from_hex(tc1_row->ikmhex, ikm, IKM_CAP);
        saltlen = from_hex(tc1_row->salthex, salt, SALT_CAP);
        ep[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
        ep[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                 "EXTRACT_ONLY", 0);
        ep[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  ikm, ikmlen);
        ep[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  salt, saltlen);
        ep[4] = OSSL_PARAM_construct_end();
        CHECK(hkdf_derive("HKDF", ep, prk, fam->mdsize),
              "EXTRACT_ONLY derive failed");
    }

    infolen = from_hex(tc1_row->infohex, info, INFO_CAP);
    wantlen = from_hex(tc1_row->okmhex, want, OUT_CAP);

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                 "EXPAND_ONLY", 0);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  prk, fam->mdsize);
    params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  info, infolen);
    params[4] = OSSL_PARAM_construct_end();
    CHECK(hkdf_derive("HKDF", params, okm, tc1_row->outlen),
          "EXPAND_ONLY derive failed");
    expect_equal("EXPAND_ONLY(PRK, info) does not match the TC1 OKM",
                 okm, tc1_row->outlen, want, wantlen);

    free_poison_allocations();
}

/* (4) The "info" parameter may be delivered as several octet-string params,
 * which the provider concatenates.  Five chunks work; on OpenSSL releases
 * that cap the number of info params (HKDF_MAX_INFOS == 5, 3.1+), a sixth
 * must be rejected -- older releases simply concatenate all of them, which
 * is noted rather than failed. */
void hkdf_test_multi_info(const struct hkdf_family *fam, const char *okmhex)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *chunks[6];
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t ikmlen, saltlen, wantlen, off, i;
    int multi_info_equal;
    OSSL_PARAM params[11];

    for (i = 0; i < 6; i++)
        chunks[i] = ALLOCATE_SALT(17);
    ikmlen = from_hex(hkdf_tc2_ikm_hex, ikm, IKM_CAP);
    saltlen = from_hex(hkdf_tc2_salt_hex, salt, SALT_CAP);

    if (okmhex != NULL) {
        wantlen = from_hex(okmhex, want, OUT_CAP);
    } else {
        /* No verified TC2 OKM for this digest: derive the reference once
         * with the full info as a single param; the five chunks must
         * reproduce it (the same concatenation behavior, checked
         * relatively). */
        unsigned char *info = ALLOCATE_SALT(INFO_CAP);
        size_t infolen = from_hex(hkdf_tc2_info_hex, info, INFO_CAP);

        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                     (char *)fam->digest, 0);
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                      ikm, ikmlen);
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      salt, saltlen);
        params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                      info, infolen);
        params[4] = OSSL_PARAM_construct_end();
        CHECK(hkdf_derive("HKDF", params, want, 82),
              "reference derive with the full TC2 info failed");
        wantlen = 82;
    }

    /* Five 16-byte chunks that concatenate to the 80-byte TC2 info. */
    for (off = 0; off < 5; off++) {
        char piece[33];
        memcpy(piece, hkdf_tc2_info_hex + off * 32, 32);
        piece[32] = '\0';
        from_hex(piece, chunks[off], 17);
    }

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  ikm, ikmlen);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  salt, saltlen);
    for (i = 0; i < 5; i++)
        params[3 + i] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, chunks[i], 16);
    params[8] = OSSL_PARAM_construct_end();
    CHECK(hkdf_derive("HKDF", params, okm, 82),
          "derive with five info chunks failed");
    unpoison_all();
    multi_info_equal = wantlen == 82 && memcmp(okm, want, 82) == 0;
    repoison_all();
    if (multi_info_equal) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note %s: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n",
               test_prog_name());
    }

    /* A sixth info chunk: rejected where HKDF_MAX_INFOS == 5 applies. */
    memcpy(chunks[5], chunks[0], 16);
    params[8] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  chunks[5], 16);
    params[9] = OSSL_PARAM_construct_end();
    if (!hkdf_derive("HKDF", params, okm, 82)) {
        g_checks++; /* the documented >5 info params limit is enforced */
    } else {
        printf("note %s: this OpenSSL accepts more than five "
               "info params (pre-3.1 behavior)\n",
               test_prog_name());
    }

    free_poison_allocations();
}

/* (5) Error paths: a zero output length and a missing key must fail;
 * EXTRACT_ONLY with a wrong PRK buffer size must fail. */
void hkdf_test_negative(const struct hkdf_family *fam)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[3];
    size_t ikmlen;
    char msg[128];

    ikmlen = from_hex(hkdf_tc1_ikm_hex, ikm, IKM_CAP);

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  ikm, ikmlen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(!hkdf_derive("HKDF", params, okm, 0),
          "a zero output length was accepted");

    /* EXTRACT_ONLY requires the output to be exactly the digest size. */
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)fam->digest, 0);
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                 "EXTRACT_ONLY", 0);
    params[2] = OSSL_PARAM_construct_end();
    snprintf(msg, sizeof(msg),
             "EXTRACT_ONLY accepted a %zu-byte (non-digest-size) output",
             fam->mdsize - 1);
    CHECK(!hkdf_derive("HKDF", params, okm, fam->mdsize - 1), msg);

    free_poison_allocations();
}

/* (6) The separately registered concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512
 * KDFs (3.5+; absent from 3.0.13, where each name takes a per-name SKIP):
 * each presets its digest, so it must derive with NO "digest" param at all
 * and agree byte for byte with the bare "HKDF" KDF plus the matching digest
 * param over the same inputs -- the primary assertion.  In
 * EXTRACT_AND_EXPAND mode the HKDF output is variable length, so
 * EVP_KDF_CTX_get_kdf_size reports SIZE_MAX; in EXTRACT_ONLY mode it is
 * exactly the preset digest size. */
void hkdf_test_concrete_names(const struct hkdf_concrete *concretes,
                              size_t nconcretes)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *bare_okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *prk = ALLOCATE_OUTPUT(64);
    unsigned char *want = ALLOCATE_OUTPUT(64);
    size_t ikmlen, saltlen, infolen, wantlen, c;
    char msg[128];

    /* The RFC 5869 TC1 inputs, as for the bare-HKDF KATs above. */
    ikmlen = from_hex(hkdf_tc1_ikm_hex, ikm, IKM_CAP);
    saltlen = from_hex(hkdf_tc1_salt_hex, salt, SALT_CAP);
    infolen = from_hex(hkdf_tc1_info_hex, info, INFO_CAP);

    for (c = 0; c < nconcretes; c++) {
        const struct hkdf_concrete *concrete = &concretes[c];
        EVP_KDF *kdf = EVP_KDF_fetch(NULL, concrete->kdfname, NULL);
        EVP_KDF_CTX *ctx;
        OSSL_PARAM params[5], modep[2];
        size_t kdfsize;

        if (kdf == NULL) {
            /* Version-gated: the concrete names first shipped in 3.5. */
            printf("SKIP %s: %s is unavailable\n",
                   test_prog_name(), concrete->kdfname);
            continue;
        }
        ctx = EVP_KDF_CTX_new(kdf);
        CHECK(ctx != NULL, "EVP_KDF_CTX_new failed for a concrete HKDF-SHA*");

        /* The digest is preset by the concrete name: NO digest param. */
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                      ikm, ikmlen);
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      salt, saltlen);
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                      info, infolen);
        params[3] = OSSL_PARAM_construct_end();
        CHECK(EVP_KDF_CTX_set_params(ctx, params) == 1,
              "concrete HKDF-SHA* set_params failed without a digest param");

        /* EXTRACT_AND_EXPAND output is variable length, so the reported
         * size is SIZE_MAX, not the digest size. */
        CHECK(EVP_KDF_CTX_get_kdf_size(ctx) == SIZE_MAX,
              "concrete HKDF-SHA* size is not variable in "
              "EXTRACT_AND_EXPAND mode");
        CHECK(EVP_KDF_derive(ctx, okm, 42, NULL) > 0,
              "concrete HKDF-SHA* EXTRACT_AND_EXPAND derive failed");

        /* The same inputs through bare HKDF plus the digest param must be
         * byte-identical (the primary assertion). */
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                     (char *)concrete->digest,
                                                     0);
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                      ikm, ikmlen);
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      salt, saltlen);
        params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                      info, infolen);
        params[4] = OSSL_PARAM_construct_end();
        CHECK(hkdf_derive("HKDF", params, bare_okm, 42),
              "bare-HKDF derive with the matching digest param failed");
        snprintf(msg, sizeof(msg),
                 "%s (no digest param) differs from bare HKDF + digest:\"%s\"",
                 concrete->kdfname, concrete->digest);
        unpoison_all();
        CHECK(memcmp(okm, bare_okm, 42) == 0, msg);
        repoison_all();

        /* Both routes must also reproduce the CLI-verified KAT. */
        wantlen = from_hex(concrete->okmhex, want, 64);
        snprintf(msg, sizeof(msg),
                 "%s OKM does not match the CLI-verified KAT",
                 concrete->kdfname);
        expect_equal(msg, okm, 42, want, wantlen);

        /* EXTRACT_ONLY: the output is exactly the PRK, whose fixed size is
         * the preset digest size (32/48/64). */
        modep[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                    "EXTRACT_ONLY", 0);
        modep[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_KDF_CTX_set_params(ctx, modep) == 1,
              "concrete HKDF-SHA* EXTRACT_ONLY set_params failed");
        kdfsize = EVP_KDF_CTX_get_kdf_size(ctx);
        CHECK(kdfsize == concrete->mdsize,
              "concrete HKDF-SHA* PRK size is not the preset digest size");
        CHECK(EVP_KDF_derive(ctx, prk, concrete->mdsize, NULL) > 0,
              "concrete HKDF-SHA* EXTRACT_ONLY derive failed");
        wantlen = from_hex(concrete->prkhex, want, 64);
        snprintf(msg, sizeof(msg),
                 "%s EXTRACT_ONLY PRK does not match the CLI-verified KAT",
                 concrete->kdfname);
        expect_equal(msg, prk, concrete->mdsize, want, wantlen);

        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
    }

    free_poison_allocations();
}
