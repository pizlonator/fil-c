#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_rc4_hmac_md5 -- the legacy provider's stitched RC4-HMAC-MD5 TLS
 * record cipher.
 *
 * The record format below was pinned by reading the implementation
 * (providers/implementations/ciphers/cipher_rc4_hmac_md5_hw.c, the
 * provider rewrite of crypto/evp/e_rc4_hmac_md5.c) and by byte-level
 * scratch experiments against both the pizfix 3.6.5 and the system
 * 3.0.13 builds (identical).  For a TLS 1.2-style record with L bytes of
 * payload:
 *
 *   - the EVP key is the 16-byte RC4 stream key; the HMAC-MD5 MAC key is
 *     SET SEPARATELY with EVP_CTRL_AEAD_SET_MAC_KEY (the provider
 *     precomputes the ipad/opad MD5 states from it,
 *     cipher_hw_rc4_hmac_md5_init_mackey);
 *   - the 13-byte AAD is EVP_CTRL_AEAD_TLS1_AAD with the layout
 *     seq(8) || type(1) || version(2) || length(2).  On ENCRYPT the
 *     length field is L (the MAC input length) and the setter does NOT
 *     rewrite it; it returns the MAC size, 16.  On DECRYPT the length
 *     field is the record body length L + 16 and the setter rewrites it
 *     to L and returns 16 (cipher_hw_rc4_hmac_md5_tls_init);
 *   - the record body is EXACTLY L + 16 bytes (payload || MAC slot) and
 *     the update MUST be a single call of that size -- the cipher checks
 *     "len != payload_length + MD5_DIGEST_LENGTH" and fails otherwise;
 *   - the MAC is HMAC-MD5(mackey, AAD || payload) computed with the
 *     ipad/opad states, appended after the payload, and the WHOLE body
 *     (payload || MAC) is then RC4-encrypted with the EVP key, keystream
 *     starting at position 0 of the body (a fresh context per record);
 *   - decryption RC4-decrypts the whole body, recomputes the MAC over
 *     the recovered payload and fails the UPDATE on any mismatch
 *     (CRYPTO_memcmp in the hw), leaving the payload at output[0..L).
 *
 * The KAT is the composition: the expected record is rebuilt with
 * separate EVP_MAC(HMAC-MD5) and RC4 calls following exactly this format
 * and must equal the stitched output byte for byte.  (The non-TLS mode
 * -- no AAD set -- is plain RC4 of the update, which is asserted too.)
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rc4_hmac_md5: %s\n", msg);
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

#define MD5_MAC_LEN 16

static const char *const RCKEY_HEX =
    "000102030405060708090a0b0c0d0e0f";
static const char *const MACKEY_HEX =
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

/* Builds the 13-byte TLS AAD: seq(8) || type 0x17 || version 0x0303 ||
 * length(2).  `length` is the caller's responsibility (L on encrypt,
 * L + 16 on decrypt). */
static void build_aad(unsigned char *aad, unsigned long seq, size_t length)
{
    size_t i;

    memset(aad, 0, 13);
    for (i = 0; i < 8; i++)
        aad[7 - i] = (unsigned char)((seq >> (8 * i)) & 0xff);
    aad[8] = 0x17; /* application_data */
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = (unsigned char)(length >> 8);
    aad[12] = (unsigned char)length;
}

/* Encrypts one TLS record with the stitched cipher.  Returns the body
 * length (L + 16).  `aad` is the caller's 13-byte AAD with the length
 * field set to L (the setter leaves it alone on encrypt). */
