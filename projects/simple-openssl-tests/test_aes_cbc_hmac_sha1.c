#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/*
 * test_aes_cbc_hmac_sha1 -- the stitched AES-CBC-HMAC-SHA1 TLS-record
 * cipher (AES-128/256).
 *
 * The record format below was pinned by reading the implementation
 * (providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_hw.c, the
 * same code the legacy crypto/evp/e_aes_cbc_hmac_sha1.c carries) and by
 * byte-level experiments against both OpenSSL builds.  For a TLS 1.1/1.2
 * record (version bytes >= 0x0302 in the AAD), with L = payload length:
 *
 *   - the EVP key is the AES key (16/24/32 bytes) and the EVP IV is the
 *     16-byte CBC IV used by encryption; the HMAC key is set separately
 *     with EVP_CTRL_AEAD_SET_MAC_KEY (the TLS layer's mac key);
 *   - the 13-byte AAD is EVP_CTRL_AEAD_TLS1_AAD with the layout
 *     seq(8) || type(1) || version(2) || length(2), where the length field
 *     is the record body length WITHOUT MAC/padding, i.e. 16 + L (the
 *     explicit-IV slot plus the payload).  The setter rewrites it to L
 *     (the MAC input length) in the caller's buffer and returns the pad
 *     amount; that rewritten AAD is exactly what the MAC hashes;
 *   - the record body length fed to the update is
 *         bodylen = 16 + L + 20 (SHA1) + padbytes,
 *     padbytes = 16 - ((L + 20) mod 16), each padding byte equal to
 *     padbytes - 1 (TLS block-cipher padding);
 *   - the update MUST be a single call of exactly bodylen bytes with the
 *     input laid out as [16-byte explicit-IV slot][payload][slack]; the
 *     cipher computes the MAC and padding itself and CBC-encrypts the
 *     WHOLE body (explicit-IV slot included) starting from the EVP IV;
 *   - decryption takes the first 16 bytes of the record as the CBC IV
 *     (overriding the EVP IV), decrypts the rest, verifies MAC and
 *     padding, and fails the update on any mismatch; the payload appears
 *     at output[16 .. 16+L].
 *
 * The KAT is the composition: the expected record is rebuilt with separate
 * EVP_MAC(HMAC-SHA1) and AES-128/256-CBC calls following exactly this
 * format and must equal the stitched output byte for byte.  One AES-128
 * record is additionally embedded as a hex constant (verified against both
 * the pizfix 3.6.5 and the system 3.0.13 builds).  A TLS 1.0-style record
 * (version 0x0300: no explicit-IV slot, bodylen = ceil16(L+21), CBC over
 * payload||MAC||padding from the EVP IV) is composed and round-tripped as
 * well.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_cbc_hmac_sha1: %s\n", msg);
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

static const char *const KEY128_HEX =
    "000102030405060708090A0B0C0D0E0F";
static const char *const KEY256_HEX =
    "000102030405060708090A0B0C0D0E0F"
    "101112131415161718191A1B1C1D1E1F";
static const char *const MACKEY_HEX =
    "0FFEEDDCCBBAA9988776655443322110";
static const char *const IV_HEX =
    "00112233445566778899AABBCCDDEEFF";
static const char *const EXPIV_HEX =
    "0405060708090A0B0C0D0E0F10111213";

/* The embedded KAT: AES-128, L = 5, payload 0102030405, TLS 1.2 record. */
static const char *const KAT_PAYLOAD_HEX = "0102030405";
static const char *const KAT_RECORD_HEX =
    "1537fc822f441fcc150fd3e428745324"
    "c97b43c413da0977bb993d5d88296de6"
    "3a953340d4371189ab6242056cc56246";

#define SHA1_MAC_LEN 20

static size_t tls12_body_len(size_t L)
{
    return 16 + L + SHA1_MAC_LEN + (16 - ((L + SHA1_MAC_LEN) % 16));
}

static size_t tls10_body_len(size_t L)
{
    /* floor16(L + 20 + 16) == ceil16(L + 21): the TLS 1.0 record body. */
    return ((L + SHA1_MAC_LEN + 16) & ~(size_t)15);
}

/* Computes HMAC-SHA1(mackey, aad13 || payload) via EVP_MAC. */
static void compute_mac(const unsigned char *mackey, size_t mackeylen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *payload, size_t payloadlen,
                        unsigned char *mac)
{
    EVP_MAC *hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *mctx = EVP_MAC_CTX_new(hmac);
    OSSL_PARAM params[2];
    size_t maclen = 0;

    CHECK(mctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)"SHA1", 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(mctx, params) == 1,
          "EVP_MAC_CTX_set_params failed");
    CHECK(EVP_MAC_init(mctx, mackey, mackeylen, NULL) == 1,
          "EVP_MAC_init failed");
    CHECK(EVP_MAC_update(mctx, aad, aadlen) == 1, "EVP_MAC_update(aad) failed");
    CHECK(EVP_MAC_update(mctx, payload, payloadlen) == 1,
          "EVP_MAC_update(payload) failed");
    CHECK(EVP_MAC_final(mctx, mac, &maclen, SHA1_MAC_LEN) == 1,
          "EVP_MAC_final failed");
    CHECK(maclen == SHA1_MAC_LEN, "HMAC-SHA1 length is not 20");
    EVP_MAC_CTX_free(mctx);
    EVP_MAC_free(hmac);
}

