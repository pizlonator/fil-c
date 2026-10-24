#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_ofb -- AES-128/192/256-OFB (OFB-128) known-answer and behavior
 * tests.
 *
 * Known-answer vectors: NIST SP 800-38A, section F.4 (OFB example vectors,
 * IV 000102...0f, shared 4-block plaintext).  Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ofb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-ofb and -aes-256-ofb).  OFB is symmetric: the
 * decryption route applies exactly the same keystream, so decrypting the KAT
 * ciphertext must reproduce the plaintext.
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
    fprintf(stderr, "FAIL test_aes_ofb: %s\n", msg);
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

static const struct ofb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} ofb_kats[] = {
    { "AES-128-OFB",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "3b3fd92eb72dad20333449f8e83cfb4a"
      "7789508d16918f03f53c52dac54ed825"
      "9740051e9c5fecf64344f7a82260edcc"
      "304c6528f659c77866a510d9c1d6ae5e" },
    { "AES-192-OFB",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "cdc80d6fddf18cab34c25909c99a4174"
      "fcc28b8d4c63837c09e81700c1100401"
      "8d9a9aeac0f6596f559c6d4daf59a5f2"
      "6d9f200857ca6c3e9cac524bd9acc92a" },
    { "AES-256-OFB",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "dc7e84bfda79164b7ecd8486985d3860"
      "4febdc6740d20b3ac88f6ad82a4fb08d"
      "71ab47a086e86eedf39d1c5bba97c408"
      "0126141d67f37be8538f5a8be740e484" },
};

/* One-shot OFB encrypt/decrypt (`enc` selects the route; the keystream is
 * the same either way).  Returns the total output length written to `out`,
 * which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
static size_t ofb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
static size_t ofb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and -- because OFB uses the
 * same keystream in both directions -- decrypting the KAT ciphertext through
 * the decrypt route gives back the plaintext, for all three key sizes. */
static void test_fetched_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(ofb_kats) / sizeof(ofb_kats[0]); k++) {
        const struct ofb_kat *kat = &ofb_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-OFB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OFB cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "AES-OFB IV length is not 16");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = ofb_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("one-shot ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = ofb_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                          KAT_PTLEN + 16, 0);
        expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions; the keystream state must carry across
 * update boundaries. */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-OFB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-OFB\") failed");
    from_hex(ofb_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(ofb_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = ofb_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ofb_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (3) The decryption route applies the same keystream as the encryption
 * route: decrypt(encrypt(x)) == x and, because C = P XOR KS implies
 * C XOR KS == P, encrypt(encrypt(x)) == x as well. */
static void test_decrypt_is_encrypt(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-192-OFB", NULL);
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(53);
    unsigned char *once = ALLOCATE_OUTPUT(53 + 16);
    unsigned char *twice = ALLOCATE_OUTPUT(53 + 16);
    size_t i, once_len, twice_len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-192-OFB\") failed");
    from_hex(ofb_kats[1].keyhex, key, 24);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 53; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    once_len = ofb_crypt(cipher, key, iv, in, 53, once, 53 + 16, 1);
    CHECK(once_len == 53, "53-byte OFB encryption did not emit 53 bytes");

    twice_len = ofb_crypt(cipher, key, iv, once, once_len, twice, 53 + 16, 0);
    expect_equal("OFB decrypt route did not recover the plaintext",
                 twice, twice_len, in, 53);

    twice_len = ofb_crypt(cipher, key, iv, once, once_len, twice, 53 + 16, 1);
    expect_equal("encrypting the ciphertext again did not recover the plaintext",
                 twice, twice_len, in, 53);
    EVP_CIPHER_free(cipher);
}

/* (4) A zero-length update is a no-op that must not disturb the keystream
 * state: 5 bytes + 0 bytes + 11 bytes must equal a single 16-byte update. */
static void test_zero_byte_update_noop(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-OFB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *one = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(16 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-OFB\") failed");
    from_hex(ofb_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 3);

    ofb_crypt(cipher, key, iv, in, 16, one, 16 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte OFB update failed");
    CHECK(len == 5, "5-byte OFB update did not emit 5 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-length OFB update failed");
    CHECK(len == 0, "zero-length OFB update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte OFB update failed");
    CHECK(len == 11, "11-byte OFB update did not emit 11 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 16, &len) == 1,
          "OFB Final failed");
    CHECK(len == 0, "OFB Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(memcmp(one, many, 16) == 0,
          "5+0+11 updates diverged from a single 16-byte update");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (5) A 37-byte message (partial final block) round-trips through the
 * streaming route in both directions. */
static void test_partial_final_block(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-OFB", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-OFB\") failed");
    from_hex(ofb_kats[2].keyhex, key, 32);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 11 + 5);

    ctlen = ofb_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte OFB encryption did not emit 37 bytes");
    ptlen = ofb_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_fetched_kats();
    test_streaming();
    test_decrypt_is_encrypt();
    test_zero_byte_update_noop();
    test_partial_final_block();

    printf("test_aes_ofb: all %u checks passed\n", g_checks);
    return 0;
}
