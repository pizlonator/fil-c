#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_mac_poly1305 -- EVP_MAC "POLY1305".
 *
 * Known-answer vector: RFC 8439 section 2.5.2 -- the 32-byte one-time key
 * 85d6be78...49f51b (r = first 16 bytes, s = last 16 bytes) over the
 * 34-byte ASCII message "Cryptographic Forum Research Group" gives tag
 * a8061dc1305136c6c22b8baf0c0127a9.  Verified with pizfix/bin/openssl:
 *
 *   printf 'Cryptographic Forum Research Group' | \
 *       pizfix/bin/openssl mac -macopt hexkey:85d6... POLY1305
 *
 * (Note: the Poly1305 MAC key is 32 bytes -- r || s -- not 16; a 16-byte
 * key is rejected with "invalid key length" by every OpenSSL 3.x.  With
 * no data at all the tag equals the s half of the key, 0103808a...,
 * which the empty-message KAT below also pins down.)
 *
 * Coverage: the RFC 8439 KAT; the empty-message KAT; chunked updates
 * (partial-block buffering, crypto/poly1305/poly1305.c) for both the
 * 34-byte KAT message ({1,16,17} splits) and a 131-byte deterministic
 * message ({1,64,63,3} splits); EVP_MAC_CTX_get_mac_size == 16; and key
 * lengths 15/31/33 rejected at init (only 32 is accepted).
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
    fprintf(stderr, "FAIL test_mac_poly1305: %s\n", msg);
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

/* One-shot Poly1305.  Returns the tag length (0 on failure). */
static size_t poly_oneshot(const unsigned char *key, size_t keylen,
                           const unsigned char *data, size_t datalen,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx = NULL;
    unsigned char kcopy[32];
    size_t outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"POLY1305\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
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

/* Poly1305 with the message fed in updates cycling through `chunks`
 * (each truncated to what remains).  Must agree with poly_oneshot. */
static size_t poly_chunked(const unsigned char *key, size_t keylen,
                           const unsigned char *data, size_t datalen,
                           const size_t *chunks, size_t nchunks,
                           unsigned char *out, size_t outcap)
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx = NULL;
    unsigned char kcopy[32];
    size_t off = 0, ci = 0, outlen = 0;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"POLY1305\") failed");
    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
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

static const char *const KEY32_HEX =
    "85d6be7857556d337f4452fe42d506a8"
    "0103808afb0db2fd4abff6af4149f51b";
static const char *const MSG34_HEX = "Cryptographic Forum Research Group";

/* (1) The RFC 8439 section 2.5.2 KAT and the empty-message KAT. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg = ALLOCATE_INPUT(34);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t keylen, wantlen, gotlen;

    keylen = from_hex(KEY32_HEX, key, 32);
    CHECK(keylen == 32, "the Poly1305 MAC key is 32 bytes (r || s)");

    memcpy(msg, MSG34_HEX, 34);
    wantlen = from_hex("a8061dc1305136c6c22b8baf0c0127a9", want, 16);
    gotlen = poly_oneshot(key, 32, msg, 34, got, 16);
    expect_equal("RFC 8439 2.5.2 one-shot tag does not match the KAT",
                 got, gotlen, want, wantlen);

    wantlen = from_hex("0103808afb0db2fd4abff6af4149f51b", want, 16);
    gotlen = poly_oneshot(key, 32, NULL, 0, got, 16);
    expect_equal("empty-message tag does not match the KAT",
                 got, gotlen, want, wantlen);
}

/* (2) Chunked updates must reproduce the one-shot tag: {1,16,17} splits
 * of the 34-byte KAT message and {1,64,63,3} splits of a 131-byte
 * deterministic message (the update path buffers partial blocks). */
static void test_chunked(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *msg34 = ALLOCATE_INPUT(34);
    unsigned char *msg131 = ALLOCATE_INPUT(131);
    unsigned char *one = ALLOCATE_OUTPUT(16);
    unsigned char *many = ALLOCATE_OUTPUT(16);
    static const size_t chunks_34[] = { 1, 16, 17 };
    static const size_t chunks_131[] = { 1, 64, 63, 3 };
    size_t i, one_len, many_len;

    from_hex(KEY32_HEX, key, 32);
    memcpy(msg34, MSG34_HEX, 34);
    for (i = 0; i < 131; i++)
        msg131[i] = (unsigned char)(i * 5 + 1);

    one_len = poly_oneshot(key, 32, msg34, 34, one, 16);
    many_len = poly_chunked(key, 32, msg34, 34, chunks_34, 3, many, 16);
    expect_equal("{1,16,17} splits of the 34-byte message differ from one-shot",
                 many, many_len, one, one_len);

    one_len = poly_oneshot(key, 32, msg131, 131, one, 16);
    many_len = poly_chunked(key, 32, msg131, 131, chunks_131, 4, many, 16);
    expect_equal("{1,64,63,3} splits of the 131-byte message differ from one-shot",
                 many, many_len, one, one_len);

    /* byte-at-a-time over the long message, too */
    many_len = poly_chunked(key, 32, msg131, 131, chunks_131 + 3, 1, many, 16);
    expect_equal("3-byte-chunk digest differs from one-shot",
                 many, many_len, one, one_len);
}

/* (3) The tag size is 16 and only a 32-byte key is accepted: 15, 31 and
 * 33 bytes must fail at init. */
static void test_key_lengths(void)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *msg = ALLOCATE_INPUT(34);
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx = NULL;
    size_t keylen, i;
    int ok;

    CHECK(mac != NULL, "EVP_MAC_fetch(\"POLY1305\") failed");
    keylen = from_hex(KEY32_HEX, key, 32);
    memcpy(msg, MSG34_HEX, 34);
    for (i = keylen; i < 64; i++)
        key[i] = (unsigned char)(i + 1); /* tail bytes for the 33-byte case */

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    CHECK(EVP_MAC_init(ctx, key, 32, NULL) == 1,
          "a 32-byte key must be accepted");
    CHECK(EVP_MAC_CTX_get_mac_size(ctx) == 16,
          "Poly1305 EVP_MAC_CTX_get_mac_size is not 16");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    ok = EVP_MAC_init(ctx, key, 15, NULL);
    CHECK(!ok, "a 15-byte key was accepted");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    ok = EVP_MAC_init(ctx, key, 31, NULL);
    CHECK(!ok, "a 31-byte key was accepted");
    EVP_MAC_CTX_free(ctx);

    ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx != NULL, "EVP_MAC_CTX_new failed");
    ok = EVP_MAC_init(ctx, key, 33, NULL);
    CHECK(!ok, "a 33-byte key was accepted");
    EVP_MAC_CTX_free(ctx);

    EVP_MAC_free(mac);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_chunked();
    test_key_lengths();

    printf("test_mac_poly1305: all %u checks passed\n", g_checks);
    return 0;
}
