#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_sm2 -- the Chinese national-curve SM2 algorithms: key generation on

 * the SM2 curve, SM2 signatures (SM3 digest, Z-value from the disting-

 * uishing ID) and SM2 public-key encryption.

 *

 * Key generation: a fresh key is generated per run with EVP_PKEY_keygen

 * over the "SM2" curve, so the KATs here are behavioral rather than

 * byte-exact (SM2 signing and encryption are both randomized).

 *

 * Distinguishing ID (distid): OpenSSL 3 computes the Z-value from the ID

 * set on the signature context via the "distid" OSSL_PARAM.  Verified

 * empirically on OpenSSL 3.0.13 and 3.6.5 (and against the CLI, see

 * below):

 *

 *   - With no ID set, the Z-value is computed over an EMPTY ID: OpenSSL 3

 *     does NOT substitute the GM/T 0003 default ID "1234567812345678"

 *     (sm2_sig.c leaves ctx->id NULL until an app sets "distid"; a

 *     signature made with the default ID does NOT verify under the

 *     explicit ASCII "1234567812345678", which is asserted below).  The

 *     CLI cross-check agrees: our default-ID signature verifies with

 *     `openssl pkeyutl -verify -rawin` with no ID option.

 *   - A signature made with a custom ID verifies only with that same ID

 *     and fails under the default ID (and vice versa).

 *   - The "distid" parameter is not accepted at EVP_DigestSignInit_ex time

 *     on OpenSSL 3.0 (it works from 3.2 on), so it is set after init via

 *     EVP_PKEY_CTX_set_params, which works on both.

 *

 * Encryption: SM2 ciphertext is a DER SEQUENCE {x, y, hash, cipher} in

 * C1C3C2 order.  Message sizes {1,127,255,1024} round-trip; a 0-byte

 * message FAILS to encrypt on both target versions (asserted and

 * documented); a tampered ciphertext fails to decrypt.  There is no

 * "encrypt-param" (C1C2C3/C1C3C2) setting anywhere in OpenSSL 3's SM2

 * implementation -- crypto/sm2/sm2_crypt.c only ever produces C1C3C2 --

 * so the documented alternative exercised here is the explicit SM3

 * "digest" parameter on the encryption context.

 *

 * CLI cross-checks performed during development (PEM files under /tmp; the

 * suite's programs do no file I/O): our SM2 public key + signature verify

 * with `openssl pkeyutl -verify -pubin -inkey pub.pem -sigfile sig.bin

 * -rawin -in msg.bin` (default empty ID) and, for the custom ID, with the

 * additional `-pkeyopt distid:filc-test-id`; a CLI-made signature verifies

 * in our code.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_sm2: %s\n", msg);
    exit(1);
}


#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)


/* Generates a fresh SM2 key pair. */
static EVP_PKEY *gen_key(void)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "SM2", NULL);
    OSSL_PARAM params[2];
    char curve[] = "SM2";
    EVP_PKEY *key = NULL;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(SM2) failed");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 curve, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the SM2 group name failed");
    CHECK(EVP_PKEY_generate(ctx, &key) == 1, "SM2 key generation failed");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* Signs with SM3 and the given distid (NULL = default/empty ID).  The

 * distid is set after init via OSSL_PARAMs, the route that works on both

 * OpenSSL 3.0 and 3.2+. */
static int sm2_sign(EVP_PKEY *key, const unsigned char *distid,
                    size_t distidlen, const unsigned char *msg,
                    size_t msglen, unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = NULL;
    int ok;

    *siglen = 200;
    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit_ex(mctx, &pctx, "SM3", NULL, NULL, key, NULL) == 1,
          "EVP_DigestSignInit_ex(SM3) failed");
    if (distid != NULL) {
        OSSL_PARAM params[2];
        static unsigned char *idbuf;
        idbuf = ALLOCATE_KEY(distidlen);
        memcpy(idbuf, distid, distidlen);
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_DIST_ID,
                                                      idbuf, distidlen);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the SM2 distid failed");
    }
    ok = EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}


/* Verifies with SM3 and the given distid (NULL = default/empty ID). */
static int sm2_verify(EVP_PKEY *key, const unsigned char *distid,
                      size_t distidlen, const unsigned char *msg,
                      size_t msglen, const unsigned char *sig, size_t siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = NULL;
    int ok;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit_ex(mctx, &pctx, "SM3", NULL, NULL, key,
                                  NULL) == 1,
          "EVP_DigestVerifyInit_ex(SM3) failed");
    if (distid != NULL) {
        OSSL_PARAM params[2];
        static unsigned char *idbuf;
        idbuf = ALLOCATE_KEY(distidlen);
        memcpy(idbuf, distid, distidlen);
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_DIST_ID,
                                                      idbuf, distidlen);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the SM2 distid failed");
    }
    ok = EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1;
    EVP_MD_CTX_free(mctx);
    return ok;
}


