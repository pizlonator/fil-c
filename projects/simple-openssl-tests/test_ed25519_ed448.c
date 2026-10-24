#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_ed25519_ed448 -- Ed25519 and Ed448 (PureEdDSA), with the RFC 8032

 * known-answer vectors, the contextualized (Ed25519ctx) and prehashed

 * (Ed25519ph) instances, and the Ed448 context variant.

 *

 * Known-answer vectors, taken verbatim from RFC 8032 and verified with

 * filcc-compiled scratch probes (and the pizfix CLI for TEST 1) before

 * being embedded:

 *

 *   - Section 7.1 TEST 1 (empty message), TEST 2 (1 byte 0x72) and

 *     TEST 3 (2 bytes 0xaf82) for Ed25519.  PureEdDSA is deterministic, so

 *     signing the same message with the same key must reproduce the RFC

 *     signature byte-for-byte; the tests check both directions (our sign

 *     == RFC sig, and the RFC sig verifies).

 *   - Section 7.2 "foo" for Ed25519ctx (context = "foo"), via the

 *     "instance" = "ED25519CTX" and "context-string" parameters.

 *   - Section 7.3 "TEST abc" for Ed25519ph, via "instance" = "ED25519PH".

 *     The SHA-512 prehash happens inside the provider: no digest name is

 *     passed to EVP_DigestSignInit_ex (EdDSA forbids explicit digests).

 *   - Section 7.4 "Blank" (empty message) and "1 octet" (0x03) for Ed448,

 *     plus the "1 octet (with context)" vector (context = "foo").

 *

 * The instance/context parameters only exist from OpenSSL 3.2 on; on 3.0

 * the corresponding subtests are skipped with a note (the plain Ed25519 /

 * Ed448 KATs are version-independent).  The gate is a scan of

 * EVP_PKEY_CTX_settable_params for the "instance"/"context-string" names.

 *

 * The Ed25519 keys/signatures were also cross-checked against the CLI

 * during development (PEM files under /tmp + `openssl pkeyutl -verify

 * -pubin -inkey pub.pem -sigfile sig.bin -rawin -in msg.bin`); that is a

 * development-time check only -- the suite's programs do no file I/O.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_ed25519_ed448: %s\n", msg);
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


/* Copies a hex constant into a buffer.  EdDSA keys are octet strings (the

 * RFC byte order), so no byte-swapping.  Returns the byte length. */
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


/* Imports an EdDSA key pair (32-byte Ed25519 / 57-byte Ed448 seed plus the

 * matching public key).  Buffers are static: the key references them. */
static EVP_PKEY *make_key(const char *alg, const unsigned char *seed,
                          size_t seedlen, const unsigned char *pub,
                          size_t publen)
{
    static unsigned char *sbuf, *pbuf;
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    sbuf = ALLOCATE_KEY(seedlen);
    memcpy(sbuf, seed, seedlen);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                    sbuf, seedlen);
    pbuf = ALLOCATE_KEY(publen);
    memcpy(pbuf, pub, publen);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                    pbuf, publen);
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, alg, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the EdDSA key");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* One-shot sign with optional OSSL params (instance/context-string). */
static int ed_sign(EVP_PKEY *key, const OSSL_PARAM *params,
                   const unsigned char *msg, size_t msglen,
                   unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    int ok;

    *siglen = 256;
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit_ex(mctx, NULL, NULL, NULL, NULL, key,
                                params) == 1,
          "EVP_DigestSignInit_ex failed");
    ok = EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}


/* One-shot verify with optional OSSL params; returns 1 on success. */
static int ed_verify(EVP_PKEY *key, const OSSL_PARAM *params,
                     const unsigned char *msg, size_t msglen,
                     const unsigned char *sig, size_t siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    int ok;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit_ex(mctx, NULL, NULL, NULL, NULL, key,
                                  params) == 1,
          "EVP_DigestVerifyInit_ex failed");
    ok = EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}


