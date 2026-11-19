/*

 * test_sm4_common.c -- shared implementation of the SM4 family tests (API
 * in test_sm4_common.h; one binary per cipher, one struct sm4_kat row per
 * binary: SM4-ECB/CBC/CTR/OFB/CFB plus SM4-GCM/CCM/XTS).

 * Known-answer vectors:

 *   - SM4-ECB is anchored by the GB/T 32907-2016 standard ECB vector
 *     (key = plaintext = 0123456789abcdeffedcba9876543210 -> ciphertext
 *     681edf34d206965e86b3e94f536e4246), verified with

 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -sm4-ecb -nopad -K <keyhex>

 *   - The other classical-mode constants were verified the same way
 *     against the authoritative pizfix 3.6.5 CLI and cross-checked on the
 *     system 3.0.13 (identical values).  The stream-mode IV is
 *     fedcba98765432100123456789abcdef (CBC shares it), the CTR rows use
 *     the initial counter 000102...0f, the CBC/CTR KATs cover the 64-byte
 *     plaintext and the OFB/CFB KATs the 32-byte one; the GB/T 16-byte
 *     plaintext (the same bytes as the key) anchors ECB.  All of these
 *     family-wide constants live in the arrays below and every per-cipher
 *     binary picks its mode's row.

 *   - SM4-XTS/GCM/CCM postdate OpenSSL 3.0 (they fetch on 3.6.5, fail on
 *     3.0.13), and `openssl enc` cannot do XTS or AEAD anyway, so their
 *     constants were derived with a scratch probe compiled by
 *     build/bin/filcc against the pizfix 3.6.5 build.  Their binaries
 *     probe their own cipher right after test_parse_args()
 *     (sm4_probe_or_skip) and print the documented version-gated SKIP line
 *     when the fetch fails; on a build that has them, the sections run.

 * AEAD ordering rules mirror test_aes_gcm_small/test_aes_ccm_small: the
 * GCM IV length is set before the init that passes the IV; AAD updates
 * (out == NULL) come first; CCM needs ivlen AND taglen before the
 * key+nonce init, announces the payload length through a NULL update, and
 * takes the payload in ONE update (a second data update fails).  The GCM/
 * CCM/XTS implementations live in their own binaries (each has exactly
 * one); this module carries only what more than one binary needs.
 */

#include "test_sm4_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that
 * they are address constants usable in the static struct sm4_kat
 * initializers of the per-cipher binaries. */

/* The GB/T 32907-2016 128-bit key (every SM4 cipher uses it; SM4 has a
 * single key size). */
const char sm4_key_hex[] =
    "0123456789abcdeffedcba9876543210";

/* The GB/T 16-byte plaintext (the same bytes as the key); anchors the ECB
 * row. */
const char sm4_pt16_hex[] =
    "0123456789abcdeffedcba9876543210";

/* Stream-mode and CBC IV (128-bit). */
const char sm4_iv_hex[] =
    "fedcba98765432100123456789abcdef";

/* The CTR initial counter (the CTR row is the only one that does not use
 * sm4_iv_hex). */
const char sm4_ctr_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* The 32-byte stream-mode plaintext (OFB/CFB). */
const char sm4_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* The 64-byte CBC/CTR plaintext. */
const char sm4_pt64_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

/* AEAD shared material: 16-byte plaintext 000102...0f, 12-byte nonce and
 * the AEAD key (the GCM and CCM sections share all three; the GCM AAD and
 * the per-variant KAT ciphertexts/tags live in their own binaries). */
const char sm4_aead_key_hex[] =
    "00112233445566778899aabbccddeeff";
const char sm4_aead_pt_hex[] =
    "000102030405060708090a0b0c0d0e0f";
const char sm4_aead_nonce_hex[] =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0";

/* Fetches the cipher named by a KAT row, CHECK()ing success (the
 * classical SM4 modes are core OpenSSL 3 functionality: never SKIP, only
 * FAIL). */