/* One-shot SM2 encrypt; returns 1 on success. */
static int sm2_encrypt(EVP_PKEY *key, const char *digest,
                       const unsigned char *in, size_t inlen,
                       unsigned char *out, size_t *outlen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t blen = 2048;
    int ok;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_encrypt_init(ctx) == 1, "SM2 encrypt_init failed");
    if (digest != NULL) {
        OSSL_PARAM params[2];
        char name[32];
        snprintf(name, sizeof(name), "%s", digest);
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_DIGEST,
                                                     name, 0);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
              "setting the SM2 cipher digest failed");
    }
    ok = EVP_PKEY_encrypt(ctx, out, &blen, in, inlen) == 1;
    *outlen = ok ? blen : 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* One-shot SM2 decrypt; returns 1 on success. */
static int sm2_decrypt(EVP_PKEY *key, const unsigned char *in, size_t inlen,
                       unsigned char *out, size_t *outlen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t blen = 2048;
    int ok;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_decrypt_init(ctx) == 1, "SM2 decrypt_init failed");
    ok = EVP_PKEY_decrypt(ctx, out, &blen, in, inlen) == 1;
    *outlen = ok ? blen : 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* (1) Signatures: the default (empty) ID, a custom ID, and the cross-ID

 * failures -- including the documented fact that OpenSSL 3 does not

 * substitute the GM/T default ID "1234567812345678". */
static void test_sign_verify(void)
{
    EVP_PKEY *key = gen_key();
    const char *msg = "SM2 signature test message";
    unsigned char custom_id[] = { 'f', 'i', 'l', 'c', '-', 't', 'e', 's', 't',
                                  '-', 'i', 'd' };
    unsigned char gmt_id[] = { '1', '2', '3', '4', '5', '6', '7', '8',
                               '1', '2', '3', '4', '5', '6', '7', '8' };
    unsigned char *in = ALLOCATE_INPUT(strlen(msg));
    unsigned char *sig_def = ALLOCATE_OUTPUT(200);
    unsigned char *sig_cus = ALLOCATE_OUTPUT(200);
    unsigned char *sig_gmt = ALLOCATE_OUTPUT(200);
    size_t inlen = strlen(msg);
    size_t deflen = 200, cuslen = 200, gmtlen = 200;
    char mbuf[128];

    memcpy(in, msg, inlen);

    CHECK(sm2_sign(key, NULL, 0, in, inlen, sig_def, &deflen) == 1,
          "SM2 signing with the default ID failed");
    CHECK(deflen > 0 && deflen <= 200, "SM2 default-ID signature length bogus");
    CHECK(sm2_verify(key, NULL, 0, in, inlen, sig_def, deflen) == 1,
          "SM2 default-ID signature does not verify");

    snprintf(mbuf, sizeof(mbuf), "custom distid \"%s\"", "filc-test-id");
    CHECK(sm2_sign(key, custom_id, sizeof(custom_id), in, inlen,
                   sig_cus, &cuslen) == 1,
          "SM2 signing with a custom ID failed");
    CHECK(sm2_verify(key, custom_id, sizeof(custom_id), in, inlen,
                     sig_cus, cuslen) == 1,
          "SM2 custom-ID signature does not verify under its own ID");
    CHECK(sm2_verify(key, NULL, 0, in, inlen, sig_cus, cuslen) != 1,
          "SM2 custom-ID signature verified under the default ID");
    CHECK(sm2_verify(key, NULL, 0, in, inlen, sig_def, deflen) == 1,
          "SM2 default-ID signature does not verify (sanity)");
    CHECK(sm2_verify(key, custom_id, sizeof(custom_id), in, inlen,
                     sig_def, deflen) != 1,
          "SM2 default-ID signature verified under the custom ID");

    /* Documented: OpenSSL 3 uses an EMPTY default ID, not the GM/T
     * "1234567812345678".  A signature made with that explicit ID must
     * therefore not verify as a default-ID signature, and vice versa. */
    CHECK(sm2_sign(key, gmt_id, sizeof(gmt_id), in, inlen,
                   sig_gmt, &gmtlen) == 1,
          "SM2 signing with the explicit GM/T ID failed");
    CHECK(sm2_verify(key, gmt_id, sizeof(gmt_id), in, inlen,
                     sig_gmt, gmtlen) == 1,
          "SM2 explicit GM/T-ID signature does not verify under its own ID");
    CHECK(sm2_verify(key, NULL, 0, in, inlen, sig_gmt, gmtlen) != 1,
          "SM2 explicit GM/T-ID signature verified under the default ID "
          "(the default ID would not be empty)");
    CHECK(sm2_verify(key, gmt_id, sizeof(gmt_id), in, inlen,
                     sig_def, deflen) != 1,
          "SM2 default-ID signature verified under the GM/T ID");
    EVP_PKEY_free(key);
}


