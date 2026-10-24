#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_cmac -- EVP_MAC "CMAC" with AES-128/192/256-CBC and
 * DES-EDE3-CBC.
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -cipher aes-128-cbc -macopt hexkey:2b7e1516... CMAC
 *
 *   AES-128:  RFC 4493 examples 1-4 (Mlen 0/128/320/512 bits over the
 *             SP 800-38A message 6bc1bee2..., key 2b7e1516...).  Example
 *             1 (the empty message) is the subkey-generation test: it can
 *             only come out right if K1/K2 were derived correctly.
 *   AES-192/256: NIST SP 800-38B examples 1-4 over the same message.
 *   DES-EDE3-CBC: key 010203...18 (24 bytes) over the 16-byte prefix;
 *             no published vector, so the constant was produced by the
 *             authoritative CLI and cross-checked on the system 3.0.13
 *             and pizfix 3.6.5 stacks (8-byte tag, DES block size).
 *
 * Coverage: one-shot KATs; partial updates (the 40-byte example 3 message
 * split as {17, 23} bytes) == one-shot; EVP_MAC_CTX_get_mac_size (16 for
 * AES, 8 for DES-EDE3-CBC); and a wrong cipher rejected at init (an
 * AES-256-CBC cipher with a 16-byte key fails the key-length check on
 * every OpenSSL 3.x; non-CBC modes are only rejected at set_params time
 * in newer releases, so they are not used for the negative test).
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
    fprintf(stderr, "FAIL test_mac_cmac: %s\n", msg);
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

/* One-shot CMAC.  The cipher is selected through the "cipher" parameter
 * and the key through EVP_MAC_init.  Returns the tag length (0 on
 * failure). */
static size_t cmac_oneshot(const char *ciphername, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)ciphername, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data, datalen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* CMAC with the message fed as updates of exactly {a, b, rest} bytes.
 * Must agree with cmac_oneshot. */
static size_t cmac_chunked(const char *ciphername, const unsigned char *key,
                           size_t keylen, const unsigned char *data, size_t datalen,
                           size_t a, size_t b, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[32];
    size_t off = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)ciphername, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok && a > 0 && datalen > 0) {
        size_t n = a < datalen ? a : datalen;
        ok = EVP_MAC_update(ctx, data, n);
        off = n;
    }
    if (ok && b > 0 && off < datalen) {
        size_t n = b < datalen - off ? b : datalen - off;
        ok = EVP_MAC_update(ctx, data + off, n);
        off += n;
    }
    if (ok && off < datalen)
        ok = EVP_MAC_update(ctx, data + off, datalen - off);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* The SP 800-38A message (also RFC 4493's M) and the RFC 4493 key. */
static const char *const MSG64_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

static const struct cmac_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    size_t msglen;     /* 0, 16, 40 or 64 */
    const char *taghex;
} cmac_kats[] = {
    { "RFC 4493 AES-128 example 1", "AES-128-CBC",
      "2b7e151628aed2a6abf7158809cf4f3c", 0,
      "bb1d6929e95937287fa37d129b756746" },
    { "RFC 4493 AES-128 example 2", "AES-128-CBC",
      "2b7e151628aed2a6abf7158809cf4f3c", 16,
      "070a16b46b4d4144f79bdd9dd04a287c" },
    { "RFC 4493 AES-128 example 3", "AES-128-CBC",
      "2b7e151628aed2a6abf7158809cf4f3c", 40,
      "dfa66747de9ae63030ca32611497c827" },
    { "RFC 4493 AES-128 example 4", "AES-128-CBC",
      "2b7e151628aed2a6abf7158809cf4f3c", 64,
      "51f0bebf7e3b9d92fc49741779363cfe" },
    { "SP 800-38B AES-192 example 1", "AES-192-CBC",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 0,
      "d17ddf46adaacde531cac483de7a9367" },
    { "SP 800-38B AES-192 example 2", "AES-192-CBC",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 16,
      "9e99a7bf31e710900662f65e617c5184" },
    { "SP 800-38B AES-192 example 3", "AES-192-CBC",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 40,
      "8a1de5be2eb31aad089a82e6ee908b0e" },
    { "SP 800-38B AES-192 example 4", "AES-192-CBC",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 64,
      "a1d5df0eed790f794d77589659f39a11" },
    { "SP 800-38B AES-256 example 1", "AES-256-CBC",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 0,
      "028962f61b7bf89efc6b551f4667d983" },
    { "SP 800-38B AES-256 example 2", "AES-256-CBC",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 16,
      "28a7023f452e8f82bd4bf28d8c37c35c" },
    { "SP 800-38B AES-256 example 3", "AES-256-CBC",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 40,
      "aaf3d8f1de5640c232f5b169b9c911e6" },
    { "SP 800-38B AES-256 example 4", "AES-256-CBC",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 64,
      "e1992190549f6ed5696a2c056c315410" },
};