/* Returns 1 if the EdDSA signature context accepts `param` (used to gate

 * the OpenSSL 3.2+ instance/context-string subtests). */
static int ctx_accepts(EVP_PKEY *key, const char *param)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = NULL;
    const OSSL_PARAM *settable;
    int found = 0;

    CHECK(EVP_DigestSignInit_ex(mctx, &pctx, NULL, NULL, NULL, key, NULL) == 1,
          "EVP_DigestSignInit_ex failed in ctx_accepts");
    settable = EVP_PKEY_CTX_settable_params(pctx);
    for (; settable != NULL && settable->key != NULL; settable++) {
        if (strcmp(settable->key, param) == 0) {
            found = 1;
            break;
        }
    }
    EVP_MD_CTX_free(mctx);
    return found;
}


/* RFC 8032 section 7.1 TEST 1-3 for Ed25519 (key, message, signature). */
static const struct {
    const char *skhex, *pkhex, *msghex, *sighex;
} ed25519_kats[] = {
    { "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
      "",
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
      "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b" },
    { "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c",
      "72",
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
      "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00" },
    { "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025",
      "af82",
      "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
      "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a" },
};


static void test_ed25519_rfc_kats(void)
{
    size_t t;
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *sig = ALLOCATE_OUTPUT(128);
    unsigned char *want = ALLOCATE_OUTPUT(128);
    unsigned char *seed = ALLOCATE_KEY(32);
    unsigned char *pub = ALLOCATE_KEY(32);
    size_t siglen = 0, wantlen, msglen;

    for (t = 0; t < sizeof(ed25519_kats) / sizeof(ed25519_kats[0]); t++) {
        char mbuf[128];
        EVP_PKEY *key;
        seed = ALLOCATE_KEY(32);
        pub = ALLOCATE_KEY(32);
        from_hex(ed25519_kats[t].skhex, seed, 32);
        from_hex(ed25519_kats[t].pkhex, pub, 32);
        msglen = from_hex(ed25519_kats[t].msghex, msg, 64);
        wantlen = from_hex(ed25519_kats[t].sighex, want, 128);
        CHECK(wantlen == 64, "RFC 8032 Ed25519 signature length mismatch");

        key = make_key("ED25519", seed, 32, pub, 32);
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: signing failed", t + 1);
        CHECK(ed_sign(key, NULL, msg, msglen, sig, &siglen) == 1, mbuf);
        CHECK(siglen == 64, "Ed25519 signature is not 64 bytes");
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: our signature != RFC signature", t + 1);
        CHECK(memcmp(sig, want, 64) == 0, mbuf);
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: RFC signature does not verify", t + 1);
        CHECK(ed_verify(key, NULL, msg, msglen, want, wantlen) == 1, mbuf);
        EVP_PKEY_free(key);
    }
}


/* Signs our own message with TEST 1's key and verifies it (deterministic

 * route sanity), then checks that a tampered signature fails. */
static void test_ed25519_sign_verify_negative(void)
{
    unsigned char *msg = ALLOCATE_INPUT(25);
    unsigned char *sig = ALLOCATE_OUTPUT(128);
    unsigned char *bad = ALLOCATE_OUTPUT(128);
    unsigned char *seed = ALLOCATE_KEY(32);
    unsigned char *pub = ALLOCATE_KEY(32);
    EVP_PKEY *key;
    const char *msgtext = "Ed25519 says hi to Fil-C";
    size_t msglen = strlen(msgtext), siglen = 0;
    size_t i;

    from_hex(ed25519_kats[0].skhex, seed, 32);
    from_hex(ed25519_kats[0].pkhex, pub, 32);
    memcpy(msg, msgtext, msglen);
    key = make_key("ED25519", seed, 32, pub, 32);

    CHECK(ed_sign(key, NULL, msg, msglen, sig, &siglen) == 1 && siglen == 64,
          "signing our own Ed25519 message failed");
    CHECK(ed_verify(key, NULL, msg, msglen, sig, siglen) == 1,
          "our own Ed25519 signature does not verify");
    /* Determinism: signing twice gives the identical 64 bytes. */
    {
        unsigned char *sig2 = ALLOCATE_OUTPUT(128);
        size_t sig2len = 0;
        CHECK(ed_sign(key, NULL, msg, msglen, sig2, &sig2len) == 1 &&
              sig2len == siglen && memcmp(sig2, sig, siglen) == 0,
              "Ed25519 signing is not deterministic");
    }
    for (i = 0; i < 64; i++)
        bad[i] = sig[i];
    bad[3] ^= 0x10;
    CHECK(ed_verify(key, NULL, msg, msglen, bad, siglen) != 1,
          "tampered Ed25519 signature verified");
    EVP_PKEY_free(key);
}


