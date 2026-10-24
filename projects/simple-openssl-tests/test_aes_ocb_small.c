#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aes_ocb_small -- AES-128/192/256-OCB known-answer and behavior tests
 * for small payloads.  (A sibling program covers a >= 1 MiB payload.)
 *
 * Rules exercised here (verified against crypto/modes/ocb128.c and
 * providers/implementations/ciphers/cipher_aes_ocb.c):
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
 *     is replayed in the same order.  This program asserts that observed
 *     behavior rather than a rejection.
 *
 * Known-answer vectors: RFC 7253 Appendix A sample tuples for AES-128 (all
 * verified through this EVP flow against the authoritative pizfix OpenSSL
 * 3.6.5 and cross-checked on 3.0.13), the RFC's 96-bit-tag tuple with the
 * second key, and AES-256/AES-192 tuples derived with a scratch probe (the
 * RFC publishes sample results only for 128-bit keys); the derived tuples
 * were cross-checked byte for byte against the system OpenSSL 3.0.13.
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
    fprintf(stderr, "FAIL test_aes_ocb_small: %s\n", msg);
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

/* RFC 7253 Appendix A.  The nonce increments per tuple; A/P are 00.. sequences. */
static const char *const OCB_KEY128_HEX =
    "000102030405060708090A0B0C0D0E0F";
static const char *const OCB_KEY128B_HEX =  /* the 96-bit-tag tuple's key */
    "0F0E0D0C0B0A09080706050403020100";
static const char *const OCB_KEY192_HEX =   /* derived tuple's key */
    "000102030405060708090A0B0C0D0E0F1011121314151617";
static const char *const OCB_KEY256_HEX =   /* derived tuple's key */
    "0F0E0D0C0B0A090807060504030201000102030405060708090A0B0C0D0E0F10";

static const struct ocb_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *noncehex;
    const char *aadhex;  /* "" means empty AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;   /* "" means tag-only tuple */
    const char *taghex;
} ocb_kats[] = {
    { "A.1 #1", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221100", "", "", "",
      "785407BFFFC8AD9EDCC5520AC9111EE6" },
    { "A.1 #2", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221101",
      "0001020304050607", "0001020304050607",
      "6820B3657B6F615A",
      "5725BDA0D3B4EB3A257C9AF1F8F03009" },
    { "A.1 #3", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221102",
      "0001020304050607", "",
      "",
      "81017F8203F081277152FADE694A0A00" },
    { "A.1 #4", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221103",
      "", "0001020304050607",
      "45DD69F8F5AAE724",
      "14054CD1F35D82760B2CD00D2F99BFA9" },
    { "A.1 #5", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221104",
      "000102030405060708090A0B0C0D0E0F",
      "000102030405060708090A0B0C0D0E0F",
      "571D535B60B277188BE5147170A9A22C",
      "3AD7A4FF3835B8C5701C1CCEC8FC3358" },
    { "A.1 #13", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA9988776655443322110D",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "D5CA91748410C1751FF8A2F618255B68A0A12E093FF45460"
      "6E59F9C1D0DDC54B65E8628E568BAD7A",
      "ED07BA06A4A69483A7035490C5769E60" },
    { "A.2 96-bit tag", "AES-128-OCB", OCB_KEY128B_HEX,
      "BBAA9988776655443322110D",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "1792A4E31E0755FB03E31B22116E6C2DDF9EFD6E33D536F1"
      "A0124B0A55BAE884ED93481529C76B6A",
      "D0C515F4D1CDD4FDAC4F02AA" },
    { "AES-256 (derived)", "AES-256-OCB", OCB_KEY256_HEX,
      "BBAA9988776655443322110D",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "B2AA438B428CDAC8AA35BDFE9D5ACF7A81A1524755A8DC8F"
      "A37F51CE2AF20CD6E706BF734678481C",
      "2777FC08E154387CD69217463EB31781" },
    { "AES-192 (derived)", "AES-192-OCB", OCB_KEY192_HEX,
      "BBAA9988776655443322110D",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
      "2021222324252627",
      "86b418c98b14be157cad07cf20189dc8"
      "7a9e6fd19a07bec6d6acef811d9a860c"
      "9671666bf650dd2b",
      "6e9ca93fc80b9dcf1b25ae2fda9b2520" },
};

/* One-shot OCB encrypt: ivlen/taglen before the key+nonce init, AAD first
 * (out == NULL), data, Final, GET_TAG.  `out` needs inlen + 16 bytes. */
static void ocb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
static void ocb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
static void ocb_encrypt_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *nonce, size_t ivlen,
                               unsigned char *tag, size_t taglen,
                               const unsigned char *aad, size_t aadlen,
                               const size_t *aadchunks, size_t naadchunks,
                               const unsigned char *pt, size_t ptlen,
                               const size_t *ptchunks, size_t nptchunks,
                               unsigned char *out, size_t outcap,
                               size_t *outlen)
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

/* (1) RFC 7253 KATs through the one-shot helpers. */
static void test_kats(void)
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

    for (k = 0; k < sizeof(ocb_kats) / sizeof(ocb_kats[0]); k++) {
        const struct ocb_kat *kat = &ocb_kats[k];
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
}

/* (2) IV length rules: the default is 12; 1..15 round-trip; 0 and 16 are
 * rejected. */
