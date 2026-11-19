/*
 * test_aes_cfb8_common.c -- shared implementation of the AES-CFB8 family
 * tests (API in test_aes_cfb8_common.h; one binary per cipher, one struct
 * cfb8_kat row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, section F.3 (the CFB8 example
 * vectors; IV 000102...0f, shared 4-block plaintext processed one byte at a
 * time).  Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb8 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb8 and -aes-256-cfb8; the per-binary
 * provenance notes say which row is which).
 *
 * CFB8 shifts the feedback register one byte at a time, so the defining
 * property tested here is byte-granular state continuity: feeding the
 * plaintext one byte per update must produce exactly the one-shot
 * ciphertext.  (Implementation note: crypto/modes/cfb128.c
 * CRYPTO_cfb128_8_encrypt() keeps the state entirely in the shifted IV and
 * leaves the *num position alone, so EVP_CIPHER_CTX_get_num() stays 0;
 * that is pinned down in the byte-at-a-time section.)
 */

#include "test_aes_cfb8_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.3 material: the same 4-block plaintext and IV for all
 * three key sizes.  The constants are arrays (not pointers) so that they are
 * address constants usable in static initializers of the per-cipher
 * binaries. */
const char cfb8_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
const char cfb8_kat_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-CFB8 is
 * core OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *cfb8_fetch_cipher(const struct cfb8_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot CFB8 encrypt/decrypt.  Returns the total output length written
 * to `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t cfb8_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (the last chunk is truncated to what remains).  Splits must reproduce the
 * one-shot result exactly. */
size_t cfb8_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot KATs in both directions: CFB8 decryption
 * uses the forward cipher, so decrypting the KAT ciphertext reproduces the
 * plaintext. */
void cfb8_test_fetched_kat(const struct cfb8_kat *kat)
{
    EVP_CIPHER *cipher = cfb8_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CFB8_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");
    CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
          "CFB8 block size is not 1");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "AES-CFB8 IV length is not 16");

    from_hex(cfb8_kat_iv_hex, iv, 16);
    from_hex(cfb8_kat_pt_hex, pt, CFB8_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CFB8_KAT_PTLEN);

    ctlen = cfb8_crypt(cipher, key, iv, pt, CFB8_KAT_PTLEN, ct,
                       CFB8_KAT_PTLEN + 16, 1);
    expect_equal("one-shot ciphertext does not match the CFB8 KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb8_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       CFB8_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the CFB8 KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, CFB8_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) The key CFB8 property: byte-at-a-time updates must be identical to one
 * big update, in both directions. */
void cfb8_test_byte_at_a_time(const struct cfb8_kat *kat)
{
    EVP_CIPHER *cipher = cfb8_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CFB8_KAT_PTLEN);
    unsigned char *one = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 16);
    unsigned char *many = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 16);
    EVP_CIPHER_CTX *ctx;
    int len, j;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb8_kat_iv_hex, iv, 16);
    from_hex(cfb8_kat_pt_hex, pt, CFB8_KAT_PTLEN);

    cfb8_crypt(cipher, key, iv, pt, CFB8_KAT_PTLEN, one, CFB8_KAT_PTLEN + 16,
               1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    for (j = 0; j < (int)CFB8_KAT_PTLEN; j++) {
        CHECK(EVP_EncryptUpdate(ctx, many + j, &len, pt + j, 1) == 1,
              "1-byte CFB8 update failed");
        CHECK(len == 1, "1-byte CFB8 update did not emit 1 byte");
    }
    CHECK(EVP_EncryptFinal_ex(ctx, many + CFB8_KAT_PTLEN, &len) == 1,
          "CFB8 Final failed");
    CHECK(len == 0, "CFB8 Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "CFB8 keeps no byte counter, so num must stay 0");
    EVP_CIPHER_CTX_free(ctx);

    unpoison_all();
    CHECK(memcmp(one, many, CFB8_KAT_PTLEN) == 0,
          "byte-at-a-time CFB8 encryption differs from one-shot encryption");
    repoison_all();
    g_checks++;

    /* Same property on the decrypt route. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    for (j = 0; j < (int)CFB8_KAT_PTLEN; j++)
        CHECK(EVP_DecryptUpdate(ctx, many + j, &len, one + j, 1) == 1,
              "1-byte CFB8 decrypt update failed");
    CHECK(EVP_DecryptFinal_ex(ctx, many + CFB8_KAT_PTLEN, &len) == 1,
          "CFB8 decrypt Final failed");
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(many, pt, CFB8_KAT_PTLEN) == 0,
          "byte-at-a-time CFB8 decryption differs from one-shot decryption");
    repoison_all();
    g_checks++;

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Mixed chunk sizes ({1,15,16,17,31}) must also reproduce the one-shot
 * result, in both directions. */
void cfb8_test_streaming(const struct cfb8_kat *kat)
{
    EVP_CIPHER *cipher = cfb8_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CFB8_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CFB8_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb8_kat_iv_hex, iv, 16);
    from_hex(cfb8_kat_pt_hex, pt, CFB8_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CFB8_KAT_PTLEN);

    ctlen = cfb8_stream(cipher, key, iv, pt, CFB8_KAT_PTLEN, ct,
                        CFB8_KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb8_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                        CFB8_KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, CFB8_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) A zero-byte update is a no-op that must not disturb the shifted-IV
 * state: 5 + 0 + 11 bytes == one 16-byte update, and a 37-byte message
 * round-trips through split updates. */
void cfb8_test_zero_update_and_roundtrip(const struct cfb8_kat *kat)
{
    EVP_CIPHER *cipher = cfb8_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *one = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *pt37 = ALLOCATE_INPUT(37);
    unsigned char *ct37 = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *back37 = ALLOCATE_OUTPUT(37 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb8_kat_iv_hex, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 3);
    for (i = 0; i < 37; i++)
        pt37[i] = (unsigned char)(i * 11 + 5);

    cfb8_crypt(cipher, key, iv, in, 16, one, 16 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte CFB8 update failed");
    CHECK(len == 5, "5-byte CFB8 update did not emit 5 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-byte CFB8 update failed");
    CHECK(len == 0, "zero-byte CFB8 update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte CFB8 update failed");
    CHECK(len == 11, "11-byte CFB8 update did not emit 11 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 16, &len) == 1, "CFB8 Final failed");
    CHECK(len == 0, "CFB8 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(one, many, 16) == 0,
          "5+0+11 updates diverged from a single 16-byte update");
    repoison_all();
    g_checks++;

    ctlen = cfb8_stream(cipher, key, iv, pt37, 37, ct37, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CFB8 encryption did not emit 37 bytes");
    ptlen = cfb8_stream(cipher, key, iv, ct37, ctlen, back37, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 back37, ptlen, pt37, 37);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
