/*

 * test_aes_siv_common.c -- shared implementation of the AES-SIV family
 * tests (API in test_aes_siv_common.h; one binary per cipher, one RFC 5297
 * A.1/A.2 row pair per binary).

 * AES-128/192/256-SIV (RFC 5297 S2V-CMAC-AES + CTR): the SIV key is TWICE
 * the underlying cipher key (32/48/64 bytes for AES-128/192/256-SIV),
 * there is no IV, AAD updates (any number, including the RFC's nonce as
 * the last one) precede the single payload update, and a context performs
 * exactly ONE crypto operation per init (crypto/modes/siv128.c counts
 * crypto_ok down to zero, so a second payload update on the same context
 * fails).  The 16-byte synthetic IV is carried as the EVP tag: GET_TAG
 * after encrypting, SET_TAG before decrypting, and DecryptFinal reports
 * the verification result.
 *
 * S2V has distinct branches for messages shorter than 16 bytes (pad with
 * 0x80 and double) and at least 16 bytes (xorend), so the roundtrip sizes
 * deliberately straddle that boundary: {0,1,15,16,17,1024} (the 0 size is
 * the separate empty-payload section below).
 *
 * Version note for the empty payload: on 3.6.x a zero-length payload
 * update computes the synthetic IV and Final succeeds; on 3.0.x the same
 * flow leaves the tag unset and Final fails.  The empty case is handled
 * separately so the binaries assert whichever behavior the running
 * OpenSSL exhibits instead of hard-coding one of them.
 *
 * Known-answer vectors: RFC 5297 A.1 (deterministic example, one AD) and
 * A.2 (nonce-based example, two ADs and the nonce as the last AAD update).
 * The RFC publishes samples only for 128-bit keys, so the AES-128-SIV rows
 * carry the RFC constants (both verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13; the A.2 vector exercises the multiple-AAD-updates path).
 * The AES-192-SIV and AES-256-SIV rows use the same A.1/A.2 inputs with
 * the 000102.. key pattern continued to twice the cipher key size (48/64
 * bytes); their expected SIV/ciphertext constants were derived with the
 * same scratch EVP probe against pizfix 3.6.5 and cross-checked byte for
 * byte against the system OpenSSL 3.0.13.
 */

#include "test_aes_siv_common.h"

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/* The RFC 5297 A.1/A.2 input material (cipher-independent; see the header
 * comment).  The constants are arrays (not pointers). */
const char siv_a1_ad_hex[] =
    "101112131415161718191a1b1c1d1e1f2021222324252627";
const char siv_a1_pt_hex[] = "112233445566778899aabbccddee";
const char siv_a2_ad1_hex[] =
    "00112233445566778899aabbccddeeff"
    "deaddadadeaddadaffeeddcc"
    "bbaa99887766554433221100";
const char siv_a2_ad2_hex[] = "102030405060708090a0";
const char siv_a2_nonce_hex[] = "09f911029d74e35bd84156c5635688c0";
const char siv_a2_pt_hex[] =
    "7468697320697320736f6d6520706c61"
    "696e7465787420746f20656e63727970"
    "74207573696e67205349562d414553";

/* SIV encrypt: key (2x), AAD updates, ONE payload update, Final, GET_TAG.
 * The payload update runs even for a zero-length payload: S2V must see the
 * (empty) message for the tag to be computed, and Final alone does not
 * perform the crypto operation. */
void siv_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char * const *ads, const size_t *adlens,
                 size_t nads, const unsigned char *pt, size_t ptlen,
                 unsigned char *out, size_t outcap, size_t *outlen,
                 unsigned char *siv)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    size_t i;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(ptlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    for (i = 0; i < nads; i++)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, ads[i], (int)adlens[i]) == 1,
              "AAD EVP_EncryptUpdate failed");
    /* The payload update runs even for a zero-length payload: S2V must see
     * the (empty) message for the tag to be computed, and Final alone does
     * not perform the crypto operation. */
    CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    len += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, siv) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* SIV decrypt with the synthetic IV set before the payload update. */
void siv_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *siv,
                 const unsigned char * const *ads, const size_t *adlens,
                 size_t nads, const unsigned char *ct, size_t ctlen,
                 unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char siv_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, flen = 0;
    size_t i;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(siv_copy, siv, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, siv_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(ctlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    for (i = 0; i < nads; i++)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, ads[i], (int)adlens[i]) == 1,
              "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, ct, (int)ctlen) == 1,
          "payload EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_DecryptFinal_ex failed (SIV mismatch?)");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* Fetches a row's SIV cipher, CHECK()ing success and the doubled key
 * length (keylen = strlen(keyhex) / 2 is the expected 2x cipher key
 * size). */
