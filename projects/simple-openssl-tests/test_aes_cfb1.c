#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_cfb1 -- AES-128/192/256-CFB1 in both of its OpenSSL operating
 * modes: byte mode (the default) and bit-length mode.
 *
 * Byte-mode known-answer vectors: encrypting the NIST SP 800-38A shared
 * 4-block plaintext with IV 000102...0f (bit-length mode has no independent
 * KAT because `openssl enc` cannot pass bit lengths; it is verified against
 * byte mode below, which is exact since byte mode feeds 8 bits per byte).
 * Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb1 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb1 and -aes-256-cfb1).
 *
 * Bit-length mode (evidence: providers/implementations/ciphers/
 * ciphercommon_hw.c ossl_cipher_hw_generic_cfb1() branches on dat->use_bits,
 * and crypto/modes/cfb128.c CRYPTO_cfb128_1_encrypt() takes a *bit* count)
 * is enabled through the public route
 *
 *     EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS)
 *
 * which forwards OSSL_CIPHER_PARAM_USE_BITS ("use-bits") to the provider
 * (crypto/evp/evp_lib.c EVP_CIPHER_CTX_set_flags).  The flags -> provider
 * forwarding needs an initialized provider context, so the route used here
 * is the same two-phase pattern as the AEAD IV-length route: a bare init
 * first, then set_flags, then the init that passes key+IV.  Two pitfalls
 * were established by scratch experiments (filcc against pizfix 3.6.5 and
 * gcc against system 3.0.13, identical behavior):
 *
 *   - set_flags() BEFORE any init is silently dropped (no provider context
 *     exists yet), and
 *   - on a context whose flag bit is already set, a later set_flags() is a
 *     no-op (EVP forwards only flag *changes*), so a full re-init drops the
 *     context back to byte mode.
 *
 * Consequently every bit-mode operation below runs on a freshly created
 * context with the flag set exactly once, between the two inits.
 *
 * In bit mode the `inl` argument of EVP_EncryptUpdate is a BIT count: the
 * update writes ceil(inl/8) bytes and reports *outl = inl (bits).  Because a
 * silent byte-mode fallback would interpret inl as a byte count, every
 * bit-mode buffer below is sized for the byte-count interpretation too
 * (inl + 16 bytes, tail filled with 0xAA): a fallback then stays inside the
 * allocation and fails the equality/disciminator CHECKs cleanly instead of
 * overreading under Fil-C.
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
    fprintf(stderr, "FAIL test_aes_cfb1: %s\n", msg);
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

static const struct cfb1_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} cfb1_kats[] = {
    { "AES-128-CFB1",
      "2b7e151628aed2a6abf7158809cf4f3c",
      "68b3a264f838f5f8c3101070d1ab4c2e"
      "22e7f950383a0b71ade4fad0095cb188"
      "a57972c3c1882615f7511411fbebf119"
      "3997069704fc1d1f27028434c99e60f4" },
    { "AES-192-CFB1",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
      "9359bbb8ff599a3d90712530ca1d4f5b"
      "3eeef5b80a3be274805571771967a293"
      "61a277b4d4e02f337a84c418901a920c"
      "17ebbf7027e2f55e46490997c5235da9" },
    { "AES-256-CFB1",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",
      "9029c2ba5b7d440b562023deec3de592"
      "8e4fd76528e8cc3a548a0a49edf001d0"
      "d163541e6192479f27fe19a4f75d600d"
      "e033103f1d2bc1794ce1cf1464c0603b" },
};

/* One-shot CFB1 encrypt/decrypt in BYTE mode (inl is a byte count; each byte
 * feeds 8 one-bit CFB steps).  Returns the total output length written to
 * `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
static size_t cfb1_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* Creates a fresh context with bit-length mode enabled using the public
 * two-phase route, then passes key+IV.  A fresh context is used for every
 * bit-mode operation because EVP forwards only flag *changes* to the
 * provider: on a context whose EVP_CIPH_FLAG_LENGTH_BITS bit is already set,
 * a second set_flags() is a no-op and a subsequent (re-)init would silently
 * drop the context back to byte mode (scratch-verified on 3.0.13 and 3.6.5;
 * see the header comment).  Returns the context; caller frees it. */
static EVP_CIPHER_CTX *bitmode_ctx_new(const EVP_CIPHER *cipher,
                                       const unsigned char *key,
                                       const unsigned char *iv, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "bare EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "bare EVP_DecryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++; /* set_flags() reports no status; the behavioral checks
                 * below prove the mode actually took effect. */
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
              "bit-mode key EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
              "bit-mode key EVP_DecryptInit_ex failed");
    return ctx;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (the last chunk is truncated to what remains).  Byte mode only. */
static size_t cfb1_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) Byte-mode EVP_CIPHER_fetch one-shot KATs for all three key sizes, plus
 * the decrypt direction (CFB1 decryption uses the forward cipher). */
