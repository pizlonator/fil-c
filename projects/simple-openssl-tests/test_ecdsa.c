#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_ecdsa -- ECDSA over the NIST P-256/P-384/P-521 curves and

 * secp256k1: key generation, one-shot and streaming DigestSign/Verify,

 * tamper and wrong-digest rejection, DER size bounds, and the RFC 6979

 * deterministic-ECDSA known-answer vector.

 *

 * Known-answer vector: RFC 6979 A.2.5, P-256 with SHA-256 over "sample".

 * The embedded private key (x = C9AFA9D8...6721, U = xG with the Ux/Uy

 * from the RFC) and the expected DER signature were taken verbatim from

 * the RFC and cross-checked against OpenSSL 3.6.5's own test data

 * (test/recipes/30-test_evp_data/evppkey_ecdsa_rfc6979.txt).  Deterministic

 * signing is requested with the "nonce-type" = 1 (RFC 6979) parameter.

 * The parameter is silently ignored by OpenSSL 3.0/3.1, so the subtest

 * verifies with a read-back that the setting stuck and prints a SKIP note

 * otherwise.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5:

 *

 *   - ECDSA k is RANDOM by default (not deterministic), so two signatures

 *     of the same message differ; streaming Update+Final signatures are

 *     therefore not byte-equal to one-shot signatures.  What is asserted is

 *     that both verify.  (With nonce-type 1 the two routes agree, which the

 *     RFC 6979 subtest demonstrates.)

 *   - DER signatures vary in length (leading zero bytes of r/s) within

 *     ECDSA_size() bounds.

 *

 * The public keys/signatures produced here were additionally cross-checked

 * against the OpenSSL CLI during development (PEM_write_PUBKEY to /tmp,

 * then `openssl dgst -sha256 -verify pub.pem -signature sig.bin`), which is

 * a development-time check only: the suite's programs do no file I/O.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_ecdsa: %s\n", msg);
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


/* OSSL_PARAM integers are native-endian (OSSL_PARAM(3)); see
 * test_rsa_pkcs1.c. */
static size_t from_hex_le(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = from_hex(hex, dst, dstcap);
    size_t i;
    for (i = 0; i < n / 2; i++) {
        unsigned char t = dst[i];
        dst[i] = dst[n - 1 - i];
        dst[n - 1 - i] = t;
    }
    return n;
}


static const char *const MESSAGE = "ECDSA known-answer test message";


/* Generates a key on `curve` through EVP_PKEY_keygen. */
static EVP_PKEY *gen_key(const char *curve)
{
    OSSL_PARAM params[2];
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    EVP_PKEY *key = NULL;
    char name[32];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(EC) failed");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    snprintf(name, sizeof(name), "%s", curve);
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 name, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the EC group name failed");
    CHECK(EVP_PKEY_generate(ctx, &key) == 1, "EVP_PKEY_generate failed");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* One-shot ECDSA sign; returns the DER signature length. */
static size_t ecdsa_sign(EVP_PKEY *key, const EVP_MD *md,
                         const unsigned char *msg, size_t msglen,
                         unsigned char *sig, size_t sigcap)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    size_t siglen = sigcap;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, NULL, md, NULL, key) == 1,
          "EVP_DigestSignInit failed");
    CHECK(EVP_DigestSign(mctx, sig, &siglen, msg, msglen) == 1,
          "EVP_DigestSign failed");
    EVP_MD_CTX_free(mctx);
    return siglen;
}


/* One-shot ECDSA verify; returns 1 on success. */
static int ecdsa_verify(EVP_PKEY *key, const EVP_MD *md, int saltlen_unused,
                        const unsigned char *msg, size_t msglen,
                        const unsigned char *sig, size_t siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    int ok;

    (void)saltlen_unused;
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit(mctx, NULL, md, NULL, key) == 1,
          "EVP_DigestVerifyInit failed");
    ok = EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}


