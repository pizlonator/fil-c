/*

 * test_aes_gcm_siv_common.c -- shared implementation of the AES-GCM-SIV
 * family tests (API in test_aes_gcm_siv_common.h; one binary per cipher,
 * one RFC 8452 KAT row set per binary).

 * AES-*-GCM-SIV was added in OpenSSL 3.5, so on older builds (e.g. 3.0.13)
 * the fetch fails and every binary takes the documented whole-program SKIP
 * path (each binary probes its own cipher right after test_parse_args()).
 * On a build that has it, the behavior tested here is
 * (providers/implementations/ciphers/cipher_aes_gcm_siv.c / _hw.c):

 *   - the nonce is exactly 12 bytes and there is no IV-length knob;
 *   - AAD updates (out == NULL) may be split arbitrarily (the AAD is
 *     accumulated and zero-padded to a 16-byte boundary internally);
 *   - the payload is one-shot per init: used_enc/used_dec make a second
 *     data update fail;
 *   - the 16-byte tag is retrieved via GET_TAG after encrypting and set via
 *     SET_TAG before decrypting; the verification happens at DecryptFinal
 *     (the CTR step already ran during the update).

 * Known-answer vectors: RFC 8452 Appendix C.1 (AES-128) and C.2 (AES-256).
 * RFC 8452 defines the algorithm only for 128- and 256-bit keys and
 * publishes no AES-192 vectors, so the two "192 (derived)" rows were
 * derived with a scratch probe against the authoritative pizfix OpenSSL
 * 3.6.5 (via build/bin/filcc); there is nothing to cross-check on 3.0.13,
 * which lacks the algorithm entirely.  All rows were verified by running
 * them through this exact EVP flow.
 */

#include "test_aes_gcm_siv_common.h"

#include <openssl/evp.h>

/* The fixed 12-byte RFC 8452 nonce (an array so that the per-cipher
 * binaries can use it as an address constant). */
const char gsiv_nonce_hex[] =
    "030000000000000000000000";

/* GCM-SIV encrypt: AAD first (out == NULL), then ONE payload update, Final,
 * GET_TAG.  The nonce is exactly 12 bytes and is passed at init. */
static void gsiv_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce,
                         const unsigned char *aad, size_t aadlen,
                         const unsigned char *pt, size_t ptlen,
                         unsigned char *out, size_t outcap, size_t *outlen,
                         unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, scratch = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(ptlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    /* AAD updates report their length through *outl (like OCB, unlike
     * GCM); the ciphertext length is accumulated from data updates only. */
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &scratch, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    if (ptlen)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen) == 1,
              "payload EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    len += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* GCM-SIV decrypt with tag verification at Final. */
