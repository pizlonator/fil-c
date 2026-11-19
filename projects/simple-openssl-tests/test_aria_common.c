/*

 * test_aria_common.c -- shared implementation of the ARIA family tests (API
 * in test_aria_common.h; one binary per cipher, one struct row per binary:
 * ARIA-{128,192,256} in ECB/CBC/CTR/OFB/CFB/CFB8/CFB1 plus ARIA-{128,192,
 * 256}-GCM and ARIA-{128,192,256}-CCM).

 * Known-answer vectors:

 *   - The classical-mode constants were verified against the authoritative
 *     pizfix OpenSSL 3.6.5 CLI,
 *
 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -aria-128-ecb -nopad -K <keyhex>
 *
 *     and likewise for every classical (key size, mode) combination of the
 *     family (128/192/256 x ECB/CBC/CTR/OFB/CFB/CFB8/CFB1, over the 64-byte
 *     CBC/CTR plaintext or the 32-byte stream-mode plaintext), with each
 *     ciphertext decrypted back to its plaintext by the same CLI; every
 *     derivation was cross-checked to be byte-identical under the system
 *     OpenSSL 3.0.13.  The keys are the family pattern 00112233... (16/24/32
 *     bytes) and the ECB plaintext is the RFC 3713-style block
 *     0123456789abcdeffedcba9876543210; RFC 5794 itself specifies GCM (not
 *     ECB) vectors, so the ECB constants are CLI-derived.  The CBC IV is
 *     0f1e2d3c4b5a69788796a5b4c3d2e1f0 and the stream-mode IV is
 *     fedcba98765432100123456789abcdef, so for every key size the OFB and
 *     CFB KATs agree on the first block by construction, both being
 *     plaintext XOR E(IV) there.  The CTR KATs cover 64 bytes under the
 *     initial counter 000102...0f.
 *   - The GCM/CCM constants cannot come from `openssl enc` (no AEAD
 *     support); they were derived with a scratch probe compiled by
 *     build/bin/filcc against the pizfix 3.6.5 and cross-checked to be
 *     identical on the system 3.0.13 build (both runs printed the same
 *     ct/tag for every key size).  Their shared shape: the 16-byte plaintext
 *     000102...0f under the first 12 bytes of the CBC IV, the 20-byte GCM
 *     AAD feedfacedeadbeeffeedfacedeadbeefabaddad2, 16-byte GCM tags, and
 *     12-byte CCM tags (ivlen 12, taglen 12).
 *
 * AEAD ordering rules (mirroring test_aes_gcm_small/test_aes_ccm_small):
 * the GCM IV length is set with EVP_CTRL_AEAD_SET_IVLEN before the init
 * that passes the IV; AAD updates (out == NULL) come first; CCM needs
 * ivlen AND taglen before the key+nonce init, announces the payload
 * length through a NULL update, and takes the payload in ONE update.
 *
 * Version note: ARIA is core OpenSSL 3 functionality, so none of the family
 * binaries ever skips -- a missing cipher is a FAIL, not a SKIP.
 */

#include "test_aria_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that they
 * are address constants usable in the static struct *_kat initializers of
 * the per-cipher binaries. */

/* The family keys: 00112233445566778899aabbccddeeff, extended to 192/256
 * bits with 0011223344556677 (and a second 00112233..eeff block). */
const char aria_key_128_hex[] =
    "00112233445566778899aabbccddeeff";
const char aria_key_192_hex[] =
    "00112233445566778899aabbccddeeff"
    "0011223344556677";
const char aria_key_256_hex[] =
    "00112233445566778899aabbccddeeff"
    "00112233445566778899aabbccddeeff";

/* The RFC 3713-style 16-byte block (the ECB plaintext). */
const char aria_pt16_hex[] =
    "0123456789abcdeffedcba9876543210";

/* The 32-byte stream-mode plaintext (OFB/CFB/CFB8/CFB1; CBC and CTR use the
 * 64-byte one below). */
const char aria_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* The 64-byte CBC/CTR plaintext (the SP 800-38A shared vector). */
const char aria_pt64_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

/* The CBC IV; its first 12 bytes double as the GCM/CCM nonce. */
const char aria_cbc_iv_hex[] =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0";

/* The OFB/CFB/CFB8/CFB1 IV. */
const char aria_stream_iv_hex[] =
    "fedcba98765432100123456789abcdef";

/* The CTR initial counter (the CTR rows are the only classical ones that do
 * not use aria_cbc_iv_hex). */
const char aria_ctr_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* The 16-byte GCM/CCM plaintext. */
const char aria_aead_pt_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* The 20-byte GCM AAD. */
const char aria_gcm_aad_hex[] =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";

/* Fetches a core ARIA cipher by name, CHECK()ing success (ARIA is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *aria_fetch_checked(const char *ciphername)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* Converts a row's IV constant into a labeled working buffer.  NULL rows
 * stay NULL: ECB takes no IV (the original test passed NULL there). */
static unsigned char *row_iv(const struct aria_kat *kat)
{
    unsigned char *iv;

    if (kat->ivhex == NULL)
        return NULL;
    iv = ALLOCATE_IV(16);
    from_hex(kat->ivhex, iv, 16);
    return iv;
}

