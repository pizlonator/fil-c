#include "test_common.h"

#include "test_ed25519_ed448_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_ed25519 -- Ed25519 and its instances (PureEdDSA): the RFC 8032

 * section 7.1 known-answer vectors, sign/verify negatives, and the

 * contextualized (Ed25519ctx) and prehashed (Ed25519ph) instances (one

 * binary per algorithm; the shared sign/verify helpers, the instance gate,

 * and the vector provenance live in test_ed25519_ed448_common.c, and the

 * Ed448 vectors are the sibling binary's).

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

 *

 * The instance/context parameters only exist from OpenSSL 3.2 on; on 3.0

 * the corresponding subtests are skipped with a note (the plain Ed25519

 * KATs are version-independent).  The gate is a scan of

 * EVP_PKEY_CTX_settable_params for the "instance"/"context-string" names.

 *
 */


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

        key = ed25519_ed448_make_key("ED25519", seed, 32, pub, 32);
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: signing failed", t + 1);
        CHECK(ed25519_ed448_sign(key, NULL, msg, msglen, sig, &siglen) == 1,
              mbuf);
        CHECK(siglen == 64, "Ed25519 signature is not 64 bytes");
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: our signature != RFC signature", t + 1);
        unpoison_all();
        CHECK(memcmp(sig, want, 64) == 0, mbuf);
        repoison_all();
        snprintf(mbuf, sizeof(mbuf),
                 "RFC 8032 7.1 TEST %zu: RFC signature does not verify", t + 1);
        CHECK(ed25519_ed448_verify(key, NULL, msg, msglen, want, wantlen) == 1,
              mbuf);
        EVP_PKEY_free(key);
    }

    free_poison_allocations();
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
    key = ed25519_ed448_make_key("ED25519", seed, 32, pub, 32);

    CHECK(ed25519_ed448_sign(key, NULL, msg, msglen, sig, &siglen) == 1 &&
          siglen == 64,
          "signing our own Ed25519 message failed");
    CHECK(ed25519_ed448_verify(key, NULL, msg, msglen, sig, siglen) == 1,
          "our own Ed25519 signature does not verify");
    /* Determinism: signing twice gives the identical 64 bytes. */
    {
        unsigned char *sig2 = ALLOCATE_OUTPUT(128);
        size_t sig2len = 0;
        unpoison_all();
        CHECK(ed25519_ed448_sign(key, NULL, msg, msglen, sig2, &sig2len) == 1 &&
              sig2len == siglen && memcmp(sig2, sig, siglen) == 0,
              "Ed25519 signing is not deterministic");
        repoison_all();
    }
    for (i = 0; i < 64; i++)
        bad[i] = sig[i];
    bad[3] ^= 0x10;
    CHECK(ed25519_ed448_verify(key, NULL, msg, msglen, bad, siglen) != 1,
          "tampered Ed25519 signature verified");
    EVP_PKEY_free(key);

    free_poison_allocations();
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
    key = ed25519_ed448_make_key("ED25519", seed, 32, pub, 32);
    if (!ed25519_ed448_ctx_accepts(key, "instance") ||
        !ed25519_ed448_ctx_accepts(key, "context-string")) {
        printf("SKIP %s: Ed25519ctx/ph instances are "
               "unavailable\n", test_prog_name());
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
        CHECK(ed25519_ed448_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed25519ctx signing failed");
        unpoison_all();
        CHECK(siglen == 64 && memcmp(sig, want, 64) == 0,
              "Ed25519ctx signature != RFC 8032 7.2 \"foo\" vector");
        repoison_all();
        CHECK(ed25519_ed448_verify(key, params, msg, msglen, want, wantlen) == 1,
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
    key = ed25519_ed448_make_key("ED25519", seed, 32, pub, 32);
    if (ed25519_ed448_ctx_accepts(key, "instance")) {
        char inst[] = "ED25519PH";
        OSSL_PARAM params[2];
        size_t i = 0;

        params[i++] = OSSL_PARAM_construct_utf8_string("instance", inst, 0);
        params[i] = OSSL_PARAM_construct_end();
        msglen = from_hex("616263", msg, 16);
        wantlen = from_hex(ED25519PH_SIG_HEX, want, 128);
        CHECK(wantlen == 64, "Ed25519ph KAT signature length mismatch");
        CHECK(ed25519_ed448_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed25519ph signing failed");
        unpoison_all();
        CHECK(siglen == 64 && memcmp(sig, want, 64) == 0,
              "Ed25519ph signature != RFC 8032 7.3 vector");
        repoison_all();
        CHECK(ed25519_ed448_verify(key, params, msg, msglen, want, wantlen) == 1,
              "Ed25519ph RFC signature does not verify");
    }
    EVP_PKEY_free(key);

    free_poison_allocations();
}


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    test_ed25519_rfc_kats();
    test_ed25519_sign_verify_negative();
    test_ed25519_instances();

    test_report_pass();
    return 0;
}
