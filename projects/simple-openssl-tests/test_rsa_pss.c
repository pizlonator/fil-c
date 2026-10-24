#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

/*

 * test_rsa_pss -- RSA-2048-PSS sign/verify with the same fixed key as

 * test_rsa_pkcs1.c (generated once with pizfix/bin/openssl genrsa 2048 and

 * embedded as hex constants; OSSL_PARAM integers are native-endian, so the

 * big-endian constants are byte-swapped into the working buffers).

 *

 * PSS parameters are set through OSSL_PARAMs: pad-mode "pss" and the

 * "saltlen" integer (RSA_PSS_SALTLEN_DIGEST = -1, RSA_PSS_SALTLEN_AUTO =

 * -2, RSA_PSS_SALTLEN_MAX = -3, or an explicit byte count).

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - salt lengths DIGEST_LENGTH (32 for SHA-256), 20 and MAX (emLen -

 *     hLen - 2 = 190) all sign and verify.

 *   - RSA_PSS_SALTLEN_AUTO (-2) on verify recovers the actual salt length,

 *     so signatures made with any of the above verify with -2.

 *   - SHA-512 works with RSA-2048 (emLen 256 >= hLen + 2 = 66).

 *   - A salt length larger than the maximum (300 > 190) fails at sign time.

 *   - A tampered signature fails verification.

 *

 * Known-answer vector: `pizfix/bin/openssl dgst -sha256 -sign key.pem

 * -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32` over the KAT

 * message produced KAT_SIG_HEX below.  PSS signing is randomized, so the

 * deterministic check is the verify direction: our code must accept that

 * CLI-made signature.  (The reverse direction -- our signature verifying

 * under the same CLI command -- was confirmed during development and

 * requires no embedded constant, since our PSS signatures differ per run.)

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
    fprintf(stderr, "FAIL test_rsa_pss: %s\n", msg);
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


/* The fixed RSA-2048 key (same key as test_rsa_pkcs1.c). */
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


static const char KAT_MSG[] = "Fil-C RSA PKCS#1 v1.5 known-answer message";
/* CLI-made PSS signature (SHA-256, saltlen 32) over KAT_MSG. */
static const char *const KAT_SIG_HEX =
    "0012915a8cd227e3826274154dccefe92eb8c5934dfb4a12f9b692ed29bab7aa"
    "e10a3ac47328dd003d57bbf38f575d084d82aa6a7f4deb47ca11ebe974793b0a"
    "cc1ccb5606f5a9c4383d6580e1b2f81e6eb26ac6dbbbda12d124bb642aae6f50"
    "b9c5fe3b6d159d1695f178fecaa887ea171a9bf2b11470bbeb1ff4a3019c3a4e"
    "a16bf8d2fca23f07d5dd74eb9ac74eb2fef8fe269fcfed834833712496961b87"
    "fb16f338a619d4c261001a83f77321465b2fd2de8d14d777fc16593898bbdba2"
    "5f1dd0e44a890f5962fe92e814ebc2e25ae175295ded3168b6fe90c2ac9bb1ec"
    "8d3d1560074c8d2b98ee22b977bd1cddca5ab0acb3529735f22e9bbb47728fc7";


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
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}


/* Prepares a PSS sign (for_sign != 0) or verify context for `md` with the

 * given salt length.  The pad-mode/saltlen params are set after the
 * sign/verify init, as the context only accepts them once the operation is
 * bound. */
static EVP_PKEY_CTX *pss_ctx(EVP_PKEY *key, const EVP_MD *md, int saltlen,
                             int for_sign)
{
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    char pad[] = "pss";
    int sl = saltlen;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(for_sign ? EVP_PKEY_sign_init(ctx) == 1
                   : EVP_PKEY_verify_init(ctx) == 1,
          "PSS operation init failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_PAD_MODE,
                                                 pad, 0);
    params[1] = OSSL_PARAM_construct_int(OSSL_SIGNATURE_PARAM_PSS_SALTLEN, &sl);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting PSS pad-mode/saltlen params failed");
    CHECK(EVP_PKEY_CTX_set_signature_md(ctx, md) == 1,
          "EVP_PKEY_CTX_set_signature_md failed");
    return ctx;
}


/* Digests the message and PSS-signs it.  Returns the signature length. */
static size_t pss_sign(EVP_PKEY *key, const EVP_MD *md, int saltlen,
                       const unsigned char *msg, size_t msglen,
                       unsigned char *sig, size_t sigcap)
{
    unsigned char *dig = ALLOCATE_OTHER(64);
    unsigned int diglen = 0;
    EVP_PKEY_CTX *ctx;
    size_t siglen = sigcap;

    CHECK((size_t)EVP_MD_get_size(md) <= 64, "unexpected digest size");
    CHECK(EVP_Digest(msg, msglen, dig, &diglen, md, NULL) == 1,
          "EVP_Digest failed");
    ctx = pss_ctx(key, md, saltlen, 1);
    CHECK(EVP_PKEY_sign(ctx, sig, &siglen, dig, diglen) == 1,
          "PSS signing failed");
    CHECK(siglen == 256, "PSS signature is not 256 bytes");
    EVP_PKEY_CTX_free(ctx);
    return siglen;
}


