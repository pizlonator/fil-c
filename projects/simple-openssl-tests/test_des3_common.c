/*

 * test_des3_common.c -- shared implementation of the three-key DES
 * (DES-EDE3) family tests (API in test_des3_common.h; one binary per
 * cipher, one struct des3_kat row per binary: DES-EDE3 in ECB/CBC/OFB/
 * CFB/CFB8/CFB1).

 * Providers: unlike single DES and DESX (legacy provider, see
 * test_des_common.c), the DES-EDE3 ciphers are supplied by the DEFAULT
 * provider, so no provider loading is needed here (with the legacy
 * provider also loaded they still fetch from "default" -- see the probe
 * table in test_des.c's history).

 * Known-answer vectors: verified against the authoritative pizfix
 * OpenSSL 3.6.5 CLI,

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3 \
 *       -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The key is 0123456789abcdef23456789abcdef013456789abcdef012 (24 bytes,
 * a family-wide constant below); the 8-byte IV and the 8/16/32-byte
 * plaintexts are family-wide too, while every row's KAT ciphertext lives
 * in that cipher's binary.

 * The 8-byte block semantics mirror test_des: nopad needs exact multiples
 * of 8, padded operations roundtrip odd sizes, CBC decryption holds the
 * last block until Final, and the OFB/CFB/CFB8 shift registers carry
 * state across updates.  DES-EDE3 has a FIXED 24-byte key length (the
 * provider TDES flags carry no VARIABLE_LENGTH bit), which the wrong-key-
 * length section (des3_test_wrong_key_length, written against DES-EDE3)
 * exercises through the same careful route as test_aes_ecb: a bare init,
 * then EVP_CIPHER_CTX_set_key_length() (which must refuse to shrink the
 * key), and only then the key init.
 */

#include "test_des3_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that
 * they are address constants usable in the static struct des3_kat
 * initializers of the per-cipher binaries. */

/* The fixed 24-byte DES-EDE3 key. */
const char des3_key_hex[] =
    "0123456789abcdef23456789abcdef013456789abcdef012";

/* The family-wide 8-byte IV. */
const char des3_iv_hex[] =
    "0102030405060708";

/* The family plaintexts: 8 bytes (the ECB row), 16 bytes (the CBC row),
 * and 32 bytes (the stream-mode rows). */
const char des3_pt8_hex[] =
    "4142434445464748";
const char des3_pt16_hex[] =
    "0123456789abcdef0123456789abcdef";
const char des3_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* Fetches the cipher named by a KAT row, CHECK()ing success (DES-EDE3 is
 * core OpenSSL 3 functionality, default provider: never SKIP, only
 * FAIL). */
static EVP_CIPHER *des3_fetch_cipher(const struct des3_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* Converts a row's IV constant into a labeled working buffer.  "" rows
 * stay NULL: ECB takes no IV (the original test passed NULL there). */
static const unsigned char *row_iv(const struct des3_kat *kat,
                                   unsigned char *ivbuf)
{
    if (kat->ivhex[0] == '\0')
        return NULL;
    from_hex(kat->ivhex, ivbuf, 8);
    return ivbuf;
}

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract). */
size_t des3_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* Encrypt/decrypt with an explicit padding flag (mode-agnostic: the ECB
 * row passes a NULL IV). */
size_t des3_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
size_t des3_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch(ciphername): the fixed 24-byte key length, the
 * one-shot KAT, and decrypting the KAT ciphertext back to its plaintext. */
