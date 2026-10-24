#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_des -- the legacy provider's single-DES family: DES-ECB/CBC/OFB/
 * CFB/CFB1/CFB8, DESX-CBC, and the two-key DES-EDE family (ECB/CBC/CFB/
 * OFB; the EDE ciphers come from the default provider).
 *
 * Loading the legacy provider explicitly turns OFF default-provider
 * auto-load, so both providers are loaded here (see the "Behavioral
 * notes" section of README.md); the DES-EDE ciphers then still fetch
 * from the default provider.
 *
 * Known-answer vectors: verified against the authoritative pizfix
 * OpenSSL 3.6.5 CLI with the legacy provider enabled,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * Note `openssl enc -desx-cbc` also works with both providers loaded.
 * The two-key DES-EDE stream modes were verified the same way, with the
 * default provider alone supplying the cipher and no -nopad needed
 * (CFB/OFB emit one byte per input byte, so `enc` adds no padding):
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede-cfb \
 *       -K <keyhex> -iv <ivhex>      (likewise -des-ede-ofb)
 *
 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * padded operations roundtrip odd sizes; CBC decryption holds the last
 * block until Final; CFB8 runs one bit-byte per 8 input bits; CFB1 runs
 * in byte mode by default and in bit-length mode via the
 * EVP_CIPH_FLAG_LENGTH_BITS route documented in test_aes_cfb1.  The EDE
 * stream modes DES-EDE-CFB/DES-EDE-OFB are 8-byte-block CFB64/OFB with a
 * 16-byte (two-key) key and an 8-byte IV; EVP reports block size 1 for
 * them, so every update streams its input through byte-for-byte and the
 * shift register plus num offset must carry across updates.  The
 * DESX-CBC 24-byte key is [8-byte DES key][8-byte input whitening]
 * [8-byte output whitening] (cipher_desx_hw.c: DES_set_key_unchecked on
 * key[0..7], ks2 = key[8..15], ks3 = key[16..23]); the composition check
 * below rebuilds DESX from per-block DES-ECB calls exactly as
 * DES_xcbc_encrypt chains them (chain_0 = IV, chain_i = the WHITENED
 * previous ciphertext).
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
    fprintf(stderr, "FAIL test_des: %s\n", msg);
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

static const char *const DESKEY_HEX = "0123456789abcdef";
static const char *const IV8_HEX = "0102030405060708";
static const char *const PT8_HEX = "4142434445464748";
static const char *const PT16_HEX =
    "0123456789abcdef0123456789abcdef";
static const char *const PT32_HEX =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* DESX-CBC: 24-byte key = [DES key][input whitening][output whitening]. */
static const char *const DESXKEY_HEX =
    "0102030405060708090a0b0c0d0e0f10"
    "1112131415161718";
static const char *const DESX_CT_HEX =
    "08a2885ed8f904ce693763307fbf570f";

/* Two-key DES-EDE. */
static const char *const EDEKEY_HEX =
    "0123456789abcdef23456789abcdef01";

/* Two-key DES-EDE stream modes over EDEKEY_HEX + IV8_HEX, 32 bytes of
 * PT32_HEX (oracle commands in the header comment; identical from the
 * pizfix 3.6.5 and system 3.0.13 CLIs). */
static const char *const EDE_CFB_CT_HEX =
    "b233a10f047d2665c88cdc7de11d93e5"
    "07f7af0d9393f2d190866abed4985417";
static const char *const EDE_OFB_CT_HEX =
    "b233a10f047d26655050f492f0906a6c"
    "1d829b3f7c5f988fe883a88ff3103280";

static const struct des_kat {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    size_t ptlen;
    const char *cthex;
} des_kats[] = {
    { "DES-ECB", DESKEY_HEX, "", PT8_HEX, 8,
      "8df6a7a3feae6d34" },
    { "DES-CBC", DESKEY_HEX, IV8_HEX, PT16_HEX, 16,
      "3c144b1ff1adace9f62b10b5b3b92367" },
    { "DES-OFB", DESKEY_HEX, IV8_HEX, PT32_HEX, 32,
      "e7ac3c7c22bd1909dcd8d522572d714a"
      "04fe28c67c180bab5850042209eee757" },
    { "DES-CFB", DESKEY_HEX, IV8_HEX, PT32_HEX, 32,
      "e7ac3c7c22bd1909110f832bb76c786c"
      "0507aa1bf0eb325d9888d6d6b19ec2f3" },
    { "DES-CFB8", DESKEY_HEX, IV8_HEX, PT32_HEX, 32,
      "e788798e955d41d7ec86f1d625b7ba7c"
      "fe86ca12b47363c6aecd77b8cb4dfa3d" },
    { "DES-CFB1", DESKEY_HEX, IV8_HEX, PT32_HEX, 32,
      "b0047cfd4ae7e6c1c490d237bc3c1938"
      "9d723230649de06a62e8e92519898265" },
    { "DES-EDE", EDEKEY_HEX, "", PT8_HEX, 8,
      "5b4605e606393286" },
    { "DES-EDE-CBC", EDEKEY_HEX, IV8_HEX, PT16_HEX, 16,
      "203b62b1fd1194a7b3850fb3d5e33c85" },
    { "DES-EDE-CFB", EDEKEY_HEX, IV8_HEX, PT32_HEX, 32,
      EDE_CFB_CT_HEX },
    { "DES-EDE-OFB", EDEKEY_HEX, IV8_HEX, PT32_HEX, 32,
      EDE_OFB_CT_HEX },
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

/* (1) The DES/DES-EDE KATs through EVP_CIPHER_fetch (the legacy provider
 * must be loaded for DES-*, the default provider supplies the DES-EDE
 * family), plus decrypting each KAT ciphertext back to its plaintext. */
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

    for (k = 0; k < sizeof(des_kats) / sizeof(des_kats[0]); k++) {
        const struct des_kat *kat = &des_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

        /* The legacy provider is loaded in main(); a NULL fetch here is a
         * broken build, not a version gate. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a DES cipher");
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) ==
                  strlen(kat->keyhex) / 2,
              "fetched DES cipher has an unexpected key length");
        from_hex(kat->keyhex, key, 24);
        from_hex(kat->ivhex, iv, 8);
        from_hex(kat->pthex, pt, 32);
        ct_wantlen = from_hex(kat->cthex, ct_want, 32);

        ctlen = cipher_crypt(cipher, key,
                             kat->ivhex[0] ? iv : NULL, pt, kat->ptlen, ct,
                             32 + 16, 1);
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

/* (2) 8-byte-block semantics for DES-CBC: nopad exact-multiple roundtrip,
 * padded roundtrips of odd sizes, streaming {1,7,8,9} equality, and the
 * 0-byte no-op. */
static void test_block_semantics(void)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9 };
    static const size_t pad_sizes[] = { 1, 7, 8, 9, 33 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-CBC", NULL);
    unsigned char *key = ALLOCATE_KEY(8);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *pt_back = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *oneshot = ALLOCATE_OUTPUT(33 + 24);
    EVP_CIPHER_CTX *ctx;
    size_t s, i, ctlen, backlen, olen;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-CBC\") failed");
    from_hex(DESKEY_HEX, key, 8);
    from_hex(IV8_HEX, iv, 8);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    /* nopad: 16 bytes roundtrip as an exact multiple of 8. */
    ctlen = cbc_crypt(cipher, key, iv, pt, 16, ct, 33 + 24, 1, 0);
    CHECK(ctlen == 16, "nopad DES-CBC encryption of 16 bytes != 16 bytes");
    backlen = cbc_crypt(cipher, key, iv, ct, 16, pt_back, 33 + 24, 0, 0);
    expect_equal("nopad DES-CBC roundtrip failed", pt_back, backlen, pt, 16);

    /* pad: odd sizes roundtrip (33 bytes -> 40 with a full pad block). */
    for (s = 0; s < sizeof(pad_sizes) / sizeof(pad_sizes[0]); s++) {
        size_t n = pad_sizes[s];
        size_t padded = (n / 8 + 1) * 8;
        char msg[128];

        ctlen = cbc_crypt(cipher, key, iv, pt, n, ct, 33 + 24, 1, 1);
        snprintf(msg, sizeof(msg),
                 "padded DES-CBC encryption of %zu bytes gave %zu, not %zu",
                 n, ctlen, padded);
        CHECK(ctlen == padded, msg);
        backlen = cbc_crypt(cipher, key, iv, ct, ctlen, pt_back, 33 + 24, 0, 1);
        snprintf(msg, sizeof(msg), "padded DES-CBC %zu-byte roundtrip failed", n);
        expect_equal(msg, pt_back, backlen, pt, n);
    }

    /* Streaming {1,7,8,9}: equals one-shot in both directions (nopad). */
    olen = cbc_crypt(cipher, key, iv, pt, 16, oneshot, 33 + 24, 1, 0);
    ctlen = stream_chunked(cipher, key, iv, pt, 16, ct, 33 + 24, 1,
                           stream_chunks, 4);
    expect_equal("streaming DES-CBC ciphertext does not match one-shot",
                 ct, ctlen, oneshot, olen);
    backlen = stream_chunked(cipher, key, iv, oneshot, olen, pt_back,
                             33 + 24, 0, stream_chunks, 4);
    expect_equal("streaming DES-CBC decryption did not recover the plaintext",
                 pt_back, backlen, pt, 16);

    /* 0-byte no-op: an empty update succeeds, emits nothing, Final too. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 0) == 1,
          "0-byte DES-CBC update failed");
    CHECK(len == 0, "0-byte DES-CBC update emitted bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &flen) == 1,
          "0-byte DES-CBC final failed");
    CHECK(flen == 0, "0-byte nopad DES-CBC final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (3) CFB8: byte-at-a-time updates must reproduce the one-shot ciphertext
 * (the 8-bit shift register carries the byte boundary), for both
 * directions. */
static void test_cfb8_bytewise(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-CFB8", NULL);
    unsigned char *key = ALLOCATE_KEY(8);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *bybyte = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    size_t i, olen, blen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-CFB8\") failed");
    from_hex(DESKEY_HEX, key, 8);
    from_hex(IV8_HEX, iv, 8);
    from_hex(PT32_HEX, pt, 32);

    olen = cipher_crypt(cipher, key, iv, pt, 32, oneshot, 32 + 16, 1);

    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "EVP_CIPHER_CTX_set_padding(0) failed");
        for (i = 0; i < 32; i++) {
            CHECK(EVP_EncryptUpdate(ctx, bybyte + i, &len, pt + i, 1) == 1,
                  "byte-at-a-time DES-CFB8 update failed");
            CHECK(len == 1, "DES-CFB8 1-byte update did not emit 1 byte");
        }
        CHECK(EVP_EncryptFinal_ex(ctx, bybyte + 32, &len) == 1,
              "byte-at-a-time DES-CFB8 final failed");
        EVP_CIPHER_CTX_free(ctx);
    }
    expect_equal("byte-at-a-time DES-CFB8 differs from the one-shot ciphertext",
                 bybyte, 32, oneshot, olen);

    /* And decrypting byte-at-a-time recovers the plaintext. */
    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "EVP_CIPHER_CTX_set_padding(0) failed");
        for (i = 0; i < 32; i++) {
            CHECK(EVP_DecryptUpdate(ctx, back + i, &len, oneshot + i, 1) == 1,
                  "byte-at-a-time DES-CFB8 decrypt update failed");
        }
        CHECK(EVP_DecryptFinal_ex(ctx, back + 32, &len) == 1,
              "byte-at-a-time DES-CFB8 decrypt final failed");
        EVP_CIPHER_CTX_free(ctx);
    }
    blen = 32;
    expect_equal("byte-at-a-time DES-CFB8 decrypt did not recover the plaintext",
                 back, blen, pt, 32);
    EVP_CIPHER_free(cipher);
}