/* RFC 8032 sections 7.2 (Ed25519ctx "foo") and 7.3 (Ed25519ph "TEST abc"),

 * gated on the "instance"/"context-string" parameters. */
static const char *const ED25519CTX_SK_HEX =
    "0305334e381af78f141cb666f6199f57bc3495335a256a95bd2a55bf546663f6";
static const char *const ED25519CTX_PK_HEX =
    "dfc9425e4f968f7f0c29f0259cf5f9aed6851c2bb4ad8bfb860cfee0ab248292";
static const char *const ED25519CTX_MSG_HEX = "f726936d19c800494e3fdaff20b276a8";
static const char *const ED25519CTX_SIG_HEX =
    "55a4cc2f70a54e04288c5f4cd1e45a7bb520b36292911876cada7323198dd87a"
    "8b36950b95130022907a7fb7c4e9b2d5f6cca685a587b4b21f4b888e4e7edb0d";
static const char *const ED25519PH_SK_HEX =
    "833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42";
static const char *const ED25519PH_PK_HEX =
    "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf";
static const char *const ED25519PH_SIG_HEX =
    "98a70222f0b8121aa9d30f813d683f809e462b469c7ff87639499bb94e6dae41"
    "31f85042463c2a355a2003d062adf5aaa10b8c61e636062aaad11c2a26083406";


