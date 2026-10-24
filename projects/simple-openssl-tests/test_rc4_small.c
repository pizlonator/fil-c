#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_rc4_small -- RC4 and RC4-40 (legacy provider) known-answer and
 * behavior tests for small inputs.  (A sibling program covers >= 64 KiB
 * payloads.)
 *
 * Known-answer vectors: the three classic RC4 test vectors
 *
 *   key "Key"    (4b6579)          plaintext "Plaintext"      (9 bytes)
 *   key "Wiki"   (57696b69)        plaintext "pedia"          (5 bytes)
 *   key "Secret" (536563726574)    plaintext "Attack at dawn" (14 bytes)
 *
 * with the expected ciphertexts below.  They need SHORT keys, which the
 * provider route reaches through EVP_CIPHER_CTX_set_key_length() between
 * a bare init and the key init (the RC4 cipher carries the
 * VARIABLE_LENGTH flag; `openssl enc -K` always zero-pads to 16 bytes
 * and cannot produce these vectors).  The constants were verified with a
 * scratch probe compiled by build/bin/filcc against the pizfix 3.6.5
 * build and cross-checked against the system 3.0.13 (identical).  The
 * RC4-40 vector (fixed 5-byte key) came from `openssl enc -rc4-40
 * -provider legacy -provider default -K <5-byte keyhex>` on both builds.
 *
 * Behavior: RC4 state (the i, j indices and the 256-byte permutation)
 * is carried across updates by the legacy provider's RC4 implementation,
 * so any chunk split must reproduce the one-shot output exactly; the
 * bulk path in rc4_enc.c processes 8 bytes at a time (with the i/j state
 * advanced 8 per step) before a tail of single-byte steps, which the
 * 8-byte-boundary sections below exercise.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rc4_small: %s\n", msg);
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

/* RC4-encrypts `inlen` bytes one-shot with an explicit key length (the
 * key length is re-set between a bare init and the key init when it
 * differs from the default 16).  Returns the output length. */
static size_t rc4_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        size_t keylen, const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    if (keylen != 16) {
        CHECK(EVP_CIPHER_CTX_set_key_length(ctx, (int)keylen) == 1,
              "EVP_CIPHER_CTX_set_key_length failed");
        g_checks++;
    }
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_CipherUpdate failed");
    CHECK(EVP_CipherFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_CipherFinal_ex failed");
    CHECK(flen == 0, "RC4 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated). */
static size_t rc4_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                          size_t keylen, const unsigned char *in, size_t inlen,
                          unsigned char *out, size_t outcap, int enc,
                          const size_t *chunks, size_t nchunks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    if (keylen != 16) {
        CHECK(EVP_CIPHER_CTX_set_key_length(ctx, (int)keylen) == 1,
              "EVP_CIPHER_CTX_set_key_length failed");
        g_checks++;
    }
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_CipherUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_CipherUpdate failed in streaming test");
        total += len;
        off += n;
    }
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &flen) == 1,
          "EVP_CipherFinal_ex failed in streaming test");
    total += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

static const struct rc4_kat {
    const char *keyhex;
    const char *pthex;
    const char *cthex;
} rc4_kats[] = {
    { "4b6579",                          /* "Key" */
      "506c61696e74657874",              /* "Plaintext" */
      "bbf316e8d940af0ad3" },
    { "57696b69",                        /* "Wiki" */
      "7065646961",                      /* "pedia" */
      "1021bf0420" },
    { "536563726574",                    /* "Secret" */
      "41747461636b206174206461776e",    /* "Attack at dawn" */
      "45a01f645fc35b383552544b9bf5" },
};