/* Streaming ECDSA sign via EVP_DigestSignUpdate + EVP_DigestSignFinal. */
static size_t ecdsa_sign_stream(EVP_PKEY *key, const EVP_MD *md,
                                const unsigned char *msg, size_t msglen,
                                unsigned char *sig, size_t sigcap)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    size_t siglen = sigcap;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, NULL, md, NULL, key) == 1,
          "EVP_DigestSignInit failed");
    CHECK(EVP_DigestSignUpdate(mctx, msg, 1) == 1,
          "EVP_DigestSignUpdate (first byte) failed");
    CHECK(EVP_DigestSignUpdate(mctx, msg + 1, msglen - 1) == 1,
          "EVP_DigestSignUpdate (rest) failed");
    CHECK(EVP_DigestSignFinal(mctx, sig, &siglen) == 1,
          "EVP_DigestSignFinal failed");
    EVP_MD_CTX_free(mctx);
    return siglen;
}


/* (1) Keygen + one-shot sign/verify on all four curves, with DER length

 * bounds. */
static void test_keygen_sign_verify(void)
{
    static const char *const curves[] = {
        "prime256v1", "secp384r1", "secp521r1", "secp256k1"
    };
    size_t c;
    size_t msglen = strlen(MESSAGE);
    unsigned char *msg = ALLOCATE_INPUT(msglen);
    unsigned char *sig = ALLOCATE_OUTPUT(200);

    memcpy(msg, MESSAGE, msglen);

    for (c = 0; c < sizeof(curves) / sizeof(curves[0]); c++) {
        char mbuf[128];
        EVP_PKEY *key = gen_key(curves[c]);
        int maxsig = EVP_PKEY_get_size(key);
        size_t siglen;

        CHECK(maxsig > 0 && maxsig <= 200, "ECDSA_size is out of range");
        snprintf(mbuf, sizeof(mbuf), "%s: one-shot sign failed", curves[c]);
        siglen = ecdsa_sign(key, EVP_sha256(), msg, msglen, sig, 200);
        CHECK(siglen > 0 && (size_t)maxsig >= siglen, mbuf);
        snprintf(mbuf, sizeof(mbuf), "%s: one-shot verify failed", curves[c]);
        CHECK(ecdsa_verify(key, EVP_sha256(), 0, msg, msglen, sig, siglen) == 1,
              mbuf);
        EVP_PKEY_free(key);
    }
}


/* (2) Streaming Update+Final: the signature verifies (k is random, so the

 * DER bytes are NOT expected to equal the one-shot result). */
static void test_streaming(void)
{
    EVP_PKEY *key = gen_key("prime256v1");
    size_t msglen = strlen(MESSAGE);
    unsigned char *msg = ALLOCATE_INPUT(msglen);
    unsigned char *one = ALLOCATE_OUTPUT(200);
    unsigned char *many = ALLOCATE_OUTPUT(200);
    size_t onelen, manylen;

    memcpy(msg, MESSAGE, msglen);
    onelen = ecdsa_sign(key, EVP_sha256(), msg, msglen, one, 200);
    manylen = ecdsa_sign_stream(key, EVP_sha256(), msg, msglen, many, 200);

    CHECK(onelen > 0 && manylen > 0 &&
          (size_t)EVP_PKEY_get_size(key) >= manylen,
          "streaming ECDSA signature length out of bounds");
    CHECK(ecdsa_verify(key, EVP_sha256(), 0, msg, msglen, many, manylen) == 1,
          "streaming ECDSA signature failed to verify");
    CHECK(ecdsa_verify(key, EVP_sha256(), 0, msg, msglen, one, onelen) == 1,
          "one-shot ECDSA signature failed to verify");
    /* Documented: with random k the two signatures differ. */
    if (onelen == manylen && memcmp(one, many, onelen) == 0)
        CHECK(0, "expected randomized ECDSA signatures to differ");
    EVP_PKEY_free(key);
}


/* (3) Tampered signature fails; signature verifies under the wrong digest

 * must fail. */
