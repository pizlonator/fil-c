#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_legacy_block_ciphers -- a data-driven sweep of the remaining
 * legacy-provider block ciphers: CAST5, Blowfish, IDEA, SEED and RC2 in
 * their ECB/CBC/OFB/CFB (and RC2 40/64-bit) variants.
 *
 * Every row is {cipher name, key, IV, plaintext, ciphertext} and gets a
 * KAT one-shot, a roundtrip, and one streaming check.  All hex constants
 * were verified against the authoritative pizfix OpenSSL 3.6.5 CLI with
 * the legacy provider enabled,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -cast5-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical
 * values, except IDEA which the system 3.0.13 build does not offer at
 * all -- its rows print a per-row SKIP line there; the IDEA constants
 * come from the 3.6.5 oracle alone).  RC2-40-CBC uses a fixed 5-byte key
 * and RC2-64-CBC a fixed 8-byte key (EVP_CIPHER_get_key_length; note
 * RC2-64 is 8 bytes = 64 bits, not 6).
 *
 * The legacy provider is loaded EXPLICITLY together with the default
 * provider: loading legacy alone would disable default-provider
 * auto-load, and a row whose cipher is missing still needs the default
 * provider for nothing here but keeps the environment uniform.
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
    fprintf(stderr, "FAIL test_legacy_block_ciphers: %s\n", msg);
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

/* The default 16-byte key and 8-byte IV shared by most rows. */
static const char *const KEY16_HEX =
    "0123456789abcdeffedcba9876543210";
static const char *const IV8_HEX = "0102030405060708";
/* SEED's IV is the full 16-byte block. */
static const char *const IV16_HEX =
    "01020304050607080102030405060708";
static const char *const PT16_HEX =
    "0123456789abcdef0123456789abcdef";
static const char *const PT32_HEX =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

static const struct row {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    const char *cthex;
} rows[] = {
    { "CAST5-ECB", KEY16_HEX, "", PT16_HEX,
      "c8678f70485e9c93c8678f70485e9c93" },
    { "CAST5-CBC", KEY16_HEX, IV8_HEX, PT16_HEX,
      "04e2684cc4d83b767722ea1d8c4771dd" },
    { "CAST5-OFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "cc02818217b150783e0988f4d1a6332b"
      "bf90db9d73c93deccdab16be50cefbc4" },
    { "CAST5-CFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "cc02818217b150780809663e3b30ecc6"
      "ce894a59352cc46cf0f771a154e6647c" },
    { "BF-ECB", KEY16_HEX, "", PT16_HEX,
      "2fa4107fb809c9762fa4107fb809c976" },
    { "BF-CBC", KEY16_HEX, IV8_HEX, PT16_HEX,
      "b79bd4be4a34d3bab5e964b328ab48d5" },
    { "BF-OFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "1f1030bfccff91d3c3bb877ef80c5756"
      "d97921e14ece4f090d3ca91615a5ee2e" },
    { "BF-CFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "1f1030bfccff91d313883ca8941108c1"
      "6b767a4fbba06bab484ac83f07ffa762" },
    { "IDEA-ECB", KEY16_HEX, "", PT16_HEX,
      "73737cbbcbedb9c673737cbbcbedb9c6" },
    { "IDEA-CBC", KEY16_HEX, IV8_HEX, PT16_HEX,
      "d3034ab19093f2358a99feb08dc2c1fd" },
    { "IDEA-OFB", KEY16_HEX, IV8_HEX, PT16_HEX,
      "84e59104b8a8c8298cb5af470fd9fbf4" },
    { "IDEA-CFB", KEY16_HEX, IV8_HEX, PT16_HEX,
      "84e59104b8a8c829b9cfd19c2027d058" },
    { "SEED-ECB", KEY16_HEX, "", PT16_HEX,
      "f36e01a46033fa56fd7b1a40ed0bf836" },
    { "SEED-CBC", KEY16_HEX, IV16_HEX, PT16_HEX,
      "1e3bbe6b4933d3539ed34bc60d40764b" },
    { "SEED-OFB", KEY16_HEX, IV16_HEX, PT16_HEX,
      "9c75c7877b7936aec93575e06410768e" },
    { "SEED-CFB", KEY16_HEX, IV16_HEX, PT16_HEX,
      "9c75c7877b7936aec93575e06410768e" },
    { "RC2-ECB", KEY16_HEX, "", PT16_HEX,
      "86ff262db0e23f9986ff262db0e23f99" },
    { "RC2-CBC", KEY16_HEX, IV8_HEX, PT16_HEX,
      "cfb60fce0380f3edf31f9ef01bf6bb96" },
    { "RC2-40-CBC", "0123456789", IV8_HEX, PT16_HEX,
      "d42cf08b2780a01e7bd39f1c0435e957" },
    { "RC2-64-CBC", "0123456789abcdef", IV8_HEX, PT16_HEX,
      "b4cf0b582f283cc597d56bdf55abf287" },
    { "RC2-CFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "f0eb8b77800b63409fe15b8e0c017d68"
      "1ea42fe9af5d70be6c58f0355b242035" },
    { "RC2-OFB", KEY16_HEX, IV8_HEX, PT32_HEX,
      "f0eb8b77800b634050bc4ea886f58a3c"
      "6e7667f3bab1cae7c10ff01e39c67940" },
};

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract). */
static size_t row_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, const unsigned char *in,
                        size_t inlen, unsigned char *out, size_t outcap,
                        int enc)
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
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
 * (last one truncated); padding disabled. */
