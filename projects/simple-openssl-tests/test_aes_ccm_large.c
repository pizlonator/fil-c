#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aes_ccm_large -- AES-128/192-CCM with a large single-update payload.
 *
 * CCM fixes the payload length with the first data update and rejects a
 * second one (ciphercommon_ccm.c / crypto/modes/ccm128.c: the announced
 * length is encoded into the nonce's B0 block, and a later update with a
 * different length trips the length check), so -- unlike the GCM/OCB
 * programs -- this file has no streaming-vs-one-shot comparison; the single
 * update IS the streaming contract.  Everything here uses one data update
 * per operation:
 *
 *   - a 1 MiB payload with 255 bytes of AAD round-trips,
 *   - a second data update on the same context fails,
 *   - a zero-length payload (AAD-only record) is valid,
 *   - tampering with the ciphertext or the tag is detected inside the
 *     payload update,
 *   - the output buffer obeys the EVP contract (inl + block_size per
 *     update; the update emits exactly inl bytes and Final none).
 *
 * The helper is anchored to RFC 3610 packet vector #1 (verified against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked on 3.0.13), run
 * through the same single-update helper used for the 1 MiB payload.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_TAG(size_t n)    { return poison_alloc(POISON_TAG, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_ccm_large: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* 1 MiB payload; 255-byte AAD.  A 12-byte nonce is required: CCM encodes
 * the payload length in 15 - ivlen bytes, so 13-byte nonces (2 length bytes)
 * cap the payload at 65535 and a 1 MiB single update is rejected. */
#define LARGE_LEN (1024u * 1024u)
#define AAD_LEN 255u

/* Single-update CCM encrypt (see test_aes_ccm_small.c for the ordering
 * rules): ivlen/taglen before the key+nonce init, length announcement, AAD,
 * one data update, Final, GET_TAG.  `out` needs inlen + 16 bytes. */
static void ccm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *pt, size_t ptlen,
                        unsigned char *out, size_t outcap, size_t *outlen)
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
    CHECK(ptlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    /* One data update, exactly as large as the announced payload. */
    CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK((size_t)len == ptlen, "CCM update emitted a surprising length");
    CHECK(EVP_EncryptFinal_ex(ctx, out + ptlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = ptlen;
}

/* Single-update CCM decrypt with tag verification; the tag length is set
 * before the key+nonce init and the tag value after it. */
static void ccm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        const unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *ct, size_t ctlen,
                        unsigned char *out, size_t outcap, size_t *outlen)
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
    CHECK(ctlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
          "payload-length announcement failed");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, ct, (int)ctlen) == 1,
          "payload EVP_DecryptUpdate failed (tag mismatch?)");
    CHECK((size_t)len == ctlen, "CCM update emitted a surprising length");
    CHECK(EVP_DecryptFinal_ex(ctx, out + ctlen, &flen) == 1,
          "EVP_DecryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = ctlen;
}

/* Fills a buffer with a deterministic pattern (no KAT dependence). */
static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 37 + (i >> 10) * 11 + 5) & 0xff);
}

/* (1) KAT anchor: RFC 3610 #1 through the same single-update helpers. */
static void test_kat_anchor(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
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
    from_hex("C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF", key, 16);
    from_hex("00000003020100A0A1A2A3A4A5", nonce, 16);
    aadlen = from_hex("0001020304050607", aad, 16);
    ptlen = from_hex("08090A0B0C0D0E0F101112131415161718191A1B1C1D1E", pt, 32);
    ct_wantlen = from_hex("588C979A61C663D2F066D0C2C0F989806D5F6B61DAC384",
                          ct_want, 32);
    from_hex("17E8D12CFDF926E0", tag_want, 16);

    ccm_encrypt(cipher, key, nonce, 13, tag, 8, aad, aadlen, pt, ptlen,
                ct, 32 + 16, &ctlen);
    CHECK(ctlen == ct_wantlen, "KAT ciphertext length mismatch");
    CHECK(memcmp(ct, ct_want, ct_wantlen) == 0,
          "ciphertext does not match RFC 3610 #1");
    CHECK(memcmp(tag, tag_want, 8) == 0, "tag does not match RFC 3610 #1");

    ccm_decrypt(cipher, key, nonce, 13, tag_want, 8, aad, aadlen,
                ct_want, ct_wantlen, back, 32 + 16, &backlen);
    CHECK(backlen == ptlen && memcmp(back, pt, ptlen) == 0,
          "decrypting the KAT ciphertext did not give the plaintext");
    EVP_CIPHER_free(cipher);
}

