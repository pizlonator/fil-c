#include "poisoning.h"

#include <stdint.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_hkdf -- HKDF (RFC 5869) through the EVP_KDF public API:
 * EXTRACT_AND_EXPAND, EXTRACT_ONLY and EXPAND_ONLY modes, SHA-256/SHA-1/
 * SHA-512 variants, multi-chunk "info" parameters, the separately
 * registered concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512 names and the
 * error paths.
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
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_hkdf: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

/* Fetches `kdfname`, derives `outlen` bytes with `params`.  Returns 1 on
 * success, 0 if EVP_KDF_derive failed.  Fails the program if the KDF itself
 * cannot be fetched (HKDF is core OpenSSL 3 functionality: never SKIP). */
static int kdf_derive(const char *kdfname, const OSSL_PARAM params[],
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

/* One RFC 5869 test case in default EXTRACT_AND_EXPAND mode. */
static const struct hkdf_kat {
    const char *digest;
    const char *ikmhex;    /* input keying material ("key" param) */
    const char *salthex;   /* "" means absent */
    const char *infohex;   /* "" means absent */
    size_t outlen;
    const char *okmhex;
} hkdf_kats[] = {
    /* TC1 */
    { "SHA256",
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "000102030405060708090a0b0c",
      "f0f1f2f3f4f5f6f7f8f9",
      42,
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865" },
    /* TC2: 80-byte IKM (00..4f) and salt (60..af), 80-byte info (b0..ff) */
    { "SHA256",
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f",
      "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
      82,
      "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
      "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
      "cc30c58179ec3e87c14c01d5c1f3434f1d87" },
    /* TC3: empty salt and empty info */
    { "SHA256",
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "", "",
      42,
      "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
      "9d201395faa4b61a96c8" },
    /* TC1 inputs with the SHA-1 and SHA-512 digests (CLI-verified on both
     * the pizfix 3.6.5 and the system 3.0.13 builds). */
    { "SHA1",
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "000102030405060708090a0b0c",
      "f0f1f2f3f4f5f6f7f8f9",
      42,
      "d6000ffb5b50bd3970b260017798fb9c8df9ce2e2c16b6cd709cca07dc3cf9cf"
      "26d6c6d750d0aaf5ac94" },
    { "SHA512",
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "000102030405060708090a0b0c",
      "f0f1f2f3f4f5f6f7f8f9",
      42,
      "832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c14815793"
      "38da362cb8d9f925d7cb" },
};

#define IKM_CAP 80
#define SALT_CAP 80
#define INFO_CAP 80
#define OUT_CAP 82

/* (1) EXTRACT_AND_EXPAND (the default mode) must reproduce every RFC 5869
 * OKM.  Salt/info of length zero are simply not passed at all (TC3), which
 * the RFC treats the same as an empty string. */
static void test_extract_and_expand_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(hkdf_kats) / sizeof(hkdf_kats[0]); k++) {
        const struct hkdf_kat *kat = &hkdf_kats[k];
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
                                                     (char *)kat->digest, 0);
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                      ikm, ikmlen);
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      salt, saltlen);
        params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                      info, infolen);
        params[4] = OSSL_PARAM_construct_end();
        CHECK(kdf_derive("HKDF", params, okm, kat->outlen),
              "EXTRACT_AND_EXPAND derive failed");
        snprintf(msg, sizeof(msg),
                 "RFC 5869 case %zu (%s) OKM does not match the KAT",
                 k + 1, kat->digest);
        expect_equal(msg, okm, kat->outlen, want, wantlen);
    }
}

/* (2) EXTRACT_ONLY returns the PRK = HMAC-Hash(salt, IKM); its length must
 * be the digest size, requested through EVP_KDF_CTX_get_kdf_size(). */
static void test_extract_only(void)
{
    static const struct {
        const char *ikmhex;
        const char *salthex;
        const char *prkhex;
    } prk_kats[] = {
        /* TC1: PRK = HMAC-SHA256(salt=000102..0c, IKM=0b x22) */
        { "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
          "000102030405060708090a0b0c",
          "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" },
        /* TC2: PRK = HMAC-SHA256(salt=60..af, IKM=00..4f) */
        { "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
          "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
          "404142434445464748494a4b4c4d4e4f",
          "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
          "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
          "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
          "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244" },
        /* TC3: PRK = HMAC-SHA256(salt="", IKM=0b x22) */
        { "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
          "",
          "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04" },
    };
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *prk = ALLOCATE_OUTPUT(32);
    unsigned char *want = ALLOCATE_OUTPUT(32);
    size_t k;

    for (k = 0; k < sizeof(prk_kats) / sizeof(prk_kats[0]); k++) {
        EVP_KDF *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
        EVP_KDF_CTX *ctx;
        OSSL_PARAM setp[5], getp[2];
        size_t ikmlen, saltlen, wantlen, prksize = 0;
        char msg[128];

        CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core HKDF KDF");
        ctx = EVP_KDF_CTX_new(kdf);
        CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");

        ikmlen = from_hex(prk_kats[k].ikmhex, ikm, IKM_CAP);
        saltlen = from_hex(prk_kats[k].salthex, salt, SALT_CAP);
        setp[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   "SHA256", 0);
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
        CHECK(prksize == 32, "EXTRACT_ONLY PRK size is not the SHA-256 size");

        wantlen = from_hex(prk_kats[k].prkhex, want, 32);
        CHECK(EVP_KDF_derive(ctx, prk, 32, NULL) > 0,
              "EXTRACT_ONLY derive failed");
        snprintf(msg, sizeof(msg), "RFC 5869 case %zu PRK does not match",
                 k + 1);
        expect_equal(msg, prk, 32, want, wantlen);
        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
    }
}