static size_t stitch_encrypt(const EVP_CIPHER *cipher, const unsigned char *rckey,
                             const unsigned char *mackey, unsigned char *aad,
                             const unsigned char *payload, size_t L,
                             unsigned char *record, size_t reccap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char in[1024 + MD5_MAC_LEN];
    int pad, len = 0, flen = 0;

    CHECK(L <= 1024, "payload larger than the scratch buffer");
    CHECK(L + MD5_MAC_LEN <= reccap, "record buffer too small");

    memcpy(in, payload, L);
    memset(in + L, 0, MD5_MAC_LEN); /* the MAC slot is filled by the cipher */

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, rckey, NULL) == 1,
          "stitched encrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, MD5_MAC_LEN,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad == MD5_MAC_LEN,
          "the encrypt AAD setter did not report the 16-byte MAC size");
    CHECK(EVP_EncryptUpdate(ctx, record, &len, in, (int)(L + MD5_MAC_LEN)) == 1,
          "the stitched encrypt update failed");
    CHECK((size_t)len == L + MD5_MAC_LEN,
          "the stitched encrypt update did not emit the whole body");
    CHECK(EVP_EncryptFinal_ex(ctx, record + len, &flen) == 1,
          "the stitched encrypt final failed");
    CHECK(flen == 0, "the stitched encrypt final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Decrypts one TLS record in place; the payload must appear at
 * record[0..L).  `aad` is the caller's 13-byte AAD with the length field
 * set to L + 16; the setter rewrites it to L. */
static void stitch_decrypt(const EVP_CIPHER *cipher, const unsigned char *rckey,
                           const unsigned char *mackey, unsigned char *aad,
                           unsigned char *record, size_t L)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int pad, len = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, rckey, NULL) == 1,
          "stitched decrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, MD5_MAC_LEN,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad == MD5_MAC_LEN,
          "the decrypt AAD setter did not report the 16-byte MAC size");
    CHECK(aad[11] == (unsigned char)(L >> 8) && aad[12] == (unsigned char)L,
          "the decrypt AAD length field was not rewritten to L");
    CHECK(EVP_DecryptUpdate(ctx, record, &len, record,
                            (int)(L + MD5_MAC_LEN)) == 1,
          "the stitched decrypt update failed (MAC invalid?)");
    CHECK((size_t)len == L + MD5_MAC_LEN,
          "the stitched decrypt update did not consume the whole body");
    CHECK(EVP_DecryptFinal_ex(ctx, record + len, &flen) == 1,
          "the stitched decrypt final failed");
    EVP_CIPHER_CTX_free(ctx);
}

/* Computes HMAC-MD5(mackey, aad || payload) via EVP_MAC. */
static void compute_mac(const unsigned char *mackey, const unsigned char *aad,
                        const unsigned char *payload, size_t L,
                        unsigned char *mac)
{
    EVP_MAC *hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *mctx = EVP_MAC_CTX_new(hmac);
    OSSL_PARAM params[2];
    size_t maclen = 0;

    CHECK(mctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)"MD5", 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(mctx, params) == 1,
          "EVP_MAC_CTX_set_params failed");
    CHECK(EVP_MAC_init(mctx, mackey, MD5_MAC_LEN, NULL) == 1,
          "EVP_MAC_init failed");
    CHECK(EVP_MAC_update(mctx, aad, 13) == 1, "EVP_MAC_update(aad) failed");
    CHECK(EVP_MAC_update(mctx, payload, L) == 1,
          "EVP_MAC_update(payload) failed");
    CHECK(EVP_MAC_final(mctx, mac, &maclen, MD5_MAC_LEN) == 1,
          "EVP_MAC_final failed");
    CHECK(maclen == MD5_MAC_LEN, "HMAC-MD5 length is not 16");
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(hmac);
}

/* (1) Roundtrips over several record sizes: encrypt, decrypt, payload
 * recovered. */
