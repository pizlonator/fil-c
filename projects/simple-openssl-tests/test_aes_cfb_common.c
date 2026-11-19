/*
 * test_aes_cfb_common.c -- shared implementation of the AES-CFB(128) family
 * tests (API in test_aes_cfb_common.h; one binary per cipher, one struct
 * cfb_kat row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, section F.3 (the CFB128 example
 * vectors for all three key sizes; both the .Encrypt and the .Decrypt
 * examples are covered, since CFB decryption uses the forward cipher --
 * each binary carries its row's .Encrypt constants, and decrypting them
 * reproduces the plaintext).  Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb -K <keyhex> -iv 000102...0f
 *   printf '<ct hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -d -aes-128-cfb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb and -aes-256-cfb; the per-binary provenance
 * notes say which row is which).  CFB128 keeps a 16-byte shift register in
 * the context; EVP_CIPHER_CTX_get_num() reports the byte offset inside that
 * register, which the num-continuity section pins down.
 */

#include "test_aes_cfb_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.3 material: the same 4-block plaintext and IV for all
 * three key sizes.  The constants are arrays (not pointers) so that they are
 * address constants usable in static initializers of the per-cipher
 * binaries. */
const char cfb_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
const char cfb_kat_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-CFB is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *cfb_fetch_cipher(const struct cfb_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot CFB128 encrypt/decrypt.  Returns the total output length written
 * to `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t cfb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
size_t cfb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot KATs in both directions: encrypting the
 * plaintext matches the F.3 CFB128 .Encrypt vectors, and decrypting those
 * ciphertexts through the decrypt route (which uses the forward cipher)
 * gives back the plaintext. */
void cfb_test_fetched_kat(const struct cfb_kat *kat)
{
    EVP_CIPHER *cipher = cfb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CFB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CFB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CFB_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CFB_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "AES-CFB IV length is not 16");

    from_hex(cfb_kat_iv_hex, iv, 16);
    from_hex(cfb_kat_pt_hex, pt, CFB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CFB_KAT_PTLEN);

    ctlen = cfb_crypt(cipher, key, iv, pt, CFB_KAT_PTLEN, ct,
                      CFB_KAT_PTLEN + 16, 1);
    expect_equal("one-shot ciphertext does not match the CFB128 KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                      CFB_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the CFB128 KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, CFB_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions. */
void cfb_test_streaming(const struct cfb_kat *kat)
{
    EVP_CIPHER *cipher = cfb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CFB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CFB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CFB_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CFB_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb_kat_iv_hex, iv, 16);
    from_hex(cfb_kat_pt_hex, pt, CFB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CFB_KAT_PTLEN);

    ctlen = cfb_stream(cipher, key, iv, pt, CFB_KAT_PTLEN, ct,
                       CFB_KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cfb_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       CFB_KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, CFB_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) num-state continuity: CFB128's shift register position is observable
 * through EVP_CIPHER_CTX_get_num() and must survive zero-length updates.
 * After 5 bytes the position is 5; after 11 more it wraps to 0; a further 3
 * bytes move it to 3 again. */
void cfb_test_num_continuity(const struct cfb_kat *kat)
{
    EVP_CIPHER *cipher = cfb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(19);
    unsigned char *one = ALLOCATE_OUTPUT(19 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(19 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb_kat_iv_hex, iv, 16);
    for (i = 0; i < 19; i++)
        in[i] = (unsigned char)(i * 3);

    cfb_crypt(cipher, key, iv, in, 19, one, 19 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte CFB update failed");
    CHECK(len == 5, "5-byte CFB update did not emit 5 bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "shift-register position after a 5-byte update is not 5");

    /* A zero-byte update is a no-op that leaves the state untouched. */
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-length CFB update failed");
    CHECK(len == 0, "zero-length CFB update emitted bytes");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 5,
          "zero-length update disturbed the shift-register position");

    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte CFB update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 0,
          "shift-register position did not wrap to 0 after 16 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 16, &len, in + 16, 3) == 1,
          "3-byte CFB update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 3,
          "shift-register position after a 3-byte update is not 3");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 19, &len) == 1,
          "CFB Final failed");
    CHECK(len == 0, "CFB Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    unpoison_all();
    CHECK(memcmp(one, many, 19) == 0,
          "5+0+11+3 updates diverged from a single 19-byte update");
    repoison_all();
    g_checks++;
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) The same continuity holds on the decrypt route, and a 37-byte message
 * round-trips through split updates in both directions. */
void cfb_test_decrypt_num_and_roundtrip(const struct cfb_kat *kat)
{
    EVP_CIPHER *cipher = cfb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cfb_kat_iv_hex, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 11 + 5);

    ctlen = cfb_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte CFB encryption did not emit 37 bytes");

    ptlen = cfb_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);

    /* Decrypt-route num: identical shift-register bookkeeping. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, pt_back, &len, ct, 7) == 1,
          "7-byte CFB decrypt update failed");
    CHECK(EVP_CIPHER_CTX_get_num(ctx) == 7,
          "decrypt shift-register position after a 7-byte update is not 7");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
