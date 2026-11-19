/*

 * test_aes_ocb_common.c -- shared implementation of the AES-OCB family
 * tests (API in test_aes_ocb_common.h; one binary per cipher, one or more
 * struct ocb_kat rows per binary).

 * Rules exercised by the small-payload binaries (verified against
 * crypto/modes/ocb128.c and providers/implementations/ciphers/
 * cipher_aes_ocb.c):
 *
 *   - the nonce may be 1..15 bytes; the default is 12; 16 is rejected
 *     (CRYPTO_ocb128_setiv: "Spec says IV is 120 bits or fewer");
 *   - the tag may be 1..16 bytes (default 16); 0 and 17 are rejected;
 *   - only the LAST AAD/data update may carry a partial block -- the
 *     provider buffers partial blocks and flushes them at Final or on the
 *     next full block, so splitting an otherwise legal input across
 *     updates reproduces the one-shot result exactly;
 *   - an AAD update after the first data update is ACCEPTED by the
 *     provider (unlike GCM): the AAD hash chain is independent of the data
 *     checksum, so the result is self-consistent and decrypts when the AAD
 *     is replayed in the same order.  The binaries assert that observed
 *     behavior rather than a rejection.
 *
 * Rules exercised by the large-payload binaries (test_aes_192_ocb_large.c
 * and test_aes_256_ocb_large.c): the oracle is self-consistency, as in
 * test_aes_gcm_large -- a 1 MiB payload fed through mixed {16384, 16391}
 * byte updates must produce exactly the one-shot ciphertext and tag.
 * 16391 = 16384 + 7 forces a partial block to be carried in the provider's
 * data buffer across updates and flushed by the next update, exercising
 * both the bulk per-block path (crypto/modes/ocb128.c streams full blocks
 * when the stream callback is available) and the buffered-tail path.  A
 * 64 KiB AAD is fed in two 32 KiB chunks and must match the one-shot AAD.
 * AAD updates report the number of AAD bytes hashed through *outl (unlike
 * GCM, which reports 0), so ciphertext lengths are accumulated from data
 * updates only.
 *
 * Known-answer vectors: RFC 7253 Appendix A sample tuples for AES-128 (all
 * verified through this EVP flow against the authoritative pizfix OpenSSL
 * 3.6.5 and cross-checked on 3.0.13), the RFC's 96-bit-tag tuple with the
 * second key, and AES-256/AES-192 tuples derived with a scratch probe (the
 * RFC publishes sample results only for 128-bit keys); the derived tuples
 * were cross-checked byte for byte against the system OpenSSL 3.0.13.
 */

#include "test_aes_ocb_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* The 40-byte 000102.. A/P sequence of the RFC's largest sample tuple and
 * the anchor nonce it uses.  The constants are arrays (not pointers) so
 * that the per-cipher binaries can use them in static struct ocb_kat
 * initializers. */
const char ocb_seq40_hex[] =
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B"
    "1C1D1E1F2021222324252627";
const char ocb_nonce_anchor_hex[] = "BBAA9988776655443322110D";

#define LARGE_LEN (1024u * 1024u)   /* 1 MiB payload */
#define AAD_LEN   (64u * 1024u)     /* 64 KiB AAD */

/* The large regime's chunk grids: 16391 = 16384 + 7 forces a partial block
 * to ride in the provider's buffer across updates (see the header comment);
 * the 64 KiB AAD goes through two 32 KiB chunks. */
static const size_t DATA_CHUNKS[] = { 16384, 16391 };
static const size_t AAD_CHUNKS[] = { 32768, 32768 };

