#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

/*

 * test_rsa_raw -- RSA_NO_PADDING ("textbook RSA") with the same fixed key

 * as test_rsa_pkcs1.c, plus the RSA-KEM (RSASVE) encapsulation added in

 * OpenSSL 3.2.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - With RSA_NO_PADDING the plaintext/ciphertext block is exactly the

 *     modulus length (256 bytes).  A 256-byte block whose first 64 bytes

 *     are zero (i.e. a value well below n) round-trips and the decrypt

 *     returns ALL 256 bytes, including the leading zeros.

 *   - A block shorter than the modulus is rejected (padding "none" requires

 *     flen == RSA_size).

 *   - A 256-byte block >= n (all 0xFF bytes) is rejected with

 *     "data too large for the modulus".

 *   - The padding can be selected both through the OSSL_PKEY_PARAM_PAD_MODE

 *     parameter ("none") and through EVP_PKEY_CTX_set_rsa_padding with

 *     RSA_NO_PADDING.

 *   - RSASVE (RSA as a KEM: shared secret = RSA-PKCS1-encrypt of a random

 *     32-byte seed) is available from OpenSSL 3.2; on older builds the

 *     subtest is skipped with a note (it is the only SKIP-able feature

 *     here -- everything else exists in all OpenSSL 3.x).

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rsa_raw: %s\n", msg);
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


/* Sets RSA_NO_PADDING via the OSSL_PKEY_PARAM_PAD_MODE parameter ("none"). */
static void set_pad_mode_param(EVP_PKEY_CTX *ctx)
{
    OSSL_PARAM params[2];
    char none[] = "none";

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_PAD_MODE,
                                                 none, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting pad-mode \"none\" through OSSL_PARAMs failed");
}


/* (1) Round-trip of a full 256-byte block with 64 leading zeros, selecting

 * the padding through the OSSL_PKEY_PARAM_PAD_MODE parameter.  The decrypt

 * must return all 256 bytes including the leading zeros, byte-exact. */
static void test_roundtrip_pad_mode_param(EVP_PKEY *key)
{
    unsigned char *pt = ALLOCATE_INPUT(256);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(key, NULL);
    EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(key, NULL);
    size_t ctlen = 256, backlen = 256;
    size_t i;

    memset(pt, 0, 64);
    for (i = 64; i < 256; i++)
        pt[i] = (unsigned char)(i * 7 + 1);

    CHECK(ectx != NULL && dctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
    set_pad_mode_param(ectx);
    CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, pt, 256) == 1,
          "no-padding encryption of a 256-byte block failed");
    CHECK(ctlen == 256, "no-padding ciphertext is not 256 bytes");

    CHECK(EVP_PKEY_decrypt_init(dctx) == 1, "EVP_PKEY_decrypt_init failed");
    set_pad_mode_param(dctx);
    CHECK(EVP_PKEY_decrypt(dctx, back, &backlen, ct, ctlen) == 1,
          "no-padding decryption failed");
    CHECK(backlen == 256, "no-padding decrypt did not return all 256 bytes "
                          "(leading zeros must be preserved)");
    CHECK(memcmp(back, pt, 256) == 0,
          "no-padding roundtrip did not recover the block byte-exactly");
    EVP_PKEY_CTX_free(ectx);
    EVP_PKEY_CTX_free(dctx);
}


/* (2) The same round-trip with EVP_PKEY_CTX_set_rsa_padding(RSA_NO_PADDING). */
static void test_roundtrip_set_rsa_padding(EVP_PKEY *key)
{
    unsigned char *pt = ALLOCATE_INPUT(256);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    unsigned char *back = ALLOCATE_OUTPUT(256);
    EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(key, NULL);
    EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(key, NULL);
    size_t ctlen = 256, backlen = 256;
    size_t i;

    memset(pt, 0, 64);
    for (i = 64; i < 256; i++)
        pt[i] = (unsigned char)(0xF0 - i);

    CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
    CHECK(EVP_PKEY_CTX_set_rsa_padding(ectx, RSA_NO_PADDING) == 1,
          "EVP_PKEY_CTX_set_rsa_padding(RSA_NO_PADDING) failed");
    CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, pt, 256) == 1,
          "no-padding encryption (set_rsa_padding route) failed");
    CHECK(EVP_PKEY_decrypt_init(dctx) == 1, "EVP_PKEY_decrypt_init failed");
    CHECK(EVP_PKEY_CTX_set_rsa_padding(dctx, RSA_NO_PADDING) == 1,
          "EVP_PKEY_CTX_set_rsa_padding(RSA_NO_PADDING) failed");
    CHECK(EVP_PKEY_decrypt(dctx, back, &backlen, ct, ctlen) == 1,
          "no-padding decryption (set_rsa_padding route) failed");
    CHECK(backlen == 256 && memcmp(back, pt, 256) == 0,
          "no-padding roundtrip (set_rsa_padding route) failed");
    EVP_PKEY_CTX_free(ectx);
    EVP_PKEY_CTX_free(dctx);
}


