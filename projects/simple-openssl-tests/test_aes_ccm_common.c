/*

 * test_aes_ccm_common.c -- shared implementation of the AES-CCM family
 * tests (API in test_aes_ccm_common.h; one binary per cipher and payload
 * regime, one struct ccm_kat row set per binary).

 * Ordering rules (verified empirically against providers/implementations/
 * ciphers/ciphercommon_ccm.c on both OpenSSL builds, and documented in
 * EVP_EncryptInit(3)):

 *   - Defaults are ivlen = 7 (!) and taglen = 12; the nonce must be 7..13
 *     bytes and the tag 4..16 bytes and EVEN.
 *   - ivlen and taglen are set BEFORE the init call that passes the key and
 *     nonce (ccm_init rejects an iv that does not match the configured len).
 *   - The total payload length must be announced first, via an
 *     EVP_*Update(ctx, NULL, &len, NULL, ptlen) call (in == NULL, out ==
 *     NULL); AAD updates with a nonzero length are rejected before that.
 *   - AAD updates use out == NULL and may be repeated; the payload itself
 *     must be delivered in ONE data update -- a second data update fails
 *     (CRYPTO_ccm128_encrypt() re-derives the announced length from the
 *     nonce and errors on the mismatch; decrypt resets tag_set instead).
 *   - An AAD-only operation (empty payload) is valid: Final processes the
 *     zero-length payload and produces the tag.

 * The family splits into two suites over the same single-update helpers:

 *   - the small suite: the key size's RFC 3610 KAT rows plus the
 *     parameter-matrix and ordering-rule sections;
 *   - the large suite (1 MiB, only 128/192 as in the original combined
 *     program): CCM fixes the payload length with the first data update and
 *     rejects a second one (ciphercommon_ccm.c / crypto/modes/ccm128.c: the
 *     announced length is encoded into the nonce's B0 block, and a later
 *     update with a different length trips the length check), so -- unlike
 *     the GCM/OCB programs -- there is no streaming-vs-one-shot comparison;
 *     the single update IS the streaming contract.  Everything here uses
 *     one data update per operation.

 * Known-answer vectors: RFC 3610 packet vectors #1 and #6 (M = 8, 13-byte
 * nonces, cleartext headers used as AAD).  RFC 3610 publishes only 128-bit
 * key vectors, so the "RFC3610#1-192 (derived)" row (the #1 shape with a
 * 24-byte key) was derived with a scratch probe against the authoritative
 * pizfix OpenSSL 3.6.5 and cross-checked byte for byte against the system
 * OpenSSL 3.0.13.  All rows were verified by running them through this
 * exact EVP flow against both OpenSSL builds.

 * A 12-byte nonce is required for the large payloads: CCM encodes the
 * payload length in 15 - ivlen bytes, so 13-byte nonces (2 length bytes)
 * cap the payload at 65535 and a 1 MiB single update is rejected.
 */

#include "test_aes_ccm_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* The RFC 3610 packet vector #1 tuple (M = 8, 13-byte nonce, cleartext
 * header octets as AAD) and its derived 24-byte-key reshaping.  The
 * constants are arrays (not pointers) so that the per-cipher binaries can
 * use them in static struct ccm_kat initializers. */
const char ccm_rfc3610_key_hex[] =
    "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF";
const char ccm_rfc3610_nonce_hex[] =
    "00000003020100A0A1A2A3A4A5";
const char ccm_rfc3610_aad_hex[] =
    "0001020304050607";
const char ccm_rfc3610_pt_hex[] =
    "08090A0B0C0D0E0F101112131415161718191A1B1C1D1E";
const char ccm_rfc3610_ct_hex[] =
    "588C979A61C663D2F066D0C2C0F989806D5F6B61DAC384";
const char ccm_rfc3610_tag_hex[] =
    "17E8D12CFDF926E0";
/* The #1 shape with the 24-byte key (the 128-bit key continued by the same
 * byte pattern): no published RFC 3610 counterpart exists; derived with a
 * scratch probe against pizfix OpenSSL 3.6.5 and cross-checked byte for
 * byte against system 3.0.13. */
const char ccm_rfc3610_192_key_hex[] =
    "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7";
const char ccm_rfc3610_192_ct_hex[] =
    "579fb86eddb4a64aae5fe96dbd75440533a9fc3a845736";
const char ccm_rfc3610_192_tag_hex[] =
    "67aec80ac588ab16";