/* One-shot OCB encrypt.  `out` needs inlen + 16 bytes. */
void ocb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *nonce, size_t ivlen,
                 unsigned char *tag, size_t taglen,
                 const unsigned char *aad, size_t aadlen,
                 const unsigned char *pt, size_t ptlen,
                 unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, scratch = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(ptlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    /* Note: OCB AAD updates report the number of AAD bytes hashed through
     * *outl (unlike GCM, which reports 0), so the ciphertext length is
     * accumulated from the data updates only. */
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &scratch, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    if (ptlen)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen) == 1,
              "payload EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    len += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* One-shot OCB decrypt with tag verification (the tag is set before the
 * payload update). */
void ocb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *nonce, size_t ivlen,
                 const unsigned char *tag, size_t taglen,
                 const unsigned char *aad, size_t aadlen,
                 const unsigned char *ct, size_t ctlen,
                 unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, scratch = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    /* The tag length must match the length of the tag value set below, so
     * configure it first (the default is 16). */
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                              tag_copy) == 1,
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

/* Streaming OCB encrypt: AAD updates first (chunks), then payload updates
 * (chunks), Final, GET_TAG.  Must match the one-shot result exactly. */
void ocb_encrypt_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const size_t *aadchunks, size_t naadchunks,
                        const unsigned char *pt, size_t ptlen,
                        const size_t *ptchunks, size_t nptchunks,
                        unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off, ci;
    int len = 0, scratch = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(ptlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    off = 0;
    ci = 0;
    while (off < aadlen) {
        size_t n = aadchunks[ci++ % naadchunks];
        if (n > aadlen - off)
            n = aadlen - off;
        /* OCB AAD updates report hashed bytes via *outl; ignored here. */
        CHECK(EVP_EncryptUpdate(ctx, NULL, &scratch, aad + off, (int)n) == 1,
              "streaming AAD EVP_EncryptUpdate failed");
        off += n;
    }
    off = 0;
    ci = 0;
    while (off < ptlen) {
        size_t n = ptchunks[ci++ % nptchunks];
        if (n > ptlen - off)
            n = ptlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, pt + off, (int)n) == 1,
              "streaming payload EVP_EncryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "streaming EVP_EncryptFinal_ex failed");
    dlen += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)dlen;
}

/* Streaming OCB decrypt with tag verification, through the same chunk
 * grids as ocb_encrypt_stream. */
void ocb_decrypt_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        const unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const size_t *aadchunks, size_t naadchunks,
                        const unsigned char *ct, size_t ctlen,
                        const size_t *ctchunks, size_t nctchunks,
                        unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    size_t off, ci;
    int len = 0, scratch = 0, dlen = 0, flen = 0;
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
    CHECK(ctlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    off = 0;
    ci = 0;
    while (off < aadlen) {
        size_t n = aadchunks[ci++ % naadchunks];
        if (n > aadlen - off)
            n = aadlen - off;
        CHECK(EVP_DecryptUpdate(ctx, NULL, &scratch, aad + off, (int)n) == 1,
              "streaming AAD EVP_DecryptUpdate failed");
        off += n;
    }
    off = 0;
    ci = 0;
    while (off < ctlen) {
        size_t n = ctchunks[ci++ % nctchunks];
        if (n > ctlen - off)
            n = ctlen - off;
        CHECK(EVP_DecryptUpdate(ctx, out + dlen, &len, ct + off, (int)n) == 1,
              "streaming EVP_DecryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "streaming EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += flen;
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)dlen;
}

/* Fills a buffer with a deterministic pattern (no KAT dependence). */
void ocb_fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 29 + (i >> 8) * 13 + 3) & 0xff);
}

/* (1) The RFC 7253 KAT rows through the one-shot helpers; each tuple is
 * also decrypted back to its plaintext. */
