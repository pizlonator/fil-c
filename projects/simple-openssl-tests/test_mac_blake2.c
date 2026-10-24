#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_blake2 -- EVP_MAC "BLAKE2BMAC" / "BLAKE2SMAC" (keyed BLAKE2).
 *
 * Every constant below was produced and verified with pizfix/bin/openssl,
 * e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl mac -macopt hexkey:000102...3f \
 *       -macopt size:20 BLAKE2BMAC
 *
 * with key 000102...3f (64 bytes) for BLAKE2BMAC, 000102...1f (32 bytes)
 * for BLAKE2SMAC, 000102...0f (16 bytes) for the key-length variants, and
 * the message "abc".
 *
 * Coverage: output size sweeps via the "size" parameter ({64,32,20,1} for
 * BLAKE2BMAC, {32,16,1} for BLAKE2SMAC) with an EVP_MAC_CTX_get_mac_size
 * cross-check; key lengths {16,64} and {16,32} accepted; a zero-length
 * key and an over-long key (65 for b, 33 for s) rejected; an unsupported
 * output size (65 for b, 33 for s) rejected; and chunked updates
 * (one-shot == chunked over a 200-byte deterministic message).
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
    fprintf(stderr, "FAIL test_mac_blake2: %s\n", msg);
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

/* One keyed BLAKE2 computation.  size == 0 leaves the provider default
 * (64 for BLAKE2BMAC, 32 for BLAKE2SMAC).  keylen == 0 means: do not set
 * the key at all (which every OpenSSL 3.x rejects -- the negative tests
 * rely on that).  Returns the tag length (0 on failure). */
static size_t b2_oneshot(const char *macname, const unsigned char *key,
                         size_t keylen, size_t size,
                         const unsigned char *data, size_t datalen,
                         unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, macname, NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch failed for a keyed BLAKE2 MAC");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0) {
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(size) failed");
    }
    if (key != NULL) {
        CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
        memcpy(kcopy, key, keylen);
    }
    ok = EVP_MAC_init(ctx, key != NULL ? kcopy : NULL, keylen, NULL);
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data, datalen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* Keyed BLAKE2 with the message fed in updates cycling through `chunks`.
 * Must agree with b2_oneshot. */
static size_t b2_chunked(const char *macname, const unsigned char *key,
                         size_t keylen, size_t size,
                         const unsigned char *data, size_t datalen,
                         const size_t *chunks, size_t nchunks,
                         unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, macname, NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    size_t off = 0, ci = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch failed for a keyed BLAKE2 MAC");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    if (size > 0) {
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(size) failed");
    }
    CHECK(keylen <= sizeof(kcopy), "key longer than the working key buffer");
    memcpy(kcopy, key, keylen);
    ok = EVP_MAC_init(ctx, kcopy, keylen, NULL);
    while (ok && off < datalen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > datalen - off)
            n = datalen - off;
        ok = EVP_MAC_update(ctx, data + off, n);
        off += n;
    }
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

static const char *const KEY64_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f";
static const char *const KEY32_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f";
static const char *const KEY16_HEX = "000102030405060708090a0b0c0d0e0f";

static const struct b2mac_kat {
    const char *macname;
    const char *keyhex;
    size_t size;           /* 0: the provider default */
    size_t defsize;        /* the default size, for the failure message */
    const char *taghex;
} b2mac_kats[] = {
    { "BLAKE2BMAC", KEY64_HEX, 64, 64,
      "06bbc3dedf13a31139498655251b7588ccd3bb5aaa071b2d44d8e0a04095579ed590fbfdcf941f4370ce5ce623624e7a76d33e7a8109dcda9b57d72f8f8efa51" },
    { "BLAKE2BMAC", KEY64_HEX, 32, 64,
      "dff38c978666dff5631db35ca15535520d134f5c8060ea569c6a178ad393719f" },
    { "BLAKE2BMAC", KEY64_HEX, 20, 64,
      "f3464811aec9776024bd78c73dbaad63a62c509b" },
    { "BLAKE2BMAC", KEY64_HEX, 1, 64,
      "46" },
    { "BLAKE2SMAC", KEY32_HEX, 32, 32,
      "a281f725754969a702f6fe36fc591b7def866e4b70173ece402fc01c064d6b65" },
    { "BLAKE2SMAC", KEY32_HEX, 16, 32,
      "61ba5f165c194692e09d12520cc4c74a" },
    { "BLAKE2SMAC", KEY32_HEX, 1, 32,
      "d1" },
    { "BLAKE2BMAC", KEY16_HEX, 0, 64,
      "4c76bc7ad0fc52e4bde231b38727c331172cfe3eeaf10cd2fa5c65abbdb8faeaad2da338531b382ac103f10ccaf41f74d8870d016c48b269c0d9a5cf5752c8c3" },
    { "BLAKE2SMAC", KEY16_HEX, 0, 32,
      "033a40b64c6296fc2e3584d2a06bcf1a8003ebb9b8007940e2f820b645917a4e" },
};

