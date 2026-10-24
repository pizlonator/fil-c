#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_gcm_siv -- RFC 8452 AES-GCM-SIV tests.
 *
 * AES-*-GCM-SIV was added in OpenSSL 3.5, so on older builds (e.g. 3.0.13)
 * the fetch fails and the program takes the documented SKIP path.  On a
 * build that has it, the behavior tested here is (providers/implementations/
 * ciphers/cipher_aes_gcm_siv.c / _hw.c):
 *
 *   - the nonce is exactly 12 bytes and there is no IV-length knob;
 *   - AAD updates (out == NULL) may be split arbitrarily (the AAD is
 *     accumulated and zero-padded to a 16-byte boundary internally);
 *   - the payload is one-shot per init: used_enc/used_dec make a second
 *     data update fail;
 *   - the 16-byte tag is retrieved via GET_TAG after encrypting and set via
 *     SET_TAG before decrypting; the verification happens at DecryptFinal
 *     (the CTR step already ran during the update).
 *
 * Known-answer vectors: RFC 8452 Appendix C.1 (AES-128) and C.2 (AES-256).
 * RFC 8452 defines the algorithm only for 128- and 256-bit keys and
 * publishes no AES-192 vectors, so the two "192 (derived)" rows were
 * derived with a scratch probe against the authoritative pizfix OpenSSL
 * 3.6.5 (via build/bin/filcc); there is nothing to cross-check on 3.0.13,
 * which lacks the algorithm entirely.  All rows were verified by running
 * them through this exact EVP flow.
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
    fprintf(stderr, "FAIL test_aes_gcm_siv: %s\n", msg);
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

static const char *const NONCE_HEX =
    "030000000000000000000000";

/* RFC 8452 key convention: 01 followed by zero bytes up to the key size.
 * The 192-bit variant is this file's own extension (no RFC vectors). */
static const char *const KEY192_GSIV_HEX =
    "010000000000000000000000000000000000000000000000";

/* RFC 8452 Appendix C vectors (Key = 01 00..00 pattern of the key length). */
static const struct gsiv_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *aadhex;  /* "" means empty AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;   /* "" means tag-only vector */
    const char *taghex;
} gsiv_kats[] = {
    { "C.1 #1", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "", "", "",
      "DC20E2D83F25705BB49E439ECA56DE25" },
    { "C.1 #4", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "",
      "01000000000000000000000000000000",
      "743F7C8077AB25F8624E2E948579CF77",
      "303AAF90F6FE21199C6068577437A0C4" },
    { "C.1 4B+12AAD", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "010000000000000000000000",
      "02000000",
      "A8FE3E87",
      "07EB1F84FB28F8CB73DE8E99E2F48A14" },
    { "C.1 20B+18AAD", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "010000000000000000000000000000000200",
      "0300000000000000000000000000000004000000",
      "6BB0FECF5DED9B77F902C7D5DA236A4391DD0297",
      "24AFC9805E976F451E6D87F6FE106514" },
    { "C.2 4B+12AAD", "AES-256-GCM-SIV",
      "01000000000000000000000000000000"
      "00000000000000000000000000000000",
      "010000000000000000000000",
      "02000000",
      "22B3F4CD",
      "1835E517741DFDDCCFA07FA4661B74CF" },
    { "C.2 #4", "AES-256-GCM-SIV",
      "01000000000000000000000000000000"
      "00000000000000000000000000000000",
      "",
      "01000000000000000000000000000000",
      "85A01B63025BA19B7FD3DDFC033B3E76",
      "C9EAC6FA700942702E90862383C6C366" },
    { "192 4B+12AAD (derived)", "AES-192-GCM-SIV", KEY192_GSIV_HEX,
      "010000000000000000000000",
      "02000000",
      "7236a9d5",
      "9fca93a4e49ce098ad646bf0f408bade" },
    { "192 16B (derived)", "AES-192-GCM-SIV", KEY192_GSIV_HEX,
      "",
      "01000000000000000000000000000000",
      "75a96f1f1cbfa93e2cd69e8a18bf3bab",
      "82e6a81be803dc33f56a637fcaa70fec" },
};

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

/* (1) RFC 8452 Appendix C KATs. */
static void test_kats(void)
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

    for (k = 0; k < sizeof(gsiv_kats) / sizeof(gsiv_kats[0]); k++) {
        const struct gsiv_kat *kat = &gsiv_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        size_t aadlen, ptlen, ct_wantlen, ctlen, backlen;

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(NONCE_HEX, nonce, 16);
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
}

/* (2) AAD in two chunks must equal one-shot AAD, for AES-128 and AES-192. */
static void test_aad_chunking(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM-SIV", gsiv_kats[0].keyhex },
        { "AES-192-GCM-SIV", KEY192_GSIV_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(40);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct1 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t c, i, ct1len, ct2len;

    for (i = 0; i < 40; i++)
        aad[i] = (unsigned char)(i * 3 + 1);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 7 + 5);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(cases[c].keyhex, key, 32);
        from_hex(NONCE_HEX, nonce, 16);

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
    }
}

/* (3) Payload roundtrips for sizes {0,16,17,64}, for AES-192 and AES-256. */
static void test_roundtrip_sizes(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-192-GCM-SIV", KEY192_GSIV_HEX },
        { "AES-256-GCM-SIV", gsiv_kats[4].keyhex },
    };
    static const size_t sizes[] = { 0, 16, 17, 64 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(9);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t s, c, i, ctlen, backlen;
    char msg[128];

    from_hex(NONCE_HEX, nonce, 16);
    for (i = 0; i < 9; i++)
        aad[i] = (unsigned char)(i + 2);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 11 + 3);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(cases[c].keyhex, key, 32);

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
    }
}

/* (4) One payload update per init: a second data update must fail
 * (used_enc / used_dec in cipher_aes_gcm_siv_hw.c), for AES-128 and
 * AES-192. */
static void test_second_update_fails(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM-SIV", gsiv_kats[0].keyhex },
        { "AES-192-GCM-SIV", KEY192_GSIV_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t c, i;

    from_hex(NONCE_HEX, nonce, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 1);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(cases[c].keyhex, key, 32);

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
    }
}

/* (5) Tamper detection: a flipped ciphertext byte or tag bit must fail
 * DecryptFinal -- for AES-128 and AES-192. */
static void test_tamper(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM-SIV", gsiv_kats[0].keyhex },
        { "AES-192-GCM-SIV", KEY192_GSIV_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t c, i, ctlen;

    from_hex(NONCE_HEX, nonce, 16);
    for (i = 0; i < 12; i++)
        aad[i] = (unsigned char)(i + 1);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for an available AES-GCM-SIV cipher");
        from_hex(cases[c].keyhex, key, 32);

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
    }
}

int main(int argc, char **argv)
{
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, "AES-128-GCM-SIV", NULL);
    EVP_CIPHER *probe192 = EVP_CIPHER_fetch(NULL, "AES-192-GCM-SIV", NULL);

    poison_parse_args(argc, argv);

    if (probe == NULL || probe192 == NULL) {
        /* AES-GCM-SIV is version-gated (added in OpenSSL 3.5). */
        printf("SKIP test_aes_gcm_siv: %s is unavailable\n",
               probe == NULL ? "AES-128-GCM-SIV" : "AES-192-GCM-SIV");
        return 0;
    }
    EVP_CIPHER_free(probe);
    EVP_CIPHER_free(probe192);

    test_kats();
    test_aad_chunking();
    test_roundtrip_sizes();
    test_second_update_fails();
    test_tamper();

    printf("test_aes_gcm_siv: all %u checks passed\n", g_checks);
    return 0;
}
