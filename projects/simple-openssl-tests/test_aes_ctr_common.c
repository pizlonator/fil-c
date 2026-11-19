/*
 * test_aes_ctr_common.c -- shared implementation of the AES-CTR family tests
 * (API in test_aes_ctr_common.h; one binary per cipher, one struct ctr_kat
 * row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, section F.5 (CTR example vectors,
 * counter/IV f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff, shared 4-block plaintext).
 * Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ctr -K <keyhex> -iv f0f1...feff
 *
 * (and likewise for -aes-192-ctr and -aes-256-ctr; the per-binary provenance
 * notes say which row is which).
 *
 * Counter-carry vectors: the SP 800-38A keys/plaintext with IV
 * fffffffffffffffffffffffffffffffd (the shared constant below) and 64 bytes
 * of plaintext, so that the fourth block is enciphered with counter
 * 0x0000...0000 after the 128-bit wrap; each binary carries its own
 * ciphertext for that IV, verified against the same CLI with -iv ff..fd.
 */

#include "test_aes_ctr_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.5 material: the same 4-block plaintext, the F.5 IV,
 * and the counter-wrap IV for all three key sizes.  The constants are arrays
 * (not pointers) so that they are address constants usable in static
 * initializers of the per-cipher binaries. */
const char ctr_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
const char ctr_kat_iv_hex[] =
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
const char ctr_carry_iv_hex[] =
    "fffffffffffffffffffffffffffffffd";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-CTR is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *ctr_fetch_cipher(const struct ctr_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot CTR encrypt/decrypt (CTR is symmetric; `enc` selects the route).
 * Returns the total output length written to `out`, which must have room for
 * inlen + block_size bytes per the EVP output-buffer contract. */
size_t ctr_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
size_t ctr_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and decrypt roundtrip. */
void ctr_test_fetched_kat(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CTR_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CTR_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "AES-CTR IV length is not 16");

    from_hex(ctr_kat_iv_hex, iv, 16);
    from_hex(ctr_kat_pt_hex, pt, CTR_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CTR_KAT_PTLEN);

    ctlen = ctr_crypt(cipher, key, iv, pt, CTR_KAT_PTLEN, ct,
                      CTR_KAT_PTLEN + 16, 1);
    expect_equal("one-shot ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ctr_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                      CTR_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, CTR_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions (CTR keeps the partial keystream block
 * in the context, so every split point must be transparent). */
void ctr_test_streaming(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CTR_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CTR_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ctr_kat_iv_hex, iv, 16);
    from_hex(ctr_kat_pt_hex, pt, CTR_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CTR_KAT_PTLEN);

    ctlen = ctr_stream(cipher, key, iv, pt, CTR_KAT_PTLEN, ct,
                       CTR_KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ctr_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       CTR_KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, CTR_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Counter carry across the 128-bit boundary: with IV ff..fd the fourth
 * 16-byte block is enciphered with counter 0x0000...0000.  One-shot and
 * split, checked against kat->carryhex. */
void ctr_test_counter_carry(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CTR_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CTR_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 16);
    unsigned char *ct_split = ALLOCATE_OUTPUT(CTR_KAT_PTLEN + 16);
    size_t ct_wantlen, ctlen, ct_splitlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ctr_carry_iv_hex, iv, 16);
    from_hex(ctr_kat_pt_hex, pt, CTR_KAT_PTLEN);
    ct_wantlen = from_hex(kat->carryhex, ct_want, CTR_KAT_PTLEN);

    ctlen = ctr_crypt(cipher, key, iv, pt, CTR_KAT_PTLEN, ct,
                      CTR_KAT_PTLEN + 16, 1);
    expect_equal("counter-carry ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ct_splitlen = ctr_stream(cipher, key, iv, pt, CTR_KAT_PTLEN, ct_split,
                             CTR_KAT_PTLEN + 16, 1);
    expect_equal("counter-carry split-update ciphertext differs from one-shot",
                 ct_split, ct_splitlen, ct, ctlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) Partial final block: CTR is a stream mode, so a 37-byte message (two
 * blocks plus five bytes) round-trips, in one shot and in place. */
void ctr_test_partial_final_block(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ctr_kat_iv_hex, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 3 + 9);

    ctlen = ctr_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CTR encryption did not emit 37 bytes");
    ptlen = ctr_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) In-place operation: CTR must accept out == in for both directions. */
void ctr_test_in_place(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *buf = ALLOCATE_OUTPUT(64 + 16); /* in place, contract sized */
    unsigned char *ref = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *in = ALLOCATE_INPUT(64);
    size_t i, ctlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ctr_kat_iv_hex, iv, 16);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 5 + 2);

    ctlen = ctr_crypt(cipher, key, iv, in, 64, ref, 64 + 16, 1);

    memcpy(buf, in, 64);
    ctr_crypt(cipher, key, iv, buf, 64, buf, 64 + 16, 1);   /* in-place encrypt */
    expect_equal("in-place CTR encryption differs from out-of-place",
                 buf, 64, ref, ctlen);

    ctr_crypt(cipher, key, iv, buf, 64, buf, 64 + 16, 0);   /* in-place decrypt */
    expect_equal("in-place CTR roundtrip did not recover the plaintext",
                 buf, 64, in, 64);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) Two fresh contexts with the same key and IV produce identical
 * keystreams; the partial-block counter position (EVP_CIPHER_CTX_get_num)
 * tracks how much of the current keystream block was consumed. */
void ctr_test_keystream_and_num(const struct ctr_kat *kat)
{
    EVP_CIPHER *cipher = ctr_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *zero = ALLOCATE_INPUT(48);
    unsigned char *ks1 = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ks2 = ALLOCATE_OUTPUT(48 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(ctr_kat_iv_hex, iv, 16);
    memset(zero, 0, 48);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks1, &len, zero, 48) == 1,
          "EVP_EncryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "second EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks2, &len, zero, 48) == 1,
          "EVP_EncryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);

    CHECK(len == 48, "48-byte keystream extraction did not emit 48 bytes");
    unpoison_all();
    CHECK(memcmp(ks1, ks2, 48) == 0,
          "two fresh contexts produced different keystreams");
    repoison_all();

    /* Partial-block state: after a 5-byte update the context reports 5 bytes
     * of the current keystream block consumed; 11 more complete the block. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ks1, &len, zero, 5) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "counter position after a 5-byte update is not 5");
    CHECK(EVP_EncryptUpdate(ctx, ks1 + 5, &len, zero + 5, 0) == 1,
          "zero-length CTR update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "zero-length update disturbed the counter position");
    CHECK(EVP_EncryptUpdate(ctx, ks1 + 5, &len, zero + 5, 11) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "counter position did not return to 0 after a full block");
    unpoison_all();
    for (i = 0; i < 16; i++)
        if (ks1[i] != ks2[i])
            test_failed("5+0+11 updates diverged from the one-shot keystream");
    repoison_all();
    g_checks++;
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
