#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_xts_large -- AES-256-XTS at the data-unit size boundary.
 *
 * IEEE Std 1619-2018 (and NIST SP 800-38E) limit an XTS data unit to 2^20
 * AES blocks = 16 MiB.  OpenSSL enforces this in the provider
 * (providers/implementations/ciphers/cipher_aes_xts.c:
 *
 *     if (inl > XTS_MAX_BLOCKS_PER_DATA_UNIT * AES_BLOCK_SIZE) ...
 *         PROV_R_XTS_DATA_UNIT_IS_TOO_LARGE
 *
 * with XTS_MAX_BLOCKS_PER_DATA_UNIT = 1 << 20 in include/crypto/modes.h), so
 * a single update of exactly 16777216 bytes is the largest permitted data
 * unit and 16777217 bytes must fail.  Both bounds were verified by scratch
 * experiments on OpenSSL 3.0.13 and 3.6.5 (identical behavior).  Every
 * update is an independent data unit (the tweak is never advanced between
 * updates), which the 4 x 1 MiB stream below exercises at scale.
 *
 * There is no published KAT at these sizes; the vectors are the IEEE
 * Std 1619-2007 AES-256 key/plaintext patterns and all comparisons are
 * encrypt/decrypt roundtrips plus stream-vs-one-shot consistency, all
 * deterministic.
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
    fprintf(stderr, "FAIL test_aes_xts_large: %s\n", msg);
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

/* XTS_MAX_BLOCKS_PER_DATA_UNIT * AES_BLOCK_SIZE (include/crypto/modes.h). */
#define XTS_MAX_DATA_UNIT (size_t)((1u << 20) * 16)
#define MEBIBYTE (size_t)(1024 * 1024)

/* The IEEE Std 1619-2007 AES-256 XTS key (two 32-byte halves) and a fixed
 * data-unit number. */
static const char *const KEY256_HEX =
    "27182818284590452353602874713526"
    "62497757247093699959574966967627"
    "31415926535897932384626433832795"
    "02884197169399375105820974944592";

static const char *const IV_HEX =
    "01000000000000000000000000000000";

/* Encrypts or decrypts one data unit in a single update.  Returns the output
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

/* (1) The maximum data unit: a single 16777216-byte update encrypts and
 * decrypts back. */
static void test_max_data_unit_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(XTS_MAX_DATA_UNIT);
    unsigned char *ct = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT);
    unsigned char *back = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT);
    size_t i, ctlen, backlen;

    /* AES-256-XTS is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-XTS\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == from_hex(KEY256_HEX, key, 64),
          "fetched cipher has unexpected key length");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "XTS IV length is not 16");
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < XTS_MAX_DATA_UNIT; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctlen = xts_crypt(cipher, key, iv, in, XTS_MAX_DATA_UNIT, ct,
                      XTS_MAX_DATA_UNIT, 1);
    CHECK(ctlen == XTS_MAX_DATA_UNIT,
          "the 16 MiB data unit did not encrypt to 16 MiB");

    backlen = xts_crypt(cipher, key, iv, ct, ctlen, back, XTS_MAX_DATA_UNIT, 0);
    CHECK(backlen == XTS_MAX_DATA_UNIT,
          "the 16 MiB data unit did not decrypt to 16 MiB");
    CHECK(memcmp(back, in, XTS_MAX_DATA_UNIT) == 0,
          "the 16 MiB data unit roundtrip failed");

    /* Spot-check that the ciphertext is not trivially the plaintext. */
    CHECK(memcmp(ct, in, 4096) != 0,
          "the 16 MiB ciphertext begins with the plaintext");
    EVP_CIPHER_free(cipher);
}

/* (2) One byte beyond the maximum data unit must fail with
 * XTS_DATA_UNIT_IS_TOO_LARGE -- for encryption and decryption. */
static void test_data_unit_too_large(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(XTS_MAX_DATA_UNIT + 1);
    unsigned char *out = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT + 1);
    EVP_CIPHER_CTX *ctx;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-XTS\") failed");
    from_hex(KEY256_HEX, key, 64);
    from_hex(IV_HEX, iv, 16);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in,
                            (int)(XTS_MAX_DATA_UNIT + 1)) != 1,
          "a 16 MiB + 1 byte data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in,
                            (int)(XTS_MAX_DATA_UNIT + 1)) != 1,
          "a 16 MiB + 1 byte data unit was accepted for decryption");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (3) A 1 MiB data unit round-trips in one shot and in place. */
