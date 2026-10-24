#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

/*

 * test_rsa_oaep -- RSA-2048-OAEP boundary, parameter-variation and

 * known-answer tests with the same fixed key as test_rsa_pkcs1 (generated

 * once with pizfix/bin/openssl genrsa 2048 and embedded as hex constants;

 * OSSL_PARAM integers are native-endian, so the big-endian constants are

 * byte-swapped into the working buffers).

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - OAEP message-size boundary: emlen - 2*hLen - 2.  With SHA-1

 *     (hLen = 20): 256 - 40 - 2 = 214 bytes OK, 215 fails.  With SHA-256

 *     (hLen = 32): 256 - 64 - 2 = 190 bytes OK, 191 fails.

 *   - The MGF1 digest may differ from the OAEP digest (mgf1-digest param):

 *     OAEP-SHA1 with MGF1-SHA256 round-trips.

 *   - An OAEP label (oaep-label param) participates in the round-trip;

 *     decrypting with the wrong label fails.

 *   - Tampering with an OAEP ciphertext makes the decrypt fail outright

 *     (OAEP has a hard padding-error failure, unlike PKCS#1 type 2).

 *

 * Known-answer vector: `pizfix/bin/openssl pkeyutl -encrypt -pubin -inkey

 * pub.pem -pkeyopt rsa_padding_mode:oaep -pkeyopt rsa_oaep_md:sha256

 * -pkeyopt rsa_mgf1_md:sha256` over the 16-byte message below produced

 * KAT_CT_HEX.  OAEP encryption is randomized, so the deterministic check is

 * the decrypt direction: our decrypt of the CLI ciphertext must return the

 * message exactly.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rsa_oaep: %s\n", msg);
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


/* Same, but byte-reversed: OSSL_PARAM integers are native-endian
 * (OSSL_PARAM(3)); see test_rsa_pkcs1.c. */
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


/* The OAEP known-answer vector: ciphertext of "0123456789abcdef" under
 * OAEP-SHA256 with MGF1-SHA256 and an empty label, produced by the CLI. */
static const char KAT_MSG_HEX[] = "30313233343536373839616263646566";
static const char *const KAT_CT_HEX =
    "92e4d2aade4097ddd0718fddf687aba09430c42424b7ba7a60a4af7a5e6e2e1d"
    "62c6b23b9e79e1287b4a91de374a60fb03c4268ec8eed9dfbc5164a15c7c3c63"
    "e943ad7f70eb8b4c93323113458a06169c034f15dc9023224c4e473196b7b9fe"
    "1304881648278564ae7eeabaa53f83a27570c72b2e9722a7bc6a05d616b7cc0c"
    "305b4e656f46e53f5f72c3b8b4dad83fae1c0e561e663ec5bf344d853f8f8ae6"
    "5b23db9fe07fdcb40c21c4029650221c89961df9c17c18592f1d528838d5033f"
    "96ae09007be7f8b6297cfbbe8f36b547bc08bc3d18f0c471be925000d2076526"
    "24111526fdfc503a203e3d09f7dde101d7cce4de14432040a84d7d992548581c";


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


/* One-shot OAEP encrypt with the given OAEP digest, MGF1 digest and label.
 * Any of the three may be NULL for the OpenSSL default.  Returns 1 on
 * success (storing the ciphertext length, always a full 256-byte modulus
 * block) and 0 if the encryption itself failed.  Parameter setup failures
 * are fatal. */
static int oaep_encrypt(EVP_PKEY *key, const char *oaepmd, const char *mgf1md,
                        const unsigned char *label, size_t labellen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t *outlen)
{
    OSSL_PARAM params[4];
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    char pad[] = "oaep";
    char md1[32], md2[32];
    size_t blen = 256;
    int ok;
    size_t i = 0;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_encrypt_init(ctx) == 1, "EVP_PKEY_encrypt_init failed");
    params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_PAD_MODE,
                                                   pad, 0);
    if (oaepmd != NULL) {
        snprintf(md1, sizeof(md1), "%s", oaepmd);
        params[i++] = OSSL_PARAM_construct_utf8_string(
            OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST, md1, 0);
    }
    if (mgf1md != NULL) {
        snprintf(md2, sizeof(md2), "%s", mgf1md);
        params[i++] = OSSL_PARAM_construct_utf8_string(
            OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST, md2, 0);
    }
    if (label != NULL)
        params[i++] = OSSL_PARAM_construct_octet_string(
            OSSL_ASYM_CIPHER_PARAM_OAEP_LABEL, (void *)label, labellen);
    params[i] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting OAEP encryption params failed");
    ok = EVP_PKEY_encrypt(ctx, out, &blen, in, inlen) == 1;
    if (ok)
        CHECK(blen == 256, "OAEP ciphertext is not a full modulus block");
    *outlen = blen;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* One-shot OAEP decrypt; mirror of oaep_encrypt().  Returns 1 on success
 * and stores the plaintext length. */
