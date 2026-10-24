#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_ctr -- AES-128/192/256-CTR known-answer and behavior tests.
 *
 * Known-answer vectors: NIST SP 800-38A, section F.5 (CTR example vectors,
 * counter/IV f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff, shared 4-block plaintext).
 * Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ctr -K <keyhex> -iv f0f1...feff
 *
 * The counter-carry section uses the SP 800-38A key/plaintext with IV
 * fffffffffffffffffffffffffffffffd and 64 bytes of plaintext, so that the
 * fourth block is enciphered with counter 0x0000...0000 after the 128-bit
 * wrap; its ciphertext was verified against the same CLI.
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
    fprintf(stderr, "FAIL test_aes_ctr: %s\n", msg);
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

/* Shared SP 800-38A material. */
#define KAT_PTLEN 64

static const char *const KAT_PT_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

static const struct ctr_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} ctr_kats[] = {
    { "AES-128-CTR",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "874d6191b620e3261bef6864990db6ce"
      "9806f66b7970fdff8617187bb9fffdff"
      "5ae4df3edbd5d35e5b4f09020db03eab"
      "1e031dda2fbe03d1792170a0f3009cee" },
    { "AES-192-CTR",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "1abc932417521ca24f2b0459fe7e6e0b"
      "090339ec0aa6faefd5ccc2c6f4ce8e94"
      "1e36b26bd1ebc670d1bd1d665620abf7"
      "4f78a7f6d29809585a97daec58c6b050" },
    { "AES-256-CTR",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "601ec313775789a5b7a7f504bbf3d228"
      "f443e3ca4d62b59aca84e990cacaf5c5"
      "2b0930daa23de94ce87017ba2d84988d"
      "dfc9c58db67aada613c2dd08457941a6" },
};

static const char *const KAT_IV_HEX =
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

/* Counter-carry vectors: SP 800-38A keys/plaintext, IV ff..fd, 64 bytes
 * (the 4th block uses counter 0x0000...0000 -- the 128-bit carry).
 * Verified against pizfix/bin/openssl enc -aes-<n>-ctr -K <key> -iv ff..fd. */
static const char *const CARRY_IV_HEX =
    "fffffffffffffffffffffffffffffffd";

static const char *const CARRY_128_HEX =
    "953b86f8c8073dbe7e23a5132ffd65c8"
    "7f9a9ee1e5f6536db62d818609e163f2"
    "ba3a9a47e1ab62e5eccbbd032574f843"
    "8b684f49c5f702a49369b13c5f77637f";
static const char *const CARRY_192_HEX =
    "9c4c15d9d83c72a9416b76444ad20b41"
    "4de612fb986512ea8b367261ef5ccbc9"
    "07ca2d0fbdaad7e0d004a17368b78a5d"
    "d4da09cb96e73e84325860b50c01665b";
static const char *const CARRY_256_HEX =
    "df1b19459e821b099fa3ed1c76994b65"
    "e1b03d150b59699e5c1947ae7e9cb8b5"
    "0bf435676b06c0cf7f3dc7d777173223"
    "13f7d2c44b80edc1ba660dbba57c9f44";

/* One-shot CTR encrypt/decrypt (CTR is symmetric; `enc` selects the route).
 * Returns the total output length written to `out`, which must have room for
 * inlen + block_size bytes per the EVP output-buffer contract. */
static size_t ctr_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, const unsigned char *in,
                        size_t inlen, unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_CipherUpdate failed");
    total = len;
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &len) == 1,
          "EVP_CipherFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (the last chunk is truncated to what remains).  Splits must reproduce the
 * one-shot result exactly. */
static size_t ctr_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *iv, const unsigned char *in,
                         size_t inlen, unsigned char *out, size_t outcap, int enc)
{
    static const size_t chunks[] = { 1, 15, 16, 17, 31 };
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_CipherUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_CipherUpdate failed in streaming test");
        total += len;
        off += n;
    }
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &len) == 1,
          "EVP_CipherFinal_ex failed in streaming test");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and decrypt roundtrip for all
 * three key sizes. */
