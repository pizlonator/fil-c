#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aria -- ARIA-128/192/256 in ECB/CBC/CTR/OFB/CFB/CFB1/CFB8 plus
 * ARIA-GCM and ARIA-CCM (all three key sizes for the AEADs).
 *
 * Known-answer vectors:
 *
 *   - The classical-mode constants were verified against the authoritative
 *     pizfix OpenSSL 3.6.5 CLI,
 *
 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -aria-128-ecb -nopad -K <keyhex>
 *
 *     and likewise for every classical (key size, mode) combination in
 *     this file (128/192/256 x ECB/CBC/CTR/OFB/CFB/CFB8/CFB1), with each
 *     ciphertext decrypted back to its plaintext by the same CLI; every
 *     derivation was cross-checked to be byte-identical under the system
 *     OpenSSL 3.0.13.  The ECB keys are 00112233... (16/24/32 bytes), the
 *     ECB plaintext is the RFC 3713-style block
 *     0123456789abcdeffedcba9876543210; RFC 5794 itself specifies GCM
 *     (not ECB) vectors, so the ECB constants are CLI-derived.
 *   - The GCM/CCM constants cannot come from `openssl enc` (no AEAD
 *     support); they were derived with a scratch probe compiled by
 *     build/bin/filcc against the pizfix 3.6.5 and cross-checked to be
 *     identical on the system 3.0.13 build (both runs printed the same
 *     ct/tag for every key size).
 *
 * AEAD ordering rules (mirroring test_aes_gcm_small/test_aes_ccm_small):
 * the GCM IV length is set with EVP_CTRL_AEAD_SET_IVLEN before the init
 * that passes the IV; AAD updates (out == NULL) come first; CCM needs
 * ivlen AND taglen before the key+nonce init, announces the payload
 * length through a NULL update, and takes the payload in ONE update.
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
    fprintf(stderr, "FAIL test_aria: %s\n", msg);
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

static const char *const K128_HEX = "00112233445566778899aabbccddeeff";
static const char *const K192_HEX =
    "00112233445566778899aabbccddeeff"
    "0011223344556677";
static const char *const K256_HEX =
    "00112233445566778899aabbccddeeff"
    "00112233445566778899aabbccddeeff";
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
static const char *const CBCIV_HEX =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0";
static const char *const STREAMIV_HEX =
    "fedcba98765432100123456789abcdef";
static const char *const CTRIV_HEX =
    "000102030405060708090a0b0c0d0e0f";

/* GCM/CCM shared material: 16-byte plaintext 000102...0f under CBCIV_HEX. */
static const char *const AEAD_PT_HEX =
    "000102030405060708090a0b0c0d0e0f";
/* AAD for the GCM behavior section. */
static const char *const GCM_AAD_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";

static const struct ecb_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
} ecb_kats[] = {
    { "ARIA-128-ECB", K128_HEX,
      "32c6837885b6cf3a7c8fd39c9d801747" },
    { "ARIA-192-ECB", K192_HEX,
      "757d767d1bac6012624ed0502ecea0a1" },
    { "ARIA-256-ECB", K256_HEX,
      "06700293e2b6ba0eaaf5f6da89c93ed3" },
};

static const struct gcm_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
    const char *taghex;
} gcm_kats[] = {
    { "ARIA-128-GCM", K128_HEX,
      "29981dedab64449e4266cfe65cb77657",
      "bc5d8f78978c319439447ebfc8d1e562" },
    { "ARIA-192-GCM", K192_HEX,
      "1057a6be3da3fdbdf2e6677c4f51eb1e",
      "14bf315b02f452ba0d90632512b34cf6" },
    { "ARIA-256-GCM", K256_HEX,
      "2cd6a57c20a088cd85a84a40a06cec77",
      "a19891323a2220323a06378a7469cacd" },
};

static const struct ccm_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
    const char *taghex;
} ccm_kats[] = {
    { "ARIA-128-CCM", K128_HEX,
      "f6cf49fbc5c1431fa1b569af438fa085",
      "15fcccc398b289c7e12a5cdd" },
    { "ARIA-192-CCM", K192_HEX,
      "af1c9ec046862b16657f2b480e6f5ab1",
      "96eeb2a9d8f90b7793fb0f91" },
    { "ARIA-256-CCM", K256_HEX,
      "72e66f705df3e51087274c7fc426abee",
      "478d61d4083321e9d834a541" },
};

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

/* GCM encrypt with the payload split into updates of {5, 5, 6} bytes (last
 * truncated); must reproduce the one-shot ciphertext and tag. */