static void test_byte_mode_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(cfb1_kats) / sizeof(cfb1_kats[0]); k++) {
        const struct cfb1_kat *kat = &cfb1_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
        unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
        unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 16);
        size_t keylen, ct_wantlen, ctlen, ptlen;

        /* AES-CFB1 is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CFB1 cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
              "CFB1 block size is not 1");

        from_hex(KAT_IV_HEX, iv, 16);
        from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
        ct_wantlen = from_hex(kat->cthex, ct_want, KAT_PTLEN);

        ctlen = cfb1_crypt(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 16, 1);
        expect_equal("byte-mode ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);

        ptlen = cfb1_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                           KAT_PTLEN + 16, 0);
        expect_equal("byte-mode decrypting the KAT ciphertext did not give the plaintext",
                     pt_back, ptlen, pt, KAT_PTLEN);

        EVP_CIPHER_free(cipher);
    }
}

/* (2) Byte-mode streaming: updates of {1,15,16,17,31} bytes must reproduce
 * the one-shot ciphertext, and a 37-byte message round-trips. */
static void test_byte_mode_streaming(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(KAT_PTLEN + 32);
    unsigned char *pt37 = ALLOCATE_INPUT(37);
    unsigned char *ct37 = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *back37 = ALLOCATE_OUTPUT(37 + 16);
    size_t ct_wantlen, ctlen, ptlen, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB1\") failed");
    from_hex(cfb1_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT_HEX, pt, KAT_PTLEN);
    ct_wantlen = from_hex(cfb1_kats[0].cthex, ct_want, KAT_PTLEN);

    ctlen = cfb1_stream(cipher, key, iv, pt, KAT_PTLEN, ct, KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb1_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                        KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, KAT_PTLEN);

    for (i = 0; i < 37; i++)
        pt37[i] = (unsigned char)(i * 3 + 9);
    ctlen = cfb1_stream(cipher, key, iv, pt37, 37, ct37, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CFB1 encryption did not emit 37 bytes");
    ptlen = cfb1_stream(cipher, key, iv, ct37, ctlen, back37, 37 + 16, 0);
    expect_equal("37-byte roundtrip failed", back37, ptlen, pt37, 37);
    EVP_CIPHER_free(cipher);
}

/* (3) Byte-mode state continuity: a zero-length update is a no-op, and the
 * one-bit feedback state (which lives in the shifted IV) must carry across
 * update boundaries: 5 + 0 + 11 bytes == one 16-byte update. */
