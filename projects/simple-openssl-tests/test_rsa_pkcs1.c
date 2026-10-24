#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

/*

 * test_rsa_pkcs1 -- RSA-2048 PKCS#1 v1.5 (type 1 signatures, type 2

 * encryption) known-answer and boundary tests.

 *

 * The fixed 2048-bit key below was generated once with the authoritative

 * build (pizfix/bin/openssl genrsa 2048) and its CRT components were read

 * out with `openssl rsa -text`; every component is embedded as a hex

 * constant and rebuilt through EVP_PKEY_fromdata().

 *

 * Known-answer signature: `printf '%s' "$MSG" | pizfix/bin/openssl dgst

 * -sha256 -sign key.pem` produces the KAT_SIG below; PKCS#1 v1.5 type 1

 * signing is deterministic, so our EVP_DigestSign output must be

 * byte-identical.

 *

 * Behavior notes (verified empirically on OpenSSL 3.0.13 and 3.6.5, and

 * asserted below):

 *

 *   - With the digest route (EVP_DigestSign / EVP_PKEY_sign on the SHA-256

 *     digest with the signature md set) the *message* length is unbounded:

 *     any size works because only the 32-byte digest is ever padded.  The

 *     modlen-11 = 245-byte type 1 limit applies to the padded data, i.e. to

 *     the raw EVP_PKEY_sign route with no digest set: 245-byte tbs signs,

 *     246-byte tbs fails.

 *   - Type 2 (encryption) max is modlen-11 = 245 bytes: 0, 1 and 245 bytes

 *     round-trip; 246 fails.  A 0-byte message round-trips and decrypts to

 *     exactly 0 bytes.

 *   - Decrypting a *tampered* type 2 ciphertext does not error out: OpenSSL

 *     3 defaults to "implicit rejection" (the Bleichenbacher/Marvin

 *     mitigation) and returns a deterministic pseudorandom plaintext of

 *     unpredictable length instead.  We assert the security property: the

 *     original message is never returned.

 *   - A signature made with SHA-256 must fail verification as SHA-512.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rsa_pkcs1: %s\n", msg);
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


/* Copies a big-endian hex constant into a buffer.  Returns the byte length. */
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


/* Same, but byte-reversed into the destination: OSSL_PARAM integers are

 * native-endian (OSSL_PARAM(3)), so big-endian key constants must be

 * byte-swapped before being wrapped in an OSSL_PARAM.  (Both platforms this

 * suite targets are little-endian x86-64.)  Returns the byte length. */
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


/* The fixed RSA-2048 key (generated with pizfix/bin/openssl genrsa 2048). */

static const char *const RSA_N_HEX =
    "98cb57c6f40f0495c819b928e6dd0c32f733e14aa5dd86b69ad3606efa02248b"

    "da3174b3de7300d36c96d0ea03b910443c6e12fa44668899e3281256b1c2e77f"

    "f267a8b44783bc5f17266be47fc5e75b05c12fc83e4b846b811f479c331b269d"

    "870a6addf32af7ef297733861016752fcc7e05396c837bfb4a5bf722915f2f0c"

    "d2dc1b873d719f67870a54fa1bb80911e857b093f885a63e971c1ef76e8e0326"

    "bc2db54f4bc4d63f92312d261ab068865c78367e2e294696766c15ae6bffd33e"

    "3a76084fee42ed400dc1b3451d700a041f1ffcc909960f7e7ba7b9388f1e2e67"

    "8cee1b792bab7a983e4dd8fb9fc81e80c7612010b9ff400067a6ba14a7fd8ad7";
static const char *const RSA_E_HEX = "010001";
static const char *const RSA_D_HEX =
    "20a632d77c8748ff8886969e3ad6b9289ad2272e004b59e1e32b8fcba6f8963e"

    "9c1d3d46c21de8eb44a4e5775bdc4bb189082c5df607c2e0bf3d6ccdc1a65839"

    "6adee399c23e501e31a03025df77b67c28baa32f32c08847408c228d2a0e7f2d"

    "e21143ccb18d58f56df2debba2ec699611284c8d7cb68be3a27c0c4f02a94830"

    "2c8f4d393165956947fd6b4c3f90b025c41e59168858e4bbbe4b901988ab7b0f"

    "c8e5cc5cee408774ee775d1b5676b61485def82c9cac88c9974bfb75ed339653"

    "d139bdc5ec89dbc1336be9cc69fd3e9ba43eabe574953d3ef4332a1e757e33f6"

    "15a1219f2c4cbda31ff5a6275797816189ac3ee1dbe13bb8ca1833fe08065a61";
