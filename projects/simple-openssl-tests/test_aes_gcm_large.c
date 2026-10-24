#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_gcm_large -- AES-128/192/256-GCM with LARGE, multi-update
 * payloads.
 *
 * The oracle for the large payloads is self-consistency: a payload of 2 MiB
 * fed through updates of sizes {16,17,64,100,128,129,3071,3072,3073,65536}
 * must produce exactly the same ciphertext and tag as a single one-shot
 * update of the same 2 MiB (and decrypt must mirror that).  The chunk sizes
 * deliberately straddle the internal GHASH regimes of crypto/modes/gcm128.c:
 * the sub-block "mres" buffer (16/17/64/100/128/129), the AES_GCM_ENC_BYTES =
 * 128 staging kernel (3071/3072/3073), and the GHASH_CHUNK = 3072 bulk path
 * plus whole 65536-byte updates (see gcm128.c around lines 350 and 869).
 *
 * The helpers are anchored to the published McGrew/Viega KATs TC3/TC4 (also
 * embedded in test_aes_gcm_small.c; every hex value was verified against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked on 3.0.13).  The
 * published spec has no AES-192 tuple for the TC3 shape, so the TC3-192
 * anchor below was derived with a scratch probe against pizfix OpenSSL
 * 3.6.5 and cross-checked byte for byte against system OpenSSL 3.0.13.  The
 * same KAT vectors are re-run through the very one-shot and streaming
 * helpers that process the 2 MiB payloads, including one run where the
 * 64-byte KAT plaintext is fed through the large chunk list itself.
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
    fprintf(stderr, "FAIL test_aes_gcm_large: %s\n", msg);
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

/* Total large payload: 2 MiB = 2 * 1024 * 1024 bytes. */
#define LARGE_LEN (2u * 1024u * 1024u)
#define AAD_LARGE_LEN 1024u

/* The update sizes the streaming helpers cycle through. */
static const size_t CHUNKS[] = {
    16, 17, 64, 100, 128, 129, 3071, 3072, 3073, 65536
};

/* McGrew/Viega KAT material (verified; identical to test_aes_gcm_small.c).
 * KEY192_HEX is KEY128 followed by its first 8 bytes. */
static const char *const KEY128_HEX =
    "feffe9928665731c6d6a8f9467308308";
static const char *const KEY192_HEX =
    "feffe9928665731c6d6a8f9467308308feffe9928665731c";
static const char *const KEY256_HEX =
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
static const char *const IV12_HEX =
    "cafebabefacedbaddecaf888";
static const char *const AAD20_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";
static const char *const PT64_HEX =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b391aafd255";
static const char *const PT60_HEX =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b39";
static const char *const TC3_CT_HEX =
    "42831ec2217774244b7221b784d0d49c"
    "e3aa212f2c02a4e035c17e2329aca12e"
    "21d514b25466931c7d8f6a5aac84aa05"
    "1ba30b396a0aac973d58e091473f5985";
static const char *const TC3_TAG_HEX =
    "4d5c2af327cd64a62cf35abd2ba6fab4";
static const char *const TC4_TAG_HEX =
    "5bc94fbc3221a5db94fae95ae7121a47";
/* TC3 shape with the 24-byte KEY192_HEX key: no published McGrew/Viega or
 * CAVS counterpart exists; derived with a scratch probe against pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against system 3.0.13. */
static const char *const TC3_192_CT_HEX =
    "3980ca0b3c00e841eb06fac4872a2757"
    "859e1ceaa6efd984628593b40ca1e19c"
    "7d773d00c144c525ac619d18c84a3f47"
    "18e2448b2fe324d9ccda2710acade256";
static const char *const TC3_192_TAG_HEX =
    "9924a7c8587336bfb118024db8674a14";

/* One-shot GCM encrypt: one init, one AAD update (out == NULL), one data
 * update, Final, GET_TAG.  `out` must have room for inlen + 16. */