/* 1 MiB payload; 255-byte AAD (large suite). */
#define LARGE_LEN (1024u * 1024u)
#define AAD_LEN 255u

/* Fetches a core AES-CCM cipher, CHECK()ing success. */
static EVP_CIPHER *ccm_fetch_checked(const char *ciphername,
                                     const char *failmsg)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);

    CHECK(cipher != NULL, failmsg);
    return cipher;
}

/* Single-update CCM encrypt: ivlen/taglen before the key+nonce init, length
 * announcement, AAD, single data update, Final, GET_TAG.  Returns the ct
 * length; the tag (taglen bytes) lands in `tag`. */
static size_t ccm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *nonce, size_t ivlen,
                          unsigned char *tag, size_t taglen,
                          const unsigned char *aad, size_t aadlen,
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
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    /* The payload update is issued even for a zero-length payload: on
     * OpenSSL 3.0.x a payload never delivered through an update (out !=
     * NULL) does not set the tag at Final, while 3.6.5 does.  The explicit
     * zero-length update makes the flow identical on both. */
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

/* Single-update CCM decrypt with tag verification.  The tag LENGTH must be
 * configured before the key+nonce init (CCM bakes it into the B0 flags
 * byte), and the expected tag VALUE after it but before the payload update,
 * whose failure is where a tag mismatch is reported.  Returns the recovered
 * length. */
static size_t ccm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                          const unsigned char *nonce, size_t ivlen,
                          const unsigned char *tag, size_t taglen,
                          const unsigned char *aad, size_t aadlen,
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
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    /* See ccm_encrypt: the payload update runs even when empty.  The tag
     * verification happens inside this update. */
    CHECK(EVP_DecryptUpdate(ctx, pt, &len, ct, (int)ctlen) == 1,
          "payload EVP_DecryptUpdate failed (tag mismatch?)");
    CHECK((size_t)len == ctlen, "CCM update emitted a surprising length");
    CHECK(EVP_DecryptFinal_ex(ctx, pt + ctlen, &flen) == 1,
          "EVP_DecryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return ctlen;
}

/* Fills a buffer with a deterministic pattern (no KAT dependence). */
static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 37 + (i >> 10) * 11 + 5) & 0xff);
}

/* (s1) The RFC 3610 KAT rows (and the derived reshapings) of the binary's
 * cipher. */
