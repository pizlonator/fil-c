#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_rc4_large -- RC4 (legacy provider) at >= 64 KiB through odd update
 * grids.  The legacy RC4 core processes 8 bytes at a time through its
 * bulk path (rc4_enc.c) with the i/j stream state carried across
 * updates, so a long run of uneven updates must reproduce the one-shot
 * keystream exactly -- that equality is the oracle here (RC4 has no
 * per-size KATs beyond the classic small vectors in the sibling
 * program).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rc4_large: %s\n", msg);
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

#define SIZE (64u * 1024u) /* 64 KiB */

/* One-shot RC4 with the default 16-byte key; returns the output length. */
static size_t rc4_oneshot(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *in, size_t inlen,
                          unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_CipherUpdate failed");
    CHECK(EVP_CipherFinal_ex(ctx, out + len, &flen) == 1,
          "one-shot EVP_CipherFinal_ex failed");
    CHECK(flen == 0, "RC4 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated).  Returns the output length and the update count. */
static size_t rc4_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *in, size_t inlen,
                          unsigned char *out, size_t outcap, int enc,
                          const size_t *chunks, size_t nchunks,
                          size_t *updates)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0, ups = 0;
    int len = 0, total = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_CipherUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_CipherUpdate failed in the chunked run");
        total += len;
        off += n;
        ups++;
    }
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &flen) == 1,
          "EVP_CipherFinal_ex failed in the chunked run");
    total += flen;
    *updates = ups;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) 64 KiB through the odd grid {1000, 1, 64, 8} == one-shot, in both
 * directions, with 200+ updates proving the state continuity. */
static void test_64k(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(SIZE);
    unsigned char *oneshot = ALLOCATE_OUTPUT(SIZE);
    unsigned char *chunked = ALLOCATE_OUTPUT(SIZE);
    unsigned char *back = ALLOCATE_OUTPUT(SIZE);
    static const size_t chunks[] = { 1000, 1, 64, 8 };
    size_t i, olen, clen, backlen, ups;

    /* RC4 is legacy-provider functionality loaded in main(); a NULL
     * fetch here is a broken build, not a version gate. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    for (i = 0; i < SIZE; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    olen = rc4_oneshot(cipher, key, in, SIZE, oneshot, SIZE, 1);
    CHECK(olen == SIZE, "the 64 KiB one-shot output length is wrong");

    clen = rc4_chunked(cipher, key, in, SIZE, chunked, SIZE, 1, chunks,
                       sizeof(chunks) / sizeof(chunks[0]), &ups);
    CHECK(ups > 200, "too few updates to exercise state continuity");
    expect_equal("the 64 KiB chunked ciphertext differs from the one-shot "
                 "ciphertext", chunked, clen, oneshot, olen);

    backlen = rc4_chunked(cipher, key, oneshot, olen, back, SIZE, 0, chunks,
                          sizeof(chunks) / sizeof(chunks[0]), &ups);
    expect_equal("the 64 KiB chunked decrypt did not recover the plaintext",
                 back, backlen, in, SIZE);
    EVP_CIPHER_free(cipher);
}

/* (2) 128 KiB one-shot roundtrip and a byte-at-a-time decrypt of the
 * first 64 bytes (state continuity across the 8-byte bulk boundary). */
static void test_128k_and_bytewise(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(2 * SIZE);
    unsigned char *ct = ALLOCATE_OUTPUT(2 * SIZE);
    unsigned char *back = ALLOCATE_OUTPUT(2 * SIZE);
    EVP_CIPHER_CTX *ctx;
    size_t i, ctlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("fedcba9876543210fedcba9876543210", key, 16);
    for (i = 0; i < 2 * SIZE; i++)
        in[i] = (unsigned char)(i * 13 + 5);

    ctlen = rc4_oneshot(cipher, key, in, 2 * SIZE, ct, 2 * SIZE, 1);
    CHECK(ctlen == 2 * SIZE, "the 128 KiB one-shot output length is wrong");
    rc4_oneshot(cipher, key, ct, ctlen, back, 2 * SIZE, 0);
    CHECK(memcmp(back, in, 2 * SIZE) == 0,
          "the 128 KiB RC4 roundtrip failed");
    g_checks++;

    /* Byte-at-a-time decrypt of the first 64 bytes of the ciphertext. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    for (i = 0; i < 64; i++) {
        CHECK(EVP_DecryptUpdate(ctx, back + i, &len, ct + i, 1) == 1,
              "the byte-at-a-time RC4 decrypt update failed");
        CHECK(len == 1, "a 1-byte RC4 update did not emit 1 byte");
    }
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(back, in, 64) == 0,
          "the byte-at-a-time RC4 decrypt of the first 64 bytes failed");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    /* Loading the legacy provider disables default-provider auto-load, so
     * both are loaded explicitly (RC4 lives in the legacy provider). */
    CHECK(OSSL_PROVIDER_load(NULL, "legacy") != NULL,
          "OSSL_PROVIDER_load(\"legacy\") failed");
    CHECK(OSSL_PROVIDER_load(NULL, "default") != NULL,
          "OSSL_PROVIDER_load(\"default\") failed");

    test_64k();
    test_128k_and_bytewise();

    printf("test_rc4_large: all %u checks passed\n", g_checks);
    return 0;
}
