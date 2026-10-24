#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aes_gcm_small -- AES-128/192/256-GCM known-answer and behavior tests
 * for small inputs.  (A sibling program will cover large, multi-block-payload
 * sizes; this one keeps every payload <= 64 bytes.)
 *
 * Known-answer vectors: the classic McGrew/Viega GCM spec test cases, as
 * distributed in OpenSSL's own CAVS data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt in the OpenSSL
 * source).  Naming here follows that spec:
 *
 *   TC1:  zero key, zero IV, empty plaintext, no AAD -> tag only
 *   TC2:  zero key, zero IV, one 16-byte zero block
 *   TC3:  key feffe9..08, IV cafe..888, 64-byte plaintext, no AAD
 *   TC4:  same key/IV, 60-byte plaintext, AAD feedface..d2
 *   TC15: TC3 with an AES-256 key
 *   TC16: TC4 with an AES-256 key
 *
 * The McGrew/Viega spec and the CAVS data publish no AES-192 tuple for the
 * TC3/TC4 key/IV/plaintext shapes, so the "TC3-192 (derived)" and
 * "TC4-192 (derived)" rows were derived with a scratch probe against the
 * authoritative pizfix OpenSSL 3.6.5 (via build/bin/filcc) and cross-checked
 * byte for byte against the system OpenSSL 3.0.13.
 *
 * Every hex value below was verified by running the vector through the
 * EVP API compiled against the authoritative pizfix OpenSSL 3.6.5 (via
 * build/bin/filcc) and cross-checked against the system OpenSSL 3.0.13.
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
    fprintf(stderr, "FAIL test_aes_gcm_small: %s\n", msg);
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

/* Shared KAT material.  TC1's key and IV are all zero bytes. */
static const char *const KEY128_HEX =
    "feffe9928665731c6d6a8f9467308308";
static const char *const KEY192_HEX =   /* KEY128 followed by its first 8 bytes */
    "feffe9928665731c6d6a8f9467308308feffe9928665731c";
static const char *const KEY256_HEX =
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
static const char *const IV12_HEX =
    "cafebabefacedbaddecaf888";
static const char *const AAD20_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";
static const char *const PT60_HEX =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b39";
static const char *const PT64_HEX =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b391aafd255";

static const struct gcm_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *aadhex;  /* "" means no AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;
    const char *taghex;
} gcm_kats[] = {
    { "TC1", "AES-128-GCM",
      "00000000000000000000000000000000",
      "000000000000000000000000",
      "", "",
      "",
      "58e2fccefa7e3061367f1d57a4e7455a" },
    { "TC2", "AES-128-GCM",
      "00000000000000000000000000000000",
      "000000000000000000000000",
      "",
      "00000000000000000000000000000000",
      "0388dace60b6a392f328c2b971b2fe78",
      "ab6e47d42cec13bdf53a67b21257bddf" },
    { "TC3", "AES-128-GCM",
      "feffe9928665731c6d6a8f9467308308",
      "cafebabefacedbaddecaf888",
      "", PT64_HEX,
      "42831ec2217774244b7221b784d0d49c"
      "e3aa212f2c02a4e035c17e2329aca12e"
      "21d514b25466931c7d8f6a5aac84aa05"
      "1ba30b396a0aac973d58e091473f5985",
      "4d5c2af327cd64a62cf35abd2ba6fab4" },
    { "TC4", "AES-128-GCM",
      "feffe9928665731c6d6a8f9467308308",
      "cafebabefacedbaddecaf888",
      AAD20_HEX, PT60_HEX,
      "42831ec2217774244b7221b784d0d49c"
      "e3aa212f2c02a4e035c17e2329aca12e"
      "21d514b25466931c7d8f6a5aac84aa05"
      "1ba30b396a0aac973d58e091",
      "5bc94fbc3221a5db94fae95ae7121a47" },
    { "TC15", "AES-256-GCM", KEY256_HEX, IV12_HEX, "", PT64_HEX,
      "522dc1f099567d07f47f37a32a84427d"
      "643a8cdcbfe5c0c97598a2bd2555d1aa"
      "8cb08e48590dbb3da7b08b1056828838"
      "c5f61e6393ba7a0abcc9f662898015ad",
      "b094dac5d93471bdec1a502270e3cc6c" },
    { "TC16", "AES-256-GCM", KEY256_HEX, IV12_HEX, AAD20_HEX, PT60_HEX,
      "522dc1f099567d07f47f37a32a84427d"
      "643a8cdcbfe5c0c97598a2bd2555d1aa"
      "8cb08e48590dbb3da7b08b1056828838"
      "c5f61e6393ba7a0abcc9f662",
      "76fc6ece0f4e1768cddf8853bb2d551b" },
    { "TC3-192 (derived)", "AES-192-GCM", KEY192_HEX, IV12_HEX, "", PT64_HEX,
      "3980ca0b3c00e841eb06fac4872a2757"
      "859e1ceaa6efd984628593b40ca1e19c"
      "7d773d00c144c525ac619d18c84a3f47"
      "18e2448b2fe324d9ccda2710acade256",
      "9924a7c8587336bfb118024db8674a14" },
    { "TC4-192 (derived)", "AES-192-GCM", KEY192_HEX, IV12_HEX, AAD20_HEX, PT60_HEX,
      "3980ca0b3c00e841eb06fac4872a2757"
      "859e1ceaa6efd984628593b40ca1e19c"
      "7d773d00c144c525ac619d18c84a3f47"
      "18e2448b2fe324d9ccda2710",
      "2519498e80f1478f37ba55bd6d27618c" },
};

/* One-shot GCM encrypt.  AAD is fed first with out == NULL, then the
 * plaintext, then Final, then the tag is read out.  The IV length is set
 * with EVP_CTRL_AEAD_SET_IVLEN *before* the init call that passes the IV --
 * otherwise OpenSSL would copy the default 12 bytes out of the caller's
 * buffer regardless of the intended length. */