/* CBC-encrypts bodylen bytes (padding disabled) -- the composition's last
 * step. */
static void cbc_encrypt_raw(const EVP_CIPHER *cipher, const unsigned char *key,
                            const unsigned char *iv, unsigned char *body,
                            size_t bodylen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "composition CBC init failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "composition CBC set_padding failed");
    CHECK(EVP_EncryptUpdate(ctx, body, &len, body, (int)bodylen) == 1,
          "composition CBC update failed");
    CHECK((size_t)len == bodylen, "composition CBC update length mismatch");
    CHECK(EVP_EncryptFinal_ex(ctx, body + len, &flen) == 1,
          "composition CBC final failed");
    CHECK(flen == 0, "composition CBC final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
}

/* Builds the expected TLS 1.1/1.2 record body for `payload` (composition
 * of the stitched cipher). */
static void compose_tls12_record(const EVP_CIPHER *cipher,
                                 const unsigned char *enckey,
                                 const unsigned char *mackey,
                                 size_t mackeylen, const unsigned char *iv,
                                 const unsigned char *expiv,
                                 const unsigned char *payload, size_t L,
                                 unsigned char *record)
{
    unsigned char aad[13];
    unsigned char mac[SHA1_MAC_LEN];
    size_t bodylen, padbytes, i;

    bodylen = tls12_body_len(L);
    padbytes = bodylen - 16 - L - SHA1_MAC_LEN;

    /* seq(8) || type 0x17 || version 0x0303 || length = 16 + L ... */
    memset(aad, 0, 13);
    aad[0] = 0x00;
    aad[7] = 0x01; /* seq = 0000000000000001 */
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = (unsigned char)((16 + L) >> 8);
    aad[12] = (unsigned char)(16 + L);
    /* ... rewritten by the setter to the MAC input length L. */
    aad[11] = (unsigned char)(L >> 8);
    aad[12] = (unsigned char)L;

    compute_mac(mackey, mackeylen, aad, 13, payload, L, mac);

    memcpy(record, expiv, 16);
    memcpy(record + 16, payload, L);
    memcpy(record + 16 + L, mac, SHA1_MAC_LEN);
    for (i = 0; i < padbytes; i++)
        record[16 + L + SHA1_MAC_LEN + i] = (unsigned char)(padbytes - 1);

    cbc_encrypt_raw(cipher, enckey, iv, record, bodylen);
}

/* Encrypts a TLS 1.1/1.2 record with the stitched cipher.  Returns the
 * body length; `record` receives it (the first 16 bytes are the caller's
 * explicit-IV slot, which the cipher encrypts along with everything
 * else). */
static size_t stitch_encrypt_tls12(const EVP_CIPHER *cipher,
                                   const unsigned char *enckey,
                                   const unsigned char *mackey,
                                   size_t mackeylen, const unsigned char *iv,
                                   unsigned char *expiv,
                                   const unsigned char *payload, size_t L,
                                   unsigned char *record, size_t reccap)
{
    unsigned char aad[13];
    unsigned char input[2048];
    int pad, len = 0, flen = 0;
    size_t bodylen;

    CHECK(L <= 1024, "payload larger than the scratch buffer");
    bodylen = tls12_body_len(L);
    CHECK(bodylen + 16 <= reccap, "record buffer too small");

    memset(aad, 0, 13);
    aad[7] = 0x01;
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = (unsigned char)((16 + L) >> 8);
    aad[12] = (unsigned char)(16 + L);

    memcpy(input, expiv, 16);
    memcpy(input + 16, payload, L);

    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
              "stitched encrypt init failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY,
                                  (int)mackeylen, (void *)mackey) == 1,
              "EVP_CTRL_AEAD_SET_MAC_KEY failed");
        pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
        CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed");
        CHECK((size_t)pad == bodylen - 16 - L,
              "the returned AAD pad amount is unexpected");
        /* The setter rewrote the AAD length field to L. */
        CHECK(aad[11] == (unsigned char)(L >> 8) &&
                  aad[12] == (unsigned char)L,
              "the AAD length field was not rewritten to the MAC length");
        CHECK(EVP_EncryptUpdate(ctx, record, &len, input, (int)bodylen) == 1,
              "stitched encrypt update failed");
        CHECK((size_t)len == bodylen,
              "the stitched update did not encrypt the whole body");
        CHECK(EVP_EncryptFinal_ex(ctx, record + len, &flen) == 1,
              "stitched encrypt final failed");
        EVP_CIPHER_CTX_free(ctx);
    }
    return bodylen;
}

