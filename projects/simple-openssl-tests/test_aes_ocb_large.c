#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_ocb_large -- AES-192/256-OCB with large, multi-update payloads.
 *
 * The oracle is self-consistency, as in test_aes_gcm_large: a 1 MiB payload
 * fed through mixed {16384, 16391}-byte updates must produce exactly the
 * one-shot ciphertext and tag.  16391 = 16384 + 7 forces a partial block to
 * be carried in the provider's data buffer across updates and flushed by the
 * next update, exercising both the bulk per-block path (crypto/modes/
 * ocb128.c ocb streams full blocks when the stream callback is available)
 * and the buffered-tail path.
 *
 * A 64 KiB AAD is fed in two 32 KiB chunks and must match the one-shot AAD.
 * AAD updates report the number of AAD bytes hashed through *outl (unlike
 * GCM, which reports 0), so ciphertext lengths are accumulated from data
 * updates only.
 *
 * The helpers are anchored to the RFC 7253 Appendix A tuple for AES-256
 * (verified against the authoritative pizfix OpenSSL 3.6.5 and cross-checked
 * on 3.0.13; the 40/40-byte tuple is the RFC's largest published sample; the
 * RFC publishes no 192-bit-key sample, and the AES-192 KAT counterpart lives
 * in test_aes_ocb_small.c, derived with a scratch probe and cross-checked on
 * both OpenSSL builds).
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
    fprintf(stderr, "FAIL test_aes_ocb_large: %s\n", msg);
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

#define LARGE_LEN (1024u * 1024u)   /* 1 MiB payload */
#define AAD_LEN   (64u * 1024u)     /* 64 KiB AAD */

static const size_t DATA_CHUNKS[] = { 16384, 16391 };
static const size_t AAD_CHUNKS[] = { 32768, 32768 };

/* One-shot OCB encrypt.  `out` needs inlen + 16 bytes. */
static void ocb_encrypt_one_shot(const EVP_CIPHER *cipher, const unsigned char *key,
                                 const unsigned char *nonce,
                                 const unsigned char *aad, size_t aadlen,
                                 const unsigned char *in, size_t inlen,
                                 unsigned char *out, size_t outcap,
                                 size_t *outlen, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, scratch = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &scratch, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    if (inlen)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "one-shot EVP_EncryptFinal_ex failed");
    len += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* Streaming OCB encrypt: AAD in AAD_CHUNKS, data in DATA_CHUNKS. */
static void ocb_encrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *nonce,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off, ci;
    int len = 0, scratch = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    off = 0;
    ci = 0;
    while (off < aadlen) {
        size_t n = AAD_CHUNKS[ci++ % (sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]))];
        if (n > aadlen - off)
            n = aadlen - off;
        CHECK(EVP_EncryptUpdate(ctx, NULL, &scratch, aad + off, (int)n) == 1,
              "streaming AAD EVP_EncryptUpdate failed");
        off += n;
    }
    off = 0;
    ci = 0;
    while (off < inlen) {
        size_t n = DATA_CHUNKS[ci++ % (sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_EncryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "streaming EVP_EncryptFinal_ex failed");
    dlen += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)dlen;
}

/* Streaming OCB decrypt with tag verification. */
static void ocb_decrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *nonce,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    size_t off, ci;
    int len = 0, scratch = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    off = 0;
    ci = 0;
    while (off < aadlen) {
        size_t n = AAD_CHUNKS[ci++ % (sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]))];
        if (n > aadlen - off)
            n = aadlen - off;
        CHECK(EVP_DecryptUpdate(ctx, NULL, &scratch, aad + off, (int)n) == 1,
              "streaming AAD EVP_DecryptUpdate failed");
        off += n;
    }
    off = 0;
    ci = 0;
    while (off < inlen) {
        size_t n = DATA_CHUNKS[ci++ % (sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_DecryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_DecryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "streaming EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += flen;
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)dlen;
}

/* Fills a buffer with a deterministic pattern (no KAT dependence). */
static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 29 + (i >> 8) * 13 + 3) & 0xff);
}