static void test_fetched_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(ctr_kats) / sizeof(ctr_kats[0]); k++) {
        const struct ctr_kat *kat = &ctr_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-CTR is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CTR cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "AES-CTR IV length is not 16");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = ctr_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("one-shot ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = ctr_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                          KAT_PTLEN + 16, 0);
        expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions (CTR keeps the partial keystream block
 * in the context, so every split point must be transparent). */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CTR\") failed");
    from_hex(ctr_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(ctr_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = ctr_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ctr_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (3) Counter carry across the 128-bit boundary: with IV ff..fd the fourth
 * 16-byte block is enciphered with counter 0x0000...0000.  All three key
 * sizes, one-shot and split, checked against the CLI-verified ciphertexts. */
static void test_counter_carry(void)
{
    static const char *const carry_hex[] = { CARRY_128_HEX, CARRY_192_HEX,
                                             CARRY_256_HEX };
    size_t k;
    for (k = 0; k < sizeof(ctr_kats) / sizeof(ctr_kats[0]); k++) {
        const struct ctr_kat *kat = &ctr_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *ct_split = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t ct_wantlen, ctlen, ct_splitlen;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CTR cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(CARRY_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(carry_hex[k], ct_want, KAT_PTLEN);

        ctlen = ctr_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("counter-carry ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ct_splitlen = ctr_stream(cipher, key, iv, pt, KAT_PTLEN, ct_split,
                                 KAT_PTLEN + 16, 1);
        expect_equal("counter-carry split-update ciphertext differs from one-shot",
                     ct_split, ct_splitlen, ct, ctlen);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) Partial final block: CTR is a stream mode, so a 37-byte message (two
 * blocks plus five bytes) round-trips, in one shot and in place. */
static void test_partial_final_block(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CTR\") failed");
    from_hex(ctr_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 3 + 9);

    ctlen = ctr_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CTR encryption did not emit 37 bytes");
    ptlen = ctr_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);
    EVP_CIPHER_free(cipher);
}

/* (5) In-place operation: CTR must accept out == in for both directions. */
static void test_in_place(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *buf = ALLOCATE_OUTPUT(64 + 16); /* in place, contract sized */
    unsigned char *ref = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *in = ALLOCATE_INPUT(64);
    size_t i, ctlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CTR\") failed");
    from_hex(ctr_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 5 + 2);

    ctlen = ctr_crypt(cipher, key, iv, in, 64, ref, 64 + 16, 1);

    memcpy(buf, in, 64);
    ctr_crypt(cipher, key, iv, buf, 64, buf, 64 + 16, 1);   /* in-place encrypt */
    expect_equal("in-place CTR encryption differs from out-of-place",
                 buf, 64, ref, ctlen);

    ctr_crypt(cipher, key, iv, buf, 64, buf, 64 + 16, 0);   /* in-place decrypt */
    expect_equal("in-place CTR roundtrip did not recover the plaintext",
                 buf, 64, in, 64);
    EVP_CIPHER_free(cipher);
}

/* (6) Two fresh contexts with the same key and IV produce identical
 * keystreams; the partial-block counter position (EVP_CIPHER_CTX_get_num)
 * tracks how much of the current keystream block was consumed. */
static void test_keystream_and_num(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *zero = ALLOCATE_INPUT(48);
    unsigned char *ks1 = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ks2 = ALLOCATE_OUTPUT(48 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CTR\") failed");
    from_hex(ctr_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    memset(zero, 0, 48);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks1, &len, zero, 48) == 1,
          "EVP_EncryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "second EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks2, &len, zero, 48) == 1,
          "EVP_EncryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(len == 48, "48-byte keystream extraction did not emit 48 bytes");
    CHECK(memcmp(ks1, ks2, 48) == 0,
          "two fresh contexts produced different keystreams");

    /* Partial-block state: after a 5-byte update the context reports 5 bytes
     * of the current keystream block consumed; 11 more complete the block. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks1, &len, zero, 5) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "counter position after a 5-byte update is not 5");
    CHECK(EVP_EncryptUpdate(ctx, ks1 + 5, &len, zero + 5, 0) == 1,
          "zero-length CTR update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "zero-length update disturbed the counter position");
    CHECK(EVP_EncryptUpdate(ctx, ks1 + 5, &len, zero + 5, 11) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "counter position did not return to 0 after a full block");
    for (i = 0; i < 16; i++)
        if (ks1[i] != ks2[i])
            test_failed("5+0+11 updates diverged from the one-shot keystream");
    g_checks++;
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_fetched_kats();
    test_streaming();
    test_counter_carry();
    test_partial_final_block();
    test_in_place();
    test_keystream_and_num();

    printf("test_aes_ctr: all %u checks passed\n", g_checks);
    return 0;
}