static EVP_CIPHER *sm4_fetch_cipher(const struct sm4_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* Converts a row's IV constant into a labeled working buffer.  NULL rows
 * stay NULL: ECB takes no IV (the original test passed NULL there). */
static unsigned char *row_iv(const struct sm4_kat *kat)
{
    unsigned char *iv;

    if (kat->ivhex == NULL)
        return NULL;
    iv = ALLOCATE_IV(16);
    from_hex(kat->ivhex, iv, 16);
    return iv;
}

/* One-shot classical-mode encrypt/decrypt with padding disabled.  Returns
 * the total output length written to `out` (room for inlen + block_size
 * per the EVP contract). */
size_t sm4_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* CBC encrypt/decrypt with an explicit padding flag. */
size_t sm4_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (last one truncated); padding disabled.  A split must
 * reproduce the one-shot result exactly. */
size_t sm4_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* Whole-program version gate for the post-3.0 SM4 variants: SKIP with the
 * documented reason when the build lacks the cipher, release the probe
 * handle otherwise. */
void sm4_probe_or_skip(const char *ciphername)
{
    char msg[128];
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, ciphername, NULL);

    if (probe == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", ciphername);
        test_skip(msg);
    }
    EVP_CIPHER_free(probe);
}

/* (1) EVP_CIPHER_fetch(ciphername): the family key length, the one-shot
 * KAT for the row, streaming equality, and the streaming decryption
 * recovering the plaintext. */
void sm4_test_kat(const struct sm4_kat *kat)
{
    EVP_CIPHER *cipher = sm4_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;

    keylen = from_hex(sm4_key_hex, key, 16);
    snprintf(msg, sizeof(msg), "fetched %s cipher has an unexpected key length",
             kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen, msg);

    ptlen = from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = sm4_crypt(cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    ctlen = sm4_stream(cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg),
             "%s streaming ciphertext does not match the one-shot ciphertext",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    backlen = sm4_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                         64 + 32, 0);
    snprintf(msg, sizeof(msg),
             "%s streaming decryption did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Pad/nopad roundtrips: a nopad roundtrip of the row's block-aligned
 * plaintext and a padded roundtrip of a generated 37-byte pattern (37 ->
 * 48 with a full pad block). */
void sm4_test_roundtrips(const struct sm4_kat *kat)
{
    EVP_CIPHER *cipher = sm4_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *odd = ALLOCATE_INPUT(37);
    unsigned char *odd_ct = ALLOCATE_OUTPUT(40 + 16);
    size_t i, ctlen, backlen, oddlen, oddback;
    char msg[128];

    from_hex(sm4_key_hex, key, 16);
    from_hex(kat->pthex, pt, 64);

    /* nopad: exact multiples roundtrip */
    ctlen = sm4_cbc_crypt(cipher, key, iv, pt, kat->ptlen, ct, 64 + 32, 1, 0);
    snprintf(msg, sizeof(msg),
             "nopad %s encryption of %zu bytes != %zu bytes",
             kat->ciphername, kat->ptlen, kat->ptlen);
    CHECK(ctlen == kat->ptlen, msg);
    backlen = sm4_cbc_crypt(cipher, key, iv, ct, kat->ptlen, pt_back, 64 + 32,
                            0, 0);
    snprintf(msg, sizeof(msg), "nopad %s roundtrip failed", kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);

    /* pad: odd sizes roundtrip (37 bytes -> 40 with a full pad block) */
    for (i = 0; i < 37; i++)
        odd[i] = (unsigned char)(i * 7 + 3);
    oddlen = sm4_cbc_crypt(cipher, key, iv, odd, 37, odd_ct, 48 + 16, 1, 1);
    snprintf(msg, sizeof(msg),
             "padded %s encryption of 37 bytes did not give 48",
             kat->ciphername);
    CHECK(oddlen == 48, msg);
    oddback = sm4_cbc_crypt(cipher, key, iv, odd_ct, oddlen, pt_back, 48 + 16,
                            0, 1);
    snprintf(msg, sizeof(msg),
             "padded %s roundtrip did not recover 37 bytes", kat->ciphername);
    expect_equal(msg, pt_back, oddback, odd, 37);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