static void test_ed25519_instances(void)
{
    unsigned char *msg = ALLOCATE_INPUT(16);
    unsigned char *sig = ALLOCATE_OUTPUT(128);
    unsigned char *want = ALLOCATE_OUTPUT(128);
    unsigned char *seed = ALLOCATE_KEY(32);
    unsigned char *pub = ALLOCATE_KEY(32);
    EVP_PKEY *key;
    size_t msglen, wantlen, siglen = 0;

    /* Ed25519ctx with context "foo". */
    from_hex(ED25519CTX_SK_HEX, seed, 32);
    from_hex(ED25519CTX_PK_HEX, pub, 32);
    key = make_key("ED25519", seed, 32, pub, 32);
    if (!ctx_accepts(key, "instance") || !ctx_accepts(key, "context-string")) {
        printf("SKIP test_ed25519_ed448: Ed25519ctx/ph instances are "
               "unavailable\n");
    } else {
        char ctxfoo[] = "foo";
        char inst[] = "ED25519CTX";
        OSSL_PARAM params[3];
        size_t i = 0;

        params[i++] = OSSL_PARAM_construct_utf8_string("instance", inst, 0);
        params[i++] = OSSL_PARAM_construct_octet_string("context-string",
                                                        ctxfoo, 3);
        params[i] = OSSL_PARAM_construct_end();
        msglen = from_hex(ED25519CTX_MSG_HEX, msg, 16);
        wantlen = from_hex(ED25519CTX_SIG_HEX, want, 128);
        CHECK(wantlen == 64, "Ed25519ctx KAT signature length mismatch");
        CHECK(ed_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed25519ctx signing failed");
        CHECK(siglen == 64 && memcmp(sig, want, 64) == 0,
              "Ed25519ctx signature != RFC 8032 7.2 \"foo\" vector");
        CHECK(ed_verify(key, params, msg, msglen, want, wantlen) == 1,
              "Ed25519ctx RFC signature does not verify");
        /* The context is bound: verifying with a different one fails. */
        {
            char ctxbar[] = "bar";
            OSSL_PARAM other[3];
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx = NULL;
            int rv;
            other[0] = OSSL_PARAM_construct_utf8_string("instance", inst, 0);
            other[1] = OSSL_PARAM_construct_octet_string("context-string",
                                                         ctxbar, 3);
            other[2] = OSSL_PARAM_construct_end();
            CHECK(EVP_DigestVerifyInit_ex(mctx, &pctx, NULL, NULL, NULL, key,
                                          other) == 1,
                  "EVP_DigestVerifyInit_ex (bar) failed");
            rv = EVP_DigestVerify(mctx, want, wantlen, msg, msglen);
            CHECK(rv != 1,
                  "Ed25519ctx signature verified under the wrong context");
            EVP_MD_CTX_free(mctx);
        }
    }
    EVP_PKEY_free(key);

    /* Ed25519ph over "abc" (the provider applies the SHA-512 prehash). */
    from_hex(ED25519PH_SK_HEX, seed, 32);
    from_hex(ED25519PH_PK_HEX, pub, 32);
    key = make_key("ED25519", seed, 32, pub, 32);
    if (ctx_accepts(key, "instance")) {
        char inst[] = "ED25519PH";
        OSSL_PARAM params[2];
        size_t i = 0;

        params[i++] = OSSL_PARAM_construct_utf8_string("instance", inst, 0);
        params[i] = OSSL_PARAM_construct_end();
        msglen = from_hex("616263", msg, 16);
        wantlen = from_hex(ED25519PH_SIG_HEX, want, 128);
        CHECK(wantlen == 64, "Ed25519ph KAT signature length mismatch");
        CHECK(ed_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed25519ph signing failed");
        CHECK(siglen == 64 && memcmp(sig, want, 64) == 0,
              "Ed25519ph signature != RFC 8032 7.3 vector");
        CHECK(ed_verify(key, params, msg, msglen, want, wantlen) == 1,
              "Ed25519ph RFC signature does not verify");
    }
    EVP_PKEY_free(key);
}


/* RFC 8032 section 7.4 for Ed448: "Blank" (empty message) and "1 octet"

 * (0x03), plus the "1 octet (with context)" vector (context "foo"). */
static const char *const ED448_BLANK_SK_HEX =
    "6c82a562cb808d10d632be89c8513ebf6c929f34ddfa8c9f63c9960ef6e348a3"
    "528c8a3fcc2f044e39a3fc5b94492f8f032e7549a20098f95b";
static const char *const ED448_BLANK_PK_HEX =
    "5fd7449b59b461fd2ce787ec616ad46a1da1342485a70e1f8a0ea75d80e96778"
    "edf124769b46c7061bd6783df1e50f6cd1fa1abeafe8256180";
static const char *const ED448_BLANK_SIG_HEX =
    "533a37f6bbe457251f023c0d88f976ae2dfb504a843e34d2074fd823d41a591f"
    "2b233f034f628281f2fd7a22ddd47d7828c59bd0a21bfd3980ff0d2028d4b18a"
    "9df63e006c5d1c2d345b925d8dc00b4104852db99ac5c7cdda8530a113a0f4db"
    "b61149f05a7363268c71d95808ff2e652600";
static const char *const ED448_1OCT_SK_HEX =
    "c4eab05d357007c632f3dbb48489924d552b08fe0c353a0d4a1f00acda2c463a"
    "fbea67c5e8d2877c5e3bc397a659949ef8021e954e0a12274e";
static const char *const ED448_1OCT_PK_HEX =
    "43ba28f430cdff456ae531545f7ecd0ac834a55d9358c0372bfa0c6c6798c086"
    "6aea01eb00742802b8438ea4cb82169c235160627b4c3a9480";
static const char *const ED448_1OCT_SIG_HEX =
    "26b8f91727bd62897af15e41eb43c377efb9c610d48f2335cb0bd0087810f435"
    "2541b143c4b981b7e18f62de8ccdf633fc1bf037ab7cd779805e0dbcc0aae1cb"
    "cee1afb2e027df36bc04dcecbf154336c19f0af7e0a6472905e799f1953d2a0f"
    "f3348ab21aa4adafd1d234441cf807c03a00";
static const char *const ED448_CTX_SIG_HEX =
    "d4f8f6131770dd46f40867d6fd5d5055de43541f8c5e35abbcd001b32a89f7d2"
    "151f7647f11d8ca2ae279fb842d607217fce6e042f6815ea000c85741de5c8da"
    "1144a6a1aba7f96de42505d7a7298524fda538fccbbb754f578c1cad10d54d0d"
    "5428407e85dcbc98a49155c13764e66c3c00";


static void test_ed448(void)
{
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *want = ALLOCATE_OUTPUT(256);
    unsigned char *seed = ALLOCATE_KEY(57);
    unsigned char *pub = ALLOCATE_KEY(57);
    EVP_PKEY *key;
    size_t msglen = 0, wantlen, siglen = 0;

    /* "Blank": empty message. */
    from_hex(ED448_BLANK_SK_HEX, seed, 57);
    from_hex(ED448_BLANK_PK_HEX, pub, 57);
    key = make_key("ED448", seed, 57, pub, 57);
    wantlen = from_hex(ED448_BLANK_SIG_HEX, want, 256);
    CHECK(wantlen == 114, "Ed448 Blank KAT signature length mismatch");
    CHECK(ed_sign(key, NULL, msg, 0, sig, &siglen) == 1,
          "Ed448 Blank signing failed");
    CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
          "Ed448 Blank signature != RFC 8032 7.4 vector");
    CHECK(ed_verify(key, NULL, msg, 0, want, wantlen) == 1,
          "Ed448 Blank RFC signature does not verify");
    EVP_PKEY_free(key);

    /* "1 octet": message 0x03. */
    from_hex(ED448_1OCT_SK_HEX, seed, 57);
    from_hex(ED448_1OCT_PK_HEX, pub, 57);
    key = make_key("ED448", seed, 57, pub, 57);
    msg[0] = 0x03;
    msglen = 1;
    wantlen = from_hex(ED448_1OCT_SIG_HEX, want, 256);
    CHECK(wantlen == 114, "Ed448 1-octet KAT signature length mismatch");
    CHECK(ed_sign(key, NULL, msg, msglen, sig, &siglen) == 1,
          "Ed448 1-octet signing failed");
    CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
          "Ed448 1-octet signature != RFC 8032 7.4 vector");
    EVP_PKEY_free(key);

    /* "1 octet (with context foo)" -- gated on context-string support. */
    from_hex(ED448_1OCT_SK_HEX, seed, 57);
    from_hex(ED448_1OCT_PK_HEX, pub, 57);
    key = make_key("ED448", seed, 57, pub, 57);
    if (ctx_accepts(key, "context-string")) {
        char ctxfoo[] = "foo";
        OSSL_PARAM params[2];
        size_t i = 0;

        params[i++] = OSSL_PARAM_construct_octet_string("context-string",
                                                        ctxfoo, 3);
        params[i] = OSSL_PARAM_construct_end();
        wantlen = from_hex(ED448_CTX_SIG_HEX, want, 256);
        CHECK(wantlen == 114, "Ed448 context KAT signature length mismatch");
        CHECK(ed_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed448 context signing failed");
        CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
              "Ed448 context signature != RFC 8032 7.4 vector");
        CHECK(ed_verify(key, params, msg, msglen, want, wantlen) == 1,
              "Ed448 context RFC signature does not verify");
    }
    EVP_PKEY_free(key);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_ed25519_rfc_kats();
    test_ed25519_sign_verify_negative();
    test_ed25519_instances();
    test_ed448();

    printf("test_ed25519_ed448: all %u checks passed\n", g_checks);
    return 0;
}