static void gcm_encrypt_one_shot(const EVP_CIPHER *cipher, const unsigned char *key,
                                 const unsigned char *iv, size_t ivlen,
                                 const unsigned char *aad, size_t aadlen,
                                 const unsigned char *in, size_t inlen,
                                 unsigned char *out, size_t outcap,
                                 size_t *outlen, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, dlen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    if (inlen)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_EncryptUpdate failed");
    dlen = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "one-shot EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM encrypt: AAD first, then the payload in CHUNKS-sized updates
 * (last one truncated), Final, GET_TAG. */
static void gcm_encrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *iv, size_t ivlen,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, dlen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    while (off < inlen) {
        size_t n = CHUNKS[ci++ % (sizeof(CHUNKS) / sizeof(CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_EncryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(off == inlen, "streaming updates did not cover the whole payload");
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "streaming EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM decrypt with tag verification: AAD first, then CHUNKS-sized
 * updates, SET_TAG, Final. */
static void gcm_decrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *iv, size_t ivlen,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    size_t off = 0, ci = 0;
    int len = 0, dlen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    while (off < inlen) {
        size_t n = CHUNKS[ci++ % (sizeof(CHUNKS) / sizeof(CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_DecryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_DecryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(off == inlen, "streaming updates did not cover the whole payload");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "streaming EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Fills the large buffer with a deterministic pattern (no KAT dependence). */
static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 31 + (i >> 9) * 7 + 11) & 0xff);
}

/* (1) The one-shot and streaming helpers agree with the McGrew/Viega KATs,
 * both one-shot and with the 64-byte TC3 plaintext pushed through the large
 * chunk list. */
static void test_kat_anchor(void)
{
    static const struct {
        const char *label;
        const char *ciphername;
        const char *keyhex;
        const char *pthex;
        const char *aadhex; /* "" means no AAD */
        const char *cthex;
        const char *taghex;
    } kats[] = {
        { "TC3", "AES-128-GCM", KEY128_HEX, PT64_HEX, "", TC3_CT_HEX, TC3_TAG_HEX },
        { "TC4", "AES-128-GCM", KEY128_HEX, PT60_HEX, AAD20_HEX, NULL, TC4_TAG_HEX },
        { "TC4-256", "AES-256-GCM", KEY256_HEX, PT60_HEX, AAD20_HEX, NULL, NULL },
        { "TC3-192 (derived)", "AES-192-GCM", KEY192_HEX, PT64_HEX, "",
          TC3_192_CT_HEX, TC3_192_TAG_HEX },
    };
    size_t k;
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);

    for (k = 0; k < sizeof(kats) / sizeof(kats[0]); k++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kats[k].ciphername, NULL);
        size_t ptlen, aadlen, ctlen, ct2len;
        char msg[128];

        /* AES-GCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(kats[k].keyhex, key, 32);
        from_hex(IV12_HEX, iv, 16);
        ptlen = from_hex(kats[k].pthex, pt, 64);
        aadlen = from_hex(kats[k].aadhex, aad, 20);

        gcm_encrypt_one_shot(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                             ct, 64 + 16, &ctlen, tag);
        gcm_encrypt_streaming(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                              ct2, 64 + 16, &ct2len, tag2);
        snprintf(msg, sizeof(msg), "%s: streaming ciphertext differs from one-shot",
                 kats[k].label);
        expect_equal(msg, ct2, ct2len, ct, ctlen);
        snprintf(msg, sizeof(msg), "%s: streaming tag differs from one-shot tag",
                 kats[k].label);
        expect_equal(msg, tag2, 16, tag, 16);

        /* The streaming ciphertext of the full 64-byte TC3 vector with the
         * published ciphertext embedded above must match it exactly. */
        if (kats[k].cthex != NULL) {
            unsigned char *ct_want = ALLOCATE_OUTPUT(64);
            size_t ct_wantlen = from_hex(kats[k].cthex, ct_want, 64);
            snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                     kats[k].label);
            expect_equal(msg, ct2, ct2len, ct_want, ct_wantlen);
            /* ... and the same vector fed through the LARGE chunk list
             * (single 64-byte chunk, but through the same code path). */
            gcm_encrypt_streaming(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                                  ct2, 64 + 16, &ct2len, tag2);
            expect_equal(msg, ct2, ct2len, ct_want, ct_wantlen);
        }
        if (kats[k].taghex != NULL) {
            size_t tag_wantlen = from_hex(kats[k].taghex, tag_want, 16);
            snprintf(msg, sizeof(msg), "%s: tag does not match the KAT",
                     kats[k].label);
            expect_equal(msg, tag2, 16, tag_want, tag_wantlen);
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (2) 2 MiB one-shot == streaming, for AES-128, AES-192 and AES-256, encrypt
 * and decrypt.  The 2 MiB payload is deterministic but arbitrary. */
static void test_large_stream_equals_one_shot(void)
{
    static const struct {
        const char *name;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM", KEY128_HEX },
        { "AES-192-GCM", KEY192_HEX },
        { "AES-256-GCM", KEY256_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    size_t m;

    fill_pattern(pt, LARGE_LEN);
    from_hex(IV12_HEX, iv, 16);

    for (m = 0; m < sizeof(cases) / sizeof(cases[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[m].name, NULL);
        size_t ct1len, ct2len, backlen;
        char msg[128];

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(cases[m].keyhex, key, 32);

        gcm_encrypt_one_shot(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                             ct1, LARGE_LEN + 16, &ct1len, tag1);
        CHECK(ct1len == LARGE_LEN, "one-shot GCM ciphertext length is not inlen");
        gcm_encrypt_streaming(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                              ct2, LARGE_LEN + 16, &ct2len, tag2);
        snprintf(msg, sizeof(msg),
                 "%s: streaming 2MiB ciphertext differs from one-shot",
                 cases[m].name);
        expect_equal(msg, ct2, ct2len, ct1, ct1len);
        snprintf(msg, sizeof(msg), "%s: streaming 2MiB tag differs from one-shot",
                 cases[m].name);
        expect_equal(msg, tag2, 16, tag1, 16);

        /* Decrypt the streaming ciphertext back in one shot and in chunks;
         * both must reproduce the 2 MiB plaintext. */
        gcm_decrypt_streaming(cipher, key, iv, 12, NULL, 0, ct2, ct2len,
                              back, LARGE_LEN + 16, &backlen, tag2);
        CHECK(backlen == LARGE_LEN, "streaming decrypt length is not inlen");
        snprintf(msg, sizeof(msg),
                 "%s: streaming decrypt of 2MiB did not recover the plaintext",
                 cases[m].name);
        CHECK(memcmp(back, pt, LARGE_LEN) == 0, msg);
        EVP_CIPHER_free(cipher);
    }
}

/* (3) Same as (2) but with a 1 KiB AAD fed before the data; AAD must be
 * counted identically by both update strategies. */
static void test_large_with_aad(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LARGE_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    size_t ct1len, ct2len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LARGE_LEN; i++)
        aad[i] = (unsigned char)(i * 13 + 5);

    gcm_encrypt_one_shot(cipher, key, iv, 12, aad, AAD_LARGE_LEN, pt, LARGE_LEN,
                         ct1, LARGE_LEN + 16, &ct1len, tag1);
    gcm_encrypt_streaming(cipher, key, iv, 12, aad, AAD_LARGE_LEN, pt, LARGE_LEN,
                          ct2, LARGE_LEN + 16, &ct2len, tag2);
    expect_equal("2MiB streaming ciphertext with AAD differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("2MiB streaming tag with AAD differs from one-shot tag",
                 tag2, 16, tag1, 16);
    EVP_CIPHER_free(cipher);
}

/* (4) AAD updates interleaved between data updates must fail (GCM finalizes
 * the GHASH of the AAD on the first data update; crypto/modes/gcm128.c
 * rejects later AAD).  An AAD update *before* any data is still legal and is
 * checked first so the failure is attributable to the interleaving. */
static void test_aad_interleaved(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(8192);
    unsigned char *ct = ALLOCATE_OUTPUT(8192 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    fill_pattern(pt, sizeof(pt));
    for (i = 0; i < 64; i++)
        aad[i] = (unsigned char)(i + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 64) == 1,
          "AAD update before any data failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 4096) == 1,
          "first data update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 4096, 4096) == 1,
          "second data update failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 64) != 1,
          "AAD update accepted after data updates");
    EVP_CIPHER_CTX_free(ctx);

    /* Same for the decryption direction. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, pt, 16) == 1,
          "decrypt-side data update failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 64) != 1,
          "decrypt-side AAD update accepted after data");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (5) IV lengths 8/12/16 must round-trip the full 2 MiB payload (J0
 * computation differs for non-12-byte IVs; gcm128.c GHASHes long IVs). */
static void test_iv_lengths_large(void)
{
    static const size_t ivlens[] = { 8, 12, 16 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t ctlen, backlen;
    size_t v, i;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < 16; i++)
        iv[i] = (unsigned char)(0x20 + i);

    for (v = 0; v < sizeof(ivlens) / sizeof(ivlens[0]); v++) {
        gcm_encrypt_one_shot(cipher, key, iv, ivlens[v], NULL, 0, pt, LARGE_LEN,
                             ct, LARGE_LEN + 16, &ctlen, tag);
        CHECK(ctlen == LARGE_LEN, "large encrypt with short IV lost bytes");
        gcm_decrypt_streaming(cipher, key, iv, ivlens[v], NULL, 0, ct, ctlen,
                              back, LARGE_LEN + 16, &backlen, tag);
        snprintf(msg, sizeof(msg),
                 "IV length %zu roundtrip of 2MiB did not recover the plaintext",
                 ivlens[v]);
        CHECK(backlen == LARGE_LEN && memcmp(back, pt, LARGE_LEN) == 0, msg);
    }
    EVP_CIPHER_free(cipher);
}

/* (6) Flipping any bit of the final ciphertext block must be detected by
 * DecryptFinal (the tag covers every ciphertext block via GHASH) -- for
 * AES-128 and AES-192. */
static void test_tampered_final_block(void)
{
    static const struct {
        const char *name;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM", KEY128_HEX },
        { "AES-192-GCM", KEY192_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    size_t c, ctlen;
    int len;

    from_hex(IV12_HEX, iv, 16);
    fill_pattern(pt, LARGE_LEN);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].name, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
        from_hex(cases[c].keyhex, key, 32);
        gcm_encrypt_one_shot(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                             ct, LARGE_LEN + 16, &ctlen, tag);
        CHECK(ctlen == LARGE_LEN, "unexpected one-shot ciphertext length");

        /* Tamper with the first byte of the final 16-byte block. */
        ct[ctlen - 16] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "EVP_DecryptUpdate failed on tampered data (it may emit plaintext)");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
              "decryption accepted a tampered final ciphertext block");
        EVP_CIPHER_CTX_free(ctx);

        /* And the last byte of the whole ciphertext. */
        ct[ctlen - 1] ^= 0x80;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "EVP_DecryptUpdate failed on tampered data (it may emit plaintext)");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
              "decryption accepted a tampered last ciphertext byte");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kat_anchor();
    test_large_stream_equals_one_shot();
    test_large_with_aad();
    test_aad_interleaved();
    test_iv_lengths_large();
    test_tampered_final_block();

    printf("test_aes_gcm_large: all %u checks passed\n", g_checks);
    return 0;
}
