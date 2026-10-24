#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_cfb128 -- AES-128/192/256-CFB(128) known-answer and behavior
 * tests.
 *
 * Known-answer vectors: NIST SP 800-38A, section F.3 (the CFB128 example
 * vectors for all three key sizes; both the .Encrypt and the .Decrypt
 * examples are covered below, since CFB decryption uses the forward cipher).
 * Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb -K <keyhex> -iv 000102...0f
 *   printf '<ct hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -d -aes-128-cfb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb and -aes-256-cfb).  CFB128 keeps a 16-byte
 * shift register in the context; EVP_CIPHER_CTX_get_num() reports the byte
 * offset inside that register, which the partial-update section below pins
 * down.
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
    fprintf(stderr, "FAIL test_aes_cfb128: %s\n", msg);
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

static const struct cfb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} cfb_kats[] = {
    { "AES-128-CFB",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "3b3fd92eb72dad20333449f8e83cfb4a"
      "c8a64537a0b3a93fcde3cdad9f1ce58b"
      "26751f67a3cbb140b1808cf187a4f4df"
      "c04b05357c5d1c0eeac4c66f9ff7f2e6" },
    { "AES-192-CFB",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "cdc80d6fddf18cab34c25909c99a4174"
      "67ce7f7f81173621961a2b70171d3d7a"
      "2e1e8a1dd59b88b1c8e60fed1efac4c9"
      "c05f9f9ca9834fa042ae8fba584b09ff" },
    { "AES-256-CFB",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "dc7e84bfda79164b7ecd8486985d3860"
      "39ffed143b28b1c832113c6331e5407b"
      "df10132415e54b92a13ed0a8267ae2f9"
      "75a385741ab9cef82031623d55b1e471" },
};

/* One-shot CFB128 encrypt/decrypt.  Returns the total output length written
 * to `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
static size_t cfb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
static size_t cfb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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
 * sizes: encrypting the plaintext matches the F.3 CFB128 .Encrypt vectors,
 * and decrypting those ciphertexts through the decrypt route (which uses the
 * forward cipher) matches the F.3 .Decrypt vectors. */
static void test_fetched_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(cfb_kats) / sizeof(cfb_kats[0]); k++) {
        const struct cfb_kat *kat = &cfb_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-CFB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CFB cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "AES-CFB IV length is not 16");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = cfb_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("one-shot ciphertext does not match the CFB128 KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = cfb_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                          KAT_PTLEN + 16, 0);
        expect_equal("decrypting the CFB128 KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions. */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB\") failed");
    from_hex(cfb_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(cfb_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = cfb_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (3) num-state continuity: CFB128's shift register position is observable
 * through EVP_CIPHER_CTX_get_num() and must survive zero-length updates.
 * After 5 bytes the position is 5; after 11 more it wraps to 0; a further 3
 * bytes move it to 3 again. */
static void test_num_continuity(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(19);
    unsigned char *one = ALLOCATE_OUTPUT(19 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(19 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB\") failed");
    from_hex(cfb_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 19; i++)
        in[i] = (unsigned char)(i * 3);

    cfb_crypt(cipher, key, iv, in, 19, one, 19 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte CFB update failed");
    CHECK(len == 5, "5-byte CFB update did not emit 5 bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "shift-register position after a 5-byte update is not 5");

    /* A zero-byte update is a no-op that leaves the state untouched. */
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-length CFB update failed");
    CHECK(len == 0, "zero-length CFB update emitted bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "zero-length update disturbed the shift-register position");

    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte CFB update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "shift-register position did not wrap to 0 after 16 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 16, &len, in + 16, 3) == 1,
          "3-byte CFB update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 3,
          "shift-register position after a 3-byte update is not 3");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 19, &len) == 1,
          "CFB Final failed");
    CHECK(len == 0, "CFB Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(memcmp(one, many, 19) == 0,
          "5+0+11+3 updates diverged from a single 19-byte update");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (4) The same continuity holds on the decrypt route, and a 37-byte message
 * round-trips through split updates in both directions. */
static void test_decrypt_num_and_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-CFB", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-CFB\") failed");
    from_hex(cfb_kats[2].keyhex, key, 32);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 11 + 5);

    ctlen = cfb_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CFB encryption did not emit 37 bytes");

    ptlen = cfb_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);

    /* Decrypt-route num: identical shift-register bookkeeping. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, pt_back, &len, ct, 7) == 1,
          "7-byte CFB decrypt update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 7,
          "decrypt shift-register position after a 7-byte update is not 7");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_fetched_kats();
    test_streaming();
    test_num_continuity();
    test_decrypt_num_and_roundtrip();

    printf("test_aes_cfb128: all %u checks passed\n", g_checks);
    return 0;
}
