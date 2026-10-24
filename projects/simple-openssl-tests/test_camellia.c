#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_camellia -- CAMELLIA-128/192/256 in ECB/CBC/CTR/OFB/CFB/CFB1/CFB8.
 *
 * Known-answer vectors:
 *
 *   - CAMELLIA-128-ECB is anchored by the RFC 3713 sample vector:
 *     key = plaintext = 0123456789abcdeffedcba9876543210, ciphertext
 *     67673138549669730857065648eabe43.
 *   - Every other constant was verified against the authoritative pizfix
 *     OpenSSL 3.6.5 CLI,
 *
 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -camellia-128-ecb -nopad -K <keyhex>
 *
 *     and likewise for every (key size, mode) combination in this file
 *     (128/192/256 x ECB/CBC/CTR/OFB/CFB/CFB8/CFB1, over the 64-byte
 *     CBC/CTR plaintext or the 32-byte stream-mode plaintext); every
 *     ciphertext was decrypted back to its plaintext with the same CLI,
 *     and every derivation was cross-checked to be byte-identical under
 *     the system OpenSSL 3.0.13.  The 128/192/256-bit keys extend the
 *     RFC 3713 key with 0011223344556677 (and 8899aabbccddeeff); the
 *     stream-mode IV is fedcba98765432100123456789abcdef and the CBC IV
 *     is the same value, so for every key size the CFB and OFB KATs
 *     agree on their first block (both are plaintext XOR E(IV) there).
 *
 * The CFB1 section covers byte mode (the default) and bit-length mode
 * (EVP_CIPH_FLAG_LENGTH_BITS, the same two-phase-init route
 * test_aes_cfb1 documents: a bare init first, then set_flags, then the
 * init that passes key+IV, on a freshly created context every time).
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
    fprintf(stderr, "FAIL test_camellia: %s\n", msg);
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

/* The RFC 3713 128-bit key and plaintext, extended to 192/256 bits. */
static const char *const K128_HEX =
    "0123456789abcdeffedcba9876543210";
static const char *const K192_HEX =
    "0123456789abcdeffedcba9876543210"
    "0011223344556677";
static const char *const K256_HEX =
    "0123456789abcdeffedcba9876543210"
    "00112233445566778899aabbccddeeff";
static const char *const PT16_HEX =
    "0123456789abcdeffedcba9876543210";

/* Stream-mode and CBC IV (128-bit). */
static const char *const IV128_HEX =
    "fedcba98765432100123456789abcdef";

/* 32-byte stream-mode plaintext and its KAT ciphertexts (CAMELLIA-128). */
static const char *const PT32_HEX =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";
static const char *const OFB32_HEX =
    "88b82d6aef416e6b60acf7aff9fc5d39"
    "55712efe628fdd5833b8243e92c281e5";
static const char *const CFB32_HEX =
    "88b82d6aef416e6b60acf7aff9fc5d39"
    "f4d4929957de25be5604129046d07f43";
static const char *const CFB8_32_HEX =
    "8884d17881596c98ec41c7135dfc4a1d"
    "2fae06f3a31bbf6e7539003e1dff8084";
static const char *const CFB1_32_HEX =
    "ec7e7e3e83daf3474357a10a45fc1b7b"
    "1f1d3e35e98434046dac1ec2c1cae1a0";

/* The same stream-mode KATs under the 192/256-bit key extensions. */
static const char *const OFB32_192_HEX =
    "a836b1c0e51774077455c63f34f3234c"
    "72173a75124bdd6caa1b83d7dca261a8";
static const char *const OFB32_256_HEX =
    "5bc493556c1e09b7d6f5620af6bc254e"
    "f670786534e52e55476cb121d99009f4";
static const char *const CFB32_192_HEX =
    "a836b1c0e51774077455c63f34f3234c"
    "68a4340d8ab9e801e40ad86bb56285d7";
static const char *const CFB32_256_HEX =
    "5bc493556c1e09b7d6f5620af6bc254e"
    "da29e8210924f2327b9a229b86fedc62";
static const char *const CFB8_32_192_HEX =
    "a85617d073b8d8635eaddfa4f61600b7"
    "ec258fb28bec97872d8b18ee98eb093b";
static const char *const CFB8_32_256_HEX =
    "5b488a526c519505f5d4977fbf103757"
    "93efbb20f3f5a4e897b6bf4e62ed0615";
static const char *const CFB1_32_192_HEX =
    "dd3e4f0cedb7b9199c6190810c894a97"
    "673de05e45ac4496ba9e306eea23af9b";