static void test_iv_lengths(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-OCB", NULL);
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

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-OCB\") failed");
    from_hex(OCB_KEY128_HEX, key, 16);
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

    /* 1, 12 and 15 round-trip, for AES-128 and AES-192. */
    {
        static const struct {
            const char *ciphername;
            const char *keyhex;
        } ivcases[] = {
            { "AES-128-OCB", OCB_KEY128_HEX },
            { "AES-192-OCB", OCB_KEY192_HEX },
        };
        size_t c;

        for (c = 0; c < sizeof(ivcases) / sizeof(ivcases[0]); c++) {
            EVP_CIPHER *civ = EVP_CIPHER_fetch(NULL, ivcases[c].ciphername,
                                               NULL);

            CHECK(civ != NULL,
                  "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
            from_hex(ivcases[c].keyhex, key, 32);
            for (v = 0; v < 3; v++) {
                size_t len = (v == 0) ? 1 : (v == 1) ? 12 : 15;
                ocb_encrypt(civ, key, nonce, len, tag, 16, aad, 16, pt, 32,
                            ct, 32 + 16, &ctlen);
                ocb_decrypt(civ, key, nonce, len, tag, 16, aad, 16, ct, ctlen,
                            back, 32 + 16, &backlen);
                snprintf(msg, sizeof(msg),
                         "IV length %zu roundtrip did not recover the plaintext",
                         len);
                expect_equal(msg, back, backlen, pt, 32);
            }
            EVP_CIPHER_free(civ);
        }
    }
    EVP_CIPHER_free(cipher);
}

/* (3) Tag length rules: 1..16 round-trip; 0 and 17 are rejected. */
static void test_tag_lengths(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-OCB", NULL);
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

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-OCB\") failed");
    from_hex(OCB_KEY128_HEX, key, 16);
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

    /* Tag length 0 is accepted at set time (OCB_MAX_TAG_LEN check only),
     * but the operation must fail (CRYPTO_ocb128_setiv requires 1..16).
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

    /* AES-192 subset of the same sweep: tag lengths {1,8,12,16} round-trip. */
    {
        EVP_CIPHER *c192 = EVP_CIPHER_fetch(NULL, "AES-192-OCB", NULL);

        CHECK(c192 != NULL, "EVP_CIPHER_fetch(\"AES-192-OCB\") failed");
        from_hex(OCB_KEY192_HEX, key, 32);
        for (t = 0; t < 4; t++) {
            size_t taglen = (t == 0) ? 1 : (t == 1) ? 8 : (t == 2) ? 12 : 16;

            ocb_encrypt(c192, key, nonce, 12, tag, taglen, aad, 16, pt, 32,
                        ct, 32 + 16, &ctlen);
            ocb_decrypt(c192, key, nonce, 12, tag, taglen, aad, 16, ct, ctlen,
                        back, 32 + 16, &backlen);
            snprintf(msg, sizeof(msg),
                     "AES-192-OCB tag length %zu roundtrip did not recover "
                     "the plaintext", taglen);
            expect_equal(msg, back, backlen, pt, 32);
        }
        EVP_CIPHER_free(c192);
    }
    EVP_CIPHER_free(cipher);
}

/* (4) Partial final blocks: {15} in one update, {16,7} across two updates,
 * and AAD {20,5} across two updates -- all must equal the one-shot result
 * (the provider buffers the partial block and flushes it at Final).  Runs
 * for AES-128 and AES-192. */
static void test_partial_blocks(void)
{
    static const size_t one15[] = { 15 };
    static const size_t mix[] = { 16, 7 };
    static const size_t aadchunks[] = { 20, 5 };
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-OCB", OCB_KEY128_HEX },
        { "AES-192-OCB", OCB_KEY192_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(25);
    unsigned char *pt = ALLOCATE_INPUT(23);
    unsigned char *ct1 = ALLOCATE_OUTPUT(23 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(23 + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    size_t c, ct1len, ct2len, i;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x60 + i);
    for (i = 0; i < 25; i++)
        aad[i] = (unsigned char)(i * 7 + 1);
    for (i = 0; i < 23; i++)
        pt[i] = (unsigned char)(i * 3 + 2);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
        from_hex(cases[c].keyhex, key, 32);

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
    }
}

/* (5) AAD after the first data update: the provider ACCEPTS it (unlike
 * GCM, whose gcm128.c rejects late AAD), because the OCB AAD hash chain is
 * independent of the data checksum.  Assert the observed behavior: the
 * update succeeds and the result decrypts when the AAD is replayed in the
 * same order. */
static void test_aad_after_data(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-OCB", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
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

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-OCB\") failed");
    from_hex(OCB_KEY128_HEX, key, 16);
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
}

/* (6) Tamper detection: flipping a ciphertext bit or a tag bit must fail
 * DecryptFinal -- for AES-128 and AES-192. */
static void test_tamper(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-OCB", OCB_KEY128_HEX },
        { "AES-192-OCB", OCB_KEY192_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char tag_bad[16];
    EVP_CIPHER_CTX *ctx;
    int len, flen = 0;
    size_t c, ctlen, i;

    for (i = 0; i < 16; i++)
        nonce[i] = (unsigned char)(0x80 + i);
    for (i = 0; i < 16; i++)
        aad[i] = (unsigned char)(i + 9);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 17 + 1);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-OCB cipher");
        from_hex(cases[c].keyhex, key, 32);

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
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_iv_lengths();
    test_tag_lengths();
    test_partial_blocks();
    test_aad_after_data();
    test_tamper();

    printf("test_aes_ocb_small: all %u checks passed\n", g_checks);
    return 0;
}
