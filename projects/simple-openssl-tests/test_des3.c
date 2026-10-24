#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_des3 -- three-key DES (DES-EDE3) in ECB/CBC/OFB/CFB/CFB8/CFB1.
 *
 * Unlike single DES and DESX (legacy provider, see test_des), the DES-EDE3
 * ciphers are supplied by the DEFAULT provider, so no provider loading is
 * needed here (with the legacy provider also loaded they still fetch from
 * "default" -- see the probe table in test_des.c's history).
 *
 * Known-answer vectors: verified against the authoritative pizfix
 * OpenSSL 3.6.5 CLI,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3 \
 *       -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The key is 0123456789abcdef23456789abcdef013456789abcdef012 (24 bytes).
 *
 * The 8-byte block semantics mirror test_des: nopad needs exact multiples
 * of 8, padded operations roundtrip odd sizes, CBC decryption holds the
 * last block until Final, and the OFB/CFB/CFB8 shift registers carry
 * state across updates.  DES-EDE3 has a FIXED 24-byte key length (the
 * provider TDES flags carry no VARIABLE_LENGTH bit), which the wrong-key-
 * length section exercises through the same careful route as
 * test_aes_ecb: a bare init, then EVP_CIPHER_CTX_set_key_length() (which
 * must refuse to shrink the key), and only then the key init.
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
    fprintf(stderr, "FAIL test_des3: %s\n", msg);
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

static const char *const KEY24_HEX =
    "0123456789abcdef23456789abcdef013456789abcdef012";
static const char *const IV8_HEX = "0102030405060708";
static const char *const PT8_HEX = "4142434445464748";
static const char *const PT16_HEX =
    "0123456789abcdef0123456789abcdef";
static const char *const PT32_HEX =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

static const struct des3_kat {
    const char *ciphername;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    size_t ptlen;
    const char *cthex;
} des3_kats[] = {
    { "DES-EDE3", "", PT8_HEX, 8,
      "ddb4a8bce83fd7a2" },
    { "DES-EDE3-CBC", IV8_HEX, PT16_HEX, 16,
      "cbcc5741a0df298c97b3a5917cad747d" },
    { "DES-EDE3-OFB", IV8_HEX, PT32_HEX, 32,
      "a8e9a1f9f02019748231553969e25ff7"
      "ea94f1fca931e937c58bada249679c4f" },
    { "DES-EDE3-CFB", IV8_HEX, PT32_HEX, 32,
      "a8e9a1f9f0201974387a8c1c97c6d21b"
      "f918b7563de744b19b0d0790aeecd031" },
    { "DES-EDE3-CFB8", IV8_HEX, PT32_HEX, 32,
      "a81cfc52ef28fc5f661acc55c7183aa2"
      "3a2d51fafdbea3ec402dd546a53208e7" },
    { "DES-EDE3-CFB1", IV8_HEX, PT32_HEX, 32,
      "f5f68525c37bf9e834232891126d50d0"
      "0a7f7a84916ef19dd37091d780dd4d7b" },
};

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract). */
static size_t cipher_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* CBC encrypt/decrypt with an explicit padding flag. */
static size_t cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, const unsigned char *in,
                        size_t inlen, unsigned char *out, size_t outcap,
                        int enc, int padding)
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
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
static size_t stream_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) The KATs through EVP_CIPHER_fetch; decrypting each KAT ciphertext
 * recovers its plaintext. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(32 + 16);
    size_t k, ct_wantlen, ctlen, ptlen;
    char msg[128];

    for (k = 0; k < sizeof(des3_kats) / sizeof(des3_kats[0]); k++) {
        const struct des3_kat *kat = &des3_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

        /* DES-EDE3 is core OpenSSL 3 functionality (default provider):
         * never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a DES-EDE3 cipher");
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 24,
              "fetched DES-EDE3 cipher has an unexpected key length");
        from_hex(KEY24_HEX, key, 24);
        from_hex(kat->ivhex, iv, 8);
        from_hex(kat->pthex, pt, 32);
        ct_wantlen = from_hex(kat->cthex, ct_want, 32);

        ctlen = cipher_crypt(cipher, key, kat->ivhex[0] ? iv : NULL, pt,
                             kat->ptlen, ct, 32 + 16, 1);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        ptlen = cipher_crypt(cipher, key, kat->ivhex[0] ? iv : NULL, ct_want,
                             ct_wantlen, pt_back, 32 + 16, 0);
        snprintf(msg, sizeof(msg),
                 "%s decrypting the KAT ciphertext did not give the plaintext",
                 kat->ciphername);
        expect_equal(msg, pt_back, ptlen, pt, kat->ptlen);
        EVP_CIPHER_free(cipher);
    }
}

/* (2) DES-EDE3-CBC: pad/nopad roundtrips and streaming {1,7,8,9,16,17}
 * equality with the one-shot KAT in both directions. */
static void test_cbc_streaming(void)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9, 16, 17 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-EDE3-CBC", NULL);
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(33 + 24);
    size_t i, ct_wantlen, ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-EDE3-CBC\") failed");
    from_hex(KEY24_HEX, key, 24);
    from_hex(IV8_HEX, iv, 8);
    from_hex(PT16_HEX, pt, 32);
    ct_wantlen = from_hex(des3_kats[1].cthex, ct_want, 32);

    /* nopad KAT and its streaming equality. */
    ctlen = cbc_crypt(cipher, key, iv, pt, 16, ct, 33 + 24, 1, 0);
    expect_equal("DES-EDE3-CBC one-shot ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
    ctlen = stream_chunked(cipher, key, iv, pt, 16, ct, 33 + 24, 1,
                           stream_chunks, 6);
    expect_equal("streaming DES-EDE3-CBC ciphertext does not match one-shot",
                 ct, ctlen, ct_want, ct_wantlen);
    backlen = stream_chunked(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                             33 + 24, 0, stream_chunks, 6);
    expect_equal("streaming DES-EDE3-CBC decrypt did not recover the plaintext",
                 pt_back, backlen, pt, 16);

    /* pad: odd sizes roundtrip (33 bytes -> 40 with a full pad block). */
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);
    ctlen = cbc_crypt(cipher, key, iv, pt, 33, ct, 33 + 24, 1, 1);
    CHECK(ctlen == 40, "padded DES-EDE3-CBC of 33 bytes did not give 40");
    backlen = cbc_crypt(cipher, key, iv, ct, ctlen, pt_back, 33 + 24, 0, 1);
    expect_equal("padded DES-EDE3-CBC roundtrip did not recover 33 bytes",
                 pt_back, backlen, pt, 33);
    EVP_CIPHER_free(cipher);
}