/* (1) The three classic KATs through the variable-key-length route, plus
 * decrypting each KAT ciphertext (RC4 is symmetric). */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *back = ALLOCATE_OUTPUT(16);
    size_t k, keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    for (k = 0; k < sizeof(rc4_kats) / sizeof(rc4_kats[0]); k++) {
        const struct rc4_kat *kat = &rc4_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);

        /* The legacy provider is loaded in main(); a NULL fetch here is
         * a broken build, not a version gate. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
        keylen = from_hex(kat->keyhex, key, 16);
        ptlen = from_hex(kat->pthex, pt, 16);
        ct_wantlen = from_hex(kat->cthex, ct_want, 16);

        ctlen = rc4_crypt(cipher, key, keylen, pt, ptlen, ct, 16, 1);
        snprintf(msg, sizeof(msg),
                 "classic RC4 KAT %zu: ciphertext does not match", k);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        backlen = rc4_crypt(cipher, key, keylen, ct_want, ct_wantlen, back,
                            16, 0);
        snprintf(msg, sizeof(msg), "classic RC4 KAT %zu: decrypt mismatch", k);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }
}

/* (2) RC4-40 (fixed 5-byte key): KAT plus equality with plain RC4 under
 * the same 5-byte key (RC4-40 is exactly that -- the 40-bit export
 * variant with no key-stretching at the EVP level). */
static void test_rc4_40(void)
{
    static const char *const RC4_40_CT_HEX =
        "1a37602e040e72d424d3dc63072af795";
    EVP_CIPHER *c40 = EVP_CIPHER_fetch(NULL, "RC4-40", NULL);
    EVP_CIPHER *crc4 = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct40 = ALLOCATE_OUTPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    size_t keylen, ptlen, l40, lrc4;

    CHECK(c40 != NULL, "EVP_CIPHER_fetch(\"RC4-40\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(c40) == 5,
          "RC4-40 key length is not 5");
    keylen = from_hex("0123456789", key, 16);
    ptlen = from_hex("0102030405060708090a0b0c0d0e0f10", pt, 16);
    from_hex(RC4_40_CT_HEX, ct_want, 16);

    l40 = rc4_crypt(c40, key, keylen, pt, ptlen, ct40, 16, 1);
    expect_equal("the RC4-40 ciphertext does not match the CLI-derived KAT",
                 ct40, l40, ct_want, 16);

    lrc4 = rc4_crypt(crc4, key, keylen, pt, ptlen, ct, 16, 1);
    expect_equal("RC4-40 differs from RC4 with the same 5-byte key",
                 ct40, l40, ct, lrc4);
    EVP_CIPHER_free(c40);
    EVP_CIPHER_free(crc4);
}

/* (3) One-shot sizes 0..24 with the default 16-byte key: every output
 * length equals its input length and the 8-byte-boundary lengths
 * roundtrip. */