/* (2) Encryption: round-trips for {1,127,255,1024}, the documented 0-byte

 * failure, and tamper detection. */
static void test_encrypt_decrypt(void)
{
    EVP_PKEY *key = gen_key();
    static const size_t sizes[] = {1, 127, 255, 1024};
    unsigned char *msg = ALLOCATE_INPUT(1024);
    unsigned char *ct = ALLOCATE_OUTPUT(2048);
    unsigned char *back = ALLOCATE_OUTPUT(2048);
    size_t s, i;

    for (i = 0; i < 1024; i++)
        msg[i] = (unsigned char)(i * 13 + 7);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t ctlen = 0, backlen = 0;
        char mbuf[96];
        snprintf(mbuf, sizeof(mbuf), "SM2 encryption of %zu bytes failed",
                 sizes[s]);
        CHECK(sm2_encrypt(key, NULL, msg, sizes[s], ct, &ctlen) == 1, mbuf);
        CHECK(ctlen > sizes[s] + 65 + 32,
              "SM2 ciphertext implausibly short for its payload");
        snprintf(mbuf, sizeof(mbuf), "SM2 decryption of %zu bytes failed",
                 sizes[s]);
        CHECK(sm2_decrypt(key, ct, ctlen, back, &backlen) == 1, mbuf);
        CHECK(backlen == sizes[s] && memcmp(back, msg, sizes[s]) == 0,
              "SM2 roundtrip did not recover the message");
    }

    /* Documented: a 0-byte message fails to encrypt on both 3.0.13 and
     * 3.6.5 (the SM2 KDF/hash layer requires a non-empty message). */
    {
        size_t ctlen = 0;
        CHECK(sm2_encrypt(key, NULL, msg, 0, ct, &ctlen) != 1,
              "SM2 encryption accepted a 0-byte message");
    }

    /* Tamper detection: flip a byte inside the DER ciphertext. */
    {
        size_t ctlen = 0, backlen = 0;
        CHECK(sm2_encrypt(key, NULL, msg, 64, ct, &ctlen) == 1,
              "SM2 encryption for the tamper test failed");
        ct[ctlen / 2] ^= 0x10;
        CHECK(sm2_decrypt(key, ct, ctlen, back, &backlen) != 1,
              "tampered SM2 ciphertext decrypted without error");
    }
    EVP_PKEY_free(key);
}


/* (3) The explicit "digest" parameter on the SM2 encryption context (the

 * documented stand-in for the C1C2C3/C1C3C2 "encrypt-param", which does

 * not exist in OpenSSL 3): an SM3-labeled ciphertext still round-trips. */
static void test_cipher_digest_param(void)
{
    EVP_PKEY *key = gen_key();
    unsigned char *msg = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(2048);
    unsigned char *back = ALLOCATE_OUTPUT(2048);
    unsigned char *ct2 = ALLOCATE_OUTPUT(2048);
    size_t ctlen = 0, ct2len = 0, backlen = 0;
    size_t i;

    for (i = 0; i < 32; i++)
        msg[i] = (unsigned char)(0x60 + i);

    CHECK(sm2_encrypt(key, "SM3", msg, 32, ct, &ctlen) == 1,
          "SM2 encryption with the digest=SM3 param failed");
    CHECK(sm2_decrypt(key, ct, ctlen, back, &backlen) == 1 &&
          backlen == 32 && memcmp(back, msg, 32) == 0,
          "SM2 roundtrip with the digest=SM3 param failed");

    /* Default and explicit-SM3 ciphertexts differ (the DER encoding embeds
     * the randomized ephemeral point), but both decrypt. */
    CHECK(sm2_encrypt(key, NULL, msg, 32, ct2, &ct2len) == 1 && ct2len > 0,
          "SM2 encryption with the default digest failed");
    CHECK(sm2_decrypt(key, ct2, ct2len, back, &backlen) == 1 &&
          backlen == 32 && memcmp(back, msg, 32) == 0,
          "SM2 default-digest roundtrip failed");
    EVP_PKEY_free(key);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_sign_verify();
    test_encrypt_decrypt();
    test_cipher_digest_param();

    printf("test_sm2: all %u checks passed\n", g_checks);
    return 0;
}