static void gcm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        unsigned char *tag, size_t taglen)
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
    if (inlen) {
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_EncryptUpdate failed");
        dlen = len;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* One-shot GCM decrypt with tag verification. */
static void gcm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        const unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
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
    if (inlen) {
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_DecryptUpdate failed");
        dlen = len;
    }
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM encrypt: feed the plaintext in updates of {5,5,6,16,32}
 * bytes (last one truncated).  Must reproduce the one-shot result exactly. */
static void gcm_stream_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *iv, size_t ivlen,
                               const unsigned char *aad, size_t aadlen,
                               const unsigned char *in, size_t inlen,
                               unsigned char *out, size_t outcap,
                               size_t *outlen, unsigned char *tag, size_t taglen)
{
    static const size_t chunks[] = { 5, 5, 6, 16, 32 };
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
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in streaming test");
        dlen += len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed in streaming test");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* (1) One-shot encrypt == KAT for all six vectors, including AAD ordering
 * (AAD first with out == NULL, then data, then Final, then GET_TAG). */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(gcm_kats) / sizeof(gcm_kats[0]); k++) {
        const struct gcm_kat *kat = &gcm_kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *aad = ALLOCATE_AAD(20);
        unsigned char *pt = ALLOCATE_INPUT(64);
        unsigned char *ct_want = ALLOCATE_OUTPUT(64);
        unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
        unsigned char *tag_want = ALLOCATE_TAG(16);
        unsigned char *tag = ALLOCATE_TAG(16);
        size_t ptlen, aadlen, ct_wantlen, ctlen, tag_wantlen;
        char msg[128];

        /* AES-GCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(kat->ivhex, iv, 16);
        ptlen = from_hex(kat->pthex, pt, 64);
        aadlen = from_hex(kat->aadhex, aad, 20);
        ct_wantlen = from_hex(kat->cthex, ct_want, 64);
        tag_wantlen = from_hex(kat->taghex, tag_want, 16);

        gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen, ct, 64 + 16,
                    &ctlen, tag, 16);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        snprintf(msg, sizeof(msg), "%s: tag does not match the KAT", kat->label);
        expect_equal(msg, tag, 16, tag_want, tag_wantlen);
        EVP_CIPHER_free(cipher);
    }
}

/* (2) Decrypt with the right tag succeeds and recovers the plaintext;
 * with a tampered tag DecryptFinal must fail.  Runs for AES-128 and
 * AES-192 (AES-256 shares the same code path through the KAT table). */
static void test_decrypt_and_tamper(void)
{
    static const struct {
        const char *ciphername;
        const char *keyhex;
    } cases[] = {
        { "AES-128-GCM", KEY128_HEX },
        { "AES-192-GCM", KEY192_HEX },
    };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_bad = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t c, ptlen, ctlen, backlen;
    int len;

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(cases[c].keyhex, key, 32);
        from_hex(IV12_HEX, iv, 16);
        from_hex(AAD20_HEX, aad, 20);
        ptlen = from_hex(PT60_HEX, pt, 64);
        gcm_encrypt(cipher, key, iv, 12, aad, 20, pt, ptlen, ct, 64 + 16, &ctlen,
                    tag, 16);

        gcm_decrypt(cipher, key, iv, 12, aad, 20, ct, ctlen, back, 64 + 16,
                    &backlen, tag, 16);
        expect_equal("authenticated decrypt did not recover the plaintext",
                     back, backlen, pt, ptlen);

        /* Tamper with one tag bit: Final must fail. */
        memcpy(tag_bad, tag, 16);
        tag_bad[0] ^= 0x40;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 20) == 1,
              "AAD EVP_DecryptUpdate failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "EVP_DecryptUpdate failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
              "decryption accepted a tampered tag");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