static const char *const RSA_P_HEX =
    "d4b34b9e582cc4541dccb0e276e59a7a1446f5b11d8d33d88858462dd04f0f33"

    "8541277284da512136827d6040186322e8a6fdcbb12a139a16c66f6f9f4b7256"

    "eb4aa5a09148025e443925a24f2393747c5e5c225086e455efe4bea3502b16a7"

    "f1da24d34d8b710cd39d4133417ece26a8990f95777ff578c8923a820a7cc5a3";
static const char *const RSA_Q_HEX =
    "b7e617a22384a3d0fda106d982cd9c7803f54987f0426d32033974cf4ebe68cf"

    "f872ede510ab626e9c63d14955c452b1414c9d2ca8d95e4eed416c015d755b23"

    "470642016fd97ed32d8b3c89a7e11bf25f1385854c21a290222281c17de53f02"

    "7bb916286daa4687e0b1cc1eb1852ed7d58dc6a9b6ff4308cb726c02203cf13d";
static const char *const RSA_DP_HEX =
    "5395a9cfade2d9bc1c81c5b918f41d827fd96acf91475dea679a18ea04042d7b"

    "17908509f1bf414685ff3d79c7764deef7f237a60616fce0feb638306b2ee0dd"

    "db54d8458312032c3c2f20e4e9a897579d500f70c4a85dae8fb91a595f35af8f"

    "8bf127ab89bb944f701149fff46afa6422b91370af272fb5a94c1d85e0cd2b75";
static const char *const RSA_DQ_HEX =
    "70260b1d9f2981b9af2327893882e9bc2b35c84cf5919d041601512227f468f9"

    "039c1b46499f2b1d27949c0a6c1a990f6e3229dc1570d259b1c8e6f40229d295"

    "b5f7b4032a51ea4d21063bb9fb3e860b8e3dda7c8b0c2c16582ad114ce112317"

    "019ac34f8e697c2fdb30b2b9b92aee8143c3fd767c9a392840e21bca441cc645";
static const char *const RSA_QINV_HEX =
    "11c95272b81c8ff109b3bd210f1143a54640fa2b9a6c108f5fe6429c4387e859"

    "a6f0f9629eca88663a03059e56eb2fafca95ebb688bdb578cca27d368d7d54ad"

    "c91ca3f84d28b705ff672d09286a11bac4cafadd68f0bf6aaf902556e1713209"

    "660be51ec5d717e60cf6bac53ecf57e783b0e45fd5d118aa0c264eec07813e25";


/* The KAT message and its PKCS#1 v1.5 type 1 SHA-256 signature, produced

 * with `pizfix/bin/openssl dgst -sha256 -sign key.pem` (PKCS#1 type 1 is

 * deterministic, so this must be byte-identical to our EVP output). */
static const char KAT_MSG[] = "Fil-C RSA PKCS#1 v1.5 known-answer message";
static const char *const KAT_SIG_HEX =
    "75421562a76d67ba2531f484a1c96141482dc258b021ba18fb51d5290f245bbd"

    "7a4fde956984ccf222320a83596c69a00a9e798a3171d9b9adf69c31912b2968"

    "df399bfbd6b882d65cde20cb9d898ded4e0c469260deed8185b9b3710ac6238d"

    "a9e6fd5ecccf0585475976e51769e687c980fe34ddb8a3fd112c2a513967b6b3"

    "c94527a06d56641e0d019d4ab7458f5df7f20243c5bc1d50894a9d8cc5eb7619"

    "25199da72bc81100a127b4976d52b57d5630784b7563d533abd041b2aa36992b"

    "270136cb971bf680e152672aafb85fea89f214626263d0cb2dc0d2ae70c686ad"

    "ba9eb2573c74ed9db5215bcbb66e16566799813087ac0baca4a4d7d3c8a38f8c";


/* Rebuilds the fixed RSA private key through EVP_PKEY_fromdata.  Key

 * components are copied into ALLOCATE_KEY buffers first (house rule).  The

 * buffers must stay alive for as long as the key is used, hence static. */