static void gsiv_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce,
                         const unsigned char *aad, size_t aadlen,
                         const unsigned char *ct, size_t ctlen,
                         unsigned char *out, size_t outcap, size_t *outlen,
                         const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, scratch = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(ctlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &scratch, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    if (ctlen)
        CHECK(EVP_DecryptUpdate(ctx, out, &len, ct, (int)ctlen) == 1,
              "payload EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* Fetches the row's cipher, CHECK()ing success. */
static EVP_CIPHER *gsiv_fetch_cipher(const struct gsiv_kat *kat)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

    CHECK(cipher != NULL,
          "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
    return cipher;
}

/* (1) The RFC 8452 Appendix C KATs (and derived rows) of the binary's
 * cipher. */
void gsiv_test_kats(const struct gsiv_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    size_t k;
    char msg[128];

    for (k = 0; k < nkats; k++) {
        const struct gsiv_kat *kat = &kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t aadlen, ptlen, ct_wantlen, ctlen, backlen;

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(gsiv_nonce_hex, nonce, 16);
        aadlen = from_hex(kat->aadhex, aad, 32);
        ptlen = from_hex(kat->pthex, pt, 64);
        ct_wantlen = from_hex(kat->cthex, ct_want, 64);
        from_hex(kat->taghex, tag_want, 16);

        gsiv_encrypt(cipher, key, nonce, aad, aadlen, pt, ptlen,
                     ct, 64 + 16, &ctlen, tag);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        snprintf(msg, sizeof(msg), "%s: tag does not match the KAT", kat->label);
        expect_equal(msg, tag, 16, tag_want, 16);

        gsiv_decrypt(cipher, key, nonce, aad, aadlen, ct_want, ct_wantlen,
                     back, 64 + 16, &backlen, tag_want);
        snprintf(msg, sizeof(msg),
                 "%s: decrypting the KAT did not give the plaintext", kat->label);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }

    free_poison_allocations();
}

/* (2) AAD in two chunks must equal one-shot AAD. */
void gsiv_test_aad_chunking(const struct gsiv_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(40);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct1 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher = gsiv_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i, ct1len, ct2len;

    for (i = 0; i < 40; i++)
        aad[i] = (unsigned char)(i * 3 + 1);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 7 + 5);

    from_hex(kat->keyhex, key, 32);
    from_hex(gsiv_nonce_hex, nonce, 16);

    gsiv_encrypt(cipher, key, nonce, aad, 40, pt, 64, ct1, 64 + 16,
                 &ct1len, tag1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 13) == 1,
          "AAD chunk 1 failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad + 13, 27) == 1,
          "AAD chunk 2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ct2, &len, pt, 64) == 1,
          "payload update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, ct2 + len, &flen) == 1,
          "Final failed after chunked AAD");
    ct2len = (size_t)len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag2) == 1,
          "GET_TAG failed after chunked AAD");
    EVP_CIPHER_CTX_free(ctx);
    expect_equal("two-chunk AAD ciphertext differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("two-chunk AAD tag differs from one-shot tag",
                 tag2, 16, tag1, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Payload roundtrips for sizes {0,16,17,64}. */
void gsiv_test_roundtrip_sizes(const struct gsiv_kat *kat)
{
    static const size_t sizes[] = { 0, 16, 17, 64 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(9);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher = gsiv_fetch_cipher(kat);
    size_t s, i, ctlen, backlen;
    char msg[128];

    from_hex(kat->keyhex, key, 32);
    from_hex(gsiv_nonce_hex, nonce, 16);
    for (i = 0; i < 9; i++)
        aad[i] = (unsigned char)(i + 2);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 11 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        gsiv_encrypt(cipher, key, nonce, aad, 9, pt, n, ct, 64 + 16,
                     &ctlen, tag);
        CHECK(ctlen == n, "GCM-SIV ciphertext length is not the payload length");
        gsiv_decrypt(cipher, key, nonce, aad, 9, ct, ctlen, back, 64 + 16,
                     &backlen, tag);
        snprintf(msg, sizeof(msg),
                 "GCM-SIV roundtrip of %zu bytes did not recover the plaintext",
                 n);
        expect_equal(msg, back, backlen, pt, n);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) One payload update per init: a second data update must fail
 * (used_enc / used_dec in cipher_aes_gcm_siv_hw.c). */
void gsiv_test_second_update_fails(const struct gsiv_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = gsiv_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(gsiv_nonce_hex, nonce, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 32) == 1,
          "first payload update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 32, 32) != 1,
          "a second payload update was accepted on the same context");
    EVP_CIPHER_CTX_free(ctx);

    /* Same rule on the decrypt side. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, pt, 16) == 1,
          "decrypt-side first update failed");
    CHECK(EVP_DecryptUpdate(ctx, ct + len, &len, pt + 16, 16) != 1,
          "a second payload update was accepted on decrypt");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) Tamper detection: a flipped ciphertext byte or tag bit must fail
 * DecryptFinal. */
void gsiv_test_tamper(const struct gsiv_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    EVP_CIPHER *cipher = gsiv_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i, ctlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(gsiv_nonce_hex, nonce, 16);
    for (i = 0; i < 12; i++)
        aad[i] = (unsigned char)(i + 1);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    gsiv_encrypt(cipher, key, nonce, aad, 12, pt, 64, ct, 64 + 16,
                 &ctlen, tag);
    CHECK(ctlen == 64, "unexpected ciphertext length");

    /* Flip a ciphertext byte (the CTR keystream covers it). */
    ct[0] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(tag_bad, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 12) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered ciphertext byte");
    EVP_CIPHER_CTX_free(ctx);
    ct[0] ^= 0x01;

    /* Flip a tag bit. */
    memcpy(tag_bad, tag, 16);
    tag_bad[15] ^= 0x80;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 12) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