void ccm_small_test_kats(const struct ccm_kat *kats, size_t nkats)
{
    size_t k;

    for (k = 0; k < nkats; k++) {
        const struct ccm_kat *kat = &kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *nonce = ALLOCATE_IV(16);
        unsigned char *aad = ALLOCATE_AAD(16);
        unsigned char *pt = ALLOCATE_INPUT(32);
        unsigned char *ct_want = ALLOCATE_OUTPUT(32);
        unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
        unsigned char *tag_want = ALLOCATE_TAG(16);
        unsigned char *tag = ALLOCATE_TAG(16);
        unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
        size_t aadlen, ptlen, ct_wantlen, ctlen, backlen;
        char msg[128];

        /* AES-CCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(kat->noncehex, nonce, 16);
        aadlen = from_hex(kat->aadhex, aad, 16);
        ptlen = from_hex(kat->pthex, pt, 32);
        ct_wantlen = from_hex(kat->cthex, ct_want, 32);
        from_hex(kat->taghex, tag_want, 16);

        ctlen = ccm_encrypt(cipher, key, nonce, 13, tag, 8, aad, aadlen,
                            pt, ptlen, ct, 32 + 16);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        snprintf(msg, sizeof(msg), "%s: tag does not match the KAT", kat->label);
        expect_equal(msg, tag, 8, tag_want, 8);

        backlen = ccm_decrypt(cipher, key, nonce, 13, tag_want, 8, aad, aadlen,
                              ct_want, ct_wantlen, back, 32 + 16);
        snprintf(msg, sizeof(msg),
                 "%s: decrypting the KAT ciphertext did not give the plaintext",
                 kat->label);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);

        free_poison_allocations();
    }
}

/* (s2) Roundtrip matrix: taglen {4,8,12,16} x nonce {7,12,13} x payload
 * {0,1,15,16,17} x AAD {0,32}, each with a roundtrip and a tampered-tag
 * rejection, plus a wrong-nonce rejection. */
void ccm_small_test_matrix(const char *ciphername)
{
    static const size_t taglens[] = { 4, 8, 12, 16 };
    static const size_t noncelens[] = { 7, 12, 13 };
    static const size_t ptlens[] = { 0, 1, 15, 16, 17 };
    static const size_t aadlens[] = { 0, 32 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *badtag = ALLOCATE_TAG(16);
    unsigned char *nonce2 = ALLOCATE_IV(16);
    EVP_CIPHER *cipher = ccm_fetch_checked(ciphername,
                                           "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
    size_t keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    size_t t, nv, p, a, i, ctlen, backlen;
    char msg[160];

    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xC0 + i);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 3 + 9);
    for (i = 0; i < 32; i++)
        aad[i] = (unsigned char)(i * 11 + 1);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x30 + i);
    for (i = 0; i < 16; i++)
        nonce2[i] = (unsigned char)(0x50 + i);

    for (t = 0; t < sizeof(taglens) / sizeof(taglens[0]); t++)
    for (nv = 0; nv < sizeof(noncelens) / sizeof(noncelens[0]); nv++)
    for (p = 0; p < sizeof(ptlens) / sizeof(ptlens[0]); p++)
    for (a = 0; a < sizeof(aadlens) / sizeof(aadlens[0]); a++) {
        size_t taglen = taglens[t], ivlen = noncelens[nv];
        size_t ptlen = ptlens[p], aadlen = aadlens[a];

        ctlen = ccm_encrypt(cipher, key, nonce, ivlen, tag, taglen,
                            aad, aadlen, pt, ptlen, ct, 32 + 16);
        CHECK(ctlen == ptlen, "CCM ciphertext length is not the payload length");
        backlen = ccm_decrypt(cipher, key, nonce, ivlen, tag, taglen,
                              aad, aadlen, ct, ctlen, back, 32 + 16);
        snprintf(msg, sizeof(msg),
                 "%s roundtrip failed (taglen %zu, nonce %zu, pt %zu, aad %zu)",
                 ciphername, taglen, ivlen, ptlen, aadlen);
        expect_equal(msg, back, backlen, pt, ptlen);

        /* A tampered tag must be rejected (one bit flipped).  The tag
         * verification happens inside the payload update, whose failure
         * is the observable rejection (on both 3.0.x and 3.6.x). */
        if (taglen > 0) {
            memcpy(badtag, tag, taglen);
            badtag[taglen - 1] ^= 0x01;
            {
                EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
                unsigned char tag_copy[16];
                int len = 0;

                CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
                CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
                      "EVP_DecryptInit_ex failed");
                CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                          (int)ivlen, NULL) == 1,
                      "EVP_CTRL_AEAD_SET_IVLEN failed");
                CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                          (int)taglen, NULL) == 1,
                      "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
                CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
                      "second EVP_DecryptInit_ex failed");
                memcpy(tag_copy, badtag, taglen);
                CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                          (int)taglen, tag_copy) == 1,
                      "EVP_CTRL_AEAD_SET_TAG failed");
                CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL,
                                        (int)ptlen) == 1,
                      "payload-length announcement failed");
                if (aadlen)
                    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad,
                                            (int)aadlen) == 1,
                          "AAD EVP_DecryptUpdate failed");
                CHECK(EVP_DecryptUpdate(ctx, back, &len, ct,
                                        (int)ctlen) != 1,
                      "CCM accepted a tampered tag");
                EVP_CIPHER_CTX_free(ctx);
            }
        }
    }

    /* A wrong nonce must be rejected during decryption (the tag will
     * not match; the failure surfaces at the payload update). */
    ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, 32,
                        pt, 17, ct, 32 + 16);
    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        unsigned char tag_copy[16];
        int len = 0;

        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12,
                                  NULL) == 1,
              "EVP_CTRL_AEAD_SET_IVLEN failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8,
                                  NULL) == 1,
              "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce2) == 1,
              "second EVP_DecryptInit_ex failed");
        memcpy(tag_copy, tag, 8);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8,
                                  tag_copy) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, 17) == 1,
              "payload-length announcement failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 32) == 1,
              "AAD EVP_DecryptUpdate failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
              "CCM accepted a wrong nonce");
        EVP_CIPHER_CTX_free(ctx);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s3) A second payload update on the same context must fail: the payload
 * length is fixed by the first update. */
