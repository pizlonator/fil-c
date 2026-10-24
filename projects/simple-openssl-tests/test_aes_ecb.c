#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_ecb -- AES-128/192/256-ECB known-answer and behavior tests.
 *
 * Known-answer vectors: NIST SP 800-38A, section F.1 (ECB example vectors;
 * the AES-128, AES-192, and AES-256 sets use the shared 4-block plaintext).
 * The hex below was verified against the authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ecb -nopad -K <keyhex>
 *
 * (and likewise for -aes-192-ecb and -aes-256-ecb).  ECB has no IV.  The
 * vectors are block-aligned, so every KAT comparison runs with padding
 * disabled; padding itself gets its own section below.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_ecb: %s\n", msg);
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

/* The SP 800-38A vectors: one shared plaintext, one key/ciphertext pair per
 * key size. */

#define KAT_PTLEN 64

static const char *const KAT_PT_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

static const struct ecb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} ecb_kats[] = {
    { "AES-128-ECB",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "3ad77bb40d7a3660a89ecaf32466ef97"
      "f5d3d58503b9699de785895a96fdbaaf"
      "43b1cd7f598ece23881b00e3ed030688"
      "7b0c785e27e8ad3f8223207104725dd4" },
    { "AES-192-ECB",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "bd334f1d6e45f25ff712a214571fa5cc"
      "974104846d0ad3ad7734ecb3ecee4eef"
      "ef7afd2270e2e60adce0ba2face6444e"
      "9a4b41ba738d6c72fb16691603c18e0e" },
    { "AES-256-ECB",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "f3eed1bdb5d2a03c064b5a7e3db181f8"
      "591ccb10d410ed26dc5ba74a31362870"
      "b6ed21b99ca6f4f9f153e7b1beafed1d"
      "23304b7a39f9f3ff067d8d8f9e24ecc7" },
};

/* One-shot ECB encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
static size_t ecb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *in, size_t inlen,
                          unsigned char *out, size_t outcap, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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

/* One-shot ECB decrypt; mirror image of ecb_encrypt(). */
static size_t ecb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *in, size_t inlen,
                          unsigned char *out, size_t outcap, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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
 * (the last chunk is truncated to what remains).  Splits must reproduce the
 * one-shot result exactly, which is only possible if every intermediate
 * buffer size honors the EVP contract. */
