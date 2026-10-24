#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_des3_wrap -- DES3-WRAP, the CMS triple-DES key wrap (RFC 3217,
 * default provider).
 *
 * Input rules, pinned against BOTH the pizfix 3.6.5 and the system
 * 3.0.13 builds with a scratch probe and matching the implementation
 * (providers/implementations/ciphers/cipher_tdes_wrap.c):
 *
 *   - the wrap direction requires inl % 8 == 0 ONLY.  Unlike AES-WRAP
 *     (crypto/modes/wrap128.c:55, "n >= 2" 64-bit blocks, i.e. >= 16
 *     bytes), des_ede3_wrap() has no minimum: an 8-byte input wraps to
 *     24 bytes.  15- and 17-byte inputs fail in the update.
 *   - the wrapped output is EXACTLY inl + 16 bytes: a random 8-byte IV
 *     (RAND_bytes into the context IV) is prepended and an 8-byte
 *     SHA-1-based integrity check value (ICV) is appended, the whole
 *     thing then CBC-encrypted and reversed (des_ede3_wrap).  (AES-WRAP
 *     appends 8; this cipher appends 16.)
 *   - the unwrap direction requires inl >= 24 (des_ede3_unwrap: "if
 *     (inl < 24) return -1"), i.e. at least 8 bytes of wrapped payload,
 *     and verifies the recovered ICV against SHA-1 of the plaintext, so
 *     corrupted wrapped data fails the update.
 *
 * Because the wrap embeds a RANDOM IV, the wrap direction is not
 * deterministic and has no KAT.  The embedded constant below is a
 * wrapped blob RECORDED from one authoritative-build (pizfix 3.6.5) wrap
 * of the 24-byte key data 01080f161d242b32... (bytes i*7+1) under the
 * embedded KEK; unwrapping it is deterministic and must return that key
 * data byte-for-byte on every build.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_des3_wrap: %s\n", msg);
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

static const char *const KEK_HEX =
    "0123456789abcdef23456789abcdef013456789abcdef012";

/* The recorded wrapped blob: one authoritative wrap of the 24-byte key
 * data 000102030405060708090a0b0c0d0e0f1011121314151617 under KEK_HEX
 * (the IV inside it is random; see the header comment). */
static const char *const WRAP24_BLOB_HEX =
    "99031bbc32c592f2ed5b8e20e02637ce"
    "1b3894a276890f9b6fab6c8d48700caa"
    "b854775c34f43ab2";
static const char *const KEYDATA24_HEX =
    "01080f161d242b323940474e555c636a"
    "71787f868d949ba2";

/* Wraps `inlen` bytes in one update.  `out` must have room for
 * inlen + 16 bytes (IV + payload + ICV -- the DES3-WRAP contract, larger
 * than the generic inl + block_size minimum).  Returns the wrapped
 * length. */
static size_t wrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                       const unsigned char *in, size_t inlen,
                       unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the DES3-WRAP contract (inl + 16)");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "wrap EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap EVP_EncryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Unwraps `inlen` bytes in one update; returns the key data length. */
static size_t unwrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                         const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    /* Unwrapping shrinks: it writes inlen - 16 bytes; the generic EVP
     * contract (inl + block_size) is more than enough. */
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "unwrap EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "unwrap EVP_DecryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) The unwrap-direction KAT: the recorded blob unwraps to the recorded
 * key data on both builds (deterministic operation). */
static void test_unwrap_kat(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES3-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(24);
    unsigned char *blob = ALLOCATE_INPUT(40);
    unsigned char *keydata = ALLOCATE_OUTPUT(24);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    size_t keydatalen, backlen;

    /* DES3-WRAP is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES3-WRAP\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 24,
          "DES3-WRAP key length is not 24");
    from_hex(KEK_HEX, kek, 24);
    from_hex(WRAP24_BLOB_HEX, blob, 40);
    keydatalen = from_hex(KEYDATA24_HEX, keydata, 24);

    backlen = unwrap_key(cipher, kek, blob, 40, back, 40 + 16);
    CHECK(backlen == 24, "unwrapping the recorded blob did not give 24 bytes");
    /* (blob is 40 bytes: 8-byte IV slot + 24-byte payload + 8-byte ICV) */
    expect_equal("the recorded blob does not unwrap to its key data",
                 back, backlen, keydata, keydatalen);
    EVP_CIPHER_free(cipher);
}