void ccm_small_test_second_data_update(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher = ccm_fetch_checked(ciphername,
                                           "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, keylen;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x40 + i);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i + 1);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xD0 + i);

    /* Encrypt: announce 16, deliver 16, then try to deliver 8 more. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 13, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, 16) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 16) == 1,
          "first payload update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 16, 8) != 1,
          "a second payload update was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* Decrypt: same rule (the state is consumed by the first update). */
    ccm_encrypt(cipher, key, nonce, 13, tag, 8, NULL, 0, pt, 16, ct, 32 + 16);
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 13, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, tag) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, 16) == 1,
          "payload-length announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, ct, 16) == 1,
          "first payload update failed");
    CHECK(EVP_DecryptUpdate(ctx, ct + len, &len, ct + 16, 8) != 1,
          "a second payload update was accepted on decrypt");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s4) AAD-only operation (empty payload) is valid, including decrypt; and a
 * nonzero AAD update before the length announcement must fail.
 *
 * Version note: an empty payload is accepted either delivered through an
 * explicit zero-length data update or left to Final.  On 3.6.x both produce
 * a retrievable tag; on 3.0.x only the explicit update does (Final alone
 * leaves the tag unset), so this test delivers it explicitly. */
void ccm_small_test_aad_only(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, keylen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xD0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x40 + i);
    for (i = 0; i < 32; i++)
        aad[i] = (unsigned char)(i * 5 + 2);

    /* Encrypt with no payload at all. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 13, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, 0) == 1,
          "zero-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 32) == 1,
          "AAD update after the announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 0) == 1,
          "zero-length payload update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &len) == 1,
          "AAD-only encryption failed at Final");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 8, tag) == 1,
          "GET_TAG failed after an AAD-only encryption");
    EVP_CIPHER_CTX_free(ctx);

    /* Decrypt side must accept the same AAD-only record. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 13, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, tag) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, 0) == 1,
          "zero-length announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 32) == 1,
          "AAD update after the announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, pt, &len, ct, 0) == 1,
          "zero-length payload update failed (tag mismatch?)");
    CHECK(EVP_DecryptFinal_ex(ctx, pt, &len) == 1,
          "AAD-only decryption failed at Final");
    EVP_CIPHER_CTX_free(ctx);

    /* Nonzero AAD before the length announcement must be rejected. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 13, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 32) != 1,
          "nonzero AAD accepted before the payload-length announcement");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s5) Parameter rules: the defaults are ivlen 7 / taglen 12; nonce lengths
 * outside 7..13 and odd or out-of-range tag lengths are rejected. */
void ccm_small_test_param_rules(const char *ciphername)
{
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t ivlen = 0, taglen = 0;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &ivlen);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(IVLEN) failed");
    CHECK(ivlen == 7, "default CCM IV length is not 7");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN, &taglen);
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(AEAD_TAGLEN) failed");
    CHECK(taglen == 12, "default CCM tag length is not 12");
    EVP_CIPHER_CTX_free(ctx);

    /* Nonce lengths: 7..13 are accepted (verified by the matrix for
     * 7/12/13), 6 and 14 must be rejected. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 6, NULL) != 1,
          "nonce length 6 was accepted");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 14, NULL) != 1,
          "nonce length 14 was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* Tag lengths must be even and within 4..16. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 3, NULL) != 1,
          "tag length 3 was accepted");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 5, NULL) != 1,
          "odd tag length 5 was accepted");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 18, NULL) != 1,
          "tag length 18 was accepted");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (l1) KAT anchor: the RFC 3610 #1 row of the binary's key size through the
 * same single-update helpers used for the 1 MiB payload. */
void ccm_large_test_kat_anchor(const struct ccm_kat *kat)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t aadlen, ptlen, ct_wantlen, ctlen, backlen;

    /* AES-CCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
    from_hex(kat->keyhex, key, 32);
    from_hex(kat->noncehex, nonce, 16);
    aadlen = from_hex(kat->aadhex, aad, 16);
    ptlen = from_hex(kat->pthex, pt, 32);
    ct_wantlen = from_hex(kat->cthex, ct_want, 32);
    from_hex(kat->taghex, tag_want, 16);

    ctlen = ccm_encrypt(cipher, key, nonce, 13, tag, 8, aad, aadlen,
                        pt, ptlen, ct, 32 + 16);
    CHECK(ctlen == ct_wantlen, "KAT ciphertext length mismatch");
    unpoison_all();
    CHECK(memcmp(ct, ct_want, ct_wantlen) == 0,
          "ciphertext does not match RFC 3610 #1");
    repoison_all();
    unpoison_all();
    CHECK(memcmp(tag, tag_want, 8) == 0, "tag does not match RFC 3610 #1");
    repoison_all();

    backlen = ccm_decrypt(cipher, key, nonce, 13, tag_want, 8, aad, aadlen,
                          ct_want, ct_wantlen, back, 32 + 16);
    unpoison_all();
    CHECK(backlen == ptlen && memcmp(back, pt, ptlen) == 0,
          "decrypting the KAT ciphertext did not give the plaintext");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l2) The 1 MiB payload with 255 bytes of AAD round-trips in a single
 * update, and the output buffer obeys the EVP size contract. */