void ocb_test_kats(const struct ocb_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    size_t k;
    char msg[128];

    for (k = 0; k < nkats; k++) {
        const struct ocb_kat *kat = &kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t keylen, aadlen, ptlen, ct_wantlen, ctlen, backlen;
        size_t tag_wantlen = from_hex(kat->taghex, tag_want, 16);

        /* OCB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
        keylen = from_hex(kat->keyhex, key, 32);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched OCB cipher has an unexpected key length");
        from_hex(kat->noncehex, nonce, 16);
        aadlen = from_hex(kat->aadhex, aad, 64);
        ptlen = from_hex(kat->pthex, pt, 64);
        ct_wantlen = from_hex(kat->cthex, ct_want, 64);

        ocb_encrypt(cipher, key, nonce, 12, tag, tag_wantlen, aad, aadlen,
                    pt, ptlen, ct, 64 + 16, &ctlen);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        snprintf(msg, sizeof(msg), "%s: tag does not match the KAT", kat->label);
        expect_equal(msg, tag, tag_wantlen, tag_want, tag_wantlen);

        ocb_decrypt(cipher, key, nonce, 12, tag_want, tag_wantlen, aad, aadlen,
                    ct_want, ct_wantlen, back, 64 + 16, &backlen);
        snprintf(msg, sizeof(msg),
                 "%s: decrypting the KAT ciphertext did not give the plaintext",
                 kat->label);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }

    free_poison_allocations();
}

/* (2) IV length rules: the default is 12; 1..15 round-trip; 0 and 16 are
 * rejected -- for the binary's own cipher. */
void ocb_test_iv_lengths(const char *ciphername, const char *keyhex)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t ivlen = 0, v, i, ctlen, backlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    CHECK(cipher != NULL, msg);
    from_hex(keyhex, key, 32);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 3 + 9);
    for (i = 0; i < 16; i++)
        aad[i] = (unsigned char)(i + 1);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x40 + i);

    /* Default IV length, queried via OSSL_PARAM. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &ivlen);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(IVLEN) failed");
    CHECK(ivlen == 12, "default OCB IV length is not 12");
    EVP_CIPHER_CTX_free(ctx);

    /* 0 and 16 are rejected. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 0, NULL) != 1,
          "IV length 0 was accepted");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 16, NULL) != 1,
          "IV length 16 was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 1, 12 and 15 round-trip. */
    for (v = 0; v < 3; v++) {
        size_t len = (v == 0) ? 1 : (v == 1) ? 12 : 15;
        ocb_encrypt(cipher, key, nonce, len, tag, 16, aad, 16, pt, 32,
                    ct, 32 + 16, &ctlen);
        ocb_decrypt(cipher, key, nonce, len, tag, 16, aad, 16, ct, ctlen,
                    back, 32 + 16, &backlen);
        snprintf(msg, sizeof(msg),
                 "IV length %zu roundtrip did not recover the plaintext", len);
        expect_equal(msg, back, backlen, pt, 32);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Tag length rules: 1..16 round-trip; 0 is accepted at set time
 * (OCB_MAX_TAG_LEN check only) but the operation must fail
 * (CRYPTO_ocb128_setiv requires 1..16); 17 is rejected outright -- for the
 * binary's own cipher. */
void ocb_test_tag_lengths(const char *ciphername, const char *keyhex)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    size_t t, i, ctlen, backlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    CHECK(cipher != NULL, msg);
    from_hex(keyhex, key, 32);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 5 + 3);
    for (i = 0; i < 16; i++)
        aad[i] = (unsigned char)(i * 2);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x50 + i);

    for (t = 1; t <= 16; t++) {
        ocb_encrypt(cipher, key, nonce, 12, tag, t, aad, 16, pt, 32,
                    ct, 32 + 16, &ctlen);
        ocb_decrypt(cipher, key, nonce, 12, tag, t, aad, 16, ct, ctlen,
                    back, 32 + 16, &backlen);
        snprintf(msg, sizeof(msg),
                 "tag length %zu roundtrip did not recover the plaintext", t);
        expect_equal(msg, back, backlen, pt, 32);
    }

    /* Tag length 0 is accepted at set time, but the operation must fail.
     * Tag length 17 is rejected outright. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 0, NULL) == 1,
          "setting tag length 0 failed outright");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    {
        int len = 0, flen = 0, ok = 1;
        unsigned char tag0[16];

        ok &= EVP_EncryptUpdate(ctx, NULL, &len, aad, 16) == 1;
        ok &= EVP_EncryptUpdate(ctx, ct, &len, pt, 32) == 1;
        ok &= EVP_EncryptFinal_ex(ctx, ct + len, &flen) == 1;
        ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 0, tag0) == 1;
        CHECK(!ok, "a zero-length OCB tag was produced");
    }
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 17, NULL) != 1,
          "tag length 17 was accepted");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) Partial final blocks: {15} in one update, {16,7} across two updates,
 * and AAD {20,5} across two updates -- all must equal the one-shot result
 * (the provider buffers the partial block and flushes it at Final) -- for
 * the binary's own cipher. */