static size_t ecb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap, int enc, int padding)
{
    static const size_t chunks[] = { 1, 15, 16, 17, 31 };
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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
    for (k = 0; k < sizeof(ecb_kats) / sizeof(ecb_kats[0]); k++) {
        const struct ecb_kat *kat = &ecb_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-ECB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-ECB cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 16,
              "AES-ECB block size is not 16");

        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = ecb_encrypt(cipher, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
        expect_equal("one-shot ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = ecb_decrypt(cipher, key, ct_want, ct_wantlen, pt_back,
                            KAT_PTLEN + 16, 0);
        expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) + (3) The AES-128/AES-256 KATs must also hold through the legacy static
 * EVP_aes_*_ecb() routes, and the AES-128/AES-192 KATs through
 * EVP_get_cipherbyname() (with both spellings from the spec). */
static void test_alternate_routes(void)
{
    const EVP_CIPHER *legacy128 = EVP_aes_128_ecb();
    const EVP_CIPHER *legacy256 = EVP_aes_256_ecb();
    const EVP_CIPHER *byname128 = EVP_get_cipherbyname("aes-128-ecb");
    const EVP_CIPHER *byname192 = EVP_get_cipherbyname("AES-192-ECB");
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
    size_t ct_wantlen, ctlen;

    CHECK(legacy128 != NULL, "EVP_aes_128_ecb() returned NULL");
    CHECK(legacy256 != NULL, "EVP_aes_256_ecb() returned NULL");
    CHECK(byname128 != NULL, "EVP_get_cipherbyname(\"aes-128-ecb\") returned NULL");
    CHECK(byname192 != NULL, "EVP_get_cipherbyname(\"AES-192-ECB\") returned NULL");

    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);

    /* AES-128 through both the static and the by-name route. */
    from_hex(ecb_kats[0].keyhex, key, 32);
    ct_wantlen = from_hex(ecb_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = ecb_encrypt(legacy128, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_aes_128_ecb() ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    memset(ct, 0, KAT_PTLEN + 16);
    ctlen = ecb_encrypt(byname128, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_get_cipherbyname(\"aes-128-ecb\") ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    /* AES-192 through the uppercase by-name route. */
    from_hex(ecb_kats[1].keyhex, key, 32);
    ct_wantlen = from_hex(ecb_kats[1].cthex, ct_want, KAT_PTLEN);

    memset(ct, 0, KAT_PTLEN + 16);
    ctlen = ecb_encrypt(byname192, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_get_cipherbyname(\"AES-192-ECB\") ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    /* AES-256 through the static route. */
    from_hex(ecb_kats[2].keyhex, key, 32);
    ct_wantlen = from_hex(ecb_kats[2].cthex, ct_want, KAT_PTLEN);

    memset(ct, 0, KAT_PTLEN + 16);
    ctlen = ecb_encrypt(legacy256, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 0);
    expect_equal("EVP_aes_256_ecb() ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
}

/* (4) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext.  Runs for the AES-128 KAT with padding off and
 * for the 33-byte padded payload below with padding on. */
static void test_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-ECB\") failed");
    from_hex(ecb_kats[0].keyhex, key, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(ecb_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = ecb_stream(cipher, key, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1, 0);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ecb_stream(cipher, key, ct_want, ct_wantlen, pt_back,
                       KAT_PTLEN + 32, 0, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);
    EVP_CIPHER_free(cipher);
}

/* (5) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with PKCS#7
 * padding, round-trips, and is rejected with padding disabled. */
static void test_padding(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *pt_back = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-ECB\") failed");
    from_hex(ecb_kats[0].keyhex, key, 16);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    ctlen = ecb_encrypt(cipher, key, pt, 33, ct, 33 + 16, 1);
    CHECK(ctlen == 48, "padded encryption of 33 bytes did not give 48 bytes");
    ptlen = ecb_decrypt(cipher, key, ct, ctlen, pt_back, 48 + 16, 1);
    expect_equal("padded roundtrip did not recover the 33-byte plaintext",
                 pt_back, ptlen, pt, 33);

    /* With padding disabled, a non-multiple-of-16 input cannot be encrypted:
     * the update buffers what it can (emitting the 32 whole bytes) and the
     * finalization fails because one byte remains. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 33) == 1,
          "EVP_EncryptUpdate with padding disabled failed outright");
    CHECK(len == 32, "update with padding disabled did not emit 32 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + len, &len) != 1,
          "padding-disabled encryption of 33 bytes did not fail at Final");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (6) A zero-length no-padding ECB roundtrip is legal: the update is a no-op
 * and Final emits nothing. */
static void test_zero_length(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(1);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-ECB\") failed");
    from_hex(ecb_kats[0].keyhex, key, 16);
    in[0] = 0;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, in, 0) == 1,
          "zero-length encrypt update failed");
    CHECK(len == 0, "zero-length encrypt update emitted bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &len) == 1,
          "zero-length no-padding encrypt Final failed");
    CHECK(len == 0, "zero-length no-padding encrypt Final emitted bytes");

    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, pt_back, &len, ct, 0) == 1,
          "zero-length decrypt update failed");
    CHECK(len == 0, "zero-length decrypt update emitted bytes");
    CHECK(EVP_DecryptFinal_ex(ctx, pt_back, &len) == 1,
          "zero-length no-padding decrypt Final failed");
    CHECK(len == 0, "zero-length no-padding decrypt Final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (7) A key of the wrong length must not be usable: the only public way to
 * present a 24-byte key to a fixed-length AES-128 cipher is
 * EVP_CIPHER_CTX_set_key_length() + init, and OpenSSL must reject the
 * operation at one of those two points.  (EVP reads exactly
 * EVP_CIPHER_CTX_get_key_length() bytes from the caller's buffer, so the
 * init is only attempted when set_key_length() actually resized the
 * context.) */
static void test_wrong_key_length(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *in = ALLOCATE_INPUT(16);
    int rejected, init_ok = 1, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-ECB\") failed");
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    for (i = 0; i < 24; i++)
        key[i] = (unsigned char)(i + 1);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 + i);

    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, 24) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1);
    CHECK(rejected || !init_ok,
          "a 24-byte key was accepted for AES-128-ECB");
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
    test_zero_length();
    test_wrong_key_length();

    printf("test_aes_ecb: all %u checks passed\n", g_checks);
    return 0;
}