static void gcm_stream_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *iv, size_t ivlen,
                               const unsigned char *aad, size_t aadlen,
                               const unsigned char *in, size_t inlen,
                               unsigned char *out, size_t outcap,
                               size_t *outlen, unsigned char *tag, size_t taglen)
{
    static const size_t chunks[] = { 5, 5, 6 };
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
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
    while (off < inlen) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in streaming test");
        dlen += len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed in streaming test");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* One-shot CCM encrypt (ivlen 12, taglen taglen): ivlen/taglen before the
 * key+nonce init, NULL-update length announcement, single payload update. */
static size_t ccm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *nonce, size_t ivlen,
                          unsigned char *tag, size_t taglen,
                          const unsigned char *pt, size_t ptlen,
                          unsigned char *ct, size_t ctcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(ptlen + 16 <= ctcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    /* The payload update is issued even for a zero-length payload so the
     * flow is identical across OpenSSL versions (see test_aes_ccm_small). */
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK((size_t)len == ptlen, "CCM update emitted a surprising length");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ptlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    return ptlen;
}

/* One-shot CCM decrypt with tag verification (tag length before init, tag
 * value before the payload update, which is where a mismatch is reported). */
static size_t ccm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *nonce, size_t ivlen,
                          const unsigned char *tag, size_t taglen,
                          const unsigned char *ct, size_t ctlen,
                          unsigned char *pt, size_t ptcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                              tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(ctlen + 16 <= ptcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, pt, &len, ct, (int)ctlen) == 1,
          "payload EVP_DecryptUpdate failed (tag mismatch?)");
    CHECK((size_t)len == ctlen, "CCM update emitted a surprising length");
    CHECK(EVP_DecryptFinal_ex(ctx, pt + ctlen, &flen) == 1,
          "EVP_DecryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return ctlen;
}

/* (1) The ECB KATs for all three key sizes. */
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

        /* ARIA is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core ARIA cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched ARIA cipher has an unexpected key length");
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

/* (2) CBC/CTR/OFB/CFB/CFB8/CFB1 KATs for all three key sizes, roundtrips
 * and streaming equality (CAMELLIA-style; for every key size the OFB and
 * CFB KATs agree on the first block by construction, both being plaintext
 * XOR E(IV) there). */