void ccm_large_test_roundtrip(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    size_t ctlen, backlen, i, keylen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt,
                        LARGE_LEN, ct, LARGE_LEN + 16);
    CHECK(ctlen == LARGE_LEN, "large ciphertext length is not the payload length");
    backlen = ccm_decrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, ct, ctlen,
                          back, LARGE_LEN + 16);
    CHECK(backlen == LARGE_LEN,
          "large decrypt did not return the payload length");
    unpoison_all();
    CHECK(memcmp(back, pt, LARGE_LEN) == 0,
          "1MiB single-update roundtrip did not recover the plaintext");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l3) A second data update after the 1 MiB payload must fail. */
void ccm_large_test_second_data_update(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16 + 16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, keylen, ctlen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)LARGE_LEN) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)AAD_LEN) == 1,
          "AAD update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)LARGE_LEN) == 1,
          "the 1MiB payload update failed");
    ctlen = (size_t)len;
    CHECK(ctlen == LARGE_LEN, "unexpected large update output length");
    CHECK(EVP_EncryptUpdate(ctx, ct + ctlen, &len, pt, 16) != 1,
          "a second payload update was accepted");
    /* Note: no GET_TAG here -- the rejected update corrupted the internal
     * nonce state (ccm128.c does not restore it on the length-mismatch
     * error path), so the tag is no longer retrievable. */
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l4) A zero-length payload (AAD-only record) is valid and round-trips. */
void ccm_large_test_zero_length_payload(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    size_t ctlen, backlen, i, keylen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 5 + 2);

    ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt, 0,
                        ct, 4 + 16);
    CHECK(ctlen == 0, "AAD-only encryption produced ciphertext bytes");
    backlen = ccm_decrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, ct, ctlen,
                          back, 4 + 16);
    CHECK(backlen == 0, "AAD-only decryption produced plaintext bytes");
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l5) Tampering with the ciphertext or the tag must be detected inside the
 * payload update. */
void ccm_large_test_tamper(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t ctlen, i, keylen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    ctlen = ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt,
                        LARGE_LEN, ct, LARGE_LEN + 16);

    /* Flip one bit in the middle of the 1 MiB ciphertext. */
    ct[LARGE_LEN / 2] ^= 0x10;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_bad, tag, 8);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)AAD_LEN) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
          "decryption accepted a tampered ciphertext byte");
    EVP_CIPHER_CTX_free(ctx);

    /* Flip one tag bit (with clean ciphertext). */
    ct[LARGE_LEN / 2] ^= 0x10;
    memcpy(tag_bad, tag, 8);
    tag_bad[7] ^= 0x80;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)AAD_LEN) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
          "decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l6) Output-size contract: CCM reports block_size 1, so the EVP contract
 * lets a caller pass an update buffer of exactly inl + 1 bytes; the update
 * must emit exactly inl bytes and Final none. */
void ccm_large_test_output_buffer_contract(const char *ciphername)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    size_t bs;
    /* Exactly inl + block_size, the tightest buffer the EVP contract
     * guarantees for an update. */
    unsigned char *ct;
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len, flen = 0;
    size_t i, keylen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = ccm_fetch_checked(ciphername, fetchmsg);
    bs = (size_t)EVP_CIPHER_get_block_size(cipher);
    CHECK(bs == 1, "CCM reports an unexpected block size");
    ct = ALLOCATE_OUTPUT(LARGE_LEN + bs);
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 8, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)LARGE_LEN) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)AAD_LEN) == 1,
          "AAD update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)LARGE_LEN) == 1,
          "payload update into the exact-size buffer failed");
    CHECK(len == (int)LARGE_LEN,
          "update did not emit exactly inl bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + LARGE_LEN, &flen) == 1,
          "Final failed at the end of the exact-size buffer");
    CHECK(flen == 0, "Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 8, tag) == 1,
          "GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