/* (2) The 1 MiB payload with 255 bytes of AAD round-trips in a single
 * update, and the output buffer obeys the EVP size contract -- for AES-128
 * and AES-192. */
static void test_large_roundtrip(void)
{
    static const char *const names[] = { "AES-128-CCM", "AES-192-CCM" };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t m, ctlen, backlen, i, keylen;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
        keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keylen; i++)
            key[i] = (unsigned char)(0xE0 + i);

        ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt, LARGE_LEN,
                    ct, LARGE_LEN + 16, &ctlen);
        CHECK(ctlen == LARGE_LEN, "large ciphertext length is not the payload length");
        ccm_decrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, ct, ctlen,
                    back, LARGE_LEN + 16, &backlen);
        CHECK(backlen == LARGE_LEN,
              "large decrypt did not return the payload length");
        CHECK(memcmp(back, pt, LARGE_LEN) == 0,
              "1MiB single-update roundtrip did not recover the plaintext");
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A second data update after the 1 MiB payload must fail, for AES-128
 * and AES-192. */
static void test_second_data_update(void)
{
    static const char *const names[] = { "AES-128-CCM", "AES-192-CCM" };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t m, i, keylen, ctlen;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
        keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keylen; i++)
            key[i] = (unsigned char)(0xE0 + i);

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
    }
}

/* (4) A zero-length payload (AAD-only record) is valid and round-trips. */
static void test_zero_length_payload(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t ctlen, backlen, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
    for (i = 0; i < 16; i++)
        key[i] = (unsigned char)(0xE0 + i);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 5 + 2);

    ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt, 0,
                ct, 4 + 16, &ctlen);
    CHECK(ctlen == 0, "AAD-only encryption produced ciphertext bytes");
    ccm_decrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, ct, ctlen,
                back, 4 + 16, &backlen);
    CHECK(backlen == 0, "AAD-only decryption produced plaintext bytes");
    EVP_CIPHER_free(cipher);
}

/* (5) Tampering with the ciphertext or the tag must be detected inside the
 * payload update -- for AES-128 and AES-192. */
static void test_tamper(void)
{
    static const char *const names[] = { "AES-128-CCM", "AES-192-CCM" };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t m, ctlen, i, keylen;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x10 + i);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LEN; i++)
        aad[i] = (unsigned char)(i * 3 + 1);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
        keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keylen; i++)
            key[i] = (unsigned char)(0xE0 + i);

        ccm_encrypt(cipher, key, nonce, 12, tag, 8, aad, AAD_LEN, pt, LARGE_LEN,
                    ct, LARGE_LEN + 16, &ctlen);

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
    }
}

/* (6) Output-size contract: CCM reports block_size 1, so the EVP contract
 * lets a caller pass an update buffer of exactly inl + 1 bytes; the update
 * must emit exactly inl bytes and Final none. */
static void test_output_buffer_contract(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    size_t bs;
    /* Exactly inl + block_size, the tightest buffer the EVP contract
     * guarantees for an update. */
    unsigned char *ct;
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len, flen = 0;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
    bs = (size_t)EVP_CIPHER_get_block_size(cipher);
    CHECK(bs == 1, "CCM reports an unexpected block size");
    ct = ALLOCATE_OUTPUT(LARGE_LEN + bs);
    for (i = 0; i < 16; i++)
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
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kat_anchor();
    test_large_roundtrip();
    test_second_data_update();
    test_zero_length_payload();
    test_tamper();
    test_output_buffer_contract();

    printf("test_aes_ccm_large: all %u checks passed\n", g_checks);
    return 0;
}
