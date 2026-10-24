#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_cbc -- AES-128/192/256-CBC known-answer and behavior tests.
 *
 * Known-answer vectors: NIST SP 800-38A, sections F.2.1 (AES-128),
 * F.2.5 (AES-192), and F.2.9 (AES-256).  All three use the same 4-block
 * plaintext and the same IV (00..0f); each has its own key and ciphertext.
 * The hex below was verified against:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cbc -nopad -K <key> -iv <iv>
 *
 * (and likewise for -aes-192-cbc and -aes-256-cbc).  The vectors are
 * block-aligned, so every CBC comparison in this file runs with padding
 * disabled; padding itself gets its own section below.
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
    fprintf(stderr, "FAIL test_aes_cbc: %s\n", msg);
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

/* The SP 800-38A vectors.  Shared plaintext and IV, one key/ciphertext per
 * key size (all 16 bytes of IV are 000102...0f). */
#define KAT_PTLEN 64

static const char *const KAT_IV_HEX =
    "000102030405060708090a0b0c0d0e0f";
static const char *const KAT_PT_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

static const struct cbc_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} cbc_kats[] = {
    { "AES-128-CBC",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "7649abac8119b246cee98e9b12e9197d"
      "5086cb9b507219ee95db113a917678b2"
      "73bed6b8e3c1743b7116e69e22229516"
      "3ff1caa1681fac09120eca307586e1a7" },
    { "AES-192-CBC",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "4f021db243bc633d7178183a9fa071e8"
      "b4d9ada9ad7dedf4e5e738763f69145a"
      "571b242012fb7ae07fa9baac3df102e0"
      "08b0e27988598881d920a9e64f5615cd" },
    { "AES-256-CBC",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
      "9cfc4e967edb808d679f777bc6702c7d"
      "39f23369a9d9bacfa530e26304231461"
      "b2eb05e2c39be9fcda6c19078c6a9d1b" },
};

/* One-shot CBC encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
static size_t cbc_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *iv, const unsigned char *in,
                          size_t inlen, unsigned char *out, size_t outcap,
                          int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_EncryptUpdate failed");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* One-shot CBC decrypt; mirror image of cbc_encrypt(). */
static size_t cbc_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *iv, const unsigned char *in,
                          size_t inlen, unsigned char *out, size_t outcap,
                          int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_DecryptUpdate failed");
    total = len;
    CHECK(EVP_DecryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_DecryptFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (the last chunk is truncated to what remains).  Returns the total output
 * length.  Splits must reproduce the one-shot result exactly, which is only
 * possible if every intermediate buffer size honors the EVP contract. */
static size_t cbc_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *iv, const unsigned char *in,
                         size_t inlen, unsigned char *out, size_t outcap,
                         int enc, int padding)
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > inlen - off)
            n = inlen - off;
        if (enc)
            CHECK(EVP_EncryptUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
                  "EVP_EncryptUpdate failed in streaming test");
        else
            CHECK(EVP_DecryptUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
                  "EVP_DecryptUpdate failed in streaming test");
        total += len;
        off += n;
    }
    if (enc)
        CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
              "EVP_EncryptFinal_ex failed in streaming test");
    else
        CHECK(EVP_DecryptFinal_ex(ctx, out + total, &len) == 1,
              "EVP_DecryptFinal_ex failed in streaming test");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) EVP_CIPHER_fetch(name) one-shot encrypt == KAT, and decrypting the KAT
 * ciphertext gives back the plaintext.  Runs for all three key sizes. */
static void test_fetched_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(cbc_kats) / sizeof(cbc_kats[0]); k++) {
        const struct cbc_kat *kat = &cbc_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-CBC is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CBC cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = cbc_encrypt(cipher, key, iv, pt, KAT_PTLEN, ct,
                            KAT_PTLEN + 16, 0);
        expect_equal("one-shot ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = cbc_decrypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                            KAT_PTLEN + 16, 0);
        expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) + (3) The same KAT must also hold through the legacy static
 * EVP_aes_128_cbc() route and through EVP_get_cipherbyname(). */
static void test_alternate_routes(void)
{
    const EVP_CIPHER *legacy = EVP_aes_128_cbc();
    const EVP_CIPHER *byname = EVP_get_cipherbyname("aes-128-cbc");
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
    size_t ct_wantlen, ctlen;

    CHECK(legacy != NULL, "EVP_aes_128_cbc() returned NULL");
    CHECK(byname != NULL, "EVP_get_cipherbyname(\"aes-128-cbc\") returned NULL");

    from_hex(cbc_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(cbc_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = cbc_encrypt(legacy, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_aes_128_cbc() ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    memset(ct, 0, KAT_PTLEN + 16);
    ctlen = cbc_encrypt(byname, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_get_cipherbyname() ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
}

/* (5) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC\") failed");
    from_hex(cbc_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(cbc_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = cbc_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1, 0);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cbc_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       KAT_PTLEN + 32, 0, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (6) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with
 * PKCS#5/#7 padding, round-trips, and is rejected with padding disabled. */
static void test_padding(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *pt_back = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC\") failed");
    from_hex(cbc_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    ctlen = cbc_encrypt(cipher, key, iv, pt, 33, ct, 33 + 16, 1);
    CHECK(ctlen == 48, "padded encryption of 33 bytes did not give 48 bytes");
    ptlen = cbc_decrypt(cipher, key, iv, ct, ctlen, pt_back, 48 + 16, 1);
    expect_equal("padded roundtrip did not recover the 33-byte plaintext",
                 pt_back, ptlen, pt, 33);

    /* With padding disabled, a non-multiple-of-16 input cannot be encrypted:
     * the update buffers what it can (emitting the full blocks), and the
     * finalization fails because one byte remains. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 33) == 1,
          "EVP_EncryptUpdate with padding disabled failed outright");
    CHECK(len == 32, "update with padding disabled did not emit 32 full blocks");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + len, &len) != 1,
          "padding-disabled encryption of 33 bytes did not fail at Final");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (7) Output-buffer contract: a single 16-byte update into a buffer of
 * exactly inl + block_size bytes succeeds and produces exactly that many
 * bytes (update 16 + final 16, the latter being the full pad block). */
static void test_output_buffer_contract(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(32); /* exactly inl + block_size */
    EVP_CIPHER_CTX *ctx;
    int len, total;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC\") failed");
    from_hex(cbc_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "EVP_EncryptUpdate into an exact-size buffer failed");
    CHECK(len == 16, "16-byte update did not emit 16 bytes");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed at the end of the exact-size buffer");
    CHECK(len == 16, "padding Final did not emit the 16-byte pad block");
    total += len;
    CHECK(total == 32, "update+final did not fill the exact-size buffer");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_fetched_kats();
    test_alternate_routes();
    test_streaming();
    test_padding();
    test_output_buffer_contract();

    printf("test_aes_cbc: all %u checks passed\n", g_checks);
    return 0;
}
