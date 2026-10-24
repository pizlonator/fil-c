#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_chacha20_large -- ChaCha20 at sizes where the implementation's
 * block-boundary and counter-carry code paths matter.
 *
 * Implementation evidence (providers/implementations/ciphers/
 * cipher_chacha20_hw.c):
 *
 *   - a PARTIAL 64-byte keystream block bridges uneven updates: an update
 *     that ends mid-block stashes the remaining keystream in
 *     ctx->buf/partial_len and consumes it before generating new
 *     keystream on the next update (:45-61 and :98-110), so ANY chunk
 *     split must reproduce the one-shot output exactly;
 *   - the 32-bit block counter carries: ChaCha20_ctr32 is called with
 *     the remaining counter and wraps are propagated into the NEXT STATE
 *     WORD, i.e. into the first nonce word (counter[1]++, :64 and
 *     :82-95, :102-104) -- so an IV whose counter starts at 0xffffffff
 *     wraps into the nonce after the first 64-byte block, and one-shot
 *     and chunked encryption must agree across that boundary.
 *
 * The counter-wrap KAT below (128 bytes of zeros under the RFC 8439
 * 2.4.2 key with IV ffffffff000000090000004a00000000, i.e. counter
 * 0xffffffff) was derived with a scratch probe compiled by
 * build/bin/filcc against the pizfix 3.6.5 build and cross-checked to be
 * identical on the system 3.0.13 build (`openssl enc` accepts the same
 * -iv and produces the same output, which is how the constants were
 * double-checked).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_chacha20_large: %s\n", msg);
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

static const char *const KEY_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f";
/* counter = 0xffffffff (little-endian) || nonce 000000090000004a00000000 */
static const char *const WRAPIV_HEX =
    "ffffffff000000090000004a00000000";

/* The 128-byte counter-wrap KAT (encrypting 128 zero bytes). */
static const char *const WRAP_CT128_HEX =
    "ff2941b8d740f6cbb50936bf997ebd52"
    "18cb108dc53f41c64841d0218167430c"
    "a03b770ca74ccb642a28194d1dedd2ed"
    "13151e25ec5d7faeb6d060bfb7e6b146"
    "880b67b55162bca26abe045fad14b0f4"
    "92a3f369dcd52f98bc1513eaf238a3f4"
    "34c7527121b4b756613e270395358d83"
    "1d4950b6c7812fb724dc7c9be5e5c62e";

#define ONE (2u * 1024u * 1024u) /* 2 MiB */

/* One-shot encrypt; ChaCha20 is a stream cipher so Final emits nothing.
 * Returns the output length. */