static void test_tls_roundtrip(void)
{
    static const size_t sizes[] = { 1, 16, 64, 256 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4-HMAC-MD5", NULL);
    unsigned char *rckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *aad = ALLOCATE_AAD(13);
    unsigned char *payload = ALLOCATE_INPUT(1024);
    unsigned char *record = ALLOCATE_OUTPUT(1024 + MD5_MAC_LEN);
    size_t s, i, L;

    /* RC4-HMAC-MD5 lives in the legacy provider loaded in main(); a NULL
     * fetch here is a broken build, not a version gate. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4-HMAC-MD5\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 16,
          "RC4-HMAC-MD5 key length is not 16");
    from_hex(RCKEY_HEX, rckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    for (i = 0; i < 1024; i++)
        payload[i] = (unsigned char)(i * 29 + 7);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        L = sizes[s];
        build_aad(aad, s + 1, L);
        stitch_encrypt(cipher, rckey, mackey, aad, payload, L, record,
                       1024 + MD5_MAC_LEN);
        build_aad(aad, s + 1, L + MD5_MAC_LEN); /* rewritten by the setter */
        stitch_decrypt(cipher, rckey, mackey, aad, record, L);
        CHECK(memcmp(record, payload, L) == 0,
              "the TLS record roundtrip did not recover the payload");
        g_checks++;
    }
    EVP_CIPHER_free(cipher);
}

/* (2) The composition KAT: the stitched record must equal
 * RC4(payload || HMAC-MD5(mackey, aad || payload)) for several sizes --
 * this is the byte-level format check. */
static void test_composition(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4-HMAC-MD5", NULL);
    EVP_CIPHER *rc4 = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *rckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *aad = ALLOCATE_AAD(13);
    unsigned char *payload = ALLOCATE_INPUT(512);
    unsigned char *mac = ALLOCATE_OTHER(MD5_MAC_LEN);
    unsigned char *body = ALLOCATE_OUTPUT(512 + MD5_MAC_LEN);
    unsigned char *record = ALLOCATE_OUTPUT(512 + MD5_MAC_LEN);
    EVP_CIPHER_CTX *ctx;
    size_t L, i, bodylen;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4-HMAC-MD5\") failed");
    CHECK(rc4 != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex(RCKEY_HEX, rckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    for (i = 0; i < 512; i++)
        payload[i] = (unsigned char)(i * 11 + 3);

    for (L = 1; L <= 512; L += 127) {
        build_aad(aad, 0x2a, L);
        compute_mac(mackey, aad, payload, L, mac);
        memcpy(body, payload, L);
        memcpy(body + L, mac, MD5_MAC_LEN);
        bodylen = L + MD5_MAC_LEN;

        /* RC4 the whole body (keystream from position 0). */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(ctx, rc4, NULL, rckey, NULL) == 1,
              "composition RC4 init failed");
        CHECK(EVP_EncryptUpdate(ctx, record, &len, body, (int)bodylen) == 1,
              "composition RC4 update failed");
        CHECK((size_t)len == bodylen, "composition RC4 length mismatch");
        CHECK(EVP_EncryptFinal_ex(ctx, record + len, &flen) == 1,
              "composition RC4 final failed");
        EVP_CIPHER_CTX_free(ctx);

        /* The stitched cipher must produce the identical record. */
        build_aad(aad, 0x2a, L);
        stitch_encrypt(cipher, rckey, mackey, aad, payload, L, body,
                       512 + MD5_MAC_LEN);
        expect_equal("the stitched record differs from the HMAC+RC4 "
                     "composition", body, bodylen, record, bodylen);
    }
    EVP_CIPHER_free(cipher);
    EVP_CIPHER_free(rc4);
}

/* (3) Tampered records must fail the decrypt UPDATE (the MAC comparison
 * lives in the update, before Final): a flipped payload byte, a flipped
 * MAC byte and a wrong MAC key. */