static void test_byte_mode_zero_update(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *one = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(16 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB1\") failed");
    from_hex(cfb1_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 3);

    cfb1_crypt(cipher, key, iv, in, 16, one, 16 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte CFB1 update failed");
    CHECK(len == 5, "5-byte CFB1 update did not emit 5 bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "CFB1 keeps no partial-byte counter, so num must stay 0");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-length CFB1 update failed");
    CHECK(len == 0, "zero-length CFB1 update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte CFB1 update failed");
    CHECK(len == 11, "11-byte CFB1 update did not emit 11 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 16, &len) == 1,
          "CFB1 Final failed");
    CHECK(len == 0, "CFB1 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(memcmp(one, many, 16) == 0,
          "5+0+11 updates diverged from a single 16-byte update");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

/* (4) Consistency: byte-mode encryption of n bytes == bit-mode encryption of
 * 8n bits, for full-byte bit lengths (1, 3, 5, 6, and 16 bytes).  Each
 * bit-mode update runs on a freshly reset context (see header comment). */
static void test_bit_mode_consistency(void)
{
    static const size_t sizes[] = { 1, 3, 5, 6, 16 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(128 + 16);   /* defensive: see header */
    unsigned char *byte_ref = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *bit_out = ALLOCATE_OUTPUT(128 + 16);
    size_t s, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB1\") failed");
    from_hex(cfb1_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0x6b + i * 13);
    memset(in + 16, 0xAA, 128 + 16 - 16);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        size_t bytelen = cfb1_crypt(cipher, key, iv, in, n, byte_ref,
                                    16 + 16, 1);
        EVP_CIPHER_CTX *ctx;
        int len = 0;

        CHECK(bytelen == n, "byte-mode reference length is wrong");
        ctx = bitmode_ctx_new(cipher, key, iv, 1);
        CHECK(n + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= 128 + 16,
              "output buffer smaller than the EVP update contract requires");
        CHECK(EVP_EncryptUpdate(ctx, bit_out, &len, in, (int)(8 * n)) == 1,
              "bit-mode EVP_EncryptUpdate failed");
        CHECK(len == (int)(8 * n),
              "bit-mode update did not report the bit count as its length");
        CHECK(memcmp(byte_ref, bit_out, n) == 0,
              "bit-mode encryption of 8n bits differs from byte-mode "
              "encryption of n bytes");
        EVP_CIPHER_CTX_free(ctx);
    }
    EVP_CIPHER_free(cipher);
}

/* (5) Bit mode with a bit length that is not a multiple of 8: encrypting 41
 * bits writes ceil(41/8) = 6 bytes, where the last byte carries only the top
 * bit (the remaining seven bits of the destination byte are left untouched),
 * and the 41 bits round-trip through bit-mode decryption. */
static void test_bit_mode_41_bits(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(41 + 16);    /* defensive: see header */
    unsigned char *byte_ref = ALLOCATE_OUTPUT(6 + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(41 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(41 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CFB1\") failed");
    from_hex(cfb1_kats[0].keyhex, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0x6b + i * 13);
    memset(in + 16, 0xAA, 41 + 16 - 16);
    memset(ct, 0, 41 + 16); /* destination bits beyond the 41st stay zero */
    memset(byte_ref, 0, 6 + 16);

    /* Byte-mode reference: the first 41 bits of the 6-byte (48-bit) run. */
    cfb1_crypt(cipher, key, iv, in, 6, byte_ref, 6 + 16, 1);

    ctx = bitmode_ctx_new(cipher, key, iv, 1);
    CHECK((size_t)41 + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= 41 + 16,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, in, 41) == 1,
          "41-bit EVP_EncryptUpdate failed");
    CHECK(len == 41, "41-bit update did not report 41 (bits) as its length");
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &len) == 1, "bit-mode Final failed");
    CHECK(len == 0, "bit-mode Final emitted unexpected bytes");

    CHECK(memcmp(ct, byte_ref, 5) == 0,
          "the first 40 ciphertext bits differ from the byte-mode reference");
    CHECK((ct[5] & 0x7f) == 0,
          "bit mode did not leave the trailing 7 bits of the last byte alone "
          "(bit-length mode is not active)");
    CHECK((ct[5] >> 7) == (byte_ref[5] >> 7),
          "the 41st ciphertext bit differs from the byte-mode reference");

    /* Roundtrip: decrypting the 41 ciphertext bits gives back the 41
     * plaintext bits, again leaving the trailing bits of the last byte
     * untouched (the output buffer starts zeroed). */
    EVP_CIPHER_CTX_free(ctx);
    ctx = bitmode_ctx_new(cipher, key, iv, 0);
    memset(back, 0, 41 + 16);
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, 41) == 1,
          "41-bit EVP_DecryptUpdate failed");
    CHECK(len == 41, "41-bit decrypt update did not report 41 (bits)");
    CHECK((back[5] & 0x7f) == 0,
          "bit-mode decrypt disturbed bits beyond the 41st bit");
    CHECK((back[5] >> 7) == (in[5] >> 7),
          "the 41st plaintext bit did not round-trip");
    CHECK(memcmp(back, in, 5) == 0,
          "the first 40 plaintext bits did not round-trip");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (6) Bit-mode state continuity across updates: the keystream (shifted-IV)
 * state carries across update boundaries, so 8 bits + 0 bits + 120 bits (all
 * byte-aligned in the input: each update restarts at the MSB of the byte it
 * is given) must equal a single 128-bit update (16 bytes).
 * EVP_CIPHER_CTX_get_num stays 0 because the bit position lives entirely in
 * the shifted IV. */
static void test_bit_mode_split(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-192-CFB1", NULL);
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(128 + 16);   /* defensive: see header */
    unsigned char *one = ALLOCATE_OUTPUT(128 + 16);
    unsigned char *split = ALLOCATE_OUTPUT(128 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-192-CFB1\") failed");
    from_hex(cfb1_kats[1].keyhex, key, 24);
    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0x6b + i * 13);
    memset(in + 16, 0xAA, 128 + 16 - 16);

    ctx = bitmode_ctx_new(cipher, key, iv, 1);
    CHECK((size_t)128 + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= 128 + 16,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, one, &len, in, 128) == 1,
          "128-bit one-shot update failed");
    CHECK(len == 128, "128-bit one-shot update did not report 128 (bits)");
    CHECK(EVP_EncryptFinal_ex(ctx, one + 16, &len) == 1, "bit-mode Final failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "bit-mode num is not 0 after a 128-bit update");
    EVP_CIPHER_CTX_free(ctx);

    ctx = bitmode_ctx_new(cipher, key, iv, 1);
    CHECK(EVP_EncryptUpdate(ctx, split, &len, in, 8) == 1,
          "8-bit split update failed");
    CHECK(len == 8, "8-bit split update did not report 8 (bits)");
    CHECK(EVP_EncryptUpdate(ctx, split + 1, &len, in, 0) == 1,
          "0-bit split update failed");
    CHECK(len == 0, "0-bit split update emitted bits");
    CHECK(EVP_EncryptUpdate(ctx, split + 1, &len, in + 1, 120) == 1,
          "120-bit split update failed");
    CHECK(len == 120, "120-bit split update did not report 120 (bits)");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "bit-mode num is not 0 after split updates");
    CHECK(EVP_EncryptFinal_ex(ctx, split + 16, &len) == 1, "bit-mode Final failed");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(memcmp(one, split, 16) == 0,
          "8+0+120 split bit updates diverged from a single 128-bit update");
    g_checks++;
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_byte_mode_kats();
    test_byte_mode_streaming();
    test_byte_mode_zero_update();
    test_bit_mode_consistency();
    test_bit_mode_41_bits();
    test_bit_mode_split();

    printf("test_aes_cfb1: all %u checks passed\n", g_checks);
    return 0;
}
