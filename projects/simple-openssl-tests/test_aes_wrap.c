#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_wrap -- AES-128/192/256 key wrap (RFC 3394) tests.
 *
 * The wrapped input must be a multiple of 8 bytes and at least 16 bytes
 * (crypto/modes/wrap128.c: "0 if inlen does not consist of n 64-bit blocks,
 * n >= 2"), so 15-byte and 17-byte payloads are rejected while a 16-byte
 * one wraps.  Wrapping appends exactly one 64-bit integrity block; the
 * default A6A6A6A6A6A6A6A6 initial value is used (no IV is set).
 *
 * Known-answer vectors: RFC 3394 sections 4.1 (128-bit KEK), 4.2 (192-bit
 * KEK) and 4.3 (256-bit KEK), each wrapping the 128-bit key data
 * 00112233445566778899AABBCCDDEEFF.  All three were verified through this
 * EVP flow against the authoritative pizfix OpenSSL 3.6.5 and cross-checked
 * against the system OpenSSL 3.0.13, in both the wrap and unwrap directions.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_wrap: %s\n", msg);
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

/* RFC 3394 section 4 test vectors (the CEK is the same for all three). */
static const char *const CEK_HEX =
    "00112233445566778899AABBCCDDEEFF";
static const struct wrap_kat {
    const char *label;
    const char *ciphername;
    const char *kekhex;
    const char *wraphex;
} wrap_kats[] = {
    { "RFC3394 4.1", "AES-128-WRAP",
      "000102030405060708090A0B0C0D0E0F",
      "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5" },
    { "RFC3394 4.2", "AES-192-WRAP",
      "000102030405060708090A0B0C0D0E0F1011121314151617",
      "96778B25AE6CA435F92B5B97C050AED2468AB8A17AD84E5D" },
    { "RFC3394 4.3", "AES-256-WRAP",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
      "64E8C3F9CE0F5BA263E9777905818A2A93C8191E7D6E8AE7" },
};

/* Wrap `inlen` bytes in one update; returns the wrapped length (inlen+8).
 * `out` must have room for inlen + 8 bytes (the EVP contract's inl +
 * block_size for this cipher). */
static size_t wrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                       const unsigned char *in, size_t inlen,
                       unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "wrap EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap EVP_EncryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Unwrap `inlen` bytes in one update; returns the key data length. */
static size_t unwrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                         const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "unwrap EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "unwrap EVP_DecryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) The RFC 3394 KATs, wrap and unwrap directions. */
static void test_kats(void)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *cek = ALLOCATE_INPUT(16);
    unsigned char *wrap_want = ALLOCATE_OUTPUT(24);
    unsigned char *wrap = ALLOCATE_OUTPUT(24 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 8);
    size_t k, wlen, backlen;

    for (k = 0; k < sizeof(wrap_kats) / sizeof(wrap_kats[0]); k++) {
        const struct wrap_kat *kat = &wrap_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t keklen, wrap_wantlen;

        /* AES key wrap is core OpenSSL 3 functionality: never SKIP, only
         * FAIL. */
        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
        keklen = from_hex(kat->kekhex, kek, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keklen,
              "fetched WRAP cipher has an unexpected key length");
        from_hex(CEK_HEX, cek, 16);
        wrap_wantlen = from_hex(kat->wraphex, wrap_want, 24);

        wlen = wrap_key(cipher, kek, cek, 16, wrap, 24 + 8);
        CHECK(wlen == 24, "wrapping a 16-byte CEK did not give 24 bytes");
        expect_equal("wrapped output does not match the RFC 3394 KAT",
                     wrap, wlen, wrap_want, wrap_wantlen);

        backlen = unwrap_key(cipher, kek, wrap_want, wrap_wantlen,
                             back, 24 + 8);
        expect_equal("unwrapping the RFC 3394 KAT did not give the CEK",
                     back, backlen, cek, 16);
        EVP_CIPHER_free(cipher);
    }
}

/* (2) Length rules: 15 bytes fail, 16 bytes wrap, 17 bytes fail. */
static void test_length_rules(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i, wlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-WRAP\") failed");
    for (i = 0; i < 16; i++)
        kek[i] = (unsigned char)(0xA0 + i);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    /* 15 bytes must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 15) != 1,
          "wrapping 15 bytes was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 17 bytes must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 17) != 1,
          "wrapping 17 bytes was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 16 bytes wrap and round-trip. */
    wlen = wrap_key(cipher, kek, in, 16, out, 64 + 16);
    CHECK(wlen == 24, "wrapping 16 bytes did not give 24 bytes");
    backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
    expect_equal("16-byte wrap/unwrap roundtrip failed", back, backlen, in, 16);
    EVP_CIPHER_free(cipher);
}