/* (4) CFB1 byte mode, plus what EVP_CIPH_FLAG_LENGTH_BITS does for DES:
 * the flag is accepted (forwarded as "use-bits" to the provider), but the
 * DES provider's CFB1 hardware hook (cipher_des_hw.c
 * cipher_hw_des_cfb1_cipher) has NO use_bits branch -- unlike the generic
 * ossl_cipher_hw_generic_cfb1 (ciphercommon_hw.c:87) used by AES -- so
 * the update length remains a BYTE count and 8 "flagged" bytes produce
 * exactly the byte-mode result.  (This was pinned after a Fil-C run
 * trapped the buffer overread a *bit*-count update would cause: the
 * flagged-but-ignored path would read inl bytes for inl bits.  The test
 * therefore asserts the safe, real contract: flag set or not, inl is a
 * byte count and the outputs agree.) */
static void test_cfb1_bitmode(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "DES-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(8);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(8);
    unsigned char *byte_mode = ALLOCATE_OUTPUT(8 + 16);
    unsigned char *flag_mode = ALLOCATE_OUTPUT(8 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"DES-CFB1\") failed");
    from_hex(DESKEY_HEX, key, 8);
    from_hex(IV8_HEX, iv, 8);
    for (i = 0; i < 8; i++)
        pt[i] = (unsigned char)(0x60 + i);

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

    /* Flagged context (fresh, two-phase init per test_aes_cfb1): the same
     * 8-byte update must give the same 8 bytes, because the DES provider
     * ignores use-bits. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++; /* set_flags() reports no status; the checks below prove
                 * the flag was accepted but has no effect on DES. */
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "flagged key EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, flag_mode, &len, pt, 8) == 1,
          "flagged CFB1 update failed");
    CHECK(len == 8,
          "flagged CFB1 update did not emit 8 bytes (inl is a byte count)");
    CHECK(EVP_EncryptFinal_ex(ctx, flag_mode + 8, &len) == 1,
          "flagged CFB1 final failed");
    CHECK(len == 0, "flagged CFB1 final emitted bytes");
    expect_equal("flagged CFB1 differs from byte mode (DES ignores use-bits)",
                 flag_mode, 8, byte_mode, 8);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (5) DESX-CBC: KAT, roundtrip, and the per-block composition
 * ct_i = DES-ECB(pt_i ^ in_w ^ chain_i) ^ out_w with chain_0 = IV and
 * chain_i = the WHITENED previous ciphertext (DES_xcbc_encrypt's chaining
 * order, pinned by byte-level experiments and the cipher_desx_hw.c key
 * layout: ks1 = key[0..7], in-whitening = key[8..15], out-whitening =
 * key[16..23]). */
