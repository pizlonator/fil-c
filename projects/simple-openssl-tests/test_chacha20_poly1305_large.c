#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_chacha20_poly1305_large -- ChaCha20-Poly1305 at large sizes.
 *
 * The >= 1 MiB regime exercises the AEAD path's bulk loop (the TLS-record
 * path only sees records <= 64 bytes; the AEAD path runs the whole
 * payload through ChaCha20 and Poly1305 block by block,
 * cipher_chacha20_poly1305_hw.c:266-397).  Updates may be split
 * arbitrarily: the text length accumulates in ctx->len.text and the
 * final update wraps up the AAD (aad%16 zero padding) and the 16-byte
 * aad/text length block, so ANY chunk grid must reproduce the one-shot
 * ciphertext and tag exactly.
 *
 * AAD sizes {0, 1, 15, 16, 17, 32} cover the Poly1305 mod-16 padding
 * rules (zero padding to the next 16-byte boundary, and the empty-AAD
 * case where only the length block is hashed).
 *
 * The RFC 8439 2.8.2 key/nonce are reused (their small-size KAT lives in
 * the sibling program); there is no independent large-size KAT -- the
 * one-shot output IS the oracle the chunked runs are compared against.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_TAG(size_t n)    { return poison_alloc(POISON_TAG, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_chacha20_poly1305_large: %s\n", msg);
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

/* RFC 8439 2.8.2 key and nonce. */
static const char *const KEY_HEX =
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f";
static const char *const NONCE_HEX =
    "070000004041424344454647";

#define SIZE (1024u * 1024u) /* 1 MiB */

/* One-shot encrypt: AAD (out == NULL) first, payload, Final, GET_TAG.
 * Returns the ciphertext length. */
static size_t cp_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce, const unsigned char *aad,
                         size_t aadlen, const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    if (inlen) {
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_EncryptUpdate failed");
        dlen = len;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "one-shot EVP_EncryptFinal_ex failed");
    dlen += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)dlen;
}

/* Chunked encrypt through the given update grid.  Must reproduce the
 * one-shot ciphertext and tag exactly. */
static void cp_encrypt_chunked(const EVP_CIPHER *cipher,
                               const unsigned char *key,
                               const unsigned char *nonce,
                               const unsigned char *aad, size_t aadlen,
                               const unsigned char *in, size_t inlen,
                               unsigned char *out, size_t outcap,
                               size_t *outlen, unsigned char *tag,
                               const size_t *chunks, size_t nchunks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0, updates = 0;
    int len = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in the chunked run");
        dlen += len;
        off += n;
        updates++;
    }
    CHECK(updates > nchunks, "too few updates to exercise the grid");
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed in the chunked run");
    dlen += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed in the chunked run");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* One-shot decrypt with tag verification. */
static size_t cp_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce, const unsigned char *aad,
                         size_t aadlen, const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap,
                         const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    if (inlen) {
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_DecryptUpdate failed");
        dlen = len;
    }
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)dlen;
}

/* (1) 1 MiB: the {65536, 16384, 17, 100} chunk grid reproduces the
 * one-shot ciphertext and tag exactly, and the one-shot record decrypts
 * back. */