void ocb_test_partial_blocks(const char *ciphername, const char *keyhex)
{
    static const size_t one15[] = { 15 };
    static const size_t mix[] = { 16, 7 };
    static const size_t aadchunks[] = { 20, 5 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(25);
    unsigned char *pt = ALLOCATE_INPUT(23);
    unsigned char *ct1 = ALLOCATE_OUTPUT(23 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(23 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher;
    size_t ct1len, ct2len, i;
    char msg[128];

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x60 + i);
    for (i = 0; i < 25; i++)
        aad[i] = (unsigned char)(i * 7 + 1);
    for (i = 0; i < 23; i++)
        pt[i] = (unsigned char)(i * 3 + 2);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    from_hex(keyhex, key, 32);

    /* 23 bytes in one shot vs {16,7}; 15-byte payload one-shot vs {15}. */
    ocb_encrypt(cipher, key, nonce, 12, tag1, 16, aad, 25, pt, 23,
                ct1, 23 + 16, &ct1len);
    ocb_encrypt_stream(cipher, key, nonce, 12, tag2, 16, aad, 25,
                       aadchunks, 2, pt, 23, mix, 2, ct2, 23 + 16, &ct2len);
    expect_equal("split {16,7} ciphertext differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("split {16,7} tag differs from one-shot tag",
                 tag2, 16, tag1, 16);

    ocb_encrypt(cipher, key, nonce, 12, tag1, 16, aad, 25, pt, 15,
                ct1, 23 + 16, &ct1len);
    ocb_encrypt_stream(cipher, key, nonce, 12, tag2, 16, aad, 25,
                       aadchunks, 2, pt, 15, one15, 1, ct2, 23 + 16, &ct2len);
    expect_equal("split {15} ciphertext differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("split {15} tag differs from one-shot tag",
                 tag2, 16, tag1, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) AAD after the first data update: the provider ACCEPTS it (unlike
 * GCM, whose gcm128.c rejects late AAD), because the OCB AAD hash chain is
 * independent of the data checksum.  Assert the observed behavior: the
 * update succeeds and the result decrypts when the AAD is replayed in the
 * same order -- for the binary's own cipher. */
void ocb_test_aad_after_data(const char *ciphername, const char *keyhex)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_copy[16];
    EVP_CIPHER_CTX *ctx;
    int len, flen = 0;
    size_t i, ctlen, backlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    from_hex(keyhex, key, 32);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x70 + i);
    for (i = 0; i < 20; i++)
        aad[i] = (unsigned char)(i * 11 + 3);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 13 + 7);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 32) == 1,
          "first data update failed");
    ctlen = (size_t)len;
    /* The late AAD update reports its hashed-byte count through *outl; the
     * ciphertext length is unaffected. */
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 20) == 1,
          "AAD update after the first data update was rejected");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ctlen, &flen) == 1,
          "Final failed after late AAD");
    ctlen += (size_t)flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "GET_TAG failed after late AAD");
    EVP_CIPHER_CTX_free(ctx);

    /* Decrypt with the AAD in the same (late) order must verify. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "decrypt-side data update failed");
    backlen = (size_t)len;
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 20) == 1,
          "decrypt-side late AAD update was rejected");
    CHECK(EVP_DecryptFinal_ex(ctx, back + backlen, &flen) == 1,
          "late-AAD record did not verify on decrypt");
    backlen += (size_t)flen;
    expect_equal("late-AAD roundtrip did not recover the plaintext",
                 back, backlen, pt, 32);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) Tamper detection: flipping a ciphertext bit or a tag bit must fail
 * DecryptFinal -- for the binary's own cipher. */
