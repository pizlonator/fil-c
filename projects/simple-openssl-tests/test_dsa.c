#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_dsa -- DSA signing/verification with SHA-1 and SHA-256, over

 * freshly generated 2048/256 parameters and keys, plus a fixed-key

 * CLI-cross-checked known-answer verification.

 *

 * Key generation: 2048-bit parameters with a 256-bit subgroup are generated

 * with EVP_PKEY_paramgen/EVP_PKEY_keygen (a fraction of a second); all

 * later checks use the generated key, so the KAT below deliberately uses a

 * separately embedded fixed key instead.

 *

 * Known-answer vector: a fixed 2048-bit DSA key was generated once with

 * `pizfix/bin/openssl genpkey -genparam -algorithm DSA` + `genpkey

 * -paramfile`, and `openssl dgst -sha256 -sign` produced the embedded

 * signature over the embedded message.  The test imports the key via

 * EVP_PKEY_fromdata (p, q, g, priv, pub) and must accept that CLI-made

 * signature -- the deterministic verify-direction cross-check (DSA

 * signatures themselves are randomized, so the sign direction cannot be a

 * byte-exact KAT).

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - One-shot and streaming (Update + Final) signing both verify; DSA's k

 *     is randomized, so the two DER signatures differ (documented).

 *   - A tampered signature fails verification.

 *   - A SHA-256 signature does not verify as SHA-1.

 *   - The DER signature length varies within DSA_size() bounds.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_dsa: %s\n", msg);
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


/* The fixed DSA key (from the pizfix CLI, see header comment). */
static const char *const DSA_P_HEX =
    "9f4eb3ff9b44a2bab36f4d5de1279efe89c151a7a39456a6d1d47b77d41352fb"
    "07a543bc9ef9f6f62037fc6a8ccec41a94364ac6b3f36fe59cf7cfd8f2c7d9e2"
    "3a048c3b4c6719bc9620bde02c3d1d904b60ce78c6aa52876d013070980b4870"
    "f75e6fb68feb04c5f2f4a5c64b73ab42c65686678905ce52ec594b3923cc42dc"
    "6df858fa47ef0177730bee890d4c8e374a80d9ca38f2db88388714034d7a134d"
    "e2dacf18401323531bc1fd6fa713fb4eb31976f40fc7b2caf8f812e779505471"
    "805a33bc8ce123d05619722c0c244f2a604726c56a4f687da08e7b1a9438227c"
    "eb2fc3be2aab7d2f5e72484f165f86d0aefbc7d0c28e325ccfbfaa95b58e5e81";
static const char *const DSA_Q_HEX =
    "ad7bd5877074b80d5dc2df38260cf82224910800df308afc1127e5f07457cfef";
static const char *const DSA_G_HEX =
    "4f90be8229255d83602eabb03d94466cd063fc6659994fc49ef2d15af183916d"
    "dc7a3931ca850928f32591dd3e16792502dea426edd1caefb1b62067c281a20f"
    "5bac7921fe5c0e88e0f618eb1e7835e3f44f6b8eeac15286278d080740dce2ad"
    "e9ce0f4f3f405f161a2756a37f4b62e03db8811b93f0e24a7783b12c2b0ee46b"
    "6ed3148460173c4c14c8172de36d40b71f63a5c47bd671b26bde9ad542bedb9e"
    "e5f5abde0550807c0b93831c44cd49687b8052e59049833bda0d7b2a4e761164"
    "8507ee74e2464cbc8f60ea28a780d74a62ffda7295cfb8fa1cf7a849c5344a8c"
    "105df830a7c13682774912a2da001b20a64deac8313c23c6a5ad3926623bc26c";
static const char *const DSA_PRIV_HEX =
    "6afc3d8327261509222e1bdf556ce567d8cb3a53239d069dcaa604d75a53e73a";