static void test_classical_modes(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
        const char *ivhex;
        const char *pthex;   /* 64 bytes for CBC/CTR, 32 for the rest */
        size_t ptlen;
        const char *cthex;
    } modes[] = {
        { "ARIA-128-CBC", K128_HEX, CBCIV_HEX, PT64_HEX, 64,
          "908945a7077a4311c4d6d1acb9854be3"
          "5441c1cd0fc7edc13aedde8a18e90112"
          "1f6d2100ca8e1d6c1e1459c3ce7f0a44"
          "b57894138aae8721ac3a715d00ab5ef0" },
        { "ARIA-192-CBC", K192_HEX, CBCIV_HEX, PT64_HEX, 64,
          "26745d0e789c1028cbf17d53d9a1e86d"
          "19a412e37679aef12bffc9a7d224fd6d"
          "be880f76254fa21f7a4ac7b5cd440851"
          "faabfcacb44af36d44bc6522a7f742b0" },
        { "ARIA-256-CBC", K256_HEX, CBCIV_HEX, PT64_HEX, 64,
          "c93343c6768ccd0b80bb58c6327867d9"
          "8e68f2d5a49e1017acc0798204159167"
          "7cfbe11c97eefc1545a1948ee944e3c5"
          "75982329f4e07c556fd767228ff2d6bc" },
        { "ARIA-128-CTR", K128_HEX, CTRIV_HEX, PT64_HEX, 64,
          "dd5f0b0c89d8d9c370788e617d4d98a5"
          "86bca3d53f9eaa4e34c3315ec7f7463f"
          "97dfd483532f509c8f2b6c91cf73a07"
          "494a2ea4248e5ab124685144ace67847" "8" },
        { "ARIA-192-CTR", K192_HEX, CTRIV_HEX, PT64_HEX, 64,
          "5f6b3d4ed12779c4125ddad14f0362a7"
          "e3918facc9d03b30335708aa5339e169"
          "a1c51e3ec26b8bdcc248bfa4b7059652"
          "33a2795f029701e8a5b828184b37f60d" },
        { "ARIA-256-CTR", K256_HEX, CTRIV_HEX, PT64_HEX, 64,
          "b4fffb7a4b9adf43903226896bd29cef"
          "17571573ef7e8b5eb6adf45603fee5d4"
          "d394ef586181a4ed5656c3e1c7d18607"
          "469b5f3ef382497857bc1f6d5ce974e3" },
        { "ARIA-128-OFB", K128_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "3902db49ff78dd5cb562eb8cec99a31a"
          "682671ca6e3ad352fddbb964fc8dbd34" },
        { "ARIA-192-OFB", K192_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "d8a587c28397e0f5bbb324d0cc95f4e0"
          "5518c11a0b02e7fc225d2f3c500540a5" },
        { "ARIA-256-OFB", K256_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "43cad67a3d5827f8bd63e90d01e6af7d"
          "1f22606aefecd5f3d900b1bf6a24a876" },
        { "ARIA-128-CFB", K128_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "3902db49ff78dd5cb562eb8cec99a31a"
          "7c681517d2d08d31a652cb215a28ad36" },
        { "ARIA-192-CFB", K192_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "d8a587c28397e0f5bbb324d0cc95f4e0"
          "b20c1da8c020fcd20883f1c454a8144c" },
        { "ARIA-256-CFB", K256_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "43cad67a3d5827f8bd63e90d01e6af7d"
          "6ef44ce53ec7e7c13506a353ab388824" },
        { "ARIA-128-CFB8", K128_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "397b71f1b509ae3d4a98c13dfae55430"
          "7975bb11a0ea27d0da966004d830091c" },
        { "ARIA-192-CFB8", K192_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "d8d0b19e9a4e173c9fcff6a529ae5888"
          "a0b9d52263c00794d9afa8179ed1827e" },
        { "ARIA-256-CFB8", K256_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "431a58bd3e54482e048a58100ae64efd"
          "0d6ecd2b1043c986ffac1b2d2fcb9199" },
        { "ARIA-128-CFB1", K128_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "335ac1a10f99af5ebc6ea434b844e70f"
          "6c52e6c63a62a465a7b20ecd5b83120c" },
        { "ARIA-192-CFB1", K192_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "80adf47aee8219c96d613ab72854c14a"
          "bfc09c78f20a423a686a8230167c12be" },
        { "ARIA-256-CFB1", K256_HEX, STREAMIV_HEX, PT32_HEX, 32,
          "5246f83f237ba6431bedfdb823072e70"
          "5881b51fc7f26cba388485660c33c02c" },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    size_t m, keylen, ct_wantlen, ctlen, backlen;
    char msg[128];

    for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, modes[m].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core ARIA classical mode");
        keylen = from_hex(modes[m].keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched ARIA classical cipher has an unexpected key length");
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

/* (3) GCM KATs for all three key sizes, plus the AEAD ordering rules:
 * SET_IVLEN before the key+IV init, AAD first with out == NULL, split
 * payload updates reproducing the one-shot result, and a tampered tag
 * failing DecryptFinal. */
static void test_gcm(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *ct_stream = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *tag_stream = ALLOCATE_TAG(16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(16 + 16);
    size_t k, aadlen, ptlen, ctlen, backlen, sctlen;
    char msg[128];

    from_hex(CBCIV_HEX, iv, 16);
    from_hex(AEAD_PT_HEX, pt, 16);
    aadlen = from_hex(GCM_AAD_HEX, aad, 20);
    ptlen = 16;

    for (k = 0; k < sizeof(gcm_kats) / sizeof(gcm_kats[0]); k++) {
        const struct gcm_kat *kat = &gcm_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t tag_wantlen;

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core ARIA-GCM cipher");
        from_hex(kat->keyhex, key, 32);
        tag_wantlen = from_hex(kat->taghex, tag_want, 16);

        /* No-AAD one-shot KAT. */
        gcm_encrypt(cipher, key, iv, 12, NULL, 0, pt, ptlen, ct, 16 + 16,
                    &ctlen, tag, 16);
        snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
                 kat->ciphername);
        from_hex(kat->cthex, ct_want, 16);
        expect_equal(msg, ct, ctlen, ct_want, 16);
        snprintf(msg, sizeof(msg), "%s tag does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, tag, 16, tag_want, tag_wantlen);

        /* With AAD: decrypt must recover the plaintext; a tampered tag
         * must fail Final; split payload updates must match one-shot. */
        gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen, ct,
                    16 + 16, &ctlen, tag, 16);
        gcm_decrypt(cipher, key, iv, 12, aad, aadlen, ct, ctlen, pt_back,
                    16 + 16, &backlen, tag, 16);
        snprintf(msg, sizeof(msg),
                 "%s authenticated decrypt did not recover the plaintext",
                 kat->ciphername);
        expect_equal(msg, pt_back, backlen, pt, ptlen);

        gcm_stream_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                           ct_stream, 16 + 16, &sctlen, tag_stream, 16);
        snprintf(msg, sizeof(msg),
                 "%s split-update ciphertext does not match the one-shot",
                 kat->ciphername);
        expect_equal(msg, ct_stream, sctlen, ct, ctlen);
        snprintf(msg, sizeof(msg), "%s split-update tag does not match one-shot",
                 kat->ciphername);
        expect_equal(msg, tag_stream, 16, tag, 16);
        EVP_CIPHER_free(cipher);
    }

    /* Tampered tag on ARIA-128-GCM must fail DecryptFinal. */
    {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ARIA-128-GCM", NULL);
        unsigned char tag_bad[16];
        EVP_CIPHER_CTX *ctx;
        int len;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ARIA-128-GCM\") failed");
        from_hex(K128_HEX, key, 16);
        gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen, ct, 16 + 16,
                    &ctlen, tag, 16);
        memcpy(tag_bad, tag, 16);
        tag_bad[0] ^= 0x40;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "EVP_DecryptUpdate failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
              "ARIA-GCM decryption accepted a tampered tag");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) CCM KATs for all three key sizes (ivlen 12, taglen 12), the ordering
 * rules (ivlen/taglen before the key+nonce init, NULL-update length
 * announcement, single payload update, second data update fails), and
 * roundtrips of 1-, 16- and 33-byte payloads. */