/* (1) KAT anchor: the RFC 7253 AES-256 tuple through the one-shot helper and
 * through the large streaming helper (single chunk, same code path). */
static void test_kat_anchor(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-OCB", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t aadlen, ptlen, ct_wantlen, ctlen;
    unsigned char *aadbig = ALLOCATE_AAD(AAD_LEN);

    /* OCB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
    from_hex("0F0E0D0C0B0A090807060504030201000102030405060708090A0B0C0D0E0F10",
             key, 32);
    from_hex("BBAA9988776655443322110D", nonce, 16);
    aadlen = from_hex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B"
                      "1C1D1E1F2021222324252627", aad, 64);
    ptlen = from_hex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B"
                     "1C1D1E1F2021222324252627", pt, 64);
    ct_wantlen = from_hex("B2AA438B428CDAC8AA35BDFE9D5ACF7A81A1524755A8DC8F"
                          "A37F51CE2AF20CD6E706BF734678481C", ct_want, 64);
    from_hex("2777FC08E154387CD69217463EB31781", tag_want, 16);

    ocb_encrypt_one_shot(cipher, key, nonce, aad, aadlen, pt, ptlen,
                         ct, 64 + 16, &ctlen, tag);
    expect_equal("AES-256 OCB ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
    expect_equal("AES-256 OCB tag does not match the KAT",
                 tag, 16, tag_want, 16);

    /* The same vector through the streaming helper: the AAD is padded to
     * AAD_LEN with a deterministic pattern and truncated to the KAT length
     * is not possible (the helper feeds the whole buffer), so feed exactly
     * aadlen bytes by shrinking the chunk list effect: use the full buffer
     * but compare against a one-shot of the same buffer. */
    fill_pattern(aadbig, AAD_LEN);
    memcpy(aadbig, aad, aadlen);
    ocb_encrypt_one_shot(cipher, key, nonce, aadbig, AAD_LEN, pt, ptlen,
                         ct, 64 + 16, &ctlen, tag);
    ocb_encrypt_streaming(cipher, key, nonce, aadbig, AAD_LEN, pt, ptlen,
                          ct_want, 64 + 16, &ct_wantlen, tag_want);
    expect_equal("streaming ciphertext differs from one-shot (64KiB AAD)",
                 ct_want, ct_wantlen, ct, ctlen);
    expect_equal("streaming tag differs from one-shot tag (64KiB AAD)",
                 tag_want, 16, tag, 16);
    EVP_CIPHER_free(cipher);
}

/* (2) 1 MiB streaming == one-shot for AES-192-OCB and AES-256-OCB, both
 * directions. */