static size_t row_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *iv, const unsigned char *in,
                          size_t inlen, unsigned char *out, size_t outcap,
                          int enc, const size_t *chunks, size_t nchunks)
{
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
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

/* The data-driven sweep: per row, KAT one-shot, roundtrip, and one
 * streaming equality with the {5, 3, 8, 2} grid. */
static void test_rows(void)
{
    static const size_t chunks[] = { 5, 3, 8, 2 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(32 + 32);
    size_t r, keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    for (r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        const struct row *row = &rows[r];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, row->ciphername, NULL);

        if (cipher == NULL) {
            /* Genuinely version-gated: IDEA is absent from some builds
             * (Ubuntu's 3.0.13 compiles it out).  Skip the row and keep
             * going, as the house SKIP contract requires. */
            printf("SKIP test_legacy_block_ciphers: %s is unavailable\n",
                   row->ciphername);
            continue;
        }
        keylen = from_hex(row->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has an unexpected key length");
        from_hex(row->ivhex, iv, 16);
        ptlen = from_hex(row->pthex, pt, 32);
        ct_wantlen = from_hex(row->cthex, ct_want, 32);

        /* KAT one-shot. */
        ctlen = row_crypt(cipher, key, row->ivhex[0] ? iv : NULL, pt, ptlen,
                          ct, 32 + 16, 1);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 row->ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        /* Roundtrip. */
        backlen = row_crypt(cipher, key, row->ivhex[0] ? iv : NULL, ct_want,
                            ct_wantlen, pt_back, 32 + 32, 0);
        snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
                 row->ciphername);
        expect_equal(msg, pt_back, backlen, pt, ptlen);

        /* Streaming equality, both directions. */
        ctlen = row_chunked(cipher, key, row->ivhex[0] ? iv : NULL, pt, ptlen,
                            ct, 32 + 16, 1, chunks, 4);
        snprintf(msg, sizeof(msg),
                 "%s streaming ciphertext does not match the one-shot "
                 "ciphertext", row->ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        backlen = row_chunked(cipher, key, row->ivhex[0] ? iv : NULL, ct_want,
                              ct_wantlen, pt_back, 32 + 32, 0, chunks, 4);
        snprintf(msg, sizeof(msg),
                 "%s streaming decryption did not recover the plaintext",
                 row->ciphername);
        expect_equal(msg, pt_back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    /* Loading the legacy provider disables default-provider auto-load, so
     * both are loaded explicitly (house rule 10). */
    CHECK(OSSL_PROVIDER_load(NULL, "legacy") != NULL,
          "OSSL_PROVIDER_load(\"legacy\") failed");
    CHECK(OSSL_PROVIDER_load(NULL, "default") != NULL,
          "OSSL_PROVIDER_load(\"default\") failed");

    test_rows();

    printf("test_legacy_block_ciphers: all %u checks passed\n", g_checks);
    return 0;
}