static void test_one_megabyte(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *in = ALLOCATE_INPUT(SIZE);
    unsigned char *oneshot = ALLOCATE_OUTPUT(SIZE + 16);
    unsigned char *chunked = ALLOCATE_OUTPUT(SIZE + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(SIZE + 16);
    static const size_t chunks[] = { 65536, 16384, 17, 100 };
    size_t i, aadlen, ctlen, clen, backlen;

    /* ChaCha20-Poly1305 is core OpenSSL 3 functionality: never SKIP. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    aadlen = from_hex("50515253c0c1c2c3c4c5c6c7", aad, 12);
    for (i = 0; i < SIZE; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    ctlen = cp_encrypt(cipher, key, nonce, aad, aadlen, in, SIZE, oneshot,
                       SIZE + 16, tag1);
    CHECK(ctlen == SIZE, "the one-shot ciphertext length is not SIZE");

    cp_encrypt_chunked(cipher, key, nonce, aad, aadlen, in, SIZE, chunked,
                       SIZE + 16, &clen, tag2, chunks, 4);
    CHECK(clen == SIZE, "the chunked ciphertext length is not SIZE");
    expect_equal("the 1 MiB chunked ciphertext differs from the one-shot "
                 "ciphertext", chunked, clen, oneshot, ctlen);
    expect_equal("the 1 MiB chunked tag differs from the one-shot tag",
                 tag2, 16, tag1, 16);

    backlen = cp_decrypt(cipher, key, nonce, aad, aadlen, oneshot, ctlen,
                         back, SIZE + 16, tag1);
    expect_equal("the 1 MiB authenticated decrypt did not recover the "
                 "plaintext", back, backlen, in, SIZE);
    EVP_CIPHER_free(cipher);
}

/* (2) AAD sizes {0, 1, 15, 16, 17, 32} (Poly1305 mod-16 padding
 * coverage): each roundtrips at 256 KiB. */
static void test_aad_sizes(void)
{
    static const size_t aad_sizes[] = { 0, 1, 15, 16, 17, 32 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *in = ALLOCATE_INPUT(256 * 1024);
    unsigned char *ct = ALLOCATE_OUTPUT(256 * 1024 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(256 * 1024 + 16);
    size_t s, i, aadlen, ctlen, backlen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    for (i = 0; i < 256 * 1024; i++)
        in[i] = (unsigned char)(i * 11 + 9);

    for (s = 0; s < sizeof(aad_sizes) / sizeof(aad_sizes[0]); s++) {
        aadlen = aad_sizes[s];
        for (i = 0; i < aadlen; i++)
            aad[i] = (unsigned char)(i * 3 + 1);

        ctlen = cp_encrypt(cipher, key, nonce, aad, aadlen, in,
                           256 * 1024, ct, 256 * 1024 + 16, tag);
        CHECK(ctlen == 256 * 1024, "the AAD-sweep ciphertext length is wrong");
        backlen = cp_decrypt(cipher, key, nonce, aad, aadlen, ct, ctlen, back,
                             256 * 1024 + 16, tag);
        snprintf(msg, sizeof(msg),
                 "the %zu-byte-AAD authenticated roundtrip failed", aadlen);
        expect_equal(msg, back, backlen, in, 256 * 1024);
    }
    EVP_CIPHER_free(cipher);
}

/* (3) Tamper detection at 1 MiB: a flipped bit in the final ciphertext
 * block and a flipped bit in the expected tag must both fail the
 * authenticated decrypt. */
static void test_tamper(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *in = ALLOCATE_INPUT(SIZE);
    unsigned char *ct = ALLOCATE_OUTPUT(SIZE + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_bad = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(SIZE + 16);
    EVP_CIPHER_CTX *ctx;
    size_t i, aadlen, ctlen;
    int len, t;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    aadlen = from_hex("50515253c0c1c2c3c4c5c6c7", aad, 12);
    for (i = 0; i < SIZE; i++)
        in[i] = (unsigned char)(i * 5 + 1);

    ctlen = cp_encrypt(cipher, key, nonce, aad, aadlen, in, SIZE, ct,
                       SIZE + 16, tag);

    for (t = 0; t < 2; t++) {
        memcpy(tag_bad, tag, 16);
        if (t == 0)
            ct[SIZE - 1] ^= 0x01; /* a bit in the final ciphertext block */
        else
            tag_bad[5] ^= 0x01;   /* a bit inside the expected tag */

        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "EVP_DecryptUpdate failed on the tampered record");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,
                                  tag_bad) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
              "the 1 MiB decryption accepted a tampered record");
        EVP_CIPHER_CTX_free(ctx);

        if (t == 0)
            ct[SIZE - 1] ^= 0x01;
    }
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_one_megabyte();
    test_aad_sizes();
    test_tamper();

    printf("test_chacha20_poly1305_large: all %u checks passed\n", g_checks);
    return 0;
}
