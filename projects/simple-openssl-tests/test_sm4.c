#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_sm4 -- SM4-ECB/CBC/CTR/OFB/CFB plus SM4-XTS, SM4-GCM and SM4-CCM.
 *
 * Known-answer vectors:
 *
 *   - The GB/T 32907-2016 standard ECB vector (key = plaintext =
 *     0123456789abcdeffedcba9876543210 -> ciphertext
 *     681edf34d206965e86b3e94f536e4246), verified with
 *
 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -sm4-ecb -nopad -K <keyhex>
 *
 *   - The other classical-mode constants were verified the same way
 *     against the authoritative pizfix 3.6.5 CLI and cross-checked on the
 *     system 3.0.13 (identical values).
 *   - SM4-XTS/GCM/CCM postdate OpenSSL 3.0 (they fetch on 3.6.5, fail on
 *     3.0.13), and `openssl enc` cannot do XTS or AEAD anyway, so their
 *     constants were derived with a scratch probe compiled by
 *     build/bin/filcc against the pizfix 3.6.5 build; the corresponding
 *     sections print a version-gated SKIP line when the fetch fails.
 *
 * AEAD ordering rules mirror test_aes_gcm_small/test_aes_ccm_small: the
 * GCM IV length is set before the init that passes the IV; AAD updates
 * (out == NULL) come first; CCM needs ivlen AND taglen before the
 * key+nonce init, announces the payload length through a NULL update, and
 * takes the payload in ONE update (a second data update fails).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_TAG(size_t n)    { return poison_alloc(POISON_TAG, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_sm4: %s\n", msg);
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

static const char *const K128_HEX =
    "0123456789abcdeffedcba9876543210";
static const char *const PT16_HEX =
    "0123456789abcdeffedcba9876543210";
static const char *const PT32_HEX =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";
static const char *const PT64_HEX =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
static const char *const IV128_HEX =
    "fedcba98765432100123456789abcdef";
static const char *const CTRIV_HEX =
    "000102030405060708090a0b0c0d0e0f";

/* AEAD shared material: 16-byte plaintext 000102...0f, 12-byte nonce and
 * the AAD used by the GCM behavior section. */
static const char *const AEAD_PT_HEX =
    "000102030405060708090a0b0c0d0e0f";
static const char *const AEAD_NONCE_HEX =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0";
static const char *const GCM_AAD_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";

/* SM4-XTS key = two concatenated SM4 keys; the IV is the data-unit tweak. */
static const char *const XTS_KEY_HEX =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
    "00112233445566778899aabbccddeeff";
static const char *const XTS_IV_HEX =
    "0102030405060708090a0b0c0d0e0f10";
static const char *const XTS_PT32_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "000102030405060708090a0b0c0d0e0f";
static const char *const XTS_CT32_HEX =
    "167437d385df1664e9dea08d61758673"
    "de4302fcd839179df8f5304d0b833042";

/* One-shot classical-mode encrypt/decrypt with padding disabled.  Returns
 * the total output length written to `out` (room for inlen + block_size
 * per the EVP contract). */
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
static size_t sm4_cbc(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) The GB/T standard ECB vector, streaming equality, and the
 * CBC pad/nopad roundtrips plus the CBC/CTR/OFB/CFB KATs. */
static void test_classical(void)
{
    static const struct {
        const char *ciphername;
        const char *ivhex;
        const char *pthex;
        size_t ptlen;
        const char *cthex;
    } modes[] = {
        { "SM4-CBC", IV128_HEX, PT64_HEX, 64,
          "58a407c9da5780d7c92bfa6a1a59ded1"
          "0ac66047f3a8fc6cd361228cba2f9243"
          "347c015b04929ce12ee5196f34f57d96"
          "006b90f3f14ff037c5ef7f26dd9cad7c" },
        { "SM4-CTR", CTRIV_HEX, PT64_HEX, 64,
          "6d59228313e6f73bc3b08993923bee40"
          "c12a871c5ea0509d44267c49c4af234b"
          "2c124ea66973137fe716b8d7e916d682"
          "3237723f8f458b4e9db9c9454055c9bf" },
        { "SM4-OFB", IV128_HEX, PT32_HEX, 32,
          "f181f519ed76e1ca91da7b29543b365b"
          "2db6ba0d1dd13606943215eeaba61907" },
        { "SM4-CFB", IV128_HEX, PT32_HEX, 32,
          "f181f519ed76e1ca91da7b29543b365b"
          "0ba4051427288582e6d4d9107b45ca63" },
    };
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    size_t m, ct_wantlen, ctlen, backlen;
    char msg[128];

    /* ECB KAT. */
    {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-ECB", NULL);
        size_t ptlen, keylen;

        /* SM4 is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"SM4-ECB\") failed");
        keylen = from_hex(K128_HEX, key, 16);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched SM4 cipher has an unexpected key length");
        ptlen = from_hex(PT16_HEX, pt, 64);
        ct_wantlen = from_hex("681edf34d206965e86b3e94f536e4246", ct_want, 16);

        ctlen = cipher_crypt(cipher, key, NULL, pt, ptlen, ct, 64 + 32, 1);
        expect_equal("SM4-ECB one-shot ciphertext does not match the GB/T KAT",
                     ct, ctlen, ct_want, ct_wantlen);
        ctlen = stream_chunked(cipher, key, NULL, pt, ptlen, ct, 64 + 32, 1);
        expect_equal("SM4-ECB streaming ciphertext does not match the KAT",
                     ct, ctlen, ct_want, ct_wantlen);
        backlen = stream_chunked(cipher, key, NULL, ct_want, ct_wantlen,
                                 pt_back, 64 + 32, 0);
        expect_equal("SM4-ECB streaming decryption did not recover the KAT "
                     "plaintext", pt_back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }

    /* CBC pad/nopad roundtrips. */
    {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-CBC", NULL);
        unsigned char *odd = ALLOCATE_INPUT(37);
        unsigned char *odd_ct = ALLOCATE_OUTPUT(40 + 16);
        size_t i, oddlen, oddback;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"SM4-CBC\") failed");
        from_hex(K128_HEX, key, 16);
        from_hex(IV128_HEX, iv, 16);
        from_hex(PT64_HEX, pt, 64);

        /* nopad: exact multiples roundtrip */
        ctlen = sm4_cbc(cipher, key, iv, pt, 64, ct, 64 + 32, 1, 0);
        CHECK(ctlen == 64, "nopad CBC encryption of 64 bytes != 64 bytes");
        backlen = sm4_cbc(cipher, key, iv, ct, 64, pt_back, 64 + 32, 0, 0);
        expect_equal("nopad SM4-CBC roundtrip failed", pt_back, backlen, pt, 64);

        /* pad: odd sizes roundtrip (37 bytes -> 40 with a full pad block) */
        for (i = 0; i < 37; i++)
            odd[i] = (unsigned char)(i * 7 + 3);
        oddlen = sm4_cbc(cipher, key, iv, odd, 37, odd_ct, 48 + 16, 1, 1);
        CHECK(oddlen == 48, "padded CBC encryption of 37 bytes did not give 48");
        oddback = sm4_cbc(cipher, key, iv, odd_ct, oddlen, pt_back, 48 + 16,
                          0, 1);
        expect_equal("padded SM4-CBC roundtrip did not recover 37 bytes",
                     pt_back, oddback, odd, 37);
        EVP_CIPHER_free(cipher);
    }

    /* Stream-mode KATs + streaming equality. */
    for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, modes[m].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core SM4 mode");
        from_hex(K128_HEX, key, 16);
        from_hex(modes[m].ivhex, iv, 16);
        from_hex(modes[m].pthex, pt, 64);
        ct_wantlen = from_hex(modes[m].cthex, ct_want, 64);

        ctlen = cipher_crypt(cipher, key, iv, pt, modes[m].ptlen, ct,
                             64 + 32, 1);
        snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
                 modes[m].ciphername);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

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

/* One-shot GCM encrypt: SET_IVLEN before the key+IV init, AAD first with
 * out == NULL, then the payload, then Final, then GET_TAG. */
static void gcm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, dlen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_EncryptUpdate failed");
    dlen = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* One-shot GCM decrypt with tag verification. */
static void gcm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        const unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, dlen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_DecryptUpdate failed");
    dlen = len;
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                              tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* (2) SM4-GCM (3.1+): derived KAT, IV-length-before-init, AAD-first
 * ordering, roundtrip and tamper detection. */
static void test_gcm(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16);
    unsigned char tag_bad[16];
    EVP_CIPHER_CTX *ctx;
    size_t aadlen, ptlen, ctlen, backlen;
    int len;

    if (cipher == NULL) {
        printf("SKIP test_sm4: SM4-GCM is unavailable\n");
        return;
    }
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    from_hex(AEAD_NONCE_HEX, nonce, 16);
    aadlen = from_hex(GCM_AAD_HEX, aad, 20);
    ptlen = from_hex(AEAD_PT_HEX, pt, 16);
    from_hex("09f0ac99a5389f56a6882998c1e84a82", ct_want, 16);
    from_hex("d1b887050076313ee0124d639d0da40e", tag_want, 16);

    /* No-AAD one-shot KAT (derived on 3.6.5; see the header comment). */
    gcm_encrypt(cipher, key, nonce, 12, NULL, 0, pt, ptlen, ct, 16 + 16,
                &ctlen, tag, 16);
    expect_equal("SM4-GCM ciphertext does not match the derived KAT",
                 ct, ctlen, ct_want, 16);
    expect_equal("SM4-GCM tag does not match the derived KAT",
                 tag, 16, tag_want, 16);

    /* With AAD: roundtrip. */
    gcm_encrypt(cipher, key, nonce, 12, aad, aadlen, pt, ptlen, ct, 16 + 16,
                &ctlen, tag, 16);
    gcm_decrypt(cipher, key, nonce, 12, aad, aadlen, ct, ctlen, back,
                16 + 16, &backlen, tag, 16);
    expect_equal("SM4-GCM authenticated decrypt did not recover the plaintext",
                 back, backlen, pt, ptlen);

    /* Tampered tag: Final must fail. */
    memcpy(tag_bad, tag, 16);
    tag_bad[0] ^= 0x40;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
          "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "SM4-GCM decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (3) SM4-CCM (3.1+): derived KAT (ivlen 12, taglen 12), ivlen/taglen
 * before the key+nonce init, NULL-update length announcement, single
 * payload update, second data update fails, and a roundtrip. */
static void test_ccm(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t ptlen, ctlen = 0;
    int len, flen;

    if (cipher == NULL) {
        printf("SKIP test_sm4: SM4-CCM is unavailable\n");
        return;
    }
    from_hex("00112233445566778899aabbccddeeff", key, 16);
    from_hex(AEAD_NONCE_HEX, nonce, 16);
    ptlen = from_hex(AEAD_PT_HEX, pt, 33);
    from_hex("b1d4bb0b908c33b3385e37d7111fe1c4", ct_want, 16);
    from_hex("2afb3c3d087ff69a0a15c9cd", tag_want, 16);

    /* KAT: ivlen/taglen before the key+nonce init, announcement, single
     * payload update. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK((size_t)len == ptlen, "CCM update emitted a surprising length");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ptlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 12, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    ctlen = ptlen; /* the KAT ciphertext is the announced payload */
    expect_equal("SM4-CCM ciphertext does not match the derived KAT",
                 ct, ptlen, ct_want, 16);
    expect_equal("SM4-CCM tag does not match the derived KAT",
                 tag, 12, tag_want, 12);

    /* A second data update after the announced single payload must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "first payload update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt, 1) != 1,
          "SM4-CCM accepted a second data update");
    EVP_CIPHER_CTX_free(ctx);

    /* Authenticated roundtrip. */
    {
        unsigned char tag_copy[16];
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
              "EVP_CTRL_AEAD_SET_IVLEN failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
              "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
              "second EVP_DecryptInit_ex failed");
        memcpy(tag_copy, tag, 12);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, tag_copy) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
              "payload-length announcement failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "payload EVP_DecryptUpdate failed (tag mismatch?)");
        CHECK(EVP_DecryptFinal_ex(ctx, back + ctlen, &flen) == 1,
              "EVP_DecryptFinal_ex failed");
        EVP_CIPHER_CTX_free(ctx);
        expect_equal("SM4-CCM authenticated roundtrip did not recover the "
                     "plaintext", back, ctlen, pt, ptlen);
    }
    EVP_CIPHER_free(cipher);
}