static int oaep_decrypt(EVP_PKEY *key, const char *oaepmd, const char *mgf1md,
                        const unsigned char *label, size_t labellen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t *outlen)
{
    OSSL_PARAM params[4];
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    char pad[] = "oaep";
    char md1[32], md2[32];
    size_t blen = 256;
    int ok;
    size_t i = 0;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_decrypt_init(ctx) == 1, "EVP_PKEY_decrypt_init failed");
    params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_PAD_MODE,
                                                   pad, 0);
    if (oaepmd != NULL) {
        snprintf(md1, sizeof(md1), "%s", oaepmd);
        params[i++] = OSSL_PARAM_construct_utf8_string(
            OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST, md1, 0);
    }
    if (mgf1md != NULL) {
        snprintf(md2, sizeof(md2), "%s", mgf1md);
        params[i++] = OSSL_PARAM_construct_utf8_string(
            OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST, md2, 0);
    }
    if (label != NULL)
        params[i++] = OSSL_PARAM_construct_octet_string(
            OSSL_ASYM_CIPHER_PARAM_OAEP_LABEL, (void *)label, labellen);
    params[i] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting OAEP decryption params failed");
    ok = EVP_PKEY_decrypt(ctx, out, &blen, in, inlen) == 1;
    *outlen = blen;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* (1) Message-size boundaries: OAEP-SHA1 accepts emlen-2*20-2 = 214 bytes

 * and rejects 215; OAEP-SHA256 accepts 190 and rejects 191. */
static void test_size_boundaries(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(256);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    size_t i;

    for (i = 0; i < 256; i++)
        msg[i] = (unsigned char)(i * 5 + 1);

    size_t ctlen = 0;

    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 214, ct, &ctlen) == 1 &&
          ctlen == 256,
          "OAEP-SHA1 encryption of 214 bytes failed");
    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 215, ct, &ctlen) == 0,
          "OAEP-SHA1 encryption accepted 215 bytes (max is emlen-2*hLen-2)");

    CHECK(oaep_encrypt(key, "SHA256", NULL, NULL, 0, msg, 190, ct, &ctlen) == 1 &&
          ctlen == 256,
          "OAEP-SHA256 encryption of 190 bytes failed");
    CHECK(oaep_encrypt(key, "SHA256", NULL, NULL, 0, msg, 191, ct, &ctlen) == 0,
          "OAEP-SHA256 encryption accepted 191 bytes (max is emlen-2*hLen-2)");
}


/* (2) MGF1 digest variation: OAEP-SHA1 with MGF1-SHA256 round-trips, and

 * the same message under plain OAEP-SHA1 (MGF1-SHA1) produces a different

 * ciphertext (the MGF digest is mixed into the mask). */
static void test_mgf1_variation(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    unsigned char *ct_a = ALLOCATE_OUTPUT(256);
    unsigned char *ct_b = ALLOCATE_OUTPUT(256);
    size_t backlen = 0, a, b;
    size_t i;

    for (i = 0; i < 64; i++)
        msg[i] = (unsigned char)(i * 3 + 11);

    CHECK(oaep_encrypt(key, "SHA1", "SHA256", NULL, 0, msg, 64, ct_a, &a) == 1 &&
          a == 256,
          "OAEP-SHA1/MGF1-SHA256 encryption failed");
    CHECK(oaep_decrypt(key, "SHA1", "SHA256", NULL, 0, ct_a, a, back, &backlen) == 1,
          "OAEP-SHA1/MGF1-SHA256 decryption failed");
    CHECK(backlen == 64 && memcmp(back, msg, 64) == 0,
          "OAEP-SHA1/MGF1-SHA256 roundtrip did not recover the message");

    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 64, ct_b, &b) == 1 &&
          b == 256 && memcmp(ct_a, ct_b, 256) != 0,
          "MGF1-SHA256 ciphertext equals the MGF1-SHA1 ciphertext");

    /* Decrypting the MGF1-SHA256 ciphertext as MGF1-SHA1 must fail. */
    CHECK(oaep_decrypt(key, "SHA1", NULL, NULL, 0, ct_a, a, back, &backlen) == 0,
          "OAEP decrypt with the wrong MGF1 digest succeeded");
}


