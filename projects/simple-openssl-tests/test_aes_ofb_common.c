/*
 * test_aes_ofb_common.c -- shared implementation of the AES-OFB (OFB-128)
 * family tests (API in test_aes_ofb_common.h; one binary per cipher, one
 * struct ofb_kat row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, section F.4 (OFB example vectors,
 * IV 000102...0f, shared 4-block plaintext).  Verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ofb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-ofb and -aes-256-ofb; the per-binary provenance
 * notes say which row is which).  OFB is symmetric: the decryption route
 * applies exactly the same keystream, so decrypting the KAT ciphertext must
 * reproduce the plaintext.
 */

#include "test_aes_ofb_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.4 material: the same 4-block plaintext and IV for all
 * three key sizes.  The constants are arrays (not pointers) so that they are
 * address constants usable in static initializers of the per-cipher
 * binaries. */
const char ofb_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
const char ofb_kat_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-OFB is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *ofb_fetch_cipher(const struct ofb_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot OFB encrypt/decrypt (`enc` selects the route; the keystream is
 * the same either way).  Returns the total output length written to `out`,
 * which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t ofb_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
size_t ofb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* (1) EVP_CIPHER_fetch one-shot encrypt == KAT and -- because OFB uses the
 * same keystream in both directions -- decrypting the KAT ciphertext through
 * the decrypt route gives back the plaintext. */
void ofb_test_fetched_kat(const struct ofb_kat *kat)
{
    EVP_CIPHER *cipher = ofb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(OFB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(OFB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(OFB_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(OFB_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "AES-OFB IV length is not 16");

    from_hex(ofb_kat_iv_hex, iv, 16);
    from_hex(ofb_kat_pt_hex, pt, OFB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, OFB_KAT_PTLEN);

    ctlen = ofb_crypt(cipher, key, iv, pt, OFB_KAT_PTLEN, ct,
                      OFB_KAT_PTLEN + 16, 1);
    expect_equal("one-shot ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ofb_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                      OFB_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, OFB_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Streaming: updates of {1,15,16,17,31} bytes must give exactly the
 * one-shot result in both directions; the keystream state must carry across
 * update boundaries. */
void ofb_test_streaming(const struct ofb_kat *kat)
{
    EVP_CIPHER *cipher = ofb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(OFB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(OFB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(OFB_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(OFB_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ofb_kat_iv_hex, iv, 16);
    from_hex(ofb_kat_pt_hex, pt, OFB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, OFB_KAT_PTLEN);

    ctlen = ofb_stream(cipher, key, iv, pt, OFB_KAT_PTLEN, ct,
                       OFB_KAT_PTLEN + 32, 1);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ofb_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       OFB_KAT_PTLEN + 32, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, OFB_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) The decryption route applies the same keystream as the encryption
 * route: decrypt(encrypt(x)) == x and, because C = P XOR KS implies
 * C XOR KS == P, encrypt(encrypt(x)) == x as well. */
void ofb_test_decrypt_is_encrypt(const struct ofb_kat *kat)
{
    EVP_CIPHER *cipher = ofb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(53);
    unsigned char *once = ALLOCATE_OUTPUT(53 + 16);
    unsigned char *twice = ALLOCATE_OUTPUT(53 + 16);
    size_t i, once_len, twice_len;

    from_hex(kat->keyhex, key, 32);
    from_hex(ofb_kat_iv_hex, iv, 16);
    for (i = 0; i < 53; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    once_len = ofb_crypt(cipher, key, iv, in, 53, once, 53 + 16, 1);
    CHECK(once_len == 53, "53-byte OFB encryption did not emit 53 bytes");

    twice_len = ofb_crypt(cipher, key, iv, once, once_len, twice, 53 + 16, 0);
    expect_equal("OFB decrypt route did not recover the plaintext",
                 twice, twice_len, in, 53);

    twice_len = ofb_crypt(cipher, key, iv, once, once_len, twice, 53 + 16, 1);
    expect_equal("encrypting the ciphertext again did not recover the plaintext",
                 twice, twice_len, in, 53);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) A zero-length update is a no-op that must not disturb the keystream
 * state: 5 bytes + 0 bytes + 11 bytes must equal a single 16-byte update. */
void ofb_test_zero_byte_update_noop(const struct ofb_kat *kat)
{
    EVP_CIPHER *cipher = ofb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *one = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *many = ALLOCATE_OUTPUT(16 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(ofb_kat_iv_hex, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 3);

    ofb_crypt(cipher, key, iv, in, 16, one, 16 + 16, 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, many, &len, in, 5) == 1,
          "5-byte OFB update failed");
    CHECK(len == 5, "5-byte OFB update did not emit 5 bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 0) == 1,
          "zero-length OFB update failed");
    CHECK(len == 0, "zero-length OFB update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, many + 5, &len, in + 5, 11) == 1,
          "11-byte OFB update failed");
    CHECK(len == 11, "11-byte OFB update did not emit 11 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, many + 16, &len) == 1,
          "OFB Final failed");
    CHECK(len == 0, "OFB Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    unpoison_all();
    CHECK(memcmp(one, many, 16) == 0,
          "5+0+11 updates diverged from a single 16-byte update");
    repoison_all();
    g_checks++;
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) A 37-byte message (partial final block) round-trips through the
 * streaming route in both directions. */
void ofb_test_partial_final_block(const struct ofb_kat *kat)
{
    EVP_CIPHER *cipher = ofb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(37);
    unsigned char *ct = ALLOCATE_OUTPUT(37 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(37 + 16);
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ofb_kat_iv_hex, iv, 16);
    for (i = 0; i < 37; i++)
        pt[i] = (unsigned char)(i * 11 + 5);

    ctlen = ofb_stream(cipher, key, iv, pt, 37, ct, 37 + 16, 1);
    CHECK(ctlen == 37, "37-byte OFB encryption did not emit 37 bytes");
    ptlen = ofb_stream(cipher, key, iv, ct, ctlen, pt_back, 37 + 16, 0);
    expect_equal("37-byte (partial final block) roundtrip failed",
                 pt_back, ptlen, pt, 37);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