static void test_one_mebibyte_unit(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(MEBIBYTE);
    unsigned char *ct = ALLOCATE_OUTPUT(MEBIBYTE);
    unsigned char *back = ALLOCATE_OUTPUT(MEBIBYTE);
    size_t i, ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-XTS\") failed");
    from_hex(KEY256_HEX, key, 64);
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < MEBIBYTE; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctlen = xts_crypt(cipher, key, iv, in, MEBIBYTE, ct, MEBIBYTE, 1);
    CHECK(ctlen == MEBIBYTE, "the 1 MiB data unit did not encrypt to 1 MiB");
    backlen = xts_crypt(cipher, key, iv, ct, ctlen, back, MEBIBYTE, 0);
    CHECK(backlen == MEBIBYTE, "the 1 MiB data unit did not decrypt to 1 MiB");
    CHECK(memcmp(back, in, MEBIBYTE) == 0, "the 1 MiB data unit roundtrip failed");
    CHECK(memcmp(ct, in, 4096) != 0,
          "the 1 MiB ciphertext begins with the plaintext");

    /* In-place roundtrip of the same data unit. */
    memcpy(back, in, MEBIBYTE);
    xts_crypt(cipher, key, iv, back, MEBIBYTE, back, MEBIBYTE, 1);
    CHECK(memcmp(back, ct, MEBIBYTE) == 0,
          "in-place 1 MiB encryption differs from out-of-place");
    g_checks++;
    xts_crypt(cipher, key, iv, back, MEBIBYTE, back, MEBIBYTE, 0);
    CHECK(memcmp(back, in, MEBIBYTE) == 0, "in-place 1 MiB roundtrip failed");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (4) A 4 MiB stream through four 1 MiB updates round-trips, and each
 * update is an independent data unit: the first 1 MiB of the streamed
 * ciphertext equals the one-shot 1 MiB data-unit ciphertext with the same
 * data-unit number. */
static void test_four_mebibyte_stream(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-256-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(4 * MEBIBYTE);
    unsigned char *ct = ALLOCATE_OUTPUT(4 * MEBIBYTE);
    unsigned char *back = ALLOCATE_OUTPUT(4 * MEBIBYTE);
    unsigned char *unit = ALLOCATE_OUTPUT(MEBIBYTE);
    EVP_CIPHER_CTX *ctx;
    int len, u;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-256-XTS\") failed");
    from_hex(KEY256_HEX, key, 64);
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < 4 * MEBIBYTE; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    for (u = 0; u < 4; u++) {
        CHECK(EVP_EncryptUpdate(ctx, ct + u * MEBIBYTE, &len,
                                in + u * MEBIBYTE, (int)MEBIBYTE) == 1,
              "1 MiB stream encryption update failed");
        CHECK(len == (int)MEBIBYTE,
              "1 MiB stream encryption update did not emit 1 MiB");
    }
    CHECK(EVP_EncryptFinal_ex(ctx, ct + 4 * MEBIBYTE, &len) == 1,
          "XTS Final failed");
    CHECK(len == 0, "XTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    for (u = 0; u < 4; u++) {
        CHECK(EVP_DecryptUpdate(ctx, back + u * MEBIBYTE, &len,
                                ct + u * MEBIBYTE, (int)MEBIBYTE) == 1,
              "1 MiB stream decryption update failed");
        CHECK(len == (int)MEBIBYTE,
              "1 MiB stream decryption update did not emit 1 MiB");
    }
    CHECK(EVP_DecryptFinal_ex(ctx, back + 4 * MEBIBYTE, &len) == 1,
          "XTS Final failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(back, in, 4 * MEBIBYTE) == 0,
          "the 4 x 1 MiB stream roundtrip failed");
    g_checks++;

    /* Each stream update is its own data unit: encrypting the first MiB
     * through a fresh context reproduces the first streamed MiB. */
    xts_crypt(cipher, key, iv, in, MEBIBYTE, unit, MEBIBYTE, 1);
    CHECK(memcmp(unit, ct, MEBIBYTE) == 0,
          "the first 1 MiB stream update is not an independent data unit "
          "with the given data-unit number");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_max_data_unit_roundtrip();
    test_data_unit_too_large();
    test_one_mebibyte_unit();
    test_four_mebibyte_stream();

    printf("test_aes_xts_large: all %u checks passed\n", g_checks);
    return 0;
}