static void test_sizes(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *ct = ALLOCATE_OUTPUT(24);
    unsigned char *back = ALLOCATE_OUTPUT(24);
    size_t n, i, ctlen, backlen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    for (i = 0; i < 24; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (n = 0; n <= 24; n++) {
        ctlen = rc4_crypt(cipher, key, 16, in, n, ct, 24, 1);
        snprintf(msg, sizeof(msg),
                 "RC4 one-shot encryption of %zu bytes gave %zu bytes", n,
                 ctlen);
        CHECK(ctlen == n, msg);
        if (n % 8 == 0 || n == 1) {
            backlen = rc4_crypt(cipher, key, 16, ct, ctlen, back, 24, 0);
            snprintf(msg, sizeof(msg), "the %zu-byte RC4 roundtrip failed", n);
            expect_equal(msg, back, backlen, in, n);
        }
    }
    EVP_CIPHER_free(cipher);
}

/* (4) Streaming {1,1,1,1,5} == one-shot: the i/j state carries across
 * updates, including the 8-byte bulk path boundary. */
static void test_streaming(void)
{
    static const size_t chunks[] = { 1, 1, 1, 1, 5 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *oneshot = ALLOCATE_OUTPUT(24);
    unsigned char *chunked = ALLOCATE_OUTPUT(24);
    size_t n, olen, clen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    for (n = 0; n < 24; n++)
        in[n] = (unsigned char)(n * 3 + 7);

    for (n = 9; n <= 24; n++) {
        olen = rc4_crypt(cipher, key, 16, in, n, oneshot, 24, 1);
        clen = rc4_chunked(cipher, key, 16, in, n, chunked, 24, 1, chunks, 5);
        snprintf(msg, sizeof(msg),
                 "the {1,1,1,1,5} chunked ciphertext differs from one-shot "
                 "at %zu bytes", n);
        expect_equal(msg, chunked, clen, oneshot, olen);
        clen = rc4_chunked(cipher, key, 16, oneshot, olen, chunked, 24, 0,
                           chunks, 5);
        snprintf(msg, sizeof(msg),
                 "the {1,1,1,1,5} chunked decrypt did not recover the "
                 "plaintext at %zu bytes", n);
        expect_equal(msg, chunked, clen, in, n);
    }
    EVP_CIPHER_free(cipher);
}

/* (5) 8-byte-boundary behavior: chunk grids that straddle the bulk path's
 * 8-at-a-time steps (e.g. 7+1+8, 8+8, 15+1+7) must equal one-shot for
 * lengths around every multiple of 8. */
static void test_eight_byte_boundaries(void)
{
    static const size_t grids[][3] = {
        { 7, 1, 8 }, { 8, 8, 5 }, { 15, 1, 7 }, { 9, 7, 5 }
    };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32);
    unsigned char *chunked = ALLOCATE_OUTPUT(32);
    static const size_t lens[] = { 7, 8, 9, 15, 16, 17, 23, 24 };
    size_t g, l, olen, clen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    for (l = 0; l < 32; l++)
        in[l] = (unsigned char)(l * 5 + 11);

    for (l = 0; l < sizeof(lens) / sizeof(lens[0]); l++) {
        olen = rc4_crypt(cipher, key, 16, in, lens[l], oneshot, 32, 1);
        for (g = 0; g < sizeof(grids) / sizeof(grids[0]); g++) {
            clen = rc4_chunked(cipher, key, 16, in, lens[l], chunked, 32, 1,
                               grids[g], 3);
            snprintf(msg, sizeof(msg),
                     "the {%zu,%zu,%zu} grid differs from one-shot at %zu "
                     "bytes", grids[g][0], grids[g][1], grids[g][2], lens[l]);
            expect_equal(msg, chunked, clen, oneshot, olen);
        }
    }
    EVP_CIPHER_free(cipher);
}

/* (6) Empty updates are no-ops (RC4 state untouched): a 0-byte update
 * between two real updates must not perturb the keystream. */
static void test_empty_update(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *oneshot = ALLOCATE_OUTPUT(16);
    unsigned char *with_empty = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    size_t i, olen;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 - i);

    olen = rc4_crypt(cipher, key, 16, in, 16, oneshot, 16, 1);

    /* 8 bytes, an empty update, then the remaining 8. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, with_empty, &len, in, 8) == 1,
          "the first 8-byte update failed");
    CHECK(len == 8, "the first 8-byte update did not emit 8 bytes");
    CHECK(EVP_EncryptUpdate(ctx, with_empty + 8, &len, in + 8, 0) == 1,
          "the empty update failed");
    CHECK(len == 0, "the empty update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, with_empty + 8, &len, in + 8, 8) == 1,
          "the second 8-byte update failed");
    CHECK(len == 8, "the second 8-byte update did not emit 8 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, with_empty + 16, &flen) == 1,
          "the final after the empty update failed");
    CHECK(flen == 0, "RC4 Final emitted bytes after the empty update");
    EVP_CIPHER_CTX_free(ctx);
    expect_equal("an empty update perturbed the RC4 keystream",
                 with_empty, 16, oneshot, olen);
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

    test_kats();
    test_rc4_40();
    test_sizes();
    test_streaming();
    test_eight_byte_boundaries();
    test_empty_update();

    printf("test_rc4_small: all %u checks passed\n", g_checks);
    return 0;
}