static size_t chacha20_oneshot(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *iv, const unsigned char *in,
                               size_t inlen, unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "one-shot EVP_EncryptFinal_ex failed");
    CHECK(flen == 0, "one-shot ChaCha20 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated).  Must reproduce the one-shot output exactly. */
static size_t chacha20_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *iv, const unsigned char *in,
                               size_t inlen, unsigned char *out, size_t outcap,
                               const size_t *chunks, size_t nchunks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0, updates = 0;
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(inlen + 64 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in streaming test");
        total += len;
        off += n;
        updates++;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed in streaming test");
    total += len;
    CHECK(total == (int)inlen, "the chunked output length is wrong");
    CHECK(updates > nchunks, "too few updates to exercise state continuity");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) 2 MiB through the mixed grid {1000, 64, 1, 1024, 65536} == one-shot,
 * and the one-shot ciphertext decrypts (chunked) back to the plaintext. */
static void test_two_megabytes(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(ONE);
    unsigned char *oneshot = ALLOCATE_OUTPUT(ONE);
    unsigned char *chunked = ALLOCATE_OUTPUT(ONE + 64);
    static const size_t chunks[] = { 1000, 64, 1, 1024, 65536 };
    size_t i, olen, clen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    memset(iv, 0, 16); /* counter 0, zero nonce */
    for (i = 0; i < ONE; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    olen = chacha20_oneshot(cipher, key, iv, in, ONE, oneshot, ONE);
    CHECK(olen == ONE, "the 2 MiB one-shot output length is wrong");

    clen = chacha20_chunked(cipher, key, iv, in, ONE, chunked, ONE + 64,
                            chunks, 5);
    expect_equal("the 2 MiB chunked ciphertext differs from the one-shot "
                 "ciphertext", chunked, clen, oneshot, olen);
    EVP_CIPHER_free(cipher);
}

/* (2) Decrypt direction at 2 MiB: chunked decryption of the one-shot
 * ciphertext recovers the plaintext. */
static void test_two_megabytes_decrypt(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(ONE);
    unsigned char *ct = ALLOCATE_OUTPUT(ONE);
    unsigned char *back = ALLOCATE_OUTPUT(ONE + 64);
    static const size_t chunks[] = { 1000, 64, 1, 1024, 65536 };
    EVP_CIPHER_CTX *ctx;
    size_t i, off = 0, ci = 0;
    int len = 0, total = 0;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    memset(iv, 0, 16);
    for (i = 0; i < ONE; i++)
        in[i] = (unsigned char)(i * 13 + 5);

    chacha20_oneshot(cipher, key, iv, in, ONE, ct, ONE);

    /* Chunked decrypt with the same grid. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    while (off < ONE) {
        size_t n = chunks[ci++ % 5];
        if (n > ONE - off)
            n = ONE - off;
        CHECK(EVP_DecryptUpdate(ctx, back + total, &len, ct + off, (int)n) == 1,
              "EVP_DecryptUpdate failed in the 2 MiB decrypt");
        total += len;
        off += n;
    }
    CHECK(EVP_DecryptFinal_ex(ctx, back + total, &len) == 1,
          "EVP_DecryptFinal_ex failed in the 2 MiB decrypt");
    CHECK(total == (int)ONE, "the 2 MiB decrypt output length is wrong");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(back, in, ONE) == 0,
          "the 2 MiB chunked decrypt did not recover the plaintext");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (3) Counter near 2^32: IV counter 0xffffffff wraps into the first nonce
 * word after the first 64-byte block.  One-shot and every split grid
 * {64x2, 63+2, 65+63, byte-at-a-time} must agree over 128+ bytes, and the
 * one-shot equals the embedded KAT. */
static void test_counter_wrap(void)
{
    static const char *const grids[] = { "64,64", "63,2,63", "65,63", "1" };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *zeros = ALLOCATE_INPUT(136);
    unsigned char *oneshot = ALLOCATE_OUTPUT(136);
    unsigned char *chunked = ALLOCATE_OUTPUT(136 + 64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(128);
    size_t g, ct_wantlen, olen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(WRAPIV_HEX, iv, 16);
    memset(zeros, 0, 136);
    ct_wantlen = from_hex(WRAP_CT128_HEX, ct_want, 128);

    olen = chacha20_oneshot(cipher, key, iv, zeros, 136, oneshot, 136);
    CHECK(olen == 136, "the counter-wrap one-shot output length is wrong");
    /* The KAT covers the first 128 bytes (the recorded probe output). */
    expect_equal("the counter-wrap one-shot ciphertext does not match the KAT",
                 oneshot, 128, ct_want, ct_wantlen);

    for (g = 0; g < sizeof(grids) / sizeof(grids[0]); g++) {
        size_t chunks[16];
        size_t nchunks = 0, clen, off = 0;
        const char *p = grids[g];
        char tmp[8];
        size_t t = 0;

        /* Parse the comma-separated grid from the label. */
        while (*p) {
            if (*p == ',') {
                tmp[t] = '\0';
                chunks[nchunks++] = (size_t)atoi(tmp);
                t = 0;
                p++;
            } else {
                tmp[t++] = *p++;
            }
        }
        tmp[t] = '\0';
        chunks[nchunks++] = (size_t)atoi(tmp);

        /* chunked() cycles the grid; extend to >128 bytes of input so the
         * grid repeats and no update is a plain "remaining" tail. */
        clen = chacha20_chunked(cipher, key, iv, zeros, 136, chunked, 136 + 64,
                                chunks, nchunks);
        snprintf(msg, sizeof(msg),
                 "the counter-wrap ciphertext differs from one-shot under the "
                 "{%s} grid", grids[g]);
        expect_equal(msg, chunked, clen, oneshot, olen);
        (void)off;
    }
    EVP_CIPHER_free(cipher);
}

/* (4) 64-aligned vs misaligned chunk grids both roundtrip at 1 MiB with
 * the wrap IV (counter wrap + partial-block carry combined). */
static void test_grids_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(ONE / 2);
    unsigned char *ct = ALLOCATE_OUTPUT(ONE / 2);
    unsigned char *back = ALLOCATE_OUTPUT(ONE / 2 + 64);
    static const size_t aligned[] = { 64 };
    static const size_t misaligned[] = { 65, 17, 1, 65536 };
    size_t i, ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(WRAPIV_HEX, iv, 16);
    for (i = 0; i < ONE / 2; i++)
        in[i] = (unsigned char)(i * 3 + 11);

    ctlen = chacha20_oneshot(cipher, key, iv, in, ONE / 2, ct, ONE / 2);

    backlen = chacha20_chunked(cipher, key, iv, ct, ctlen, back, ONE / 2 + 64,
                               aligned, 1);
    expect_equal("the 64-aligned-grid decrypt of the 1 MiB wrap-IV ciphertext "
                 "did not recover the plaintext", back, backlen, in, ONE / 2);

    backlen = chacha20_chunked(cipher, key, iv, ct, ctlen, back, ONE / 2 + 64,
                               misaligned, 4);
    expect_equal("the misaligned-grid decrypt of the 1 MiB wrap-IV ciphertext "
                 "did not recover the plaintext", back, backlen, in, ONE / 2);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_two_megabytes();
    test_two_megabytes_decrypt();
    test_counter_wrap();
    test_grids_roundtrip();

    printf("test_chacha20_large: all %u checks passed\n", g_checks);
    return 0;
}