/* (1) The output-size sweep and the key-length variants: one-shot ==
 * KAT, and EVP_MAC_CTX_get_mac_size tracks the requested size. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *msg = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "BLAKE2BMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[64];
    size_t k, keylen, wantlen, gotlen, macsize;
    char msgbuf[128];

    CHECK(mac != NULL, "EVP_MAC_fetch(\"BLAKE2BMAC\") failed");
    memcpy(msg, "abc", 3);
    from_hex(KEY64_HEX, key, 64);

    for (k = 0; k < sizeof(b2mac_kats) / sizeof(b2mac_kats[0]); k++) {
        const struct b2mac_kat *kat = &b2mac_kats[k];
        keylen = from_hex(kat->keyhex, key, 64);
        wantlen = from_hex(kat->taghex, want, 64);
        gotlen = b2_oneshot(kat->macname, key, keylen, kat->size, msg, 3,
                            got, 64);
        snprintf(msgbuf, sizeof(msgbuf),
                 "%s (size=%zu, default %zu) one-shot does not match the KAT",
                 kat->macname, kat->size, kat->defsize);
        expect_equal(msgbuf, got, gotlen, want, wantlen);
    }

    /* get_mac_size tracks the requested output size (queried after
     * init, where it is meaningful). */
    keylen = from_hex(KEY64_HEX, key, 64);
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    {
        size_t sz = 20;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &sz);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
              "EVP_MAC_CTX_set_params(size=20) failed");
    }
    memcpy(kcopy, key, keylen);
    CHECK(EVP_MAC_init(ctx, kcopy, keylen, NULL) == 1, "BLAKE2BMAC init failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 20, "BLAKE2BMAC mac size does not track the size param");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* (2) Chunked updates over a 200-byte deterministic message must equal
 * the one-shot tag, for both MACs at their default sizes. */
static void test_chunked(void)
{
    static const size_t chunks[] = { 1, 63, 64, 72 };
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *msg = ALLOCATE_INPUT(200);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    size_t i, one_len, many_len;

    from_hex(KEY64_HEX, key, 64);
    for (i = 0; i < 200; i++)
        msg[i] = (unsigned char)(i * 3 + 11);

    one_len = b2_oneshot("BLAKE2BMAC", key, 64, 0, msg, 200, one, 64);
    many_len = b2_chunked("BLAKE2BMAC", key, 64, 0, msg, 200, chunks, 4,
                          many, 64);
    expect_equal("chunked BLAKE2BMAC differs from one-shot",
                 many, many_len, one, one_len);

    one_len = b2_oneshot("BLAKE2SMAC", key, 32, 0, msg, 200, one, 64);
    many_len = b2_chunked("BLAKE2SMAC", key, 32, 0, msg, 200, chunks, 4,
                          many, 64);
    expect_equal("chunked BLAKE2SMAC differs from one-shot",
                 many, many_len, one, one_len);
}

/* (3) Negative tests: a zero-length key, an over-long key and an
 * unsupported output size must all fail cleanly. */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(65);
    unsigned char *msg = ALLOCATE_INPUT(3);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "BLAKE2BMAC", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char empty[1] = { 0 };
    size_t size65 = 65, size33 = 33;
    int okset, okinit;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"BLAKE2BMAC\") failed");
    memcpy(msg, "abc", 3);
    from_hex(KEY64_HEX, key, 64);
    key[64] = 0x40; /* the 65th key byte for the over-long case */

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    okinit = EVP_MAC_init(ctx, empty, 0, NULL);
    CHECK(!okinit, "BLAKE2BMAC accepted a zero-length key");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    okinit = EVP_MAC_init(ctx, key, 65, NULL);
    CHECK(!okinit, "BLAKE2BMAC accepted a 65-byte key");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size65);
    params[1] = OSSL_PARAM_construct_end();
    okset = EVP_MAC_CTX_set_params(ctx, params);
    okinit = okset ? EVP_MAC_init(ctx, key, 64, NULL) : 0;
    CHECK(!(okset && okinit), "BLAKE2BMAC accepted an output size of 65");
    EVP_MAC_CTX_free(ctx);

    EVP_MAC_free(mac);
    mac = EVP_MAC_fetch(NULL, "BLAKE2SMAC", NULL);
    CHECK(mac != NULL, "EVP_MAC_fetch(\"BLAKE2SMAC\") failed");

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    okinit = EVP_MAC_init(ctx, key, 33, NULL);
    CHECK(!okinit, "BLAKE2SMAC accepted a 33-byte key");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size33);
    params[1] = OSSL_PARAM_construct_end();
    okset = EVP_MAC_CTX_set_params(ctx, params);
    okinit = okset ? EVP_MAC_init(ctx, key, 32, NULL) : 0;
    CHECK(!(okset && okinit), "BLAKE2SMAC accepted an output size of 33");
    EVP_MAC_CTX_free(ctx);

    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_chunked();
    test_negative();

    printf("test_mac_blake2: all %u checks passed\n", g_checks);
    return 0;
}