static void test_desx(void)
{
    EVP_CIPHER *dx = EVP_CIPHER_fetch(NULL, "DESX-CBC", NULL);
    EVP_CIPHER *ecb = EVP_CIPHER_fetch(NULL, "DES-ECB", NULL);
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *composed = ALLOCATE_OUTPUT(16);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16);
    unsigned char blk[8], chain[8];
    size_t i, b, ct_wantlen, ctlen, backlen;
    int len;

    CHECK(dx != NULL, "EVP_CIPHER_fetch(\"DESX-CBC\") failed");
    CHECK(ecb != NULL, "EVP_CIPHER_fetch(\"DES-ECB\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(dx) == 24,
          "DESX-CBC key length is not 24");
    from_hex(DESXKEY_HEX, key, 24);
    from_hex(IV8_HEX, iv, 8);
    from_hex(PT16_HEX, pt, 16);
    ct_wantlen = from_hex(DESX_CT_HEX, ct_want, 16);

    ctlen = cipher_crypt(dx, key, iv, pt, 16, ct, 16 + 16, 1);
    expect_equal("DESX-CBC ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    /* Composition from per-block DES-ECB calls. */
    memcpy(chain, iv, 8);
    for (b = 0; b < 2; b++) {
        EVP_CIPHER_CTX *e = EVP_CIPHER_CTX_new();
        CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
        for (i = 0; i < 8; i++)
            blk[i] = (unsigned char)(pt[b * 8 + i] ^ key[8 + i] ^ chain[i]);
        CHECK(EVP_EncryptInit_ex2(e, ecb, key, NULL, NULL) == 1,
              "composition DES-ECB init failed");
        CHECK(EVP_CIPHER_CTX_set_padding(e, 0) == 1,
              "composition DES-ECB set_padding failed");
        CHECK(EVP_EncryptUpdate(e, composed + b * 8, &len, blk, 8) == 1,
              "composition DES-ECB update failed");
        CHECK(len == 8, "composition DES-ECB update length mismatch");
        CHECK(EVP_EncryptFinal_ex(e, composed + b * 8 + len, &len) == 1,
              "composition DES-ECB final failed");
        EVP_CIPHER_CTX_free(e);
        for (i = 0; i < 8; i++) {
            composed[b * 8 + i] ^= key[16 + i];
            chain[i] = composed[b * 8 + i];
        }
    }
    expect_equal("the DESX-CBC composition differs from the stitched output",
                 composed, 16, ct, ctlen);

    backlen = cipher_crypt(dx, key, iv, ct_want, ct_wantlen, back, 16 + 16, 0);
    expect_equal("DESX-CBC roundtrip did not recover the plaintext",
                 back, backlen, pt, 16);
    EVP_CIPHER_free(dx);
    EVP_CIPHER_free(ecb);
}