static void test_negative(void)
{
    EVP_PKEY *key = gen_key("prime256v1");
    size_t msglen = strlen(MESSAGE);
    unsigned char *msg = ALLOCATE_INPUT(msglen);
    unsigned char *sig = ALLOCATE_OUTPUT(200);
    size_t siglen;

    memcpy(msg, MESSAGE, msglen);
    siglen = ecdsa_sign(key, EVP_sha256(), msg, msglen, sig, 200);
    CHECK(siglen > 0, "sign for the negative tests failed");

    sig[siglen - 1] ^= 0x01;
    CHECK(ecdsa_verify(key, EVP_sha256(), 0, msg, msglen, sig, siglen) != 1,
          "tampered ECDSA signature verified");
    sig[siglen - 1] ^= 0x01;

    CHECK(ecdsa_verify(key, EVP_sha512(), 0, msg, msglen, sig, siglen) != 1,
          "SHA-256 ECDSA signature verified as SHA-512");
    EVP_PKEY_free(key);
}


/* (4) RFC 6979 A.2.5 deterministic ECDSA (P-256, SHA-256, "sample").  The

 * "nonce-type" = 1 parameter is only honored from OpenSSL 3.2 on; a

 * read-back gate skips the subtest with a note on 3.0/3.1. */
static const char *const RFC6979_X_HEX =
    "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721";
static const char *const RFC6979_UX_HEX =
    "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6";
static const char *const RFC6979_UY_HEX =
    "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299";
/* The RFC 6979 A.2.5 SHA-256 "sample" DER signature (r = EFD48B2A...,
 * s = F7CB1C94...). */
static const char *const RFC6979_SIG_HEX =
    "3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0e"

    "a84eaf3716022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff406"

    "4dc4ab2f843acda8";


static void test_rfc6979_deterministic(void)
{
    static unsigned char *x, *pub;
    unsigned char *want = ALLOCATE_OUTPUT(80);
    unsigned char *sig = ALLOCATE_OUTPUT(80);
    const char *msg = "sample";
    OSSL_PARAM params[4];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    EVP_MD_CTX *mctx;
    EVP_PKEY_CTX *sctx = NULL;
    unsigned int nonce = 1, nonce_back = 99;
    OSSL_PARAM setp[2], getp[2];
    size_t wantlen, siglen;
    size_t i = 0;

    x = ALLOCATE_KEY(32);
    pub = ALLOCATE_KEY(65);
    from_hex_le(RFC6979_X_HEX, x, 32);
    pub[0] = 0x04;
    from_hex(RFC6979_UX_HEX, pub + 1, 32);
    from_hex(RFC6979_UY_HEX, pub + 33, 32);

    params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                   (char *)"prime256v1", 0);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY, x, 32);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                    pub, 65);
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(EC) failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the RFC 6979 key");
    EVP_PKEY_CTX_free(ctx);

    mctx = EVP_MD_CTX_new();
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, &sctx, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestSignInit failed for the RFC 6979 subtest");
    setp[0] = OSSL_PARAM_construct_uint("nonce-type", &nonce);
    setp[1] = OSSL_PARAM_construct_end();
    getp[0] = OSSL_PARAM_construct_uint("nonce-type", &nonce_back);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(sctx, setp) == 1,
          "setting nonce-type failed");
    CHECK(EVP_PKEY_CTX_get_params(sctx, getp) == 1,
          "reading back nonce-type failed");
    if (nonce_back != 1) {
        /* OpenSSL 3.0/3.1 ignore the parameter (random k); skip gracefully. */
        printf("SKIP test_ecdsa: deterministic ECDSA (nonce-type/RFC 6979) "
               "is unavailable\n");
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(key);
        return;
    }
    siglen = 80;
    CHECK(EVP_DigestSign(mctx, sig, &siglen, (const unsigned char *)msg,
                         strlen(msg)) == 1,
          "deterministic ECDSA sign failed");
    wantlen = from_hex(RFC6979_SIG_HEX, want, 80);
    CHECK(siglen == wantlen, "deterministic ECDSA signature length mismatch");
    CHECK(memcmp(sig, want, wantlen) == 0,
          "deterministic ECDSA signature does not match the RFC 6979 A.2.5 "
          "vector");
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(key);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_keygen_sign_verify();
    test_streaming();
    test_negative();
    test_rfc6979_deterministic();

    printf("test_ecdsa: all %u checks passed\n", g_checks);
    return 0;
}