static void test_ccm(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *kat_pt = ALLOCATE_INPUT(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    size_t k, i, ctlen, backlen;
    char msg[128];

    from_hex(CBCIV_HEX, nonce, 16);
    from_hex(AEAD_PT_HEX, kat_pt, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 5 + 1);

    for (k = 0; k < sizeof(ccm_kats) / sizeof(ccm_kats[0]); k++) {
        const struct ccm_kat *kat = &ccm_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t tag_wantlen;

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core ARIA-CCM cipher");
        from_hex(kat->keyhex, key, 32);
        tag_wantlen = from_hex(kat->taghex, tag_want, 16);
        from_hex(kat->cthex, ct_want, 16);

        ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 12, kat_pt, 16,
                            ct, 16 + 16);
        snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, ct, ctlen, ct_want, 16);
        snprintf(msg, sizeof(msg), "%s tag does not match the KAT",
                 kat->ciphername);
        expect_equal(msg, tag, 12, tag_want, tag_wantlen);

        backlen = ccm_decrypt(cipher, key, nonce, 12, tag_want, 12,
                              ct, ctlen, back, 16 + 16);
        snprintf(msg, sizeof(msg),
                 "%s authenticated decrypt did not recover the KAT plaintext",
                 kat->ciphername);
        expect_equal(msg, back, backlen, kat_pt, 16);
        EVP_CIPHER_free(cipher);
    }

    /* Second data update after the announced single payload must fail. */
    {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ARIA-128-CCM", NULL);
        EVP_CIPHER_CTX *ctx;
        int len, flen;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ARIA-128-CCM\") failed");
        from_hex(K128_HEX, key, 16);
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
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, 16) == 1,
              "payload-length announcement failed");
        CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 16) == 1,
              "first payload update failed");
        CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 16, 8) != 1,
              "ARIA-CCM accepted a second data update");
        CHECK(EVP_EncryptFinal_ex(ctx, ct + 16, &flen) == 1,
              "CCM Final failed after the rejected second update");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }

    /* Roundtrips of 1, 16 and 33 bytes. */
    {
        static const size_t sizes[] = { 1, 16, 33 };
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ARIA-128-CCM", NULL);
        size_t s;

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ARIA-128-CCM\") failed");
        from_hex(K128_HEX, key, 16);
        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 12, pt, sizes[s],
                                ct, 64 + 16);
            CHECK(ctlen == sizes[s], "CCM encrypt length mismatch");
            backlen = ccm_decrypt(cipher, key, nonce, 12, tag, 12, ct, ctlen,
                                  back, 64 + 16);
            snprintf(msg, sizeof(msg),
                     "ARIA-128-CCM %zu-byte roundtrip failed", sizes[s]);
            expect_equal(msg, back, backlen, pt, sizes[s]);
        }
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_ecb_kats();
    test_classical_modes();
    test_gcm();
    test_ccm();

    printf("test_aria: all %u checks passed\n", g_checks);
    return 0;
}