static const char *const DSA_PUB_HEX =
    "80cd758e8ffcb1340e106d0a1b66b1c0053c059b0934bb02659ce450d13c4bb1"
    "aef025adbba702f27f068cd7c88a7e62b6e5a3d8520c8b4651e309f390ba996d"
    "2d118ba4a8f3ecee801f48d0212aba47e45cca832598918ba38c5a8e31ae5dce"
    "6ac10aa8db92b12476bc9300f044305eb0a9df5c8f8c3ca8fbc04ea9ad644937"
    "525e9a9b36f091bc349bc61f319e6f142a1e70a22062a532f9430373bd630a1b"
    "51569652d44b2cb0806721e6731dfe8e6cdda6acb6b2d33c1773d841e2146438"
    "5832515155f79eccdfa1919523ccb39b0fe07622cf32b9b410afbd03dcfae14b"
    "5f26db66abdd66345488d7579a5c0c8cfe88a38853063570c64b6e3736e0487b";
/* `openssl dgst -sha256 -sign` over KAT_MSG with the fixed key. */
static const char KAT_MSG[] = "DSA fixed-key known-answer message";
static const char *const KAT_SIG_HEX =
    "304402203a958e718d56f46e23e4c32f52fba1f49fcc99a99282725a570bb394"
    "ce2ac3a7022060fcdc6be9b5275f4432714d0e1d0df5afd644afe375e930a5a5"
    "df0973e171d1";


/* Generates fresh 2048/256 DSA parameters and a key. */
static EVP_PKEY *gen_key(void)
{
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
    EVP_PKEY *paramskey = NULL, *key = NULL;
    size_t pbits = 2048, qbits = 256;
    size_t i = 0;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(DSA) failed");
    CHECK(EVP_PKEY_paramgen_init(ctx) == 1, "EVP_PKEY_paramgen_init failed");
    params[i++] = OSSL_PARAM_construct_size_t(OSSL_PKEY_PARAM_FFC_PBITS,
                                              &pbits);
    params[i++] = OSSL_PARAM_construct_size_t(OSSL_PKEY_PARAM_FFC_QBITS,
                                              &qbits);
    params[i] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting DSA paramgen sizes failed");
    CHECK(EVP_PKEY_generate(ctx, &paramskey) == 1,
          "DSA parameter generation failed");
    EVP_PKEY_CTX_free(ctx);

    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, paramskey, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_pkey failed");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    CHECK(EVP_PKEY_generate(ctx, &key) == 1, "DSA key generation failed");
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(paramskey);
    return key;
}


/* Imports the fixed DSA key through EVP_PKEY_fromdata. */
static EVP_PKEY *make_fixed_key(void)
{
    static unsigned char *p, *q, *g, *priv, *pub;
    OSSL_PARAM params[6];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    p = ALLOCATE_KEY(256);
    q = ALLOCATE_KEY(32);
    g = ALLOCATE_KEY(256);
    priv = ALLOCATE_KEY(32);
    pub = ALLOCATE_KEY(256);
    from_hex_le(DSA_P_HEX, p, 256);
    from_hex_le(DSA_Q_HEX, q, 32);
    from_hex_le(DSA_G_HEX, g, 256);
    from_hex_le(DSA_PRIV_HEX, priv, 32);
    from_hex_le(DSA_PUB_HEX, pub, 256);

    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_P, p, 256);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_Q, q, 32);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_G, g, 256);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY, priv, 32);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PUB_KEY, pub, 256);
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(DSA) failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the fixed DSA key");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* (1) Fresh key: one-shot and streaming sign/verify with SHA-256 and

 * SHA-1, DER bounds. */