void ocb_test_tamper(const char *ciphername, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len, flen = 0;
    size_t ctlen, i;
    char msg[128];

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x80 + i);
    for (i = 0; i < 16; i++)
        aad[i] = (unsigned char)(i + 9);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 17 + 1);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    from_hex(keyhex, key, 32);

    ocb_encrypt(cipher, key, nonce, 12, tag, 16, aad, 16, pt, 32,
                ct, 32 + 16, &ctlen);

    /* Flip a ciphertext bit. */
    ct[5] ^= 0x20;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_bad, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 16) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright (it may emit garbage)");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered ciphertext byte");
    EVP_CIPHER_CTX_free(ctx);
    ct[5] ^= 0x20;

    /* Flip a tag bit. */
    memcpy(tag_bad, tag, 16);
    tag_bad[0] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 16) == 1,
          "AAD update failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright (it may emit garbage)");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (1, large) KAT anchor: the row's RFC 7253 tuple through the one-shot
 * helper, then the same cipher with a 64 KiB patterned AAD (the tuple's AAD
 * occupies its first aadlen bytes): streaming must equal one-shot. */
void ocb_test_kat_anchor(const struct ocb_kat *kat)
{
    EVP_CIPHER *cipher;
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t aadlen, ptlen, ct_wantlen, ctlen;
    unsigned char *aadbig;
    char msg[128];

    /* OCB is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
    from_hex(kat->keyhex, key, 32);
    from_hex(kat->noncehex, nonce, 16);
    aadlen = from_hex(kat->aadhex, aad, 64);
    ptlen = from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);
    from_hex(kat->taghex, tag_want, 16);

    ocb_encrypt(cipher, key, nonce, 12, tag, 16, aad, aadlen, pt, ptlen,
                ct, 64 + 16, &ctlen);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
    snprintf(msg, sizeof(msg), "%s tag does not match the KAT",
             kat->ciphername);
    expect_equal(msg, tag, 16, tag_want, 16);

    /* The same cipher through the streaming helper over the full 64 KiB
     * AAD buffer: compare against a one-shot of the same buffer. */
    aadbig = ALLOCATE_AAD(AAD_LEN);
    ocb_fill_pattern(aadbig, AAD_LEN);
    memcpy(aadbig, aad, aadlen);
    ocb_encrypt(cipher, key, nonce, 12, tag, 16, aadbig, AAD_LEN, pt, ptlen,
                ct, 64 + 16, &ctlen);
    ocb_encrypt_stream(cipher, key, nonce, 12, tag_want, 16, aadbig, AAD_LEN,
                       AAD_CHUNKS, sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]),
                       pt, ptlen,
                       DATA_CHUNKS, sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]),
                       ct_want, 64 + 16, &ct_wantlen);
    expect_equal("streaming ciphertext differs from one-shot (64KiB AAD)",
                 ct_want, ct_wantlen, ct, ctlen);
    expect_equal("streaming tag differs from one-shot tag (64KiB AAD)",
                 tag_want, 16, tag, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2, large) 1 MiB streaming == one-shot, both directions: the mixed
 * {16384, 16391} chunk grid must reproduce the one-shot ciphertext and tag,
 * and the streaming decrypt must recover the plaintext. */