/* (3) The stream modes: partial updates (the chunk grids above) and
 * additionally a 17-then-15 split and a 0-byte no-op for DES-EDE3-OFB. */
static void test_stream_details(void)
{
    static const size_t odd_chunks[] = { 17, 15 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-EDE3-OFB", NULL);
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *chunked = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t olen, clen, backlen;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-EDE3-OFB\") failed");
    from_hex(KEY24_HEX, key, 24);
    from_hex(IV8_HEX, iv, 8);
    from_hex(PT32_HEX, pt, 32);

    olen = cipher_crypt(cipher, key, iv, pt, 32, oneshot, 32 + 16, 1);
    clen = stream_chunked(cipher, key, iv, pt, 32, chunked, 32 + 16, 1,
                          odd_chunks, 2);
    expect_equal("17/15-split DES-EDE3-OFB ciphertext differs from one-shot",
                 chunked, clen, oneshot, olen);
    backlen = stream_chunked(cipher, key, iv, oneshot, olen, back, 32 + 16, 0,
                             odd_chunks, 2);
    expect_equal("17/15-split DES-EDE3-OFB decrypt did not recover plaintext",
                 back, backlen, pt, 32);

    /* 0-byte no-op. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, chunked, &len, pt, 0) == 1,
          "0-byte DES-EDE3-OFB update failed");
    CHECK(len == 0, "0-byte DES-EDE3-OFB update emitted bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, chunked, &flen) == 1,
          "0-byte DES-EDE3-OFB final failed");
    CHECK(flen == 0, "0-byte DES-EDE3-OFB final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (4) A 16-byte key must not be usable with DES-EDE3: the only public way
 * to present it is EVP_CIPHER_CTX_set_key_length() + init (the same
 * careful route as test_aes_ecb -- the init reads exactly
 * EVP_CIPHER_CTX_get_key_length() bytes, so it is only attempted when
 * set_key_length() actually resized the context), and OpenSSL must reject
 * the operation at one of those two points (the provider TDES flags carry
 * no VARIABLE_LENGTH bit, so set_key_length refuses). */
static void test_wrong_key_length(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-EDE3", NULL);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *key = ALLOCATE_KEY(24);
    int rejected, init_ok = 1, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-EDE3\") failed");
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    for (i = 0; i < 24; i++)
        key[i] = (unsigned char)(i + 1);

    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, 16) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1);
    CHECK(rejected || !init_ok,
          "a 16-byte key was accepted for DES-EDE3");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_cbc_streaming();
    test_stream_details();
    test_wrong_key_length();

    printf("test_des3: all %u checks passed\n", g_checks);
    return 0;
}