/* One-shot encrypt/decrypt with padding disabled; used for the KATs and the
 * decrypt direction.  Returns the total output length written to `out`
 * (room for inlen + block_size per the EVP contract). */
size_t aria_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
 * (last one truncated); padding disabled.  A split must reproduce the
 * one-shot result exactly. */
size_t aria_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch(ciphername): the one-shot KAT for the row, plus the
 * decrypt direction (decrypting the KAT ciphertext gives back the
 * plaintext). */
void aria_test_kat(const struct aria_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;

    keylen = from_hex(kat->keyhex, key, 32);
    snprintf(msg, sizeof(msg),
             "fetched %s cipher has an unexpected key length",
             kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen, msg);

    ptlen = from_hex(kat->pthex, pt, 64);
    snprintf(msg, sizeof(msg),
             "%s plaintext constant has an unexpected length",
             kat->ciphername);
    CHECK(ptlen == kat->ptlen, msg);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = aria_crypt(cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg),
             "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    backlen = aria_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                         64 + 32, 0);
    snprintf(msg, sizeof(msg),
             "%s decrypting the KAT ciphertext did not give the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot (KAT) ciphertext, and the same split on decrypt
 * must give exactly the plaintext. */
void aria_test_streaming(const struct aria_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t ct_wantlen, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = aria_stream(cipher, key, iv, pt, kat->ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg),
             "%s streaming ciphertext does not match the one-shot ciphertext",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    backlen = aria_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                          64 + 32, 0);
    snprintf(msg, sizeof(msg),
             "%s streaming decryption did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* ------------------------------------------------------------------ */
/* GCM.                                                               */
/* ------------------------------------------------------------------ */

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

/* (3) GCM KAT: the no-AAD one-shot tuple (ciphertext and 16-byte tag) under
 * the shared 12-byte IV prefix and plaintext. */
void aria_test_gcm_kat(const struct aria_gcm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    char msg[128];
    size_t ct_wantlen, tag_wantlen, ctlen;

    from_hex(kat->keyhex, key, 32);
    /* The IV buffer holds the whole 16-byte family CBC IV; the GCM code
     * below uses only its first 12 bytes (SET_IVLEN 12). */
    from_hex(aria_cbc_iv_hex, iv, 16);
    from_hex(aria_aead_pt_hex, pt, ARIA_AEAD_PT_LEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, ARIA_AEAD_PT_LEN);
    tag_wantlen = from_hex(kat->taghex, tag_want, 16);

    /* No-AAD one-shot KAT. */
    gcm_encrypt(cipher, key, iv, 12, NULL, 0, pt, ARIA_AEAD_PT_LEN, ct,
                ARIA_AEAD_PT_LEN + 16, &ctlen, tag, 16);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
    snprintf(msg, sizeof(msg), "%s tag does not match the KAT",
             kat->ciphername);
    expect_equal(msg, tag, 16, tag_want, tag_wantlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) GCM AAD ordering roundtrip: encrypt with the shared 20-byte AAD fed
 * first (out == NULL), then authenticated-decrypt and recover the
 * plaintext. */
void aria_test_gcm_aad_roundtrip(const struct aria_gcm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    char msg[128];
    size_t aadlen, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(aria_cbc_iv_hex, iv, 16); /* the code uses the first 12 bytes */
    aadlen = from_hex(aria_gcm_aad_hex, aad, 20);
    from_hex(aria_aead_pt_hex, pt, ARIA_AEAD_PT_LEN);

    /* With AAD: decrypt must recover the plaintext. */
    gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ARIA_AEAD_PT_LEN, ct,
                ARIA_AEAD_PT_LEN + 16, &ctlen, tag, 16);
    gcm_decrypt(cipher, key, iv, 12, aad, aadlen, ct, ctlen, pt_back,
                ARIA_AEAD_PT_LEN + 16, &backlen, tag, 16);
    snprintf(msg, sizeof(msg),
             "%s authenticated decrypt did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, ARIA_AEAD_PT_LEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) GCM streaming: the same AAD tuple pushed through {5,5,6}-byte updates
 * must reproduce the one-shot ciphertext and tag. */
void aria_test_gcm_streaming(const struct aria_gcm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *ct_stream = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_stream = ALLOCATE_TAG(16);
    char msg[128];
    size_t aadlen, ctlen, sctlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(aria_cbc_iv_hex, iv, 16); /* the code uses the first 12 bytes */
    aadlen = from_hex(aria_gcm_aad_hex, aad, 20);
    from_hex(aria_aead_pt_hex, pt, ARIA_AEAD_PT_LEN);

    gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ARIA_AEAD_PT_LEN, ct,
                ARIA_AEAD_PT_LEN + 16, &ctlen, tag, 16);
    gcm_stream_encrypt(cipher, key, iv, 12, aad, aadlen, pt,
                       ARIA_AEAD_PT_LEN, ct_stream, ARIA_AEAD_PT_LEN + 16,
                       &sctlen, tag_stream, 16);
    snprintf(msg, sizeof(msg),
             "%s split-update ciphertext does not match the one-shot",
             kat->ciphername);
    expect_equal(msg, ct_stream, sctlen, ct, ctlen);
    snprintf(msg, sizeof(msg), "%s split-update tag does not match one-shot",
             kat->ciphername);
    expect_equal(msg, tag_stream, 16, tag, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) GCM tamper: flipping one tag bit must fail DecryptFinal (the decrypt
 * here runs on the default 12-byte IV route, as in the original). */
void aria_test_gcm_tamper(const struct aria_gcm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char tag_bad[16];
    char msg[128];
    EVP_CIPHER_CTX *ctx;
    size_t aadlen, ctlen;
    int len;

    from_hex(kat->keyhex, key, 32);
    from_hex(aria_cbc_iv_hex, iv, 16); /* the default IV length is 12 */
    aadlen = from_hex(aria_gcm_aad_hex, aad, 20);
    from_hex(aria_aead_pt_hex, pt, ARIA_AEAD_PT_LEN);

    gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ARIA_AEAD_PT_LEN, ct,
                ARIA_AEAD_PT_LEN + 16, &ctlen, tag, 16);
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
    snprintf(msg, sizeof(msg), "%s decryption accepted a tampered tag",
             kat->ciphername);
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* ------------------------------------------------------------------ */
/* CCM.                                                               */
/* ------------------------------------------------------------------ */

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

/* The deterministic 64-byte payload of the CCM behavior sections (no KAT
 * dependence): pt[i] = i * 5 + 1. */
static void fill_ccm_pt(unsigned char *pt, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        pt[i] = (unsigned char)(i * 5 + 1);
}

/* (7) CCM KAT (ivlen 12, taglen 12): encrypt == KAT (ciphertext and 12-byte
 * tag) and authenticated-decrypting the KAT tuple gives back the
 * plaintext. */
void aria_test_ccm_kat(const struct aria_ccm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *kat_pt = ALLOCATE_INPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(ARIA_AEAD_PT_LEN + 16);
    char msg[128];
    size_t tag_wantlen, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    /* ivlen 12: the nonce is the first 12 bytes of the family CBC IV (the
     * 16-byte buffer holds the whole constant). */
    from_hex(aria_cbc_iv_hex, nonce, 16);
    from_hex(aria_aead_pt_hex, kat_pt, ARIA_AEAD_PT_LEN);
    tag_wantlen = from_hex(kat->taghex, tag_want, 16);
    from_hex(kat->cthex, ct_want, ARIA_AEAD_PT_LEN);

    ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 12, kat_pt,
                        ARIA_AEAD_PT_LEN, ct, ARIA_AEAD_PT_LEN + 16);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ARIA_AEAD_PT_LEN);
    snprintf(msg, sizeof(msg), "%s tag does not match the KAT",
             kat->ciphername);
    expect_equal(msg, tag, 12, tag_want, tag_wantlen);

    backlen = ccm_decrypt(cipher, key, nonce, 12, tag_want, 12, ct, ctlen,
                          back, ARIA_AEAD_PT_LEN + 16);
    snprintf(msg, sizeof(msg),
             "%s authenticated decrypt did not recover the KAT plaintext",
             kat->ciphername);
    expect_equal(msg, back, backlen, kat_pt, ARIA_AEAD_PT_LEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (8) CCM ordering rule: a second data update after the announced single
 * payload must fail (encrypt side; Final must still succeed afterwards). */
void aria_test_ccm_second_data_update(const struct aria_ccm_kat *kat)
{
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    char msg[128];
    int len, flen;

    from_hex(kat->keyhex, key, 32);
    from_hex(aria_cbc_iv_hex, nonce, 16); /* the code uses the first 12 bytes */
    fill_ccm_pt(pt, 64);

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
    snprintf(msg, sizeof(msg), "%s accepted a second data update",
             kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 16, 8) != 1, msg);
    CHECK(EVP_EncryptFinal_ex(ctx, ct + 16, &flen) == 1,
          "CCM Final failed after the rejected second update");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (9) CCM roundtrips of 1-, 16- and 33-byte payloads (each encrypt/decrypt
 * through the single-update CCM helpers). */
void aria_test_ccm_roundtrips(const struct aria_ccm_kat *kat)
{
    static const size_t sizes[] = { 1, 16, 33 };
    EVP_CIPHER *cipher = aria_fetch_checked(kat->ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    char msg[128];
    size_t s, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(aria_cbc_iv_hex, nonce, 16); /* the code uses the first 12 bytes */
    fill_ccm_pt(pt, 64);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 12, pt, sizes[s],
                            ct, 64 + 16);
        CHECK(ctlen == sizes[s], "CCM encrypt length mismatch");
        backlen = ccm_decrypt(cipher, key, nonce, 12, tag, 12, ct, ctlen,
                              back, 64 + 16);
        snprintf(msg, sizeof(msg), "%s %zu-byte roundtrip failed",
                 kat->ciphername, sizes[s]);
        expect_equal(msg, back, backlen, pt, sizes[s]);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
