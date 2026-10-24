#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_xts_small -- AES-128/256-XTS known-answer and behavior tests for
 * small data units.  (A sibling program covers the 16 MiB data-unit size
 * boundary.)  An XTS key is the concatenation of two same-length AES keys;
 * the 128-bit IV is the data-unit number.
 *
 * Known-answer vectors: the IEEE Std 1619-2007 XTS-AES test vectors as
 * distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt, "AES XTS test
 * vectors from IEEE Std 1619-2007" and the "Exercise different lengths
 * covering even ciphertext stealing cases" set).  Every hex constant below
 * was re-verified against the authoritative pizfix OpenSSL 3.6.5 with a
 * scratch program compiled by build/bin/filcc (`openssl enc` refuses XTS
 * ciphers, so the CLI oracle is not available here).
 *
 * Behavior established by scratch experiments (identical on 3.0.13 and
 * 3.6.5, and matching crypto/modes/xts128.c and
 * providers/implementations/ciphers/cipher_aes_xts.c):
 *
 *   - a data unit shorter than 16 bytes fails (xts128.c: "if (len < 16)
 *     return -1"), and so does a zero-length update;
 *   - ciphertext stealing works in BOTH directions: CRYPTO_xts128_encrypt()
 *     handles a non-multiple-of-16 tail when encrypting (its final
 *     "if (enc)" branch) as well as when decrypting, and OpenSSL's own test
 *     data contains 17/18/19-byte ENCRYPT vectors;
 *   - every update is an independent data unit: the tweak is never advanced
 *     between updates, so the same 16-byte input under the same IV encrypts
 *     to the same block every time;
 *   - a key whose two halves are equal is rejected when INITIALIZING for
 *     encryption (aes_xts_check_keys_differ); for decryption the check only
 *     applies inside the FIPS provider (OpenSSL's test data: "Using the same
 *     key twice for decryption is allowed outside of FIPS mode"), so the
 *     decrypt route is covered here with a roundtrip instead of a
 *     must-fail;
 *   - a data unit larger than 2^20 * 16 bytes fails with
 *     XTS_DATA_UNIT_IS_TOO_LARGE (covered by the sibling program).
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
    fprintf(stderr, "FAIL test_aes_xts_small: %s\n", msg);
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

/* The vectors.  keyhex is always 32 (AES-128-XTS) or 64 (AES-256-XTS) hex
 * digits = two concatenated AES keys; ivhex is the 128-bit data-unit number;
 * pthex/cthex are the plaintext and ciphertext of one data unit. */
static const struct xts_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *pthex;
    const char *cthex;
} xts_kats[] = {
    { "128-16B", "AES-128-XTS",
      "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0",
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10",
      "6c1625db4671522d3d7599601de7ca09ed" },
    { "128-17B", "AES-128-XTS",
      "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0",
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f1011",
      "d069444b7a7e0cab09e24447d24deb1fedbf" },
    { "128-18B", "AES-128-XTS",
      "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0",
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f101112",
      "e5df1351c0544ba1350b3363cd8ef4beedbf9d" },
    { "128-19B", "AES-128-XTS",
      "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0",
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10111213",
      "9d84c813f719aa2c7be3f66171c7c5c2edbf9dac" },
    { "128-32B", "AES-128-XTS",
      "1111111111111111111111111111111122222222222222222222222222222222",
      "33333333330000000000000000000000",
      "4444444444444444444444444444444444444444444444444444444444444444",
      "c454185e6a16936e39334038acef838bfb186fff7480adc4289382ecd6d394f0" },
    { "256-16B", "AES-256-XTS",
      "27182818284590452353602874713526"
      "62497757247093699959574966967627"
      "31415926535897932384626433832795"
      "02884197169399375105820974944592",
      "00000000000000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10",
      "b5f737852b18e4cf31d7353ff220ca5e3a" },
    { "256-98B", "AES-256-XTS",
      "27182818284590452353602874713526"
      "62497757247093699959574966967627"
      "31415926535897932384626433832795"
      "02884197169399375105820974944592",
      "00000000000000000000000000000000",
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
      "6061",
      "3a060a8cad115a6f44572e3759e43c8fcad8bfcb233ff6ad71b7c1e7ca651508"
      "860aed34ec9506d368aa50274a31c16d2deae4d64c2a8096091c093f3820fb6d"
      "21089bcedaac361e3febe706cafe14b9bc89de34258d32ec3ad59fd2986a401e"
      "6bff" },
};

/* One-shot XTS encrypt/decrypt of a single data unit.  Returns the output
 * length; `out` must have room for inlen bytes (XTS emits exactly inlen
 * bytes; the EVP layer passes outsize = inl for this block-size-1 cipher). */
static size_t xts_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
    CHECK(inlen <= outcap,
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

/* (1) One-shot KATs: encrypt the plaintext and compare against the IEEE
 * vector; decrypt the vector ciphertext and compare against the plaintext.
 * Covers 16- and 32-byte aligned data units plus the ciphertext-stealing
 * lengths 17, 18, 19, and 98 bytes. */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(xts_kats) / sizeof(xts_kats[0]); k++) {
        const struct xts_kat *kat = &xts_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(64);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(128);
        unsigned char *ct_want = ALLOCATE_OUTPUT(128);
        unsigned char *ct = ALLOCATE_OUTPUT(128);
        unsigned char *pt_back = ALLOCATE_OUTPUT(128);
        size_t keylen, ptlen, ct_wantlen, ctlen, ptlen_back;
        char msg[128];

        /* AES-XTS is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-XTS cipher");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
              "XTS reports a block size other than 1");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "XTS IV length is not 16");
        keylen = from_hex(kat->keyhex, key, 64);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        from_hex(kat->ivhex, iv, 16);
        ptlen = from_hex(kat->pthex, pt, 128);
        ct_wantlen = from_hex(kat->cthex, ct_want, 128);

        ctlen = xts_crypt(cipher, key, iv, pt, ptlen, ct, 128, 1);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        ptlen_back = xts_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                               128, 0);
        snprintf(msg, sizeof(msg), "%s: decrypting the KAT did not give the plaintext",
                 kat->label);
        expect_equal(msg, pt_back, ptlen_back, pt, ptlen);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) Data units shorter than 16 bytes must fail, and a zero-length update
 * must fail too (aes_xts_cipher rejects inl < AES_BLOCK_SIZE and in ==
 * NULL).  Each negative test uses a fresh context. */
static void test_short_data_units_fail(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-XTS\") failed");
    from_hex(xts_kats[0].keyhex, key, 32);
    from_hex(xts_kats[0].ivhex, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)i;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 15) != 1,
          "a 15-byte data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, 15) != 1,
          "a 15-byte data unit was accepted for decryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 0) != 1,
          "a zero-length data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    /* A 16-byte unit still works after all those failures (fresh context). */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "a 16-byte data unit was rejected");
    CHECK(len == 16, "16-byte XTS update did not emit 16 bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (3) Ciphertext stealing round-trips for non-multiple-of-16 lengths on the
 * decrypt route (and the matching encryption, per the KATs above).  A
 * 21-byte data unit is encrypted and decrypted back in one shot and in
 * place, and a 53-byte message round-trips through two split updates (a
 * 32-byte and a 21-byte data unit; every XTS update is a data unit of at
 * least 16 bytes). */
static void test_stealing_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(53);
    unsigned char *ct = ALLOCATE_OUTPUT(53);
    unsigned char *buf = ALLOCATE_OUTPUT(53);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-XTS\") failed");
    from_hex(xts_kats[0].keyhex, key, 32);
    from_hex(xts_kats[0].ivhex, iv, 16);
    for (i = 0; i < 53; i++)
        pt[i] = (unsigned char)(i * 3 + 1);

    /* One-shot 21-byte (stolen tail) roundtrip. */
    xts_crypt(cipher, key, iv, pt, 21, ct, 53, 1);
    xts_crypt(cipher, key, iv, ct, 21, buf, 53, 0);
    CHECK(memcmp(buf, pt, 21) == 0,
          "21-byte (stolen tail) roundtrip failed");
    g_checks++;

    /* In-place 21-byte roundtrip. */
    memcpy(buf, pt, 21);
    xts_crypt(cipher, key, iv, buf, 21, buf, 53, 1);
    CHECK(memcmp(buf, ct, 21) == 0,
          "in-place XTS encryption differs from out-of-place");
    g_checks++;
    xts_crypt(cipher, key, iv, buf, 21, buf, 53, 0);
    CHECK(memcmp(buf, pt, 21) == 0, "in-place XTS roundtrip failed");
    g_checks++;

    /* Split updates: 32 bytes + 21 bytes.  Each update is an independent
     * data unit (so both are at least 16 bytes), and decrypting the two
     * ciphertext pieces with the same IV in the same order reproduces the
     * plaintext pieces. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, buf, &len, pt, 32) == 1,
          "32-byte XTS encrypt update failed");
    CHECK(len == 32, "32-byte XTS encrypt update did not emit 32 bytes");
    CHECK(EVP_EncryptUpdate(ctx, buf + 32, &len, pt + 32, 21) == 1,
          "21-byte XTS encrypt update failed");
    CHECK(len == 21, "21-byte XTS encrypt update did not emit 21 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, buf + 53, &len) == 1,
          "XTS Final failed");
    CHECK(len == 0, "XTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, buf, 32) == 1,
          "32-byte XTS decrypt update failed");
    CHECK(len == 32, "32-byte XTS decrypt update did not emit 32 bytes");
    CHECK(EVP_DecryptUpdate(ctx, ct + 32, &len, buf + 32, 21) == 1,
          "21-byte XTS decrypt update failed");
    CHECK(len == 21, "21-byte XTS decrypt update did not emit 21 bytes");
    CHECK(EVP_DecryptFinal_ex(ctx, ct + 53, &len) == 1,
          "XTS Final failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(ct, pt, 53) == 0,
          "32+21 split data units did not round-trip");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (4) Every update is an independent data unit: the tweak is never advanced
 * between updates, so encrypting the same 16-byte block twice through one
 * context yields identical ciphertext, while the second block of a single
 * 32-byte one-shot data unit differs (its tweak has advanced). */
static void test_each_update_is_a_data_unit(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *twice = ALLOCATE_OUTPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-XTS\") failed");
    from_hex(xts_kats[0].keyhex, key, 32);
    from_hex(xts_kats[0].ivhex, iv, 16);
    for (i = 0; i < 32; i++)
        in[i] = (unsigned char)(0x40 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, twice, &len, in, 16) == 1,
          "first 16-byte XTS update failed");
    CHECK(EVP_EncryptUpdate(ctx, twice + 16, &len, in, 16) == 1,
          "second 16-byte XTS update failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(twice, twice + 16, 16) == 0,
          "two equal 16-byte updates encrypted differently: the tweak "
          "advanced between updates");
    g_checks++;

    xts_crypt(cipher, key, iv, in, 32, oneshot, 32, 1);
    CHECK(memcmp(oneshot, twice, 16) == 0,
          "the first block of a 32-byte one-shot data unit differs from a "
          "16-byte data unit with the same IV");
    CHECK(memcmp(oneshot + 16, twice + 16, 16) != 0,
          "the second block of a 32-byte data unit did not use an advanced "
          "tweak");
    EVP_CIPHER_free(cipher);
}