/* (1) One-shot KATs for all twelve AES vectors.  Example 1 of each set

 * (the empty message) doubles as the subkey-generation sanity check. */

static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t k, keylen, wantlen, gotlen;
    char msgbuf[128];

    from_hex(MSG64_HEX, msg, 64);
    for (k = 0; k < sizeof(cmac_kats) / sizeof(cmac_kats[0]); k++) {
        const struct cmac_kat *kat = &cmac_kats[k];
        keylen = from_hex(kat->keyhex, key, 32);
        wantlen = from_hex(kat->taghex, want, 16);
        gotlen = cmac_oneshot(kat->ciphername, key, keylen, msg, kat->msglen,
                              got, 16);
        snprintf(msgbuf, sizeof(msgbuf), "%s: one-shot tag does not match the KAT",
                 kat->label);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }
}

/* (2) Partial updates: the 40-byte example 3 message split as {17, 23}

 * bytes must produce exactly the one-shot tag. */

static void test_partial_updates(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t keylen, wantlen, gotlen;

    keylen = from_hex(cmac_kats[0].keyhex, key, 16);
    from_hex(MSG64_HEX, msg, 64);
    wantlen = from_hex(cmac_kats[2].taghex, want, 16);
    gotlen = cmac_chunked("AES-128-CBC", key, keylen, msg, 40, 17, 23, got, 16);
    expect_equal("partial updates ({17,23}) do not match the example 3 KAT",
                 got, gotlen, want, wantlen);
}

/* (3) DES-EDE3-CBC: an 8-byte tag, with the key length the cipher
 * expects (24 bytes). */
static void test_des_ede3(void)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OTHER(8);
    unsigned char *got = ALLOCATE_OUTPUT(8);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[24];
    size_t keylen, wantlen, gotlen = 0, macsize;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    keylen = from_hex("0102030405060708090a0b0c0d0e0f101112131415161718",
                      key, 24);
    from_hex(MSG64_HEX, msg, 64);
    wantlen = from_hex("c0b9bbee139722ab", want, 8);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"DES-EDE3-CBC", 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    if (ok)
        ok = EVP_MAC_update(ctx, msg, 16);
    if (ok)
        ok = EVP_MAC_final(ctx, got, &gotlen, 8);
    CHECK(ok, "DES-EDE3-CBC CMAC computation failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 8, "DES-EDE3-CBC CMAC size is not 8 (DES block size)");
    expect_equal("DES-EDE3-CBC CMAC does not match the verified constant",
                 got, gotlen, want, wantlen);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* (4) A wrong cipher must fail at init: AES-256-CBC with a 16-byte key

 * fails the key-length check on every OpenSSL 3.x. */

static void test_wrong_cipher(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *msg = ALLOCATE_INPUT(64);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[16];
    size_t keylen;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"CMAC\") failed");
    keylen = from_hex(cmac_kats[0].keyhex, key, 16);
    from_hex(MSG64_HEX, msg, 64);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
                                                 (char *)"AES-256-CBC", 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(cipher) failed");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    CHECK(!ok, "CMAC accepted AES-256-CBC with a 16-byte key");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_partial_updates();
    test_des_ede3();
    test_wrong_cipher();

    printf("test_mac_cmac: all %u checks passed\n", g_checks);
    return 0;
}