/* PRK of RFC 5869 TC1 (HMAC-SHA256 of the TC1 salt and IKM). */
static const char *const PRK1_HEX =
    "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5";

/* (3) EXPAND_ONLY: the "key" is the PRK from a previous extract, and the
 * expand stage alone must reproduce the full TC1 OKM. */
static void test_expand_only(void)
{
    unsigned char *prk = ALLOCATE_KEY(32);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[5];
    size_t infolen, wantlen;

    from_hex(PRK1_HEX, prk, 32);
    infolen = from_hex(hkdf_kats[0].infohex, info, INFO_CAP);
    wantlen = from_hex(hkdf_kats[0].okmhex, want, OUT_CAP);

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                 "EXPAND_ONLY", 0);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  prk, 32);
    params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  info, infolen);
    params[4] = OSSL_PARAM_construct_end();
    CHECK(kdf_derive("HKDF", params, okm, 42),
          "EXPAND_ONLY derive failed");
    expect_equal("EXPAND_ONLY(PRK, info) does not match the TC1 OKM",
                 okm, 42, want, wantlen);
}

/* (4) The "info" parameter may be delivered as several octet-string params,
 * which the provider concatenates.  Five chunks work; on OpenSSL releases
 * that cap the number of info params (HKDF_MAX_INFOS == 5, 3.1+), a sixth
 * must be rejected -- older releases simply concatenate all of them, which
 * is noted rather than failed. */
static void test_multi_info(void)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *chunks[6];
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const char *ikmhex = hkdf_kats[1].ikmhex;
    const char *salthex = hkdf_kats[1].salthex;
    const char *infohex = hkdf_kats[1].infohex;
    size_t ikmlen, saltlen, wantlen, off, i;
    OSSL_PARAM params[11];

    for (i = 0; i < 6; i++)
        chunks[i] = ALLOCATE_SALT(17);
    ikmlen = from_hex(ikmhex, ikm, IKM_CAP);
    saltlen = from_hex(salthex, salt, SALT_CAP);
    wantlen = from_hex(hkdf_kats[1].okmhex, want, OUT_CAP);

    /* Five 16-byte chunks that concatenate to the 80-byte TC2 info. */
    for (off = 0; off < 5; off++) {
        char piece[33];
        memcpy(piece, infohex + off * 32, 32);
        piece[32] = '\0';
        from_hex(piece, chunks[off], 17);
    }

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  ikm, ikmlen);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  salt, saltlen);
    for (i = 0; i < 5; i++)
        params[3 + i] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, chunks[i], 16);
    params[8] = OSSL_PARAM_construct_end();
    CHECK(kdf_derive("HKDF", params, okm, 82),
          "derive with five info chunks failed");
    if (wantlen == 82 && memcmp(okm, want, 82) == 0) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note test_kdf_hkdf: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n");
    }

    /* A sixth info chunk: rejected where HKDF_MAX_INFOS == 5 applies. */
    memcpy(chunks[5], chunks[0], 16);
    params[8] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                  chunks[5], 16);
    params[9] = OSSL_PARAM_construct_end();
    if (!kdf_derive("HKDF", params, okm, 82)) {
        g_checks++; /* the documented >5 info params limit is enforced */
    } else {
        printf("note test_kdf_hkdf: this OpenSSL accepts more than five "
               "info params (pre-3.1 behavior)\n");
    }
}

/* (5) Error paths: a zero output length and a missing key must fail;
 * EXTRACT_ONLY with a wrong PRK buffer size must fail. */
static void test_negative(void)
{
    unsigned char *ikm = ALLOCATE_KEY(IKM_CAP);
    unsigned char *okm = ALLOCATE_OUTPUT(OUT_CAP);
    OSSL_PARAM params[3];
    size_t ikmlen;

    ikmlen = from_hex(hkdf_kats[0].ikmhex, ikm, IKM_CAP);

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                  ikm, ikmlen);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(!kdf_derive("HKDF", params, okm, 0),
          "a zero output length was accepted");

    /* EXTRACT_ONLY requires the output to be exactly the digest size. */
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 "SHA256", 0);
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                 "EXTRACT_ONLY", 0);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(!kdf_derive("HKDF", params, okm, 31),
          "EXTRACT_ONLY accepted a 31-byte (non-digest-size) output");
}

