#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_cfb8 -- AES-128/192/256-CFB8 known-answer and behavior tests.
 *
 * Known-answer vectors: NIST SP 800-38A, section F.3 (the CFB8 example
 * vectors; IV 000102...0f, shared 4-block plaintext processed one byte at a
 * time).  Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb8 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb8 and -aes-256-cfb8).  The first ciphertext
 * byte of the AES-128 vector agrees with the published F.3 CFB8-AES128
 * example (3b79424c...).
 *
 * CFB8 shifts the feedback register one byte at a time, so the defining
 * property tested here is byte-granular state continuity: feeding the
 * plaintext one byte per update must produce exactly the one-shot
 * ciphertext.  (Implementation note: crypto/modes/cfb128.c
 * CRYPTO_cfb128_8_encrypt() keeps the state entirely in the shifted IV and
 * leaves the *num position alone, so EVP_CIPHER_CTX_get_num() stays 0;
 * that is pinned down below.)
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
    fprintf(stderr, "FAIL test_aes_cfb8: %s\n", msg);
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

static const char *const KAT_IV_HEX =
    "000102030405060708090a0b0c0d0e0f";

static const struct cfb8_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} cfb8_kats[] = {
    { "AES-128-CFB8",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "3b79424c9c0dd436bace9e0ed4586a4f"
      "32b9ded50ae3ba69d472e88267fb5052"
      "70cbad1e257691f7c47c5038297edda3"
      "2ff26d0ed19174096161ecc14086dd62" },
    { "AES-192-CFB8",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "cda2521ef0a905ca44cd057cbf0d47a0"
      "678a7bcfb6aeaa3047b38936021f48bb"
      "b63cefdac02b2e840904efce6f4326be"
      "228683739063dc30e937ffedd63e3c94" },
    { "AES-256-CFB8",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "dc1f1a8520a64db55fcc8ac554844e88"
      "9700adc6e10c63cf2d8cd2d8ce668f3e"
      "b9191719c47444fb43bff9b9883c2cd0"
      "51120402009f974998c89d195722a75b" },
};

/* One-shot CFB8 encrypt/decrypt.  Returns the total output length written to
 * `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
static size_t cfb8_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
static size_t cfb8_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot KATs in both directions for all three key
 * sizes: CFB8 decryption uses the forward cipher, so decrypting the KAT
 * ciphertext reproduces the plaintext. */
static void test_fetched_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(cfb8_kats) / sizeof(cfb8_kats[0]); k++) {
        const struct cfb8_kat *kat = &cfb8_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-CFB8 is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CFB8 cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
              "CFB8 block size is not 1");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "AES-CFB8 IV length is not 16");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = cfb8_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("one-shot ciphertext does not match the CFB8 KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = cfb8_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                           KAT_PTLEN + 16, 0);
        expect_equal("decrypting the CFB8 KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) The key CFB8 property: byte-at-a-time updates must be identical to one
 * big update, for every key size, in both directions. */
static void test_byte_at_a_time(void)
{
    size_t k;
    for (k = 0; k < sizeof(cfb8_kats) / sizeof(cfb8_kats[0]); k++) {
        const struct cfb8_kat *kat = &cfb8_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *one = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *many = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        EVP_CIPHER_CTX *ctx;
        int len, j;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CFB8 cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);

        cfb8_crypt(cipher, key, iv, pt, KAT_PTLEN, one, KAT_PTLEN + 16, 1);

        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        for (j = 0; j < (int)KAT_PTLEN; j++) {
            CHECK(EVP_EncryptUpdate(ctx, many + j, &len, pt + j, 1) == 1,
                  "1-byte CFB8 update failed");
            CHECK(len == 1, "1-byte CFB8 update did not emit 1 byte");
        }
        CHECK(EVP_EncryptFinal_ex(ctx, many + KAT_PTLEN, &len) == 1,
              "CFB8 Final failed");
        CHECK(len == 0, "CFB8 Final emitted unexpected bytes");
        CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
              "CFB8 keeps no byte counter, so num must stay 0");
        EVP_CIPHER_CTX_free(ctx);

        CHECK(memcmp(one, many, KAT_PTLEN) == 0,
              "byte-at-a-time CFB8 encryption differs from one-shot encryption");
        g_checks++;

        /* Same property on the decrypt route. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
        for (j = 0; j < (int)KAT_PTLEN; j++)
            CHECK(EVP_DecryptUpdate(ctx, many + j, &len, one + j, 1) == 1,
                  "1-byte CFB8 decrypt update failed");
        CHECK(EVP_DecryptFinal_ex(ctx, many + KAT_PTLEN, &len) == 1,
              "CFB8 decrypt Final failed");
        EVP_CIPHER_CTX_free(ctx);
        CHECK(memcmp(many, pt, KAT_PTLEN) == 0,
              "byte-at-a-time CFB8 decryption differs from one-shot decryption");
        g_checks++;

        EVP_CIPHER_free(cipher);
    }
}

/* (3) Mixed chunk sizes ({1,15,16,17,31}) must also reproduce the one-shot
 * result, in both directions. */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB8", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB8\") failed");
    from_hex(cfb8_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(cfb8_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = cfb8_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb8_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                        KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (4) A zero-byte update is a no-op that must not disturb the shifted-IV
 * state: 5 + 0 + 11 bytes == one 16-byte update, and a 37-byte message
 * round-trips through split updates. */
static void test_zero_update_and_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-CFB8", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *one = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *pt37 = ALLOCATE_INPUT(37);
    unsigned char *ct37 = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *back37 = ALLOCATE_OUTPUT(37 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-CFB8\") failed");
    from_hex(cfb8_kats[2].keyhex, key, 32);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 3);
    for (i = 0; i < 37; i++)
        pt37[i] = (unsigned char)(i * 11 + 5);

    cfb8_crypt(cipher, key, iv, in, 16, one, 16 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte CFB8 update failed");
    CHECK(len == 5, "5-byte CFB8 update did not emit 5 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-byte CFB8 update failed");
    CHECK(len == 0, "zero-byte CFB8 update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte CFB8 update failed");
    CHECK(len == 11, "11-byte CFB8 update did not emit 11 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 16, &len) == 1, "CFB8 Final failed");
    CHECK(len == 0, "CFB8 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(one, many, 16) == 0,
          "5+0+11 updates diverged from a single 16-byte update");
    g_checks++;

    ctlen = cfb8_stream(cipher, key, iv, pt37, 37, ct37, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CFB8 encryption did not emit 37 bytes");
    ptlen = cfb8_stream(cipher, key, iv, ct37, ctlen, back37, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 back37, ptlen, pt37, 37);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_fetched_kats();
    test_byte_at_a_time();
    test_streaming();
    test_zero_update_and_roundtrip();

    printf("test_aes_cfb8: all %u checks passed\n", g_checks);
    return 0;
}