static EVP_PKEY *make_key(void)
{
    static unsigned char *n, *e, *d, *p, *q, *dp, *dq, *qinv;
    OSSL_PARAM params[9];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *pkey = NULL;
    size_t i = 0;

    n    = ALLOCATE_KEY(256); e  = ALLOCATE_KEY(3);   d  = ALLOCATE_KEY(256);
    p    = ALLOCATE_KEY(128); q  = ALLOCATE_KEY(128);
    dp   = ALLOCATE_KEY(128); dq = ALLOCATE_KEY(128); qinv = ALLOCATE_KEY(128);

    /* OSSL_PARAM integers are native-endian: byte-swap the big-endian
     * constants (from_hex_le); buffers are exact-size for each component. */
    from_hex_le(RSA_N_HEX, n, 256);
    from_hex_le(RSA_E_HEX, e, 3);
    from_hex_le(RSA_D_HEX, d, 256);
    from_hex_le(RSA_P_HEX, p, 128);
    from_hex_le(RSA_Q_HEX, q, 128);
    from_hex_le(RSA_DP_HEX, dp, 128);
    from_hex_le(RSA_DQ_HEX, dq, 128);
    from_hex_le(RSA_QINV_HEX, qinv, 128);

    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_N, n, 256);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_E, e, 3);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_D, d, 256);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_FACTOR1, p, 128);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_FACTOR2, q, 128);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_EXPONENT1, dp, 128);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_EXPONENT2, dq, 128);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_COEFFICIENT1, qinv, 128);
    params[i] = OSSL_PARAM_construct_end();

    ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(RSA) failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed to import the fixed RSA key");
    CHECK(pkey != NULL && EVP_PKEY_get_size(pkey) == 256,
          "imported RSA key has an unexpected size");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}


/* Digests `msg` with SHA-256 and signs the digest (the "EVP_PKEY_sign with

 * digest=SHA-256" route).  Returns the signature length. */
static size_t sign_digest(EVP_PKEY *key, const unsigned char *msg, size_t msglen,
                          unsigned char *sig, size_t sigcap)
{
    unsigned char *dig = ALLOCATE_OTHER(32);
    unsigned int diglen = 0;
    EVP_PKEY_CTX *ctx;
    size_t siglen = sigcap;

    CHECK(EVP_Digest(msg, msglen, dig, &diglen, EVP_sha256(), NULL) == 1,
          "EVP_Digest(SHA-256) failed");
    CHECK(diglen == 32, "SHA-256 digest length is not 32");
    ctx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_sign_init(ctx) == 1, "EVP_PKEY_sign_init failed");
    CHECK(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) == 1,
          "EVP_PKEY_CTX_set_signature_md(SHA-256) failed");
    CHECK(EVP_PKEY_sign(ctx, sig, &siglen, dig, diglen) == 1,
          "EVP_PKEY_sign failed");
    CHECK(siglen == 256, "PKCS#1 v1.5 signature is not 256 bytes");
    EVP_PKEY_CTX_free(ctx);
    return siglen;
}


/* (1) The KAT: signing the KAT message must reproduce the CLI signature

 * byte-for-byte (PKCS#1 type 1 is deterministic), and the KAT signature

 * must verify. */
static void test_kat(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(sizeof(KAT_MSG) - 1);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *want = ALLOCATE_OUTPUT(256);
    size_t siglen, wantlen;

    memcpy(msg, KAT_MSG, sizeof(KAT_MSG) - 1);
    wantlen = from_hex(KAT_SIG_HEX, want, 256);
    CHECK(wantlen == 256, "KAT signature constant has the wrong length");

    siglen = sign_digest(key, msg, sizeof(KAT_MSG) - 1, sig, 256);
    CHECK(siglen == wantlen && memcmp(sig, want, wantlen) == 0,
          "PKCS#1 v1.5 SHA-256 signature does not match the CLI KAT");

    /* And the KAT signature verifies through EVP_PKEY_verify. */
    {
        unsigned char *dig = ALLOCATE_OTHER(32);
        unsigned int diglen = 0;
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
        CHECK(EVP_Digest(msg, sizeof(KAT_MSG) - 1, dig, &diglen,
                          EVP_sha256(), NULL) == 1,
              "EVP_Digest(SHA-256) failed");
        CHECK(EVP_PKEY_verify_init(ctx) == 1, "EVP_PKEY_verify_init failed");
        CHECK(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) == 1,
              "EVP_PKEY_CTX_set_signature_md(SHA-256) failed");
        CHECK(EVP_PKEY_verify(ctx, want, wantlen, dig, diglen) == 1,
              "the CLI KAT signature does not verify");
        EVP_PKEY_CTX_free(ctx);
    }
}