static const char *const CFB1_32_256_HEX =
    "5ef6c5c1fa7f7d09be9eead4145a751d"
    "725638a9bd60c9918ca5116f3f3044d3";

/* 64-byte CBC plaintext (the SP 800-38A shared vector) and the
 * CAMELLIA-128-CBC ciphertext under IV128_HEX. */
static const char *const PT64_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
static const char *const CBC64_HEX =
    "30be05b50273d419cf1107015d56e9c4"
    "9757a56f8ca27bb6eaa588fd164b95d0"
    "e05d414371d02bd82d1de17740dcb0ec"
    "e8e9e90eeed14699d835d5f5e3e9ab2d";
static const char *const CBC64_192_HEX =
    "acba88f7eb7d0ea364b8689b3740e409"
    "2f4e9abfd6e7b829c1e886ecfa94f345"
    "14fd4a7634a424d09bfa85828a1b0da4"
    "c19325afabc6d9bc98365e68b1e38f72";
static const char *const CBC64_256_HEX =
    "c8eb72eccaf5dd428d8264319f2dcd76"
    "25921e4e44602faea3eabb93417d153a"
    "6e9a902b2919451efa66c9bf27105631"
    "5c51a2cd76ddfef48c0bf4c21084920f";

/* CAMELLIA-128-CTR under the initial counter 000102...0f. */
static const char *const CTRIV_HEX =
    "000102030405060708090a0b0c0d0e0f";
static const char *const CTR64_HEX =
    "3b5dd6f915870064fd3296068eddc95b"
    "c02bb23a923e1a05ffb182867a7a0149"
    "ed3b7eb2f321fc0fe156ae309559698f"
    "8c441fb113d76d776e66e46640c171dc";
static const char *const CTR64_192_HEX =
    "895af9df74daf66f538ed8cda3670300"
    "024fe8874aa754b3fe9626679b0823b7"
    "1143e13cfd801f4328bff34e1e197b84"
    "6dee50a0fab2da37362ae2c8881418e4";
static const char *const CTR64_256_HEX =
    "cabc1be5af6a98e74af5be7d4bf6861a"
    "f38316b10a965ba7851e4806b3a0a840"
    "6ca2ffa0b6a5f528c0de5c6c2b8aa124"
    "ef540b600d143a10b27d83703e4d74df";

static const struct ecb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} ecb_kats[] = {
    { "CAMELLIA-128-ECB", K128_HEX,
      "67673138549669730857065648eabe43" },  /* RFC 3713 sample vector */
    { "CAMELLIA-192-ECB", K192_HEX,
      "b4993401b3e996f84ee5cee7d79b09b9" },
    { "CAMELLIA-256-ECB", K256_HEX,
      "9acc237dff16d76c20ef7c919e3a7509" },
};

/* One-shot encrypt/decrypt with padding disabled; used for the ECB and
 * stream-mode KATs and roundtrips.  Returns the total output length
 * written to `out` (room for inlen + block_size per the EVP contract). */
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
static size_t camellia_cbc(const EVP_CIPHER *cipher, const unsigned char *key,
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
 * (last one truncated); padding disabled.  A split must reproduce the
 * one-shot result exactly. */
static size_t stream_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                             const unsigned char *iv, const unsigned char *in,
                             size_t inlen, unsigned char *out, size_t outcap,
                             int enc)
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
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

/* (1) The ECB KATs for all three key sizes, fetched from the default
 * provider; decrypting the KAT ciphertext recovers the plaintext. */