static void test_tamper(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4-HMAC-MD5", NULL);
    unsigned char *rckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *aad = ALLOCATE_AAD(13);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(64 + MD5_MAC_LEN);
    EVP_CIPHER_CTX *ctx;
    size_t i, L = 64;
    int t, len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4-HMAC-MD5\") failed");
    from_hex(RCKEY_HEX, rckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    for (i = 0; i < 64; i++)
        payload[i] = (unsigned char)(i * 5 + 9);

    for (t = 0; t < 3; t++) {
        build_aad(aad, 7, L);
        stitch_encrypt(cipher, rckey, mackey, aad, payload, L, record,
                       64 + MD5_MAC_LEN);
        if (t == 0)
            record[3] ^= 0x01; /* a payload byte */
        else if (t == 1)
            record[L] ^= 0x01; /* a MAC byte */
        else
            mackey[0] ^= 0x01; /* the decrypt will use the wrong MAC key */

        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, rckey, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY,
                                  MD5_MAC_LEN, (void *)mackey) == 1,
              "EVP_CTRL_AEAD_SET_MAC_KEY failed");
        build_aad(aad, 7, L + MD5_MAC_LEN);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad) ==
                  MD5_MAC_LEN,
              "the decrypt AAD setter failed");
        CHECK(EVP_DecryptUpdate(ctx, record, &len, record,
                                (int)(L + MD5_MAC_LEN)) != 1,
              "the stitched decrypt accepted a tampered record");
        EVP_CIPHER_CTX_free(ctx);

        if (t == 0)
            record[3] ^= 0x01;
        else if (t == 1)
            record[L] ^= 0x01;
        else
            mackey[0] ^= 0x01;
    }
    EVP_CIPHER_free(cipher);
}

/* (4) Without the TLS AAD the cipher is plain RC4 of the update: a
 * 64-byte update through RC4-HMAC-MD5 (no AAD, no MAC key) must equal
 * RC4 under the same EVP key. */
static void test_nontls_is_rc4(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "RC4-HMAC-MD5", NULL);
    EVP_CIPHER *rc4 = EVP_CIPHER_fetch(NULL, "RC4", NULL);
    unsigned char *rckey = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *stitched = ALLOCATE_OUTPUT(64);
    unsigned char *plain = ALLOCATE_OUTPUT(64);
    EVP_CIPHER_CTX *ctx;
    size_t i;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"RC4-HMAC-MD5\") failed");
    CHECK(rc4 != NULL, "EVP_CIPHER_fetch(\"RC4\") failed");
    from_hex(RCKEY_HEX, rckey, 16);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, rckey, NULL) == 1,
          "non-TLS stitched init failed");
    CHECK(EVP_EncryptUpdate(ctx, stitched, &len, in, 64) == 1,
          "non-TLS stitched update failed");
    CHECK(len == 64, "the non-TLS stitched update length is wrong");
    CHECK(EVP_EncryptFinal_ex(ctx, stitched + len, &flen) == 1,
          "non-TLS stitched final failed");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, rc4, NULL, rckey, NULL) == 1,
          "plain RC4 init failed");
    CHECK(EVP_EncryptUpdate(ctx, plain, &len, in, 64) == 1,
          "plain RC4 update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, plain + len, &flen) == 1,
          "plain RC4 final failed");
    EVP_CIPHER_CTX_free(ctx);

    expect_equal("the non-TLS RC4-HMAC-MD5 mode differs from plain RC4",
                 stitched, 64, plain, 64);
    EVP_CIPHER_free(cipher);
    EVP_CIPHER_free(rc4);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    /* Loading the legacy provider disables default-provider auto-load, so
     * both are loaded explicitly (RC4-HMAC-MD5 lives in the legacy
     * provider). */
    CHECK(OSSL_PROVIDER_load(NULL, "legacy") != NULL,
          "OSSL_PROVIDER_load(\"legacy\") failed");
    CHECK(OSSL_PROVIDER_load(NULL, "default") != NULL,
          "OSSL_PROVIDER_load(\"default\") failed");

    test_tls_roundtrip();
    test_composition();
    test_tamper();
    test_nontls_is_rc4();

    printf("test_rc4_hmac_md5: all %u checks passed\n", g_checks);
    return 0;
}