void ocb_test_large_stream_equals_one_shot(const char *ciphername,
                                           size_t keylen)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher;
    size_t ct1len, ct2len, backlen, i;
    char msg[128];

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    ocb_fill_pattern(pt, LARGE_LEN);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xC0 + i);

    ocb_encrypt(cipher, key, nonce, 12, tag1, 16, NULL, 0, pt, LARGE_LEN,
                ct1, LARGE_LEN + 16, &ct1len);
    CHECK(ct1len == LARGE_LEN, "one-shot OCB ciphertext length is not inlen");
    ocb_encrypt_stream(cipher, key, nonce, 12, tag2, 16, NULL, 0,
                       AAD_CHUNKS, sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]),
                       pt, LARGE_LEN,
                       DATA_CHUNKS, sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]),
                       ct2, LARGE_LEN + 16, &ct2len);
    expect_equal("1MiB streaming ciphertext differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("1MiB streaming tag differs from one-shot tag",
                 tag2, 16, tag1, 16);

    ocb_decrypt_stream(cipher, key, nonce, 12, tag2, 16, NULL, 0,
                       AAD_CHUNKS, sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]),
                       ct2, ct2len,
                       DATA_CHUNKS, sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]),
                       back, LARGE_LEN + 16, &backlen);
    CHECK(backlen == LARGE_LEN, "streaming decrypt length is not inlen");
    unpoison_all();
    CHECK(memcmp(back, pt, LARGE_LEN) == 0,
          "streaming decrypt of 1MiB did not recover the plaintext");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3, large) A 64 KiB AAD in two 32 KiB chunks must match a one-shot AAD. */
void ocb_test_large_aad(const char *ciphername, size_t keylen)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(1024);
    unsigned char *ct1 = ALLOCATE_OUTPUT(1024 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(1024 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher;
    size_t ct1len, ct2len, i;
    char msg[128];

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    ocb_fill_pattern(aad, AAD_LEN);
    ocb_fill_pattern(pt, 1024);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xC0 + i);

    ocb_encrypt(cipher, key, nonce, 12, tag1, 16, aad, AAD_LEN, pt, 1024,
                ct1, 1024 + 16, &ct1len);
    ocb_encrypt_stream(cipher, key, nonce, 12, tag2, 16, aad, AAD_LEN,
                       AAD_CHUNKS, sizeof(AAD_CHUNKS) / sizeof(AAD_CHUNKS[0]),
                       pt, 1024,
                       DATA_CHUNKS, sizeof(DATA_CHUNKS) / sizeof(DATA_CHUNKS[0]),
                       ct2, 1024 + 16, &ct2len);
    expect_equal("64KiB two-chunk AAD ciphertext differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("64KiB two-chunk AAD tag differs from one-shot tag",
                 tag2, 16, tag1, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4, large) Tampering with the final ciphertext block must fail
 * DecryptFinal: flipping the first byte of the final 16-byte block and the
 * very last byte in turn. */
void ocb_test_tampered_final_block(const char *ciphername, size_t keylen)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_copy[16];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t ctlen, i;
    int len = 0, flen = 0;
    char msg[128];

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x20 + i);
    ocb_fill_pattern(pt, LARGE_LEN);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xC0 + i);

    ocb_encrypt(cipher, key, nonce, 12, tag, 16, NULL, 0, pt, LARGE_LEN,
                ct, LARGE_LEN + 16, &ctlen);
    CHECK(ctlen == LARGE_LEN, "unexpected one-shot ciphertext length");

    /* Flip the first byte of the final 16-byte block. */
    ct[ctlen - 16] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright (it may emit garbage)");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered final ciphertext block");
    EVP_CIPHER_CTX_free(ctx);

    /* Flip the very last byte. */
    ct[ctlen - 1] ^= 0x80;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_DecryptInit_ex failed");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "payload update failed outright (it may emit garbage)");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &flen) != 1,
          "decryption accepted a tampered last ciphertext byte");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