static EVP_CIPHER *siv_fetch_kat_cipher(const struct siv_kat *kat,
                                        size_t *keylen)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    char msg[128];

    /* AES-SIV is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-SIV cipher");
    *keylen = strlen(kat->keyhex) / 2;
    snprintf(msg, sizeof(msg), "%s key length is not %zu (2x%zu)",
             kat->ciphername, *keylen, *keylen / 2);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == *keylen, msg);
    return cipher;
}

/* (1) The row's RFC 5297 A.1 tuple: single AD. */
void siv_test_kat_a1(const struct siv_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad1 = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *siv_want = ALLOCATE_TAG(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64);
    unsigned char *siv = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64);
    EVP_CIPHER *cipher;
    const unsigned char *ads[1];
    size_t adlens[1];
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    cipher = siv_fetch_kat_cipher(kat, &keylen);
    from_hex(kat->keyhex, key, 64);
    adlens[0] = from_hex(siv_a1_ad_hex, ad1, 64);
    ptlen = from_hex(siv_a1_pt_hex, pt, 64);
    from_hex(kat->sivhex, siv_want, 16);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);
    ads[0] = ad1;

    siv_encrypt(cipher, key, ads, adlens, 1, pt, ptlen, ct, 64, &ctlen, siv);
    snprintf(msg, sizeof(msg), "%s: synthetic IV does not match the KAT",
             kat->label);
    expect_equal(msg, siv, 16, siv_want, 16);
    snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
             kat->label);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    siv_decrypt(cipher, key, siv_want, ads, adlens, 1, ct_want, ct_wantlen,
                back, 64, &backlen);
    snprintf(msg, sizeof(msg),
             "%s: decrypting the KAT did not give the plaintext", kat->label);
    expect_equal(msg, back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (1) The row's RFC 5297 A.2 tuple: two ADs and the nonce (three AAD
 * updates before the payload). */
void siv_test_kat_a2(const struct siv_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad1 = ALLOCATE_AAD(64);
    unsigned char *ad2 = ALLOCATE_AAD(16);
    unsigned char *nonce = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *siv_want = ALLOCATE_TAG(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64);
    unsigned char *siv = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64);
    EVP_CIPHER *cipher;
    const unsigned char *ads[3];
    size_t adlens[3];
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    cipher = siv_fetch_kat_cipher(kat, &keylen);
    from_hex(kat->keyhex, key, 64);
    adlens[0] = from_hex(siv_a2_ad1_hex, ad1, 64);
    adlens[1] = from_hex(siv_a2_ad2_hex, ad2, 16);
    adlens[2] = from_hex(siv_a2_nonce_hex, nonce, 16);
    ptlen = from_hex(siv_a2_pt_hex, pt, 64);
    from_hex(kat->sivhex, siv_want, 16);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);
    ads[0] = ad1;
    ads[1] = ad2;
    ads[2] = nonce;

    siv_encrypt(cipher, key, ads, adlens, 3, pt, ptlen, ct, 64, &ctlen, siv);
    snprintf(msg, sizeof(msg), "%s: synthetic IV does not match the KAT",
             kat->label);
    expect_equal(msg, siv, 16, siv_want, 16);
    snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
             kat->label);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    siv_decrypt(cipher, key, siv_want, ads, adlens, 3, ct_want, ct_wantlen,
                back, 64, &backlen);
    snprintf(msg, sizeof(msg),
             "%s: decrypting the KAT did not give the plaintext", kat->label);
    expect_equal(msg, back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Roundtrips for sizes {1,15,16,17,1024} -- the sizes straddle the S2V
 * <16 / >=16 branches (the empty size is the separate section below). */
void siv_test_roundtrip_sizes(const char *ciphername, size_t keylen)
{
    static const size_t sizes[] = { 1, 15, 16, 17, 1024 };
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(1024);
    unsigned char *ct = ALLOCATE_OUTPUT(1024);
    unsigned char *back = ALLOCATE_OUTPUT(1024);
    unsigned char *siv = ALLOCATE_TAG(16);
    const unsigned char *ads[1];
    size_t adlens[1];
    size_t s, i, ctlen, backlen;
    char msg[128];

    for (i = 0; i < 1024; i++)
        pt[i] = (unsigned char)(i * 7 + 1);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i * 3);
    ads[0] = ad;
    adlens[0] = 13;

    {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-SIV cipher");
        snprintf(msg, sizeof(msg), "%s key length is not %zu (2x%zu)",
                 ciphername, keylen, keylen / 2);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen, msg);
        for (i = 0; i < keylen; i++)
            key[i] = (unsigned char)(0xA0 + i);

        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t n = sizes[s];
            siv_encrypt(cipher, key, ads, adlens, 1, pt, n, ct, 1024,
                        &ctlen, siv);
            CHECK(ctlen == n, "SIV ciphertext length is not the payload length");
            siv_decrypt(cipher, key, siv, ads, adlens, 1, ct, ctlen,
                        back, 1024, &backlen);
            snprintf(msg, sizeof(msg),
                     "%s roundtrip of %zu bytes did not recover the plaintext",
                     ciphername, n);
            expect_equal(msg, back, backlen, pt, n);
        }
        EVP_CIPHER_free(cipher);
    }

    free_poison_allocations();
}

