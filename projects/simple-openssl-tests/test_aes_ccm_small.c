#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aes_ccm_small -- AES-128/192/256-CCM known-answer and behavior tests
 * for small payloads.  (A sibling program covers a 1 MiB single-update
 * payload.)
 *
 * Ordering rules (verified empirically against providers/implementations/
 * ciphers/ciphercommon_ccm.c on both OpenSSL builds, and documented in
 * EVP_EncryptInit(3)):
 *
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
 *
 * Known-answer vectors: RFC 3610 packet vectors #1 and #6 (M = 8, 13-byte
 * nonces, cleartext headers used as AAD).  RFC 3610 publishes only 128-bit
 * key vectors, so the "RFC3610#1-192 (derived)" row below (the #1 shape with
 * a 24-byte key) was derived with a scratch probe against the authoritative
 * pizfix OpenSSL 3.6.5 and cross-checked byte for byte against the system
 * OpenSSL 3.0.13.  All rows were verified by running them through this exact
 * EVP flow against both OpenSSL builds.
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
    fprintf(stderr, "FAIL test_aes_ccm_small: %s\n", msg);
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

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

/* The RFC 3610 vectors (M = 8, 13-byte nonces; the "cleartext header"
 * octets are the AAD) plus the derived AES-192 reshaping of vector #1. */
static const struct ccm_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *noncehex;
    const char *aadhex;
    const char *pthex;
    const char *cthex;
    const char *taghex;
} ccm_kats[] = {
    { "RFC3610#1", "AES-128-CCM",
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF",
      "00000003020100A0A1A2A3A4A5",
      "0001020304050607",
      "08090A0B0C0D0E0F101112131415161718191A1B1C1D1E",
      "588C979A61C663D2F066D0C2C0F989806D5F6B61DAC384",
      "17E8D12CFDF926E0" },
    { "RFC3610#6", "AES-128-CCM",
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF",
      "00000008070605A0A1A2A3A4A5",
      "000102030405060708090A0B",
      "0C0D0E0F101112131415161718191A1B1C1D1E1F20",
      "6FC1B011F006568B5171A42D953D469B2570A4BD87",
      "405A0443AC91CB94" },
    { "RFC3610#1-192 (derived)", "AES-192-CCM",
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7",
      "00000003020100A0A1A2A3A4A5",
      "0001020304050607",
      "08090A0B0C0D0E0F101112131415161718191A1B1C1D1E",
      "579fb86eddb4a64aae5fe96dbd75440533a9fc3a845736",
      "67aec80ac588ab16" },
};

/* CCM encrypt: ivlen/taglen before the key+nonce init, length call, AAD,
 * single data update, Final, GET_TAG.  Returns ct length; tag gets taglen. */
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

/* CCM decrypt with tag verification.  The tag LENGTH must be configured
 * before the key+nonce init (CCM bakes it into the B0 flags byte), and the
 * expected tag VALUE after it but before the payload update, whose failure
 * is where a tag mismatch is reported.  Returns the recovered length. */
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

/* (1) The RFC 3610 KATs (and the derived AES-192 reshaping of #1). */
static void test_kats(void)
{
    size_t k;
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);

    for (k = 0; k < sizeof(ccm_kats) / sizeof(ccm_kats[0]); k++) {
        const struct ccm_kat *kat = &ccm_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
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
    }
}

/* (2) Roundtrip matrix: taglen {4,8,12,16} x nonce {7,12,13} x payload
 * {0,1,15,16,17} x AAD {0,32}, for AES-128 and AES-256. */
static void test_matrix(void)
{
    static const size_t taglens[] = { 4, 8, 12, 16 };
    static const size_t noncelens[] = { 7, 12, 13 };
    static const size_t ptlens[] = { 0, 1, 15, 16, 17 };
    static const size_t aadlens[] = { 0, 32 };
    static const char *const names[] = { "AES-128-CCM", "AES-256-CCM" };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *badtag = ALLOCATE_TAG(16);
    unsigned char *nonce2 = ALLOCATE_IV(16);
    size_t t, nv, p, a, m, i, ctlen, backlen;
    char msg[160];

    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 3 + 9);
    for (i = 0; i < 32; i++)
        aad[i] = (unsigned char)(i * 11 + 1);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x30 + i);
    for (i = 0; i < 16; i++)
        nonce2[i] = (unsigned char)(0x50 + i);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);
        size_t keylen = (size_t)EVP_CIPHER_get_key_length(cipher);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
        for (i = 0; i < keylen; i++)
            key[i] = (unsigned char)(0xC0 + i);

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
                     names[m], taglen, ivlen, ptlen, aadlen);
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
    }
}

/* (2b) AES-192-CCM subset of the same matrix: taglen {4,12} x nonce
 * {7,12,13} x payload {0,16,17} x AAD {0,32}, each with a roundtrip and a
 * tampered-tag rejection. */
static void test_matrix_192(void)
{
    static const size_t taglens[] = { 4, 12 };
    static const size_t noncelens[] = { 7, 12, 13 };
    static const size_t ptlens[] = { 0, 16, 17 };
    static const size_t aadlens[] = { 0, 32 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-192-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *badtag = ALLOCATE_TAG(16);
    size_t t, nv, p, a, i, keylen, ctlen, backlen;
    char msg[160];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-192-CCM\") failed");
    keylen = (size_t)EVP_CIPHER_get_key_length(cipher);
    CHECK(keylen == 24, "AES-192-CCM key length is not 24");
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)(0xC0 + i);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 3 + 9);
    for (i = 0; i < 32; i++)
        aad[i] = (unsigned char)(i * 11 + 1);
    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x30 + i);

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
                 "AES-192-CCM roundtrip failed (taglen %zu, nonce %zu, "
                 "pt %zu, aad %zu)", taglen, ivlen, ptlen, aadlen);
        expect_equal(msg, back, backlen, pt, ptlen);

        /* A tampered tag must be rejected inside the payload update (see
         * test_matrix for the reasoning). */
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
            CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
                  "payload-length announcement failed");
            if (aadlen)
                CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
                      "AAD EVP_DecryptUpdate failed");
            CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
                  "AES-192-CCM accepted a tampered tag");
            EVP_CIPHER_CTX_free(ctx);
        }
    }
    EVP_CIPHER_free(cipher);
}

/* (3) A second payload update on the same context must fail: the payload
 * length is fixed by the first update.  Runs for AES-128 and AES-192. */
static void test_second_data_update(void)
{
    static const char *const names[] = { "AES-128-CCM", "AES-192-CCM" };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t m, i, keylen;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x40 + i);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i + 1);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-CCM cipher");
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
    }
}

/* (4) AAD-only operation (empty payload) is valid, including decrypt; and a
 * nonzero AAD update before the length announcement must fail.
 *
 * Version note: an empty payload is accepted either delivered through an
 * explicit zero-length data update or left to Final.  On 3.6.x both produce
 * a retrievable tag; on 3.0.x only the explicit update does (Final alone
 * leaves the tag unset), so this test delivers it explicitly. */
static void test_aad_only(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(32);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");
    for (i = 0; i < 16; i++)
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
}

/* (5) Parameter rules: the defaults are ivlen 7 / taglen 12; nonce lengths
 * outside 7..13 and odd or out-of-range tag lengths are rejected. */
static void test_param_rules(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CCM", NULL);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t ivlen = 0, taglen = 0;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CCM\") failed");

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

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_matrix();
    test_matrix_192();
    test_second_data_update();
    test_aad_only();
    test_param_rules();

    printf("test_aes_ccm_small: all %u checks passed\n", g_checks);
    return 0;
}