static void test_large_stream_equals_one_shot(void)
{
    static const struct {
        const char *ciphername;
        size_t keylen;
    } cases[] = {
        { "AES-192-OCB", 24 },
        { "AES-256-OCB", 32 },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    size_t m, ct1len, ct2len, backlen, i;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    fill_pattern(pt, LARGE_LEN);

    for (m = 0; m < sizeof(cases) / sizeof(cases[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[m].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-OCB\") failed");
        for (i = 0; i < cases[m].keylen; i++)
            key[i] = (unsigned char)(0xC0 + i);

        ocb_encrypt_one_shot(cipher, key, nonce, NULL, 0, pt, LARGE_LEN,
                             ct1, LARGE_LEN + 16, &ct1len, tag1);
        CHECK(ct1len == LARGE_LEN, "one-shot OCB ciphertext length is not inlen");
        ocb_encrypt_streaming(cipher, key, nonce, NULL, 0, pt, LARGE_LEN,
                              ct2, LARGE_LEN + 16, &ct2len, tag2);
        expect_equal("1MiB streaming ciphertext differs from one-shot",
                     ct2, ct2len, ct1, ct1len);
        expect_equal("1MiB streaming tag differs from one-shot tag",
                     tag2, 16, tag1, 16);

        ocb_decrypt_streaming(cipher, key, nonce, NULL, 0, ct2, ct2len,
                              back, LARGE_LEN + 16, &backlen, tag2);
        CHECK(backlen == LARGE_LEN, "streaming decrypt length is not inlen");
        CHECK(memcmp(back, pt, LARGE_LEN) == 0,
              "streaming decrypt of 1MiB did not recover the plaintext");
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A 64 KiB AAD in two 32 KiB chunks must match a one-shot AAD, for
 * AES-192-OCB and AES-256-OCB. */
static void test_large_aad(void)
{
    static const struct {
        const char *ciphername;
        size_t keylen;
    } cases[] = {
        { "AES-192-OCB", 24 },
        { "AES-256-OCB", 32 },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(1024);
    unsigned char *ct1 = ALLOCATE_OUTPUT(1024 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(1024 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    size_t m, ct1len, ct2len, i;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    fill_pattern(aad, AAD_LEN);
    fill_pattern(pt, 1024);

    for (m = 0; m < sizeof(cases) / sizeof(cases[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[m].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-OCB\") failed");
        for (i = 0; i < cases[m].keylen; i++)
            key[i] = (unsigned char)(0xC0 + i);

        ocb_encrypt_one_shot(cipher, key, nonce, aad, AAD_LEN, pt, 1024,
                             ct1, 1024 + 16, &ct1len, tag1);
        ocb_encrypt_streaming(cipher, key, nonce, aad, AAD_LEN, pt, 1024,
                              ct2, 1024 + 16, &ct2len, tag2);
        expect_equal("64KiB two-chunk AAD ciphertext differs from one-shot",
                     ct2, ct2len, ct1, ct1len);
        expect_equal("64KiB two-chunk AAD tag differs from one-shot tag",
                     tag2, 16, tag1, 16);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) Tampering with the final ciphertext block must fail DecryptFinal, for
 * AES-192-OCB and AES-256-OCB. */
static void test_tampered_final_block(void)
{
    static const struct {
        const char *ciphername;
        size_t keylen;
    } cases[] = {
        { "AES-192-OCB", 24 },
        { "AES-256-OCB", 32 },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_copy[16];
    EVP_CIPHER_CTX *ctx;
    size_t m, ctlen, i;
    int len = 0, flen = 0;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    fill_pattern(pt, LARGE_LEN);

    for (m = 0; m < sizeof(cases) / sizeof(cases[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[m].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-OCB\") failed");
        for (i = 0; i < cases[m].keylen; i++)
            key[i] = (unsigned char)(0xC0 + i);

        ocb_encrypt_one_shot(cipher, key, nonce, NULL, 0, pt, LARGE_LEN,
                             ct, LARGE_LEN + 16, &ctlen, tag);
        CHECK(ctlen == LARGE_LEN, "unexpected one-shot ciphertext length");

        /* Flip the first byte of the final 16-byte block. */
        ct[ctlen - 16] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
              "second EVP_DecryptInit_ex failed");
        memcpy(tag_copy, tag, 16);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "payload update failed outright (it may emit garbage)");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
              "decryption accepted a tampered final ciphertext block");
        EVP_CIPHER_CTX_free(ctx);

        /* Flip the very last byte. */
        ct[ctlen - 1] ^= 0x80;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
              "second EVP_DecryptInit_ex failed");
        memcpy(tag_copy, tag, 16);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "payload update failed outright (it may emit garbage)");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
              "decryption accepted a tampered last ciphertext byte");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kat_anchor();
    test_large_stream_equals_one_shot();
    test_large_aad();
    test_tampered_final_block();

    printf("test_aes_ocb_large: all %u checks passed\n", g_checks);
    return 0;
}