/* Decrypts a TLS 1.1/1.2 record in place; the payload must appear at
 * record[16 .. 16+L]. */
static void stitch_decrypt_tls12(const EVP_CIPHER *cipher,
                                 const unsigned char *enckey,
                                 const unsigned char *mackey,
                                 size_t mackeylen, const unsigned char *iv,
                                 size_t L, unsigned char *record,
                                 size_t bodylen)
{
    unsigned char aad[13];
    int pad, len = 0, flen = 0;

    /* L is implied by the record: the cipher recovers it from the padding
     * and rewrites its stored AAD copy with it. */
    (void)L;

    memset(aad, 0, 13);
    aad[7] = 0x01;
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = (unsigned char)(bodylen >> 8);
    aad[12] = (unsigned char)bodylen;

    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
              "stitched decrypt init failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY,
                                  (int)mackeylen, (void *)mackey) == 1,
              "EVP_CTRL_AEAD_SET_MAC_KEY failed");
        pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
        CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed on decrypt");
        CHECK(EVP_DecryptUpdate(ctx, record, &len, record, (int)bodylen) == 1,
              "stitched decrypt update failed (MAC or padding invalid?)");
        CHECK((size_t)len == bodylen,
              "the stitched decrypt update did not consume the whole body");
        CHECK(EVP_DecryptFinal_ex(ctx, record + len, &flen) == 1,
              "stitched decrypt final failed");
        EVP_CIPHER_CTX_free(ctx);
    }
}

/* (1) The embedded KAT record and the composition equality for AES-128. */
static void test_kat_and_composition(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC-HMAC-SHA1", NULL);
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, "AES-256-CBC-HMAC-SHA1", NULL);
    unsigned char *enckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(8);
    unsigned char *record_want = ALLOCATE_OUTPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(64);
    unsigned char *composed = ALLOCATE_OUTPUT(64);
    size_t L, bodylen;

    /* AES-CBC-HMAC-SHA1 is core OpenSSL 3 functionality (AES-NI gated);
     * never SKIP a working build, only FAIL. */
    CHECK(cipher != NULL,
          "EVP_CIPHER_fetch(\"AES-128-CBC-HMAC-SHA1\") failed");
    CHECK(probe != NULL,
          "EVP_CIPHER_fetch(\"AES-256-CBC-HMAC-SHA1\") failed");
    EVP_CIPHER_free(probe);

    from_hex(KEY128_HEX, enckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    from_hex(IV_HEX, iv, 16);
    from_hex(EXPIV_HEX, expiv, 16);
    from_hex(KAT_PAYLOAD_HEX, payload, 8);
    from_hex(KAT_RECORD_HEX, record_want, 64);
    L = 5;

    bodylen = stitch_encrypt_tls12(cipher, enckey, mackey, 16, iv, expiv,
                                   payload, L, record, 64);
    CHECK(bodylen == 48, "the KAT record body length is not 48");
    expect_equal("the stitched record does not match the embedded KAT",
                 record, bodylen, record_want, 48);

    compose_tls12_record(cipher, enckey, mackey, 16, iv, expiv, payload, L,
                         composed);
    expect_equal("the composed record differs from the embedded KAT",
                 composed, bodylen, record_want, 48);

    /* Decrypt the embedded record: the payload must come back at [16..21]. */
    stitch_decrypt_tls12(cipher, enckey, mackey, 16, iv, L, record, bodylen);
    CHECK(memcmp(record + 16, payload, L) == 0,
          "decrypting the KAT record did not recover the payload");
    EVP_CIPHER_free(cipher);
}