void des3_test_kat(const struct des3_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(32 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    size_t ct_wantlen, ctlen, ptlen;
    char msg[128];

    cipher = des3_fetch_cipher(kat);
    snprintf(msg, sizeof(msg), "fetched %s cipher has an unexpected key length",
             kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 24, msg);
    from_hex(des3_key_hex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);
    ct_wantlen = from_hex(kat->cthex, ct_want, 32);

    ctlen = des3_crypt(cipher, key, ivp, pt, kat->ptlen, ct, 32 + 16, 1);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    ptlen = des3_crypt(cipher, key, ivp, ct_want, ct_wantlen, pt_back,
                       32 + 16, 0);
    snprintf(msg, sizeof(msg),
             "%s decrypting the KAT ciphertext did not give the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, ptlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Block streaming: the nopad one-shot KAT, its streaming equality
 * under {1,7,8,9,16,17} updates in both directions, and a padded
 * roundtrip of a generated 33-byte pattern (33 -> 40 with a full pad
 * block). */
void des3_test_block_streaming(const struct des3_kat *kat)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9, 16, 17 };
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(33 + 24);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    size_t i, ct_wantlen, ctlen, backlen;
    char msg[128];

    cipher = des3_fetch_cipher(kat);
    from_hex(des3_key_hex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);
    ct_wantlen = from_hex(kat->cthex, ct_want, 32);

    /* nopad KAT and its streaming equality. */
    ctlen = des3_cbc_crypt(cipher, key, ivp, pt, kat->ptlen, ct, 33 + 24, 1, 0);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
    ctlen = des3_stream(cipher, key, ivp, pt, kat->ptlen, ct, 33 + 24, 1,
                        stream_chunks, 6);
    snprintf(msg, sizeof(msg),
             "streaming %s ciphertext does not match one-shot",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
    backlen = des3_stream(cipher, key, ivp, ct_want, ct_wantlen, pt_back,
                          33 + 24, 0, stream_chunks, 6);
    snprintf(msg, sizeof(msg),
             "streaming %s decrypt did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);

    /* pad: odd sizes roundtrip (33 bytes -> 40 with a full pad block). */
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);
    ctlen = des3_cbc_crypt(cipher, key, ivp, pt, 33, ct, 33 + 24, 1, 1);
    snprintf(msg, sizeof(msg), "padded %s of 33 bytes did not give 40",
             kat->ciphername);
    CHECK(ctlen == 40, msg);
    backlen = des3_cbc_crypt(cipher, key, ivp, ct, ctlen, pt_back, 33 + 24, 0, 1);
    snprintf(msg, sizeof(msg), "padded %s roundtrip did not recover 33 bytes",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, 33);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Stream details: a 17-then-15 split equals the one-shot result in
 * both directions (the shift register carries state across updates) and a
 * 0-byte update is a no-op. */
void des3_test_stream_details(const struct des3_kat *kat)
{
    static const size_t odd_chunks[] = { 17, 15 };
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *chunked = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t olen, clen, backlen;
    int len, flen;
    char msg[128];

    cipher = des3_fetch_cipher(kat);
    from_hex(des3_key_hex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);

    olen = des3_crypt(cipher, key, ivp, pt, 32, oneshot, 32 + 16, 1);
    clen = des3_stream(cipher, key, ivp, pt, 32, chunked, 32 + 16, 1,
                       odd_chunks, 2);
    snprintf(msg, sizeof(msg), "17/15-split %s ciphertext differs from one-shot",
             kat->ciphername);
    expect_equal(msg, chunked, clen, oneshot, olen);
    backlen = des3_stream(cipher, key, ivp, oneshot, olen, back, 32 + 16, 0,
                          odd_chunks, 2);
    snprintf(msg, sizeof(msg), "17/15-split %s decrypt did not recover plaintext",
             kat->ciphername);
    expect_equal(msg, back, backlen, pt, 32);

    /* 0-byte no-op. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    snprintf(msg, sizeof(msg), "0-byte %s update failed", kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, chunked, &len, pt, 0) == 1, msg);
    snprintf(msg, sizeof(msg), "0-byte %s update emitted bytes",
             kat->ciphername);
    CHECK(len == 0, msg);
    snprintf(msg, sizeof(msg), "0-byte %s final failed", kat->ciphername);
    CHECK(EVP_EncryptFinal_ex(ctx, chunked, &flen) == 1, msg);
    snprintf(msg, sizeof(msg), "0-byte %s final emitted bytes",
             kat->ciphername);
    CHECK(flen == 0, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) A 16-byte key must not be usable with DES-EDE3: the only public way
 * to present it is EVP_CIPHER_CTX_set_key_length() + init (the same
 * careful route as test_aes_ecb -- the init reads exactly
 * EVP_CIPHER_CTX_get_key_length() bytes, so it is only attempted when
 * set_key_length() actually resized the context), and OpenSSL must reject
 * the operation at one of those two points (the provider TDES flags carry
 * no VARIABLE_LENGTH bit, so set_key_length refuses). */
void des3_test_wrong_key_length(const struct des3_kat *kat)
{
    EVP_CIPHER *cipher = des3_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *key = ALLOCATE_KEY(24);
    int rejected, init_ok = 1, i;
    char msg[128];

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    for (i = 0; i < 24; i++)
        key[i] = (unsigned char)(i + 1);

    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, 16) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1);
    snprintf(msg, sizeof(msg), "a 16-byte key was accepted for %s",
             kat->ciphername);
    CHECK(rejected || !init_ok, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