/* (3) Splitting the 64-byte TC3 plaintext into updates of {5,5,6,16,32}
 * bytes must give the same ciphertext and the same tag -- for AES-128 and
 * AES-192 alike (the KAT row carries the cipher name and key). */
static void test_split_updates(void)
{
    /* gcm_kats indices of the two 64-byte, no-AAD rows: TC3 (AES-128) and
     * TC3-192 (derived). */
    static const size_t kat_index[] = { 2, 6 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    size_t c, ptlen, ct_wantlen, ctlen;

    for (c = 0; c < sizeof(kat_index) / sizeof(kat_index[0]); c++) {
        const struct gcm_kat *kat = &gcm_kats[kat_index[c]];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(IV12_HEX, iv, 16);
        ptlen = from_hex(kat->pthex, pt, 64);
        ct_wantlen = from_hex(kat->cthex, ct_want, 64);
        from_hex(kat->taghex, tag_want, 16);

        gcm_stream_encrypt(cipher, key, iv, 12, NULL, 0, pt, ptlen, ct, 64 + 16,
                           &ctlen, tag, 16);
        expect_equal("split-update ciphertext does not match the one-shot ciphertext",
                     ct, ctlen, ct_want, ct_wantlen);
        expect_equal("split-update tag does not match the one-shot tag",
                     tag, 16, tag_want, 16);
        EVP_CIPHER_free(cipher);
    }
}

/* (4) AAD after the first data update must fail. */
static void test_aad_ordering(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *in = ALLOCATE_INPUT(5);
    unsigned char *out = ALLOCATE_OUTPUT(5 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    from_hex(AAD20_HEX, aad, 20);
    for (i = 0; i < 5; i++)
        in[i] = (unsigned char)(0xA0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 5) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 20) != 1,
          "AAD update accepted after the first data update");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (5) IV lengths: the default is 12 (queried through OSSL_PARAM); 0 is
 * rejected; 8 and 16 work via EVP_CTRL_AEAD_SET_IVLEN; 17 also works --
 * OpenSSL's GCM accepts any IV length up to 128 bytes and computes the J0
 * through GHASH for non-12-byte IVs (so 17 round-trips too). */
static void test_iv_lengths(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv8 = ALLOCATE_IV(8);
    unsigned char *iv16 = ALLOCATE_IV(16);
    unsigned char *iv17 = ALLOCATE_IV(17);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t ptlen, ctlen, backlen, ivlen = 0;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(AAD20_HEX, aad, 20);
    ptlen = from_hex(PT60_HEX, pt, 64);
    for (i = 0; i < 8; i++)
        iv8[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < 16; i++)
        iv16[i] = (unsigned char)(0x20 + i);
    for (i = 0; i < 17; i++)
        iv17[i] = (unsigned char)(0x30 + i);

    /* Default IV length, queried via OSSL_PARAM. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &ivlen);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(OSSL_CIPHER_PARAM_IVLEN) failed");
    CHECK(ivlen == 12, "default GCM IV length is not 12");
    EVP_CIPHER_CTX_free(ctx);

    /* IV length 0 must be rejected. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 0, NULL) != 1,
          "IV length 0 was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 8, 16, and 17 byte IVs: encrypt/decrypt self-consistency, for AES-128
     * and AES-192. */
    {
        static const struct {
            const char *ciphername;
            const char *keyhex;
        } sizes_cases[] = {
            { "AES-128-GCM", KEY128_HEX },
            { "AES-192-GCM", KEY192_HEX },
        };
        size_t c;

        for (c = 0; c < sizeof(sizes_cases) / sizeof(sizes_cases[0]); c++) {
            EVP_CIPHER *csz = EVP_CIPHER_fetch(NULL, sizes_cases[c].ciphername,
                                               NULL);

            CHECK(csz != NULL,
                  "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
            from_hex(sizes_cases[c].keyhex, key, 32);

            gcm_encrypt(csz, key, iv8, 8, aad, 20, pt, ptlen, ct, 64 + 16,
                        &ctlen, tag, 16);
            gcm_decrypt(csz, key, iv8, 8, aad, 20, ct, ctlen, back, 64 + 16,
                        &backlen, tag, 16);
            expect_equal("8-byte IV roundtrip did not recover the plaintext",
                         back, backlen, pt, ptlen);

            gcm_encrypt(csz, key, iv16, 16, aad, 20, pt, ptlen, ct, 64 + 16,
                        &ctlen, tag, 16);
            gcm_decrypt(csz, key, iv16, 16, aad, 20, ct, ctlen, back, 64 + 16,
                        &backlen, tag, 16);
            expect_equal("16-byte IV roundtrip did not recover the plaintext",
                         back, backlen, pt, ptlen);

            gcm_encrypt(csz, key, iv17, 17, aad, 20, pt, ptlen, ct, 64 + 16,
                        &ctlen, tag, 16);
            gcm_decrypt(csz, key, iv17, 17, aad, 20, ct, ctlen, back, 64 + 16,
                        &backlen, tag, 16);
            expect_equal("17-byte IV roundtrip did not recover the plaintext",
                         back, backlen, pt, ptlen);
            EVP_CIPHER_free(csz);
        }
    }
    EVP_CIPHER_free(cipher);
}

/* (6) Short tags: ask for a 12-byte tag via the OSSL_CIPHER_PARAM_AEAD_TAGLEN
 * param at init time; the tag must equal the 12-byte prefix of the 16-byte
 * TC4 tag, and decryption with the short tag must round-trip. */
static void test_short_tag(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    size_t tl = 12;
    OSSL_PARAM params[2];
    EVP_CIPHER_CTX *ctx;
    size_t ptlen, ctlen, backlen, tag_wantlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    from_hex(AAD20_HEX, aad, 20);
    ptlen = from_hex(PT60_HEX, pt, 64);
    tag_wantlen = from_hex("5bc94fbc3221a5db94fae95a", tag_want, 16);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN, &tl);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, params) == 1,
          "EVP_EncryptInit_ex2 with AEAD_TAGLEN param failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 20) == 1,
          "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "EVP_EncryptUpdate failed");
    ctlen = (size_t)len;
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ctlen, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    ctlen += (size_t)len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 12, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG for the 12-byte tag failed");
    expect_equal("12-byte tag is not the prefix of the 16-byte tag",
                 tag, 12, tag_want, tag_wantlen);
    EVP_CIPHER_CTX_free(ctx);

    gcm_decrypt(cipher, key, iv, 12, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 12);
    expect_equal("short-tag decryption did not recover the plaintext",
                 back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);
}