/* (2) Compose-equals-stitch and roundtrip for several payload sizes and
 * both key sizes. */
static void test_composition_and_roundtrip(void)
{
    static const size_t sizes[] = { 1, 16, 64, 256, 1024 };
    static const struct {
        const char *name;
        const char *keyhex;
        size_t keylen;
    } ciphers[] = {
        { "AES-128-CBC-HMAC-SHA1", KEY128_HEX, 16 },
        { "AES-256-CBC-HMAC-SHA1", KEY256_HEX, 32 },
    };
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(1024);
    unsigned char *record = ALLOCATE_OUTPUT(2048);
    unsigned char *composed = ALLOCATE_OUTPUT(2048);
    size_t c, s, i, L, bodylen;
    char msg[160];

    from_hex(MACKEY_HEX, mackey, 16);
    from_hex(IV_HEX, iv, 16);
    from_hex(EXPIV_HEX, expiv, 16);
    for (i = 0; i < 1024; i++)
        payload[i] = (unsigned char)(i * 29 + 7);

    for (c = 0; c < sizeof(ciphers) / sizeof(ciphers[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphers[c].name, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES-CBC-HMAC-SHA1 cipher");
        from_hex(ciphers[c].keyhex, enckey, ciphers[c].keylen);

        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            L = sizes[s];
            bodylen = stitch_encrypt_tls12(cipher, enckey, mackey, 16, iv,
                                           expiv, payload, L, record, 2048);
            compose_tls12_record(cipher, enckey, mackey, 16, iv, expiv,
                                 payload, L, composed);
            snprintf(msg, sizeof(msg),
                     "%s: the composed record differs from the stitched "
                     "record (L=%zu)",
                     ciphers[c].name, L);
            expect_equal(msg, record, bodylen, composed, bodylen);

            stitch_decrypt_tls12(cipher, enckey, mackey, 16, iv, L,
                                 record, bodylen);
            snprintf(msg, sizeof(msg),
                     "%s: decrypting the record did not recover the payload "
                     "(L=%zu)",
                     ciphers[c].name, L);
            CHECK(memcmp(record + 16, payload, L) == 0, msg);
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A tampered record (ciphertext byte flipped) must fail the decrypt
 * update. */
static void test_tamper(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC-HMAC-SHA1", NULL);
    unsigned char *enckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(128);
    unsigned char aad[13];
    EVP_CIPHER_CTX *ctx;
    int pad, len = 0;
    size_t L = 64, bodylen, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC-HMAC-SHA1\") failed");
    from_hex(KEY128_HEX, enckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    from_hex(IV_HEX, iv, 16);
    from_hex(EXPIV_HEX, expiv, 16);
    for (i = 0; i < 64; i++)
        payload[i] = (unsigned char)(i * 3 + 1);

    bodylen = stitch_encrypt_tls12(cipher, enckey, mackey, 16, iv, expiv,
                                   payload, L, record, 128);

    /* Flip a byte in the middle of the ciphertext. */
    record[40] ^= 0x10;
    memset(aad, 0, 13);
    aad[7] = 0x01;
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = (unsigned char)(bodylen >> 8);
    aad[12] = (unsigned char)bodylen;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched decrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, 16,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed");
    CHECK(EVP_DecryptUpdate(ctx, record, &len, record, (int)bodylen) != 1,
          "decryption accepted a tampered record");
    EVP_CIPHER_CTX_free(ctx);
    record[40] ^= 0x10;

    /* Flip a byte in the transmitted explicit-IV slot (it changes the CBC
     * IV of the payload region and the MAC of nothing -- still detected
     * through the padding/MAC check). */
    record[3] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched decrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, 16,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed");
    CHECK(EVP_DecryptUpdate(ctx, record, &len, record, (int)bodylen) != 1,
          "decryption accepted a tampered IV slot");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (4) A wrong AAD length must be rejected by the AAD ctrl; a missing AAD
 * degrades to the documented streaming mode (plain CBC of the raw buffer,
 * no MAC/padding), which differs from the TLS-mode record. */
static void test_aad_rules(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC-HMAC-SHA1", NULL);
    EVP_CIPHER *cbc = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);
    unsigned char *enckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *raw = ALLOCATE_INPUT(48);
    unsigned char *streamed = ALLOCATE_OUTPUT(128);
    unsigned char aad[13];
    EVP_CIPHER_CTX *ctx;
    int pad, len = 0, flen = 0;
    size_t L = 5, bodylen, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC-HMAC-SHA1\") failed");
    CHECK(cbc != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC\") failed");
    from_hex(KEY128_HEX, enckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    from_hex(IV_HEX, iv, 16);
    from_hex(EXPIV_HEX, expiv, 16);
    from_hex(KAT_PAYLOAD_HEX, payload, 8);
    for (i = 0; i < 48; i++)
        raw[i] = (unsigned char)(i + 1);

    bodylen = tls12_body_len(L);

    /* Wrong AAD lengths must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched encrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, 16,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    memset(aad, 0, 13);
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = 0x00;
    aad[12] = 0x15;
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 12, aad);
    CHECK(pad <= 0, "a 12-byte AAD was accepted");
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 14, aad);
    CHECK(pad <= 0, "a 14-byte AAD was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* Missing AAD: the update runs in streaming mode and is exactly a
     * plain CBC encryption of the raw buffer. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched encrypt init failed");
    CHECK(EVP_EncryptUpdate(ctx, streamed, &len, raw, (int)bodylen) == 1,
          "streaming-mode update failed");
    CHECK((size_t)len == bodylen, "streaming-mode length mismatch");
    CHECK(EVP_EncryptFinal_ex(ctx, streamed + len, &flen) == 1,
          "streaming-mode final failed");
    EVP_CIPHER_CTX_free(ctx);
    {
        unsigned char *plain = ALLOCATE_OUTPUT(128);
        memcpy(plain, raw, bodylen);
        cbc_encrypt_raw(cbc, enckey, iv, plain, bodylen);
        expect_equal("streaming-mode output is not a plain CBC of the input",
                     streamed, bodylen, plain, bodylen);
    }

    /* And it must differ from the TLS-mode record of the same plaintext. */
    {
        unsigned char *tlsrec = ALLOCATE_OUTPUT(128);
        stitch_encrypt_tls12(cipher, enckey, mackey, 16, iv, expiv, payload,
                             L, tlsrec, 128);
        CHECK(memcmp(tlsrec, streamed, bodylen) != 0,
              "the streaming fallback equals the TLS-mode record");
    }
    EVP_CIPHER_free(cipher);
    EVP_CIPHER_free(cbc);
}

/* (5) A TLS 1.0-style record (version 0x0300): no explicit-IV slot, the
 * body is CBC(payload || MAC || padding) from the EVP IV, and the AAD
 * length field is the payload length itself (not rewritten). */
static void test_tls10_style(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC-HMAC-SHA1", NULL);
    unsigned char *enckey = ALLOCATE_KEY(16);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(128);
    unsigned char *composed = ALLOCATE_OUTPUT(128);
    EVP_CIPHER_CTX *ctx;
    unsigned char aad[13];
    unsigned char mac[SHA1_MAC_LEN];
    int pad, len = 0, flen = 0;
    size_t L = 20, bodylen, padbytes, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC-HMAC-SHA1\") failed");
    from_hex(KEY128_HEX, enckey, 16);
    from_hex(MACKEY_HEX, mackey, 16);
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < 64; i++)
        payload[i] = (unsigned char)(i * 7 + 3);

    bodylen = tls10_body_len(L);
    padbytes = bodylen - L - SHA1_MAC_LEN;

    /* Compose: MAC over AAD13(length = L) || payload; CBC over
     * payload || MAC || padding. */
    memset(aad, 0, 13);
    aad[7] = 0x01;
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x00; /* TLS 1.0 */
    aad[11] = (unsigned char)(L >> 8);
    aad[12] = (unsigned char)L;
    compute_mac(mackey, 16, aad, 13, payload, L, mac);

    memcpy(composed, payload, L);
    memcpy(composed + L, mac, SHA1_MAC_LEN);
    for (i = 0; i < padbytes; i++)
        composed[L + SHA1_MAC_LEN + i] = (unsigned char)(padbytes - 1);
    cbc_encrypt_raw(cipher, enckey, iv, composed, bodylen);

    /* Stitch-encrypt with the same AAD. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched encrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, 16,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    memcpy(record, payload, L); /* payload at offset 0: no IV slot */
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed");
    CHECK(aad[11] == (unsigned char)(L >> 8) && aad[12] == (unsigned char)L,
          "the TLS 1.0 AAD length field was rewritten");
    CHECK(EVP_EncryptUpdate(ctx, record, &len, record, (int)bodylen) == 1,
          "TLS 1.0-style update failed");
    CHECK((size_t)len == bodylen, "TLS 1.0-style length mismatch");
    CHECK(EVP_EncryptFinal_ex(ctx, record + len, &flen) == 1,
          "TLS 1.0-style final failed");
    EVP_CIPHER_CTX_free(ctx);
    expect_equal("the TLS 1.0-style record differs from the composition",
                 record, bodylen, composed, bodylen);

    /* Roundtrip via the stitched decrypt. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, enckey, iv) == 1,
          "stitched decrypt init failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_MAC_KEY, 16,
                              (void *)mackey) == 1,
          "EVP_CTRL_AEAD_SET_MAC_KEY failed");
    aad[11] = (unsigned char)(bodylen >> 8);
    aad[12] = (unsigned char)bodylen;
    pad = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_TLS1_AAD, 13, aad);
    CHECK(pad > 0, "EVP_CTRL_AEAD_TLS1_AAD failed on decrypt");
    CHECK(EVP_DecryptUpdate(ctx, record, &len, record, (int)bodylen) == 1,
          "TLS 1.0-style decrypt update failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(memcmp(record, payload, L) == 0,
          "the TLS 1.0-style roundtrip did not recover the payload");
    EVP_CIPHER_free(cipher);
    ERR_clear_error();
}

int main(int argc, char **argv)
{
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, "AES-128-CBC-HMAC-SHA1", NULL);
    EVP_CIPHER_CTX *probe_ctx;

    poison_parse_args(argc, argv);

    /* The stitched cipher is AES-NI gated: on a build or CPU without it,
     * the fetch or the key setup fails; handle that gracefully. */
    if (probe == NULL) {
        printf("SKIP test_aes_cbc_hmac_sha1: AES-128-CBC-HMAC-SHA1 is "
               "unavailable\n");
        return 0;
    }
    probe_ctx = EVP_CIPHER_CTX_new();
    if (EVP_EncryptInit_ex(probe_ctx, probe, NULL,
                           (const unsigned char *)
                               "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
                               "\x0b\x0c\x0d\x0e\x0f",
                           NULL) != 1) {
        printf("SKIP test_aes_cbc_hmac_sha1: the stitched cipher cannot be "
               "initialized on this CPU/build\n");
        EVP_CIPHER_CTX_free(probe_ctx);
        EVP_CIPHER_free(probe);
        return 0;
    }
    EVP_CIPHER_CTX_free(probe_ctx);
    EVP_CIPHER_free(probe);
    ERR_clear_error();

    test_kat_and_composition();
    test_composition_and_roundtrip();
    test_tamper();
    test_aad_rules();
    test_tls10_style();

    printf("test_aes_cbc_hmac_sha1: all %u checks passed\n", g_checks);
    return 0;
}