static void test_ecb_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(16 + 16);
    size_t k, keylen, ct_wantlen, ctlen, ptlen;
    char msg[128];

    from_hex(PT16_HEX, pt, 16);
    for (k = 0; k < sizeof(ecb_kats) / sizeof(ecb_kats[0]); k++) {
        const struct ecb_kat *kat = &ecb_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

        /* Camellia is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core CAMELLIA cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched CAMELLIA cipher has an unexpected key length");
        ct_wantlen = from_hex(kat->cthex, ct_want, 16);

        ctlen = cipher_crypt(cipher, key, NULL, pt, 16, ct, 16 + 16, 1);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        ptlen = cipher_crypt(cipher, key, NULL, ct_want, ct_wantlen, pt_back,
                             16 + 16, 0);
        snprintf(msg, sizeof(msg),
                 "%s decrypting the KAT ciphertext did not give the plaintext",
                 kat->ciphername);
        expect_equal(msg, pt_back, ptlen, pt, 16);
        EVP_CIPHER_free(cipher);
    }
}

/* (2) The same KATs through the legacy static EVP_camellia_128_ecb() route
 * and through EVP_get_cipherbyname("camellia-128-cbc") on the CBC KAT. */
static void test_alternate_routes(void)
{
    const EVP_CIPHER *static_ecb = EVP_camellia_128_ecb();
    const EVP_CIPHER *byname_cbc = EVP_get_cipherbyname("camellia-128-cbc");
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt16 = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *pt64 = ALLOCATE_INPUT(64);
    unsigned char *cbc_want = ALLOCATE_OUTPUT(64);
    unsigned char *cbc = ALLOCATE_OUTPUT(64 + 16);
    size_t ct_wantlen, ctlen, cbc_wantlen, cbclen;

    CHECK(static_ecb != NULL, "EVP_camellia_128_ecb() returned NULL");
    CHECK(byname_cbc != NULL,
          "EVP_get_cipherbyname(\"camellia-128-cbc\") returned NULL");

    from_hex(K128_HEX, key, 16);
    from_hex(IV128_HEX, iv, 16);
    from_hex(PT16_HEX, pt16, 16);
    ct_wantlen = from_hex(ecb_kats[0].cthex, ct_want, 16);
    from_hex(PT64_HEX, pt64, 64);
    cbc_wantlen = from_hex(CBC64_HEX, cbc_want, 64);

    ctlen = cipher_crypt(static_ecb, key, NULL, pt16, 16, ct, 16 + 16, 1);
    expect_equal("EVP_camellia_128_ecb() ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    memset(cbc, 0, 64 + 16);
    cbclen = camellia_cbc(byname_cbc, key, iv, pt64, 64, cbc, 64 + 16, 1, 0);
    expect_equal("EVP_get_cipherbyname() ciphertext does not match the CBC KAT",
                 cbc, cbclen, cbc_want, cbc_wantlen);
}

/* (3) CBC for all three key sizes: nopad KAT, streaming {1,15,16,17,31}
 * equality with the one-shot KAT in both directions, a padded roundtrip
 * of an odd length, and the padding-disabled Final failure. */
static void test_cbc(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
        const char *cthex;
    } cbc_kats[] = {
        { "CAMELLIA-128-CBC", K128_HEX, CBC64_HEX },
        { "CAMELLIA-192-CBC", K192_HEX, CBC64_192_HEX },
        { "CAMELLIA-256-CBC", K256_HEX, CBC64_256_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *odd = ALLOCATE_INPUT(33);
    unsigned char *odd_ct = ALLOCATE_OUTPUT(48 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t c, i, keylen, ct_wantlen, ctlen, ptlen, oddlen, backlen;
    char msg[128];

    from_hex(IV128_HEX, iv, 16);
    from_hex(PT64_HEX, pt, 64);
    for (i = 0; i < 33; i++)
        odd[i] = (unsigned char)(i * 3 + 7);

    for (c = 0; c < sizeof(cbc_kats) / sizeof(cbc_kats[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cbc_kats[c].ciphername, NULL);

        /* Camellia is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core CAMELLIA cipher");
        keylen = from_hex(cbc_kats[c].keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched CAMELLIA-CBC cipher has an unexpected key length");
        ct_wantlen = from_hex(cbc_kats[c].cthex, ct_want, 64);

        /* nopad KAT and its streaming equality. */
        ctlen = camellia_cbc(cipher, key, iv, pt, 64, ct, 64 + 32, 1, 0);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 cbc_kats[c].ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        ctlen = stream_chunked(cipher, key, iv, pt, 64, ct, 64 + 32, 1);
        snprintf(msg, sizeof(msg),
                 "%s streaming ciphertext does not match the one-shot KAT",
                 cbc_kats[c].ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        ptlen = stream_chunked(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                               64 + 32, 0);
        snprintf(msg, sizeof(msg),
                 "%s streaming CBC decryption did not recover the plaintext",
                 cbc_kats[c].ciphername);
        expect_equal(msg, pt_back, ptlen, pt, 64);

        /* Padded roundtrip of 33 bytes: 33 -> 48 -> 33. */
        oddlen = camellia_cbc(cipher, key, iv, odd, 33, odd_ct, 48 + 16, 1, 1);
        snprintf(msg, sizeof(msg),
                 "%s padded CBC encryption of 33 bytes did not give 48",
                 cbc_kats[c].ciphername);
        CHECK(oddlen == 48, msg);
        backlen = camellia_cbc(cipher, key, iv, odd_ct, oddlen, pt_back, 48 + 16,
                               0, 1);
        snprintf(msg, sizeof(msg),
                 "%s padded CBC roundtrip did not recover the 33-byte plaintext",
                 cbc_kats[c].ciphername);
        expect_equal(msg, pt_back, backlen, odd, 33);

        /* Padding disabled: a 33-byte input cannot finish. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "EVP_CIPHER_CTX_set_padding(0) failed");
        CHECK(EVP_EncryptUpdate(ctx, odd_ct, &len, odd, 33) == 1,
              "padding-disabled CBC update failed outright");
        CHECK(len == 32, "padding-disabled update did not emit 32 full blocks");
        CHECK(EVP_EncryptFinal_ex(ctx, odd_ct + len, &len) != 1,
              "padding-disabled encryption of 33 bytes did not fail at Final");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) The stream modes (CTR/OFB/CFB/CFB8/CFB1) for all three key sizes:
 * KAT, roundtrip of the KAT ciphertext, and streaming {1,15,16,17,31}
 * equality with the one-shot result in both directions.  The CTR KATs
 * cover 64 bytes under the 000102...0f counter, the other modes 32 bytes
 * under IV128_HEX. */
static void test_stream_modes(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
        const char *ivhex;
        const char *pthex;   /* 64 bytes for CTR, 32 for the rest */
        size_t ptlen;
        const char *cthex;
    } modes[] = {
        { "CAMELLIA-128-CTR", K128_HEX, CTRIV_HEX, PT64_HEX, 64, CTR64_HEX },
        { "CAMELLIA-192-CTR", K192_HEX, CTRIV_HEX, PT64_HEX, 64, CTR64_192_HEX },
        { "CAMELLIA-256-CTR", K256_HEX, CTRIV_HEX, PT64_HEX, 64, CTR64_256_HEX },
        { "CAMELLIA-128-OFB", K128_HEX, IV128_HEX, PT32_HEX, 32, OFB32_HEX },
        { "CAMELLIA-192-OFB", K192_HEX, IV128_HEX, PT32_HEX, 32, OFB32_192_HEX },
        { "CAMELLIA-256-OFB", K256_HEX, IV128_HEX, PT32_HEX, 32, OFB32_256_HEX },
        { "CAMELLIA-128-CFB", K128_HEX, IV128_HEX, PT32_HEX, 32, CFB32_HEX },
        { "CAMELLIA-192-CFB", K192_HEX, IV128_HEX, PT32_HEX, 32, CFB32_192_HEX },
        { "CAMELLIA-256-CFB", K256_HEX, IV128_HEX, PT32_HEX, 32, CFB32_256_HEX },
        { "CAMELLIA-128-CFB8", K128_HEX, IV128_HEX, PT32_HEX, 32, CFB8_32_HEX },
        { "CAMELLIA-192-CFB8", K192_HEX, IV128_HEX, PT32_HEX, 32, CFB8_32_192_HEX },
        { "CAMELLIA-256-CFB8", K256_HEX, IV128_HEX, PT32_HEX, 32, CFB8_32_256_HEX },
        { "CAMELLIA-128-CFB1", K128_HEX, IV128_HEX, PT32_HEX, 32, CFB1_32_HEX },
        { "CAMELLIA-192-CFB1", K192_HEX, IV128_HEX, PT32_HEX, 32, CFB1_32_192_HEX },
        { "CAMELLIA-256-CFB1", K256_HEX, IV128_HEX, PT32_HEX, 32, CFB1_32_256_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    size_t m, keylen, ct_wantlen, ptlen, ctlen, backlen;
    char msg[128];

    for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, modes[m].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core CAMELLIA stream mode");
        keylen = from_hex(modes[m].keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched CAMELLIA stream cipher has an unexpected key length");
        from_hex(modes[m].ivhex, iv, 16);
        ptlen = from_hex(modes[m].pthex, pt, 64);
        CHECK(ptlen == modes[m].ptlen,
              "stream-mode plaintext constant has an unexpected length");
        ct_wantlen = from_hex(modes[m].cthex, ct_want, 64);

        ctlen = cipher_crypt(cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 modes[m].ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        ptlen = cipher_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                             64 + 32, 0);
        snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
                 modes[m].ciphername);
        expect_equal(msg, pt_back, ptlen, pt, modes[m].ptlen);

        ctlen = stream_chunked(cipher, key, iv, pt, modes[m].ptlen, ct,
                               64 + 32, 1);
        snprintf(msg, sizeof(msg),
                 "%s streaming ciphertext does not match the one-shot ciphertext",
                 modes[m].ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        backlen = stream_chunked(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                                 64 + 32, 0);
        snprintf(msg, sizeof(msg),
                 "%s streaming decryption did not recover the plaintext",
                 modes[m].ciphername);
        expect_equal(msg, pt_back, backlen, pt, modes[m].ptlen);
        EVP_CIPHER_free(cipher);
    }
}

/* (5) CFB1 bit-length mode: the same route test_aes_cfb1 documents (bare
 * init, set_flags(EVP_CIPH_FLAG_LENGTH_BITS), key+IV init on a fresh
 * context).  In bit mode `inl` is a BIT count: the update writes
 * ceil(inl/8) bytes and reports outl = inl, and bit-mode over 8N bits must
 * agree with byte mode over N bytes.  Every bit-mode buffer is also sized
 * for the byte-count interpretation so a silent fallback stays inside the
 * allocation (see test_aes_cfb1). */
static void test_cfb1_bitmode(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "CAMELLIA-128-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(8);
    unsigned char *byte_mode = ALLOCATE_OUTPUT(8 + 16);
    unsigned char *bit_mode = ALLOCATE_OUTPUT(8 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"CAMELLIA-128-CFB1\") failed");
    from_hex(K128_HEX, key, 16);
    from_hex(IV128_HEX, iv, 16);
    for (i = 0; i < 8; i++)
        pt[i] = (unsigned char)(0x70 + i);

    /* Byte mode: 8 bytes = 64 one-bit CFB steps. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, byte_mode, &len, pt, 8) == 1,
          "byte-mode CFB1 update failed");
    CHECK(len == 8, "byte-mode CFB1 update did not emit 8 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, byte_mode + len, &len) == 1,
          "byte-mode CFB1 final failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Bit mode: 64 bits must give the same 8 bytes. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++; /* set_flags() reports no status; the equality check below
                 * proves the mode actually took effect. */
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "bit-mode key EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, bit_mode, &len, pt, 64) == 1,
          "bit-mode CFB1 update failed");
    CHECK(len == 64, "bit-mode CFB1 update did not report 64 bits");
    CHECK(EVP_EncryptFinal_ex(ctx, bit_mode + 8, &len) == 1,
          "bit-mode CFB1 final failed");
    CHECK(len == 0, "bit-mode CFB1 final emitted bytes");
    expect_equal("bit-mode CFB1 over 64 bits differs from byte mode over 8 bytes",
                 bit_mode, 8, byte_mode, 8);

    /* A partial final byte, on a FRESH encrypt context: 13 bits write
     * ceil(13/8) = 2 bytes (the trailing bits of the last destination byte
     * are left untouched -- the buffer starts zeroed), and decrypting the
     * same 13 bits on a fresh context recovers them. */
    EVP_CIPHER_CTX_free(ctx);
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++;
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "bit-mode key EVP_EncryptInit_ex failed");
    memset(bit_mode, 0, 8 + 16);
    CHECK(EVP_EncryptUpdate(ctx, bit_mode, &len, pt, 13) == 1,
          "bit-mode CFB1 13-bit update failed");
    CHECK(len == 13, "bit-mode CFB1 13-bit update did not report 13 bits");
    CHECK(EVP_EncryptFinal_ex(ctx, bit_mode + 2, &len) == 1,
          "bit-mode CFB1 13-bit final failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Decrypt the 13-bit ciphertext under bit mode: only the first 13 bits
     * of the destination are defined (the buffer starts zeroed, so the
     * untouched trailing bits stay zero, as in test_aes_cfb1). */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_DecryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++;
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "bit-mode key EVP_DecryptInit_ex failed");
    memset(byte_mode, 0, 8 + 16);
    CHECK(EVP_DecryptUpdate(ctx, byte_mode, &len, bit_mode, 13) == 1,
          "bit-mode CFB1 13-bit decrypt failed");
    CHECK(len == 13, "bit-mode CFB1 13-bit decrypt did not report 13 bits");
    CHECK(EVP_DecryptFinal_ex(ctx, byte_mode + 2, &len) == 1,
          "bit-mode CFB1 13-bit decrypt final failed");
    CHECK(byte_mode[0] == pt[0] &&
              (unsigned char)(byte_mode[1] >> 5) == (unsigned char)(pt[1] >> 5),
          "bit-mode CFB1 roundtrip of the first 13 bits failed");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_ecb_kats();
    test_alternate_routes();
    test_cbc();
    test_stream_modes();
    test_cfb1_bitmode();

    printf("test_camellia: all %u checks passed\n", g_checks);
    return 0;
}