/* (3) Labels: an empty label (the default) and a non-empty label both

 * round-trip; a label mismatch fails; the KAT decrypts. */
static void test_labels_and_kat(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(32);
    unsigned char *label = ALLOCATE_AAD(6);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    unsigned char *katmsg = ALLOCATE_INPUT(16);
    unsigned char *katct = ALLOCATE_OUTPUT(256);
    unsigned char *katback = ALLOCATE_OUTPUT(256);
    size_t backlen = 0, katbacklen = 0;
    size_t i, katmsglen;

    for (i = 0; i < 32; i++)
        msg[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < 6; i++)
        label[i] = (unsigned char)(0xC0 + i);

    /* Non-empty label round-trip. */
    CHECK(oaep_encrypt(key, "SHA1", NULL, label, 6, msg, 32, ct, &i) == 1 &&
          i == 256,
          "OAEP encryption with a label failed");
    CHECK(oaep_decrypt(key, "SHA1", NULL, label, 6, ct, i, back, &backlen) == 1,
          "OAEP decryption with the same label failed");
    CHECK(backlen == 32 && memcmp(back, msg, 32) == 0,
          "labeled OAEP roundtrip did not recover the message");

    /* Wrong label must fail. */
    CHECK(oaep_decrypt(key, "SHA1", NULL, NULL, 0, ct, i, back, &backlen) == 0,
          "OAEP decryption with an empty label accepted a labeled ciphertext");

    /* Empty-label (default) round-trip. */
    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 32, ct, &i) == 1 &&
          i == 256,
          "OAEP encryption with an empty label failed");
    CHECK(oaep_decrypt(key, "SHA1", NULL, NULL, 0, ct, i, back, &backlen) == 1,
          "OAEP decryption with an empty label failed");
    CHECK(backlen == 32 && memcmp(back, msg, 32) == 0,
          "unlabeled OAEP roundtrip did not recover the message");

    /* KAT: the CLI-produced ciphertext decrypts to the fixed message. */
    katmsglen = from_hex(KAT_MSG_HEX, katmsg, 16);
    CHECK(katmsglen == 16, "KAT message constant has the wrong length");
    CHECK(from_hex(KAT_CT_HEX, katct, 256) == 256,
          "KAT ciphertext constant has the wrong length");
    CHECK(oaep_decrypt(key, "SHA256", "SHA256", NULL, 0,
                       katct, 256, katback, &katbacklen) == 1,
          "decrypting the CLI OAEP KAT ciphertext failed");
    CHECK(katbacklen == 16 && memcmp(katback, katmsg, 16) == 0,
          "OAEP KAT plaintext mismatch");
}


/* (4) Tampering with an OAEP ciphertext must make the decrypt fail. */
static void test_tamper(EVP_PKEY *key)
{
    unsigned char *msg = ALLOCATE_INPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    size_t ctlen, backlen = 0;
    size_t i;

    for (i = 0; i < 16; i++)
        msg[i] = (unsigned char)(0x50 + i);
    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 16, ct, &ctlen) == 1 &&
          ctlen == 256,
          "OAEP encryption of the tamper-test message failed");

    ct[7] ^= 0x20;
    CHECK(oaep_decrypt(key, "SHA1", NULL, NULL, 0, ct, ctlen, back, &backlen) == 0,
          "tampered OAEP ciphertext decrypted without error");

    /* Also tamper with a high byte (the encoded leading 0x00 byte). */
    CHECK(oaep_encrypt(key, "SHA1", NULL, NULL, 0, msg, 16, ct, &ctlen) == 1,
          "OAEP encryption of the tamper-test message failed");
    ct[0] ^= 0x01;
    CHECK(oaep_decrypt(key, "SHA1", NULL, NULL, 0, ct, ctlen, back, &backlen) == 0,
          "tampered first ciphertext byte decrypted without error");
}


int main(int argc, char **argv)
{
    EVP_PKEY *key;

    poison_parse_args(argc, argv);

    key = make_key();
    test_size_boundaries(key);
    test_mgf1_variation(key);
    test_labels_and_kat(key);
    test_tamper(key);
    EVP_PKEY_free(key);

    printf("test_rsa_oaep: all %u checks passed\n", g_checks);
    return 0;
}