/* (6) DES-EDE-CFB and DES-EDE-OFB: the two-key EDE stream modes the
 * default provider lists alongside DES-EDE/DES-EDE-CBC.  Both are DES
 * CFB64/OFB over the same 16-byte key and 8-byte IV, so only the feedback
 * differs (the ciphertext shift register vs the keystream register);
 * their KATs agree on the first block and diverge after it.  EVP reports
 * block size 1 for both (every input byte is emitted immediately), so
 * partial updates MUST equal the one-shot result: the 8-byte DES shift
 * register and its num offset live in the ctx and carry across updates.
 * The 0-byte-update check pins that an empty update emits nothing and
 * leaves that state untouched. */
static void test_ede_stream_modes(void)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9 };
    static const struct ede_stream_kat {
        const char *ciphername;
        const char *cthex;
    } ede_stream_kats[] = {
        { "DES-EDE-CFB", EDE_CFB_CT_HEX },
        { "DES-EDE-OFB", EDE_OFB_CT_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *streamed = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    size_t m, ct_wantlen, olen, slen, backlen;
    char msg[128];

    from_hex(EDEKEY_HEX, key, 16);
    from_hex(IV8_HEX, iv, 8);
    from_hex(PT32_HEX, pt, 32);

    for (m = 0; m < sizeof(ede_stream_kats) / sizeof(ede_stream_kats[0]); m++) {
        const struct ede_stream_kat *kat = &ede_stream_kats[m];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

        /* The default provider is loaded in main(); a NULL fetch here is
         * a broken build, not a version gate. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for an EDE stream mode");
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 16,
              "EDE stream mode key length is not 16 (two-key EDE)");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
              "EDE stream mode does not report the stream-mode block size 1");
        ct_wantlen = from_hex(kat->cthex, ct_want, 32);

        /* One-shot encrypt == KAT. */
        olen = cipher_crypt(cipher, key, iv, pt, 32, oneshot, 32 + 16, 1);
        snprintf(msg, sizeof(msg),
                 "%s one-shot ciphertext does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, oneshot, olen, ct_want, ct_wantlen);

        /* Streaming {1,7,8,9}: equals one-shot in both directions. */
        slen = stream_chunked(cipher, key, iv, pt, 32, streamed, 32 + 16, 1,
                              stream_chunks, 4);
        snprintf(msg, sizeof(msg),
                 "%s streaming ciphertext does not match one-shot",
                 kat->ciphername);
        expect_equal(msg, streamed, slen, oneshot, olen);
        backlen = stream_chunked(cipher, key, iv, oneshot, olen, back,
                                 32 + 16, 0, stream_chunks, 4);
        snprintf(msg, sizeof(msg),
                 "%s streaming decryption did not recover the plaintext",
                 kat->ciphername);
        expect_equal(msg, back, backlen, pt, 32);

        /* 0-byte update: a no-op that neither emits nor disturbs the
         * shift register -- the full update after it still gives the
         * one-shot ciphertext, and Final emits nothing. */
        {
            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            int len = 0, flen = 0;

            CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
            CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
                  "EVP_EncryptInit_ex2 failed");
            CHECK(EVP_EncryptUpdate(ctx, streamed, &len, pt, 0) == 1,
                  "0-byte EDE stream-mode update failed");
            CHECK(len == 0, "0-byte EDE stream-mode update emitted bytes");
            CHECK(EVP_EncryptUpdate(ctx, streamed, &len, pt, 32) == 1,
                  "EDE stream-mode update after the 0-byte update failed");
            CHECK(len == 32, "EDE stream-mode update did not emit all 32 bytes");
            CHECK(EVP_EncryptFinal_ex(ctx, streamed + len, &flen) == 1,
                  "EDE stream-mode final failed");
            CHECK(flen == 0, "EDE stream-mode final emitted bytes");
            EVP_CIPHER_CTX_free(ctx);
            snprintf(msg, sizeof(msg),
                     "%s ciphertext after a 0-byte update differs from one-shot",
                     kat->ciphername);
            expect_equal(msg, streamed, (size_t)len, oneshot, olen);
        }

        /* Roundtrip: one-shot decrypt of the one-shot ciphertext. */
        backlen = cipher_crypt(cipher, key, iv, oneshot, olen, back, 32 + 16, 0);
        snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
                 kat->ciphername);
        expect_equal(msg, back, backlen, pt, 32);

        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    /* Loading the legacy provider disables default-provider auto-load, so
     * both are loaded explicitly (DES-* here lives in legacy, the
     * DES-EDE family in default). */
    CHECK(OSSL_PROVIDER_load(NULL, "legacy") != NULL,
          "OSSL_PROVIDER_load(\"legacy\") failed");
    CHECK(OSSL_PROVIDER_load(NULL, "default") != NULL,
          "OSSL_PROVIDER_load(\"default\") failed");

    test_kats();
    test_block_semantics();
    test_cfb8_bytewise();
    test_cfb1_bitmode();
    test_desx();
    test_ede_stream_modes();

    printf("test_des: all %u checks passed\n", g_checks);
    return 0;
}