/* Digests the message and verifies a PSS signature.  Returns 1 on success. */
static int pss_verify(EVP_PKEY *key, const EVP_MD *md, int saltlen,
                      const unsigned char *msg, size_t msglen,
                      const unsigned char *sig, size_t siglen)
{
    unsigned char *dig = ALLOCATE_OTHER(64);
    unsigned int diglen = 0;
    EVP_PKEY_CTX *ctx;
    int ok;

    CHECK(EVP_Digest(msg, msglen, dig, &diglen, md, NULL) == 1,
          "EVP_Digest failed");
    ctx = pss_ctx(key, md, saltlen, 0);
    ok = EVP_PKEY_verify(ctx, sig, siglen, dig, diglen) == 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* (1) saltlen DIGEST_LENGTH (-1), 20 and MAX (-3) all sign and verify; the

 * MAX saltlen is emLen - hLen - 2 = 190 for SHA-256. */
static void test_saltlen_modes(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    static const int modes[] = {RSA_PSS_SALTLEN_DIGEST, 20, RSA_PSS_SALTLEN_MAX};
    static const char *const names[] = {"digest", "20", "max"};
    size_t m, i, siglen;

    for (i = 0; i < 64; i++)
        msg[i] = (unsigned char)(i * 17 + 3);

    for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        char mbuf[128];
        snprintf(mbuf, sizeof(mbuf), "PSS saltlen %s: sign failed", names[m]);
        siglen = pss_sign(key, EVP_sha256(), modes[m], msg, 64, sig, 256);
        CHECK(siglen == 256, mbuf);
        snprintf(mbuf, sizeof(mbuf), "PSS saltlen %s: verify failed", names[m]);
        CHECK(pss_verify(key, EVP_sha256(), modes[m], msg, 64, sig, siglen) == 1,
              mbuf);
        /* And with AUTO (-2), which recovers the salt length. */
        snprintf(mbuf, sizeof(mbuf),
                 "PSS saltlen %s: verify with SALTLEN_AUTO failed", names[m]);
        CHECK(pss_verify(key, EVP_sha256(), RSA_PSS_SALTLEN_AUTO,
                         msg, 64, sig, siglen) == 1, mbuf);
    }
}


/* (2) A SHA-512 PSS signature round-trips (emLen 256 >= hLen + 2). */
static void test_sha512(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    size_t siglen;
    size_t i;

    for (i = 0; i < 64; i++)
        msg[i] = (unsigned char)(i * 29 + 7);

    siglen = pss_sign(key, EVP_sha512(), RSA_PSS_SALTLEN_DIGEST, msg, 64, sig, 256);
    CHECK(siglen == 256, "PSS SHA-512 sign failed");
    CHECK(pss_verify(key, EVP_sha512(), RSA_PSS_SALTLEN_DIGEST,
                     msg, 64, sig, siglen) == 1,
          "PSS SHA-512 verify failed");
    CHECK(pss_verify(key, EVP_sha512(), RSA_PSS_SALTLEN_AUTO,
                     msg, 64, sig, siglen) == 1,
          "PSS SHA-512 (digest saltlen) not accepted with AUTO on verify");
}


/* (3) A salt length above the maximum (emLen - hLen - 2 = 190 for SHA-256)

 * must fail at sign time. */
static void test_saltlen_too_big(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(8);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *dig = ALLOCATE_OTHER(32);
    unsigned int diglen = 0;
    EVP_PKEY_CTX *ctx;
    size_t siglen = 256;

    CHECK(EVP_Digest(msg, 8, dig, &diglen, EVP_sha256(), NULL) == 1,
          "EVP_Digest failed");
    ctx = pss_ctx(key, EVP_sha256(), 300, 1);
    CHECK(EVP_PKEY_sign(ctx, sig, &siglen, dig, diglen) != 1,
          "PSS sign accepted saltlen 300 > emLen - hLen - 2");
    EVP_PKEY_CTX_free(ctx);
}


/* (4) A tampered PSS signature must fail verification. */
static void test_tamper(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(16);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *bad = ALLOCATE_OUTPUT(256);
    size_t siglen;
    size_t i;

    for (i = 0; i < 16; i++)
        msg[i] = (unsigned char)(0x90 + i);

    siglen = pss_sign(key, EVP_sha256(), 20, msg, 16, sig, 256);
    CHECK(siglen == 256, "PSS sign for the tamper test failed");
    memcpy(bad, sig, 256);
    bad[100] ^= 0x08;
    CHECK(pss_verify(key, EVP_sha256(), 20, msg, 16, bad, 256) != 1,
          "tampered PSS signature verified");
}


/* (5) KAT: the CLI-made PSS signature (SHA-256, saltlen 32) verifies, both

 * with the exact saltlen and with AUTO. */
static void test_kat(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(sizeof(KAT_MSG) - 1);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    size_t siglen;

    memcpy(msg, KAT_MSG, sizeof(KAT_MSG) - 1);
    siglen = from_hex(KAT_SIG_HEX, sig, 256);
    CHECK(siglen == 256, "PSS KAT signature constant has the wrong length");

    CHECK(pss_verify(key, EVP_sha256(), 32, msg, sizeof(KAT_MSG) - 1,
                     sig, siglen) == 1,
          "the CLI PSS KAT signature does not verify");
    CHECK(pss_verify(key, EVP_sha256(), RSA_PSS_SALTLEN_AUTO,
                     msg, sizeof(KAT_MSG) - 1, sig, siglen) == 1,
          "the CLI PSS KAT signature does not verify with SALTLEN_AUTO");
}


int main(int argc, char **argv)
{
    EVP_PKEY *key;

    poison_parse_args(argc, argv);

    key = make_key();
    test_saltlen_modes(key);
    test_sha512(key);
    test_saltlen_too_big(key);
    test_tamper(key);
    test_kat(key);
    EVP_PKEY_free(key);

    printf("test_rsa_pss: all %u checks passed\n", g_checks);
    return 0;
}