static void test_generated_key(void)
{
    EVP_PKEY *key = gen_key();
    const char *msg = "DSA test message for the generated key";
    size_t msglen = strlen(msg);
    unsigned char *in = ALLOCATE_INPUT(msglen);
    unsigned char *sig = ALLOCATE_OUTPUT(300);
    unsigned char *sig2 = ALLOCATE_OUTPUT(300);
    size_t siglen = 300, sig2len = 300;
    int maxsig;
    EVP_MD_CTX *mctx;

    memcpy(in, msg, msglen);
    maxsig = EVP_PKEY_get_size(key);
    CHECK(maxsig > 0 && maxsig <= 300, "DSA_size is out of range");

    mctx = EVP_MD_CTX_new();
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, NULL, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestSignInit(SHA-256) failed");
    CHECK(EVP_DigestSign(mctx, sig, &siglen, in, msglen) == 1,
          "DSA one-shot sign failed");
    CHECK(siglen > 0 && (size_t)maxsig >= siglen,
          "DSA signature length out of DSA_size bounds");
    CHECK(EVP_DigestVerifyInit(mctx, NULL, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestVerifyInit(SHA-256) failed");
    CHECK(EVP_DigestVerify(mctx, sig, siglen, in, msglen) == 1,
          "DSA one-shot signature failed to verify");

    /* Streaming: Update + Final.  DSA randomizes k, so the DER bytes are
     * not expected to equal the one-shot signature (documented); what is
     * asserted is that the streaming signature verifies. */
    CHECK(EVP_DigestSignInit(mctx, NULL, EVP_sha1(), NULL, key) == 1,
          "EVP_DigestSignInit(SHA-1) failed");
    CHECK(EVP_DigestSignUpdate(mctx, in, 7) == 1,
          "EVP_DigestSignUpdate failed");
    CHECK(EVP_DigestSignUpdate(mctx, in + 7, msglen - 7) == 1,
          "EVP_DigestSignUpdate (tail) failed");
    CHECK(EVP_DigestSignFinal(mctx, sig2, &sig2len) == 1,
          "EVP_DigestSignFinal failed");
    CHECK(sig2len > 0 && (size_t)maxsig >= sig2len,
          "streaming DSA signature length out of bounds");
    CHECK(EVP_DigestVerifyInit(mctx, NULL, EVP_sha1(), NULL, key) == 1,
          "EVP_DigestVerifyInit(SHA-1) failed");
    CHECK(EVP_DigestVerify(mctx, sig2, sig2len, in, msglen) == 1,
          "streaming DSA signature failed to verify");
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(key);
}


/* (2) The fixed-key KAT: the CLI-made SHA-256 signature verifies. */
static void test_kat(void)
{
    EVP_PKEY *key = make_fixed_key();
    unsigned char *msg = ALLOCATE_INPUT(sizeof(KAT_MSG) - 1);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    size_t siglen;

    memcpy(msg, KAT_MSG, sizeof(KAT_MSG) - 1);
    siglen = from_hex(KAT_SIG_HEX, sig, 256);
    CHECK(siglen == 70 || siglen == 71 || siglen == 72,
          "KAT signature constant has an implausible DER length");
    CHECK(EVP_DigestVerifyInit(mctx, NULL, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestVerifyInit failed for the fixed DSA key");
    CHECK(EVP_DigestVerify(mctx, sig, siglen, msg, sizeof(KAT_MSG) - 1) == 1,
          "the CLI-made DSA KAT signature does not verify");
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(key);
}


/* (3) Tampered signature and wrong-digest verification must fail. */
static void test_negative(void)
{
    EVP_PKEY *key = gen_key();
    const char *msg = "DSA negative-test message";
    size_t msglen = strlen(msg);
    unsigned char *in = ALLOCATE_INPUT(msglen);
    unsigned char *sig = ALLOCATE_OUTPUT(300);
    size_t siglen = 300;
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();

    memcpy(in, msg, msglen);
    CHECK(EVP_DigestSignInit(mctx, NULL, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestSignInit failed");
    CHECK(EVP_DigestSign(mctx, sig, &siglen, in, msglen) == 1,
          "DSA sign for the negative tests failed");

    sig[5] ^= 0x01;
    CHECK(EVP_DigestVerifyInit(mctx, NULL, EVP_sha256(), NULL, key) == 1,
          "EVP_DigestVerifyInit failed");
    CHECK(EVP_DigestVerify(mctx, sig, siglen, in, msglen) != 1,
          "tampered DSA signature verified");
    sig[5] ^= 0x01;

    CHECK(EVP_DigestVerifyInit(mctx, NULL, EVP_sha1(), NULL, key) == 1,
          "EVP_DigestVerifyInit(SHA-1) failed");
    CHECK(EVP_DigestVerify(mctx, sig, siglen, in, msglen) != 1,
          "SHA-256 DSA signature verified as SHA-1");
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(key);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_generated_key();
    test_kat();
    test_negative();

    printf("test_dsa: all %u checks passed\n", g_checks);
    return 0;
}