/* (3) A block shorter than the modulus and a block >= n must both be

 * rejected. */
static void test_bad_lengths(EVP_PKEY *key)
{
    unsigned char *pt = ALLOCATE_INPUT(256);
    unsigned char *ct = ALLOCATE_OUTPUT(256);
    EVP_PKEY_CTX *ectx;
    size_t ctlen = 256;
    size_t i;

    for (i = 0; i < 256; i++)
        pt[i] = (unsigned char)(i + 1);

    /* 255 bytes: RSA_padding_add_none requires flen == RSA_size. */
    ectx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
    set_pad_mode_param(ectx);
    CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, pt, 255) != 1,
          "no-padding encryption accepted a block shorter than the modulus");
    EVP_PKEY_CTX_free(ectx);

    /* 256 bytes of 0xFF is >= n: "data too large for the modulus". */
    memset(pt, 0xFF, 256);
    ectx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(EVP_PKEY_encrypt_init(ectx) == 1, "EVP_PKEY_encrypt_init failed");
    set_pad_mode_param(ectx);
    CHECK(EVP_PKEY_encrypt(ectx, ct, &ctlen, pt, 256) != 1,
          "no-padding encryption accepted a block >= n");
    EVP_PKEY_CTX_free(ectx);
}


/* (4) RSASVE: RSA as a KEM (OpenSSL 3.2+), per NIST SP 800-56Br2 7.2.1.

 * Encapsulation generates a random z with 1 < z < n-1 (nlen = 256 bytes),

 * ciphertext = RSAEP(z), and the shared secret IS z (nlen bytes).

 * Decapsulation with the private key recovers z.  Skipped with a note on

 * OpenSSL 3.0/3.1. */
static void test_rsasve_kem(EVP_PKEY *key)
{
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    EVP_PKEY_CTX *dctx;
    unsigned char *ct, *ss, *ss2;
    size_t ctlen = 0, sslen = 0, ss2len = 0;

    CHECK(kctx != NULL, "EVP_PKEY_CTX_new_from_pkey failed");
    if (kctx == NULL || EVP_PKEY_encapsulate_init(kctx, NULL) != 1 ||
        EVP_PKEY_encapsulate(kctx, NULL, &ctlen, NULL, &sslen) != 1) {
        /* OpenSSL 3.0/3.1 accept the init call but have no RSASVE KEM. */
        printf("SKIP test_rsa_raw: RSASVE (RSA-KEM) is unavailable\n");
        EVP_PKEY_CTX_free(kctx);
        return;
    }
    CHECK(ctlen == 256, "RSASVE ciphertext length is not the modulus length");
    CHECK(sslen == 256, "RSASVE shared secret is not nlen bytes");
    ct = ALLOCATE_OUTPUT(ctlen);
    ss = ALLOCATE_OUTPUT(sslen);
    CHECK(EVP_PKEY_encapsulate(kctx, ct, &ctlen, ss, &sslen) == 1,
          "RSASVE encapsulation failed");
    EVP_PKEY_CTX_free(kctx);

    dctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(dctx != NULL && EVP_PKEY_decapsulate_init(dctx, NULL) == 1,
          "RSASVE decapsulate_init failed");
    CHECK(EVP_PKEY_decapsulate(dctx, NULL, &ss2len, ct, ctlen) == 1,
          "RSASVE decapsulate size query failed");
    CHECK(ss2len == 256, "RSASVE decapsulate secret length is not nlen");
    ss2 = ALLOCATE_OUTPUT(ss2len);
    CHECK(EVP_PKEY_decapsulate(dctx, ss2, &ss2len, ct, ctlen) == 1,
          "RSASVE decapsulation failed");
    CHECK(ss2len == sslen && memcmp(ss2, ss, sslen) == 0,
          "RSASVE decapsulation did not recover the shared secret");
    EVP_PKEY_CTX_free(dctx);
}


int main(int argc, char **argv)
{
    EVP_PKEY *key;

    poison_parse_args(argc, argv);

    key = make_key();
    test_roundtrip_pad_mode_param(key);
    test_roundtrip_set_rsa_padding(key);
    test_bad_lengths(key);
    test_rsasve_kem(key);
    EVP_PKEY_free(key);

    printf("test_rsa_raw: all %u checks passed\n", g_checks);
    return 0;
}