/* (3) A 64-byte payload round-trips for all three key sizes. */
static void test_64_byte_roundtrip(void)
{
    static const char *const names[] = {
        "AES-128-WRAP", "AES-192-WRAP", "AES-256-WRAP"
    };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    size_t m, i, keklen, wlen, backlen;

    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 13 + 5);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);
        char msg[128];

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
        keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keklen; i++)
            kek[i] = (unsigned char)(0x10 + i);
        wlen = wrap_key(cipher, kek, in, 64, out, 64 + 16);
        CHECK(wlen == 72, "wrapping 64 bytes did not give 72 bytes");
        backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
        snprintf(msg, sizeof(msg),
                 "%s 64-byte roundtrip failed", names[m]);
        expect_equal(msg, back, backlen, in, 64);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) A corrupted wrapped key must fail to unwrap: the RFC 3394 integrity
 * check (the recovered A6A6A6A6A6A6A6A6 initial value) rejects it. */
static void test_corrupted_unwrap(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(24 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    EVP_CIPHER_CTX *e, *ctx;
    int len = 0, flen = 0;
    size_t i, off;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-WRAP\") failed");
    for (i = 0; i < 16; i++)
        kek[i] = (unsigned char)(0xA0 + i);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    /* Corrupt one byte in the first block (the integrity register), one in
     * the middle and the last byte of the wrapped output. */
    for (i = 0; i < 3; i++) {
        off = (i == 0) ? 0 : (i == 1) ? 12 : 23;

        e = EVP_CIPHER_CTX_new();
        CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(e, cipher, NULL, kek, NULL) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(e, out, &len, in, 16) == 1,
              "wrap update failed");
        CHECK(EVP_EncryptFinal_ex(e, out + len, &flen) == 1,
              "wrap final failed");
        EVP_CIPHER_CTX_free(e);
        len = 0;
        flen = 0;

        out[off] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
              "unwrap accepted corrupted key material");
        EVP_CIPHER_CTX_free(ctx);
        out[off] ^= 0x01;
        len = 0;
        flen = 0;
    }
    EVP_CIPHER_free(cipher);
}

/* (5) The inverse variants (AES-128/192/256-WRAP-INV) round-trip where the
 * build offers them.  They share the RFC 3394 block structure with the wrap
 * and unwrap primitives exchanged (SP 800-38F section 5.1: the -INV ciphers
 * apply the inverse AES block transform), so only self-consistency and the
 * same length rules are asserted. */
static void test_wrap_inv(void)
{
    static const char *const names[] = {
        "AES-128-WRAP-INV", "AES-192-WRAP-INV", "AES-256-WRAP-INV"
    };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    size_t m, i, keklen, wlen, backlen;

    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 11 + 9);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);
        char msg[128];

        if (cipher == NULL) {
            printf("SKIP test_aes_wrap: %s is unavailable\n", names[m]);
            return;
        }
        keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keklen; i++)
            kek[i] = (unsigned char)(0x30 + i);
        /* 16-byte payload: wraps to exactly 24 bytes and round-trips (the
         * same length rule for every key size). */
        wlen = wrap_key(cipher, kek, in, 16, out, 64 + 16);
        CHECK(wlen == 24, "inverse wrap of 16 bytes did not give 24 bytes");
        backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
        expect_equal("16-byte inverse wrap/unwrap roundtrip failed",
                     back, backlen, in, 16);
        wlen = wrap_key(cipher, kek, in, 64, out, 64 + 16);
        CHECK(wlen == 72, "inverse wrap of 64 bytes did not give 72 bytes");
        backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
        snprintf(msg, sizeof(msg), "%s 64-byte roundtrip failed", names[m]);
        expect_equal(msg, back, backlen, in, 64);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_length_rules();
    test_64_byte_roundtrip();
    test_corrupted_unwrap();
    test_wrap_inv();

    printf("test_aes_wrap: all %u checks passed\n", g_checks);
    return 0;
}