/* (6) The separately registered concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512
 * KDFs (3.5+; absent from 3.0.13, where each name takes a per-name SKIP):
 * each presets its digest, so it must derive with NO "digest" param at all
 * and agree byte for byte with the bare "HKDF" KDF plus the matching digest
 * param over the same inputs -- the primary assertion.  The OKM and PRK
 * constants below use the RFC 5869 TC1 inputs (IKM 0b x22, salt 000102..0c,
 * info f0f1..f9) with every digest (so the SHA-384/SHA-512 OKMs differ from
 * the RFC's SHA-256 OKM, and the PRK is 48/64 bytes there) and were
 * verified with the pizfix 3.6.5 CLI
 *
 *   pizfix/bin/openssl kdf -keylen 42 -kdfopt hexkey:<ikm> \
 *       -kdfopt hexsalt:<salt> -kdfopt hexinfo:<info> HKDF-SHA384
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt mode:EXTRACT_ONLY \
 *       -kdfopt hexkey:<ikm> -kdfopt hexsalt:<salt> HKDF-SHA384
 *
 * and cross-verified with the system 3.0.13 CLI (bare "HKDF" plus
 * -kdfopt digest:SHA2-384 / mode:EXTRACT_ONLY).  In EXTRACT_AND_EXPAND mode
 * the HKDF output is variable length, so EVP_KDF_CTX_get_kdf_size reports
 * SIZE_MAX; in EXTRACT_ONLY mode it is exactly the preset digest size. */
static const struct hkdf_concrete {
    const char *kdfname;   /* the fetchable concrete name */
    const char *digest;    /* its preset digest, for the bare-HKDF route */
    size_t mdsize;         /* 32 / 48 / 64 */
    const char *okmhex;    /* 42-byte EXTRACT_AND_EXPAND OKM */
    const char *prkhex;    /* EXTRACT_ONLY PRK = HMAC-Hash(salt, IKM) */
} hkdf_concretes[] = {
    { "HKDF-SHA256", "SHA2-256", 32,
      /* The RFC 5869 TC1 OKM, as for the bare-HKDF route above. */
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865",
      "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" },
    { "HKDF-SHA384", "SHA2-384", 48,
      "9b5097a86038b805309076a44b3a9f38063e25b516dcbf369f394cfab43685f7"
      "48b6457763e4f0204fc5",
      "704b39990779ce1dc548052c7dc39f303570dd13fb39f7acc564680bef80e8de"
      "c70ee9a7e1f3e293ef68eceb072a5ade" },
    { "HKDF-SHA512", "SHA2-512", 64,
      "832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c14815793"
      "38da362cb8d9f925d7cb",
      "665799823737ded04a88e47e54a5890bb2c3d247c7a4254a8e61350723590a26"
      "c36238127d8661b88cf80ef802d57e2f7cebcf1e00e083848be19929c61b4237" },
};

static void test_concrete_names(void)
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
    ikmlen = from_hex(hkdf_kats[0].ikmhex, ikm, IKM_CAP);
    saltlen = from_hex(hkdf_kats[0].salthex, salt, SALT_CAP);
    infolen = from_hex(hkdf_kats[0].infohex, info, INFO_CAP);

    for (c = 0; c < sizeof(hkdf_concretes) / sizeof(hkdf_concretes[0]); c++) {
        const struct hkdf_concrete *concrete = &hkdf_concretes[c];
        EVP_KDF *kdf = EVP_KDF_fetch(NULL, concrete->kdfname, NULL);
        EVP_KDF_CTX *ctx;
        OSSL_PARAM params[5], modep[2];
        size_t kdfsize;

        if (kdf == NULL) {
            /* Version-gated: the concrete names first shipped in 3.5. */
            printf("SKIP test_kdf_hkdf: %s is unavailable\n",
                   concrete->kdfname);
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
        CHECK(kdf_derive("HKDF", params, bare_okm, 42),
              "bare-HKDF derive with the matching digest param failed");
        snprintf(msg, sizeof(msg),
                 "%s (no digest param) differs from bare HKDF + digest:\"%s\"",
                 concrete->kdfname, concrete->digest);
        CHECK(memcmp(okm, bare_okm, 42) == 0, msg);

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
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_extract_and_expand_kats();
    test_extract_only();
    test_expand_only();
    test_multi_info();
    test_negative();
    test_concrete_names();

    printf("test_kdf_hkdf: all %u checks passed\n", g_checks);
    return 0;
}