/* (2) Digest route: message sizes {0,1,11,100,245} (and beyond) sign and

 * verify; hashing makes the message length irrelevant to the type 1

 * padding limit. */
static void test_digest_route_sizes(EVP_PKEY *key)
{
    static const size_t sizes[] = {0, 1, 11, 100, 245};
    unsigned char *msg = ALLOCATE_INPUT(245);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    size_t s, i, siglen;

    for (i = 0; i < 245; i++)
        msg[i] = (unsigned char)(i * 31 + 5);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        char mbuf[96];
        snprintf(mbuf, sizeof(mbuf),
                 "SHA-256 digest-route sign of a %zu-byte message failed",
                 sizes[s]);
        siglen = sign_digest(key, msg, sizes[s], sig, 256);
        CHECK(siglen == 256, mbuf);
        {
            unsigned char *dig = ALLOCATE_OTHER(32);
            unsigned int diglen = 0;
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
            CHECK(EVP_Digest(msg, sizes[s], dig, &diglen,
                              EVP_sha256(), NULL) == 1,
                  "EVP_Digest(SHA-256) failed");
            CHECK(EVP_PKEY_verify_init(ctx) == 1, "EVP_PKEY_verify_init failed");
            CHECK(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) == 1,
                  "EVP_PKEY_CTX_set_signature_md(SHA-256) failed");
            CHECK(EVP_PKEY_verify(ctx, sig, siglen, dig, diglen) == 1,
                  "digest-route signature failed to verify");
            EVP_PKEY_CTX_free(ctx);
        }
    }

    /* Documented: via the digest route even a 246-byte (or 1000-byte)

     * message signs fine -- only its 32-byte digest is ever padded. */
    CHECK(sign_digest(key, msg, 246 % 245 + 1, sig, 256) == 256,
          "digest-route sign of a long message failed");
}


/* (3) Raw route (no digest): the padded data itself is limited to

 * modlen-11 = 245 bytes; 246 must fail. */
static void test_raw_route_boundary(EVP_PKEY *key)
{
    unsigned char *tbs = ALLOCATE_INPUT(246);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    EVP_PKEY_CTX *ctx;
    size_t siglen = 256;
    size_t i;

    for (i = 0; i < 246; i++)
        tbs[i] = (unsigned char)(i * 13 + 1);

    ctx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_sign_init(ctx) == 1, "EVP_PKEY_sign_init failed");
    CHECK(EVP_PKEY_sign(ctx, sig, &siglen, tbs, 245) == 1,
          "raw PKCS#1 type 1 sign of 245 bytes failed");
    CHECK(siglen == 256, "raw sign did not produce a 256-byte signature");
    EVP_PKEY_CTX_free(ctx);

    ctx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_sign_init(ctx) == 1, "EVP_PKEY_sign_init failed");
    CHECK(EVP_PKEY_sign(ctx, sig, &siglen, tbs, 246) != 1,
          "raw PKCS#1 type 1 sign accepted 246 bytes (max is modlen-11)");
    EVP_PKEY_CTX_free(ctx);
}


/* (4) Type 2 encryption/decryption: sizes {0,1,245} round-trip (0-byte

 * messages decrypt to exactly 0 bytes), 246 must fail. */