/* (7) Zero-length payload: AAD-only encryption emits no ciphertext, just
 * the tag (the classic "tag-only" GCM self-test). */
static void test_zero_length_payload(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    size_t ctlen, tag_wantlen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    from_hex(AAD20_HEX, aad, 20);
    tag_wantlen = from_hex("346434fd51d5cd0c5887ec63e39b907a", tag_want, 16);

    gcm_encrypt(cipher, key, iv, 12, aad, 20, NULL, 0, ct, 16, &ctlen, tag, 16);
    CHECK(ctlen == 0, "AAD-only encryption produced ciphertext bytes");
    snprintf(msg, sizeof(msg), "AAD-only tag does not match the KAT");
    expect_equal(msg, tag, 16, tag_want, tag_wantlen);
    EVP_CIPHER_free(cipher);
}

/* (8) Update output-buffer contract: a single 20-byte GCM update into a
 * buffer of exactly inl + 16 bytes emits exactly inl bytes (GCM is a stream
 * cipher), Final adds nothing, and the tag is still retrievable. */
static void test_output_buffer_contract(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(20);
    unsigned char *out = ALLOCATE_OUTPUT(36); /* exactly inl + 16 */
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len, total;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-GCM\") failed");
    from_hex(KEY128_HEX, key, 16);
    from_hex(IV12_HEX, iv, 16);
    for (i = 0; i < 20; i++)
        in[i] = (unsigned char)(0xB0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 20) == 1,
          "EVP_EncryptUpdate into an exact-size buffer failed");
    CHECK(len == 20, "20-byte GCM update did not emit exactly 20 bytes");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK(len == 0, "GCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    CHECK(total == 20, "total output length is not 20");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_decrypt_and_tamper();
    test_split_updates();
    test_aad_ordering();
    test_iv_lengths();
    test_short_tag();
    test_zero_length_payload();
    test_output_buffer_contract();

    printf("test_aes_gcm_small: all %u checks passed\n", g_checks);
    return 0;
}