/* (2) Input rules: 15- and 17-byte inputs FAIL in the update; 8-, 16-,
 * 24- and 64-byte inputs wrap and produce EXACTLY inl + 16 bytes; every
 * wrapped output roundtrips. */
static void test_input_rules(void)
{
    static const size_t ok_lens[] = { 8, 16, 24, 64 };
    static const size_t bad_lens[] = { 15, 17 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES3-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(24);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *wrapped = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 32);
    EVP_CIPHER_CTX *ctx;
    size_t i, wlen, backlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES3-WRAP\") failed");
    from_hex(KEK_HEX, kek, 24);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    /* Bad lengths: the update itself fails. */
    for (i = 0; i < sizeof(bad_lens) / sizeof(bad_lens[0]); i++) {
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(ctx, wrapped, &len, in,
                                (int)bad_lens[i]) != 1,
              "wrapping a non-multiple-of-8 length was accepted");
        EVP_CIPHER_CTX_free(ctx);
    }

    /* Good lengths: exactly inl + 16 bytes out, and a roundtrip. */
    for (i = 0; i < sizeof(ok_lens) / sizeof(ok_lens[0]); i++) {
        size_t n = ok_lens[i];
        char msg[128];

        wlen = wrap_key(cipher, kek, in, n, wrapped, 64 + 16);
        snprintf(msg, sizeof(msg),
                 "wrapping %zu bytes did not give exactly inl + 16 bytes", n);
        CHECK(wlen == n + 16, msg);
        backlen = unwrap_key(cipher, kek, wrapped, wlen, back, 64 + 32);
        snprintf(msg, sizeof(msg), "the %zu-byte wrap/unwrap roundtrip failed",
                 n);
        expect_equal(msg, back, backlen, in, n);
    }
    EVP_CIPHER_free(cipher);
}

/* (3) Unwrap length rules: an 8-byte input fails (inl < 24), a 16-byte
 * input fails too (only 0 payload bytes after the IV and ICV), while the
 * 24-byte blob works. */
static void test_unwrap_length_rules(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES3-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(24);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *out = ALLOCATE_OUTPUT(24 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES3-WRAP\") failed");
    from_hex(KEK_HEX, kek, 24);
    for (i = 0; i < 24; i++)
        in[i] = (unsigned char)(i * 5 + 3);

    for (i = 0; i < 2; i++) {
        size_t n = (i == 0) ? 8 : 16;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)n) != 1,
              "unwrapping fewer than 24 bytes was accepted");
        EVP_CIPHER_CTX_free(ctx);
    }
    EVP_CIPHER_free(cipher);
}

/* (4) A corrupted wrapped blob must fail to unwrap: the recovered ICV
 * (SHA-1-based) rejects it.  One flip per region: the encrypted-IV block,
 * the middle (payload) and the ICV block. */
static void test_corrupted_unwrap(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES3-WRAP", NULL);
    unsigned char *kek = ALLOCATE_KEY(24);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *wrapped = ALLOCATE_OUTPUT(24 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    size_t i, off, wlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES3-WRAP\") failed");
    from_hex(KEK_HEX, kek, 24);
    for (i = 0; i < 24; i++)
        in[i] = (unsigned char)(i * 11 + 9);

    wlen = wrap_key(cipher, kek, in, 24, wrapped, 24 + 16);
    CHECK(wlen == 40, "wrapping 24 bytes did not give 40 bytes");

    for (i = 0; i < 3; i++) {
        off = (i == 0) ? 0 : (i == 1) ? 20 : 39;
        wrapped[off] ^= 0x01;
        {
            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            int len;

            CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
            CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
                  "EVP_DecryptInit_ex failed");
            CHECK(EVP_DecryptUpdate(ctx, back, &len, wrapped,
                                    (int)wlen) != 1,
                  "unwrap accepted corrupted key material");
            EVP_CIPHER_CTX_free(ctx);
        }
        wrapped[off] ^= 0x01; /* restore for the next flip */
    }
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_unwrap_kat();
    test_input_rules();
    test_unwrap_length_rules();
    test_corrupted_unwrap();

    printf("test_des3_wrap: all %u checks passed\n", g_checks);
    return 0;
}