/* (5) Weak keys: a key whose two halves are equal must be rejected when
 * initializing for encryption.  (For decryption the default provider
 * permits it -- see the header comment -- and the route is still a
 * well-behaved, deterministic permutation on aligned data units.) */
static void test_weak_key(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *out = ALLOCATE_OUTPUT(32);
    unsigned char *back = ALLOCATE_OUTPUT(32);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-XTS\") failed");
    from_hex(xts_kats[0].ivhex, iv, 16);
    for (i = 0; i < 32; i++)
        key[i] = (unsigned char)0x77;   /* two identical halves */
    for (i = 0; i < 32; i++)
        in[i] = (unsigned char)(0x80 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) != 1,
          "encryption with equal key halves was accepted");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "decryption with equal key halves was rejected (expected: only "
          "the FIPS provider checks this)");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, 21) == 1,
          "weak-key decryption of a 21-byte unit failed");
    CHECK(len == 21, "weak-key decryption did not emit 21 bytes");
    EVP_CIPHER_CTX_free(ctx);

    /* Deterministic: a second context decrypts the same data unit to the
     * same value.  (XTS decryption is not self-inverse -- like any cipher
     * built on AES it is only the inverse of *encryption* -- so no
     * decrypt-twice property is asserted here.) */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "second weak-key decrypt init failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, in, 21) == 1,
          "second weak-key decryption update failed");
    CHECK(memcmp(back, out, 21) == 0,
          "weak-key XTS decryption is not deterministic");
    g_checks++;
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (6) A 16-byte key cannot be used for AES-128-XTS: the only public way to
 * present it is EVP_CIPHER_CTX_set_key_length() + init, and OpenSSL must
 * reject the operation at one of those two points.  (As with ECB, the init
 * is only attempted when set_key_length() actually resized the context,
 * because EVP reads EVP_CIPHER_CTX_get_key_length() bytes from the key
 * buffer.) */
static void test_wrong_key_length(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    EVP_CIPHER_CTX *ctx;
    int rejected, init_ok = 1, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-XTS\") failed");
    from_hex(xts_kats[0].ivhex, iv, 16);
    for (i = 0; i < 16; i++)
        key[i] = (unsigned char)i;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, 16) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1);
    CHECK(rejected || !init_ok,
          "a 16-byte key was accepted for AES-128-XTS");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_short_data_units_fail();
    test_stealing_roundtrip();
    test_each_update_is_a_data_unit();
    test_weak_key();
    test_wrong_key_length();

    printf("test_aes_xts_small: all %u checks passed\n", g_checks);
    return 0;
}