/* (4) SM4-XTS (3.1+): derived KAT, 32-byte roundtrip, a data unit shorter
 * than 16 bytes fails in BOTH directions, and the 21-byte ciphertext-
 * stealing path roundtrips (XTS stealing works in both directions --
 * see test_aes_xts_small). */
static void test_xts(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *tweak = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t i, ctlen, backlen;
    int len;

    if (cipher == NULL) {
        printf("SKIP test_sm4: SM4-XTS is unavailable\n");
        return;
    }
    from_hex(XTS_KEY_HEX, key, 32);
    from_hex(XTS_IV_HEX, tweak, 16);
    from_hex(XTS_PT32_HEX, pt, 64);
    from_hex(XTS_CT32_HEX, ct_want, 64);

    ctlen = cipher_crypt(cipher, key, tweak, pt, 32, ct, 64 + 16, 1);
    expect_equal("SM4-XTS 32-byte ciphertext does not match the derived KAT",
                 ct, ctlen, ct_want, 32);
    backlen = cipher_crypt(cipher, key, tweak, ct_want, 32, back, 64 + 16, 0);
    expect_equal("SM4-XTS 32-byte roundtrip did not recover the plaintext",
                 back, backlen, pt, 32);

    /* A data unit shorter than 16 bytes must fail (xts128.c: "if (len < 16)
     * return -1"), both when encrypting and when decrypting. */
    for (i = 0; i < 2; i++) {
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        if (i == 0)
            CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, tweak, NULL) == 1,
                  "XTS encrypt init failed");
        else
            CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, tweak, NULL) == 1,
                  "XTS decrypt init failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "XTS set_padding(0) failed");
        if (i == 0)
            CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 15) != 1,
                  "XTS accepted a 15-byte data unit when encrypting");
        else
            CHECK(EVP_DecryptUpdate(ctx, back, &len, ct_want, 15) != 1,
                  "XTS accepted a 15-byte data unit when decrypting");
        EVP_CIPHER_CTX_free(ctx);
    }

    /* 21 bytes: ciphertext stealing, roundtrip in both directions (the
     * derived 21-byte KAT anchors the encrypt direction). */
    {
        static const char *const CT21_HEX =
            "a97f71f8cdf224dcec346ce9ad30f7d4167437d385";
        unsigned char *ct21_want = ALLOCATE_OUTPUT(21);

        from_hex(CT21_HEX, ct21_want, 21);
        ctlen = cipher_crypt(cipher, key, tweak, pt, 21, ct, 64 + 16, 1);
        expect_equal("SM4-XTS 21-byte ciphertext does not match the derived KAT",
                     ct, ctlen, ct21_want, 21);
        backlen = cipher_crypt(cipher, key, tweak, ct, ctlen, back, 64 + 16, 0);
        expect_equal("SM4-XTS 21-byte decrypt (stealing) roundtrip failed",
                     back, backlen, pt, 21);
    }
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_classical();
    test_gcm();
    test_ccm();
    test_xts();

    printf("test_sm4: all %u checks passed\n", g_checks);
    return 0;
}