/* (3) A wrong synthetic IV must fail DecryptFinal (and the output must be
 * cleansed by the implementation). */
void siv_test_wrong_siv(const char *ciphername, size_t keylen)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32);
    unsigned char *back = ALLOCATE_OUTPUT(32);
    unsigned char *siv = ALLOCATE_TAG(16);
    unsigned char *bad = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    unsigned char siv_copy[16];
    int len = 0;
    const unsigned char *ads[1];
    size_t adlens[1];
    size_t i, ctlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i + 1);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 5 + 2);
    ads[0] = ad;
    adlens[0] = 13;

    siv_encrypt(cipher, key, ads, adlens, 1, pt, 32, ct, 32, &ctlen, siv);
    CHECK(ctlen == 32, "unexpected ciphertext length");

    memcpy(bad, siv, 16);
    bad[0] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(siv_copy, bad, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, siv_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, ad, 13) == 1,
          "AAD update failed");
    /* The verification happens inside the payload update: siv128_decrypt
     * cleanses the output and fails there. */
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
          "SIV decryption accepted a wrong synthetic IV");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) A context performs exactly ONE crypto operation per init: a second
 * payload update on the same context must fail (siv128.c crypto_ok). */
void siv_test_second_payload_fails(const char *ciphername, size_t keylen)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i, ctlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 3);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 32) == 1,
          "first payload update failed");
    ctlen = (size_t)len;
    CHECK(EVP_EncryptUpdate(ctx, ct + ctlen, &len, pt + 32, 32) != 1,
          "a second payload update was accepted on the same context");
    /* No Final/GET_TAG assertions after the rejection: 3.6.x marks the
     * context failed (final_ret = -1) while 3.0.x does not, so the
     * post-rejection Final result differs between the two. */
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) The doubled key length is enforced: requesting a 1x key length at
 * init must fail.  (A full-size key buffer is passed so the failure comes
 * from the length parameter, not from a short buffer.) */
void siv_test_wrong_key_length(const char *ciphername, size_t keylen)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(64);
    OSSL_PARAM params[2];
    EVP_CIPHER_CTX *ctx;
    size_t i;
    size_t keylen_1x = keylen / 2;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xB0 + i);

    /* The 1x key length must be rejected for a cipher whose keylen is
     * twice that. */
    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN,
                                            &keylen_1x);
    params[1] = OSSL_PARAM_construct_end();
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    snprintf(msg, sizeof(msg), "a %zu-byte key length was accepted for %s",
             keylen_1x, ciphername);
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, params) != 1, msg);
    EVP_CIPHER_CTX_free(ctx);

    /* (EVP_CIPHER_CTX_set_key_length is NOT probed here: OpenSSL 3.0.x
     * crashes inside it for fixed-length provider ciphers.) */
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2b) The empty-payload case, with the 3.0.x/3.6.x behavioral difference
 * (see the comment at the top of this file) asserted explicitly. */
void siv_test_empty_payload(const char *ciphername, size_t keylen)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4);
    unsigned char *siv = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, ad, 13) == 1,
          "AAD update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 0) == 1,
          "zero-length payload update failed");
    if (EVP_EncryptFinal_ex(ctx, ct, &flen) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, siv) == 1) {
        /* 3.6.x behavior: the empty payload produced a synthetic IV. */
        const unsigned char *ads[1];
        size_t adlens[1], backlen;

        ads[0] = ad;
        adlens[0] = 13;
        siv_decrypt(cipher, key, siv, ads, adlens, 1, ct, 0, ct, 4, &backlen);
        CHECK(backlen == 0, "empty-payload decrypt returned bytes");
    } else {
        /* 3.0.x behavior: no tag is produced for an empty payload. */
        ERR_clear_error();
    }
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