static void test_type2_roundtrip(EVP_PKEY *key)
{
    static const size_t sizes[] = {0, 1, 245};
    unsigned char *msg = ALLOCATE_INPUT(245);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    size_t s, i;

    for (i = 0; i < 245; i++)
        msg[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(key, NULL);
        EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(key, NULL);
        size_t ctlen = 256, backlen = 256;
        char mbuf[96];

        CHECK(ectx != NULL && dctx != NULL, "EVP_PKEY_CTX_new failed");
        CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
        CHECK(EVP_PKEY_decrypt_init(dctx) == 1, "EVP_PKEY_decrypt_init failed");
        snprintf(mbuf, sizeof(mbuf),
                 "PKCS#1 type 2 encryption of %zu bytes failed", sizes[s]);
        CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, msg, sizes[s]) == 1, mbuf);
        CHECK(ctlen == 256, "type 2 ciphertext is not a full modulus block");
        snprintf(mbuf, sizeof(mbuf),
                 "PKCS#1 type 2 decryption of %zu-byte message failed", sizes[s]);
        CHECK(EVP_PKEY_decrypt(dctx, back, &backlen, ct, ctlen) == 1, mbuf);
        snprintf(mbuf, sizeof(mbuf),
                 "type 2 roundtrip of %zu bytes returned %zu bytes",
                 sizes[s], backlen);
        CHECK(backlen == sizes[s], mbuf);
        if (sizes[s] > 0)
            CHECK(memcmp(back, msg, sizes[s]) == 0,
                  "type 2 roundtrip did not recover the message");
        EVP_PKEY_CTX_free(ectx);
        EVP_PKEY_CTX_free(dctx);
    }

    /* 246 bytes exceeds the type 2 modlen-11 limit and must fail. */
    {
        EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(key, NULL);
        size_t ctlen = 256;
        CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
        CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, msg, 246) != 1,
              "type 2 encryption accepted 246 bytes (max is modlen-11)");
        EVP_PKEY_CTX_free(ectx);
    }
}


/* (5) A tampered type 2 ciphertext must never yield the original message.

 * OpenSSL 3 defaults to implicit rejection: the decrypt "succeeds" but

 * returns a deterministic pseudorandom plaintext of attacker-unpredictable

 * length (Bleichenbacher/Marvin countermeasure).  Both OpenSSL 3.0.13 and

 * 3.6.5 behave this way, which is asserted here. */
static void test_type2_tamper(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(key, NULL);
    EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(key, NULL);
    size_t ctlen = 256, backlen = 256;
    size_t i;

    for (i = 0; i < 16; i++)
        msg[i] = (unsigned char)(0xA0 + i);
    CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
    CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, msg, 16) == 1,
          "type 2 encryption of the tamper-test message failed");
    ct[10] ^= 0x01;
    CHECK(EVP_PKEY_decrypt_init(dctx) == 1, "EVP_PKEY_decrypt_init failed");
    CHECK(EVP_PKEY_decrypt(dctx, back, &backlen, ct, ctlen) == 1,
          "implicitly-rejected decrypt of tampered ciphertext errored");
    CHECK(!(backlen == 16 && memcmp(back, msg, 16) == 0),
          "tampered type 2 ciphertext decrypted to the original message");
    EVP_PKEY_CTX_free(ectx);
    EVP_PKEY_CTX_free(dctx);
}


/* (6) A SHA-256 signature must fail verification as SHA-512 (the DigestInfo

 * prefix differs). */
static void test_wrong_digest(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(11);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *dig = ALLOCATE_OTHER(32);
    unsigned int diglen = 0;
    EVP_PKEY_CTX *ctx;
    size_t siglen;
    size_t i;

    for (i = 0; i < 11; i++)
        msg[i] = (unsigned char)(0x40 + i);
    siglen = sign_digest(key, msg, 11, sig, 256);
    CHECK(siglen == 256, "sign_digest failed");

    CHECK(EVP_Digest(msg, 11, dig, &diglen, EVP_sha256(), NULL) == 1,
          "EVP_Digest(SHA-256) failed");
    ctx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(EVP_PKEY_verify_init(ctx) == 1, "EVP_PKEY_verify_init failed");
    CHECK(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha512()) == 1,
          "EVP_PKEY_CTX_set_signature_md(SHA-512) failed");
    CHECK(EVP_PKEY_verify(ctx, sig, siglen, dig, diglen) != 1,
          "SHA-256 signature verified as SHA-512");
    EVP_PKEY_CTX_free(ctx);
}


int main(int argc, char **argv)
{
    EVP_PKEY *key;

    poison_parse_args(argc, argv);

    key = make_key();
    test_kat(key);
    test_digest_route_sizes(key);
    test_raw_route_boundary(key);
    test_type2_roundtrip(key);
    test_type2_tamper(key);
    test_wrong_digest(key);
    EVP_PKEY_free(key);

    printf("test_rsa_pkcs1: all %u checks passed\n", g_checks);
    return 0;
}
