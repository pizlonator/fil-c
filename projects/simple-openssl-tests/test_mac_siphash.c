#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_siphash -- EVP_MAC "SIPHASH" (SipHash-2-4).
 *
 * Known-answer vectors, verified with pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -macopt hexkey:000102...0f -macopt size:8 SIPHASH
 *
 *   key 000102...0f (16 bytes), message 000102...0e (15 bytes):
 *     8-byte tag  e545be4961ca29a1
 *     16-byte tag 5493e99933b0a8117e08ec0f97cfc3d9
 *   empty message, 8-byte tag: 310e0edd47db6f72
 *     (the reference SipHash implementation's vectors[0] for this key is
 *     726fdb47dd0e0e31 -- the same value in the opposite byte order, per
 *     OpenSSL's little-endian MAC output convention; there are no
 *     officially standardized SipHash vectors, so the CLI output is the
 *     oracle here).
 *
 * Coverage: both output sizes (the default is 16; "size" selects 8 or
 * 16); chunked updates (the update path buffers 8-byte leavings,
 * siphash prov) for one-shot vs chunked over input lengths {0,1,7,8,9,
 * 15,16,17}; EVP_MAC_CTX_get_mac_size reflecting the requested size; a
 * 15-byte key rejected at init; and an unsupported size (12) rejected.
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
    fprintf(stderr, "FAIL test_mac_siphash: %s\n", msg);
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

/* One-shot SipHash with the output size requested through the "size"
 * parameter (size == 0 means: leave the provider default of 16).
 * Returns the tag length (0 on failure). */
static size_t sip_oneshot(const unsigned char *key, size_t keylen,
                          const unsigned char *data, size_t datalen,
                          size_t size, unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "SIPHASH", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[16];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"SIPHASH\") failed");
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
    if (ok && datalen > 0)
        ok = EVP_MAC_update(ctx, data, datalen);
    if (ok)
        ok = EVP_MAC_final(ctx, out, &outlen, outcap);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? outlen : 0;
}

/* SipHash with the message fed in updates cycling through `chunks`.
 * Must agree with sip_oneshot (the update path keeps 8-byte leavings
 * buffered between updates). */
static size_t sip_chunked(const unsigned char *key, size_t keylen,
                          const unsigned char *data, size_t datalen,
                          size_t size, const size_t *chunks, size_t nchunks,
                          unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "SIPHASH", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned char kcopy[16];
    size_t off = 0, ci = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"SIPHASH\") failed");
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

static const char *const KEY16_HEX = "000102030405060708090a0b0c0d0e0f";
static const char *const MSG15_HEX = "000102030405060708090a0b0c0d0e";

/* (1) The KATs for both output sizes and for the empty message. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *msg = ALLOCATE_INPUT(15);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t wantlen, gotlen;

    from_hex(KEY16_HEX, key, 16);
    from_hex(MSG15_HEX, msg, 15);

    wantlen = from_hex("e545be4961ca29a1", want, 16);
    gotlen = sip_oneshot(key, 16, msg, 15, 8, got, 16);
    expect_equal("15-byte message 8-byte tag does not match the KAT",
                 got, gotlen, want, wantlen);

    wantlen = from_hex("5493e99933b0a8117e08ec0f97cfc3d9", want, 16);
    gotlen = sip_oneshot(key, 16, msg, 15, 16, got, 16);
    expect_equal("15-byte message 16-byte tag does not match the KAT",
                 got, gotlen, want, wantlen);

    wantlen = from_hex("310e0edd47db6f72", want, 16);
    gotlen = sip_oneshot(key, 16, NULL, 0, 8, got, 16);
    expect_equal("empty-message 8-byte tag does not match the KAT",
                 got, gotlen, want, wantlen);

    /* With no "size" parameter the default output size is 16 bytes and
     * must equal the explicit size:16 tag. */
    gotlen = sip_oneshot(key, 16, msg, 15, 0, got, 16);
    wantlen = from_hex("5493e99933b0a8117e08ec0f97cfc3d9", want, 16);
    expect_equal("default-size SipHash is not the 16-byte tag",
                 got, gotlen, want, wantlen);
}

/* (2) One-shot vs chunked equality for input lengths around the 8-byte
 * buffering boundary, for both output sizes. */
static void test_chunked_equality(void)
{
    static const size_t sizes[] = { 0, 1, 7, 8, 9, 15, 16, 17 };
    static const size_t chunks[] = { 1, 3, 5, 7, 11, 13, 17 };
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *msg = ALLOCATE_INPUT(17);
    unsigned char *one = ALLOCATE_OUTPUT(16);
    unsigned char *many = ALLOCATE_OUTPUT(16);
    size_t s, i, one_len, many_len;
    char msgbuf[128];

    from_hex(KEY16_HEX, key, 16);
    for (i = 0; i < 17; i++)
        msg[i] = (unsigned char)i;

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        size_t outsize = (s % 2 == 0) ? 8 : 16;
        one_len = sip_oneshot(key, 16, msg, n, outsize, one, 16);
        many_len = sip_chunked(key, 16, msg, n, outsize, chunks,
                               sizeof(chunks) / sizeof(chunks[0]), many, 16);
        snprintf(msgbuf, sizeof(msgbuf),
                 "chunked SipHash differs from one-shot at input length %zu "
                 "(size %zu)", n, outsize);
        expect_equal(msgbuf, many, many_len, one, one_len);
    }

    /* The default-size (16) path must agree with the explicit size:16
     * path through the chunked route as well. */
    one_len = sip_oneshot(key, 16, msg, 17, 0, one, 16);
    many_len = sip_chunked(key, 16, msg, 17, 16, chunks,
                           sizeof(chunks) / sizeof(chunks[0]), many, 16);
    expect_equal("default-size chunked SipHash differs from size:16 one-shot",
                 many, many_len, one, one_len);
}

/* (3) The mac size reflects the requested size; key length 15 fails at
 * init; size 12 fails. */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *badkey = ALLOCATE_KEY(15);
    unsigned char *msg = ALLOCATE_INPUT(15);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "SIPHASH", NULL);
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    size_t size = 8, macsize;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"SIPHASH\") failed");
    from_hex(KEY16_HEX, key, 16);
    from_hex(MSG15_HEX, msg, 15);
    memset(badkey, 0, 15);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(ctx, params) == 1,
          "EVP_MAC_CTX_set_params(size=8) failed");
    CHECK(EVP_MAC_init(ctx, key, 16, NULL) == 1,
          "SipHash init with a 16-byte key failed");
    macsize = EVP_MAC_CTX_get_mac_size(ctx);
    CHECK(macsize == 8, "SipHash mac size does not reflect the size param");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    ok = EVP_MAC_init(ctx, badkey, 15, NULL);
    CHECK(!ok, "a 15-byte SipHash key was accepted");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    size = 12;
    params[0] = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &size);
    params[1] = OSSL_PARAM_construct_end();
    ok = EVP_MAC_CTX_set_params(ctx, params);
    CHECK(!(ok && EVP_MAC_init(ctx, key, 16, NULL)),
          "SipHash accepted an unsupported output size (12)");
    EVP_MAC_CTX_free(ctx);

    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_chunked_equality();
    test_negative();

    printf("test_mac_siphash: all %u checks passed\n", g_checks);
    return 0;
}
