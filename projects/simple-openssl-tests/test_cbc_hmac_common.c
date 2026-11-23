/*
 * test_cbc_hmac_common.c -- shared implementation of the stitched AES-CBC-
 * HMAC TLS-record cipher family tests (API in test_cbc_hmac_common.h; one
 * binary per cipher, one struct cbhmac_spec row per binary).
 *
 * The record format below was pinned by reading the implementation
 * (providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_hw.c and
 * cipher_aes_cbc_hmac_sha256_hw.c -- the same code the legacy
 * crypto/evp/e_aes_cbc_hmac_sha{1,256}.c files carry) and by byte-level
 * experiments against both OpenSSL builds.  For a TLS 1.1/1.2 record
 * (version bytes >= 0x0302 in the AAD), with L = payload length:
 *
 *   - the EVP key is the AES key (16/32 bytes) and the EVP IV is the
 *     16-byte CBC IV used by encryption; the HMAC key is set separately
 *     with EVP_CTRL_AEAD_SET_MAC_KEY (the TLS layer's mac key);
 *   - the 13-byte AAD is EVP_CTRL_AEAD_TLS1_AAD with the layout
 *     seq(8) || type(1) || version(2) || length(2), where the length field
 *     is the record body length WITHOUT MAC/padding, i.e. 16 + L (the
 *     explicit-IV slot plus the payload).  The setter rewrites it to L
 *     (the MAC input length) in the caller's buffer and returns the pad
 *     amount; that rewritten AAD is exactly what the MAC hashes;
 *   - the record body length fed to the update is
 *         bodylen = 16 + L + MACLEN + padbytes,
 *     with MACLEN = 20 (SHA1) or 32 (SHA256) and
 *     padbytes = 16 - ((L + MACLEN) mod 16), each padding byte equal to
 *     padbytes - 1 (TLS block-cipher padding);
 *   - the update MUST be a single call of exactly bodylen bytes with the
 *     input laid out as [16-byte explicit-IV slot][payload][slack]; the
 *     cipher computes the MAC and padding itself and CBC-encrypts the
 *     WHOLE body (explicit-IV slot included) starting from the EVP IV;
 *   - decryption takes the first 16 bytes of the record as the CBC IV
 *     (overriding the EVP IV), decrypts the rest, verifies MAC and
 *     padding, and fails the update on any mismatch; the payload appears
 *     at output[16 .. 16+L]; the decrypt-side AAD setter returns the MAC
 *     length (20 / 32), which the TLS layer uses to locate the MAC.
 *
 * The KAT is the composition: the expected record is rebuilt with separate
 * EVP_MAC(HMAC-SHA1 / HMAC-SHA256) and AES-128/256-CBC calls following
 * exactly this format and must equal the stitched output byte for byte.
 * One AES-128 record per digest is additionally embedded as a hex constant
 * in its binary (verified against both the pizfix 3.6.5 and the system
 * 3.0.13 builds); the AES-256 rows carry no embedded constant, so for them
 * the composition itself is the KAT.  A TLS 1.0-style record (version
 * 0x0300: no explicit-IV slot, bodylen = floor16(L + MACLEN + 16), CBC over
 * payload||MAC||padding from the EVP IV) is composed and round-tripped as
 * well.
 *
 * The encrypt-then-MAC variants (AES-128/192/256-CBC-HMAC-SHA256-ETM, where
 * the record is IV || ciphertext || MAC over AAD || ciphertext) are only
 * registered on ARM64 builds of OpenSSL 3.5+; on x86 (both 3.0.13 and
 * 3.6.5) the fetch fails, so they take the documented whole-program SKIP
 * path in their binaries.  The ETM composition, for a future build that
 * offers them: MAC over the 13-byte AAD followed by the CIPHERTEXT only,
 * record = explicit-IV || ciphertext || MAC.
 */

#include "test_cbc_hmac_common.h"

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/* Shared KAT material (identical in the two old programs).  The constants
 * are arrays (not pointers) so that the per-cipher binaries can use them
 * in static struct cbhmac_spec initializers. */
const char cbhmac_key_128_hex[] =
    "000102030405060708090A0B0C0D0E0F";
const char cbhmac_key_256_hex[] =
    "000102030405060708090A0B0C0D0E0F"
    "101112131415161718191A1B1C1D1E1F";
const char cbhmac_mackey_hex[] =
    "0FFEEDDCCBBAA9988776655443322110";
const char cbhmac_iv_hex[] =
    "00112233445566778899AABBCCDDEEFF";
const char cbhmac_expiv_hex[] =
    "0405060708090A0B0C0D0E0F10111213";
/* The 5-byte KAT payload (L = 5) shared by every embedded KAT record. */
const char cbhmac_kat_payload_hex[] = "0102030405";

static size_t tls12_body_len(size_t L, size_t maclen)
{
    return 16 + L + maclen + (16 - ((L + maclen) % 16));
}

static size_t tls10_body_len(size_t L, size_t maclen)
{
    /* floor16(L + maclen + 16) == ceil16(L + maclen + 1): the TLS 1.0
     * record body. */
    return ((L + maclen + 16) & ~(size_t)15);
}

/* Computes HMAC-<digest>(mackey, aad13 || payload) via EVP_MAC. */
static void compute_mac(const char *digestname, size_t maclen,
                        const unsigned char *mackey, size_t mackeylen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *payload, size_t payloadlen,
                        unsigned char *mac)
{
    EVP_MAC *hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *mctx;
    OSSL_PARAM params[2];
    char msg[160];
    size_t gotlen = 0;

    CHECK(hmac != NULL, "EVP_MAC_fetch(\"HMAC\") failed");
    mctx = EVP_MAC_CTX_new(hmac);
    CHECK(mctx != NULL, "EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 (char *)digestname, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MAC_CTX_set_params(mctx, params) == 1,
          "EVP_MAC_CTX_set_params failed");
    CHECK(EVP_MAC_init(mctx, mackey, mackeylen, NULL) == 1,
          "EVP_MAC_init failed");
    CHECK(EVP_MAC_update(mctx, aad, aadlen) == 1, "EVP_MAC_update(aad) failed");
    CHECK(EVP_MAC_update(mctx, payload, payloadlen) == 1,
          "EVP_MAC_update(payload) failed");
    CHECK(EVP_MAC_final(mctx, mac, &gotlen, maclen) == 1,
          "EVP_MAC_final failed");
    snprintf(msg, sizeof(msg), "HMAC-%s length is not %zu", digestname,
             maclen);
    CHECK(gotlen == maclen, msg);
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
                                 const struct cbhmac_spec *spec,
                                 const unsigned char *enckey,
                                 const unsigned char *mackey,
                                 size_t mackeylen, const unsigned char *iv,
                                 const unsigned char *expiv,
                                 const unsigned char *payload, size_t L,
                                 unsigned char *record)
{
    unsigned char aad[13];
    unsigned char mac[32];      /* no VLAs: 32 covers both MAC lengths */
    size_t maclen = spec->maclen;
    size_t bodylen, padbytes, i;

    bodylen = tls12_body_len(L, maclen);
    padbytes = bodylen - 16 - L - maclen;

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

    compute_mac(spec->digestname, maclen, mackey, mackeylen, aad, 13,
                payload, L, mac);

    memcpy(record, expiv, 16);
    memcpy(record + 16, payload, L);
    memcpy(record + 16 + L, mac, maclen);
    for (i = 0; i < padbytes; i++)
        record[16 + L + maclen + i] = (unsigned char)(padbytes - 1);

    cbc_encrypt_raw(cipher, enckey, iv, record, bodylen);
}

/* Encrypts a TLS 1.1/1.2 record with the stitched cipher.  Returns the
 * body length; `record` receives it (the first 16 bytes are the caller's
 * explicit-IV slot, which the cipher encrypts along with everything
 * else). */
static size_t stitch_encrypt_tls12(const EVP_CIPHER *cipher,
                                   const struct cbhmac_spec *spec,
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
    size_t maclen = spec->maclen;
    size_t bodylen;

    CHECK(L <= 1024, "payload larger than the scratch buffer");
    bodylen = tls12_body_len(L, maclen);
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
                                 const struct cbhmac_spec *spec,
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
        /* The decrypt-side AAD setter returns the MAC length (verified for
         * both SHA1 = 20 and SHA256 = 32). */
        CHECK(pad == (int)spec->maclen,
              "the decrypt-side AAD pad amount is not the MAC length");
        CHECK(EVP_DecryptUpdate(ctx, record, &len, record, (int)bodylen) == 1,
              "stitched decrypt update failed (MAC or padding invalid?)");
        CHECK((size_t)len == bodylen,
              "the stitched decrypt update did not consume the whole body");
        CHECK(EVP_DecryptFinal_ex(ctx, record + len, &flen) == 1,
              "stitched decrypt final failed");
        EVP_CIPHER_CTX_free(ctx);
    }
}

/* The whole-program gate: the stitched cipher is AES-NI gated, so on a
 * build or CPU without it, the fetch or the key setup fails; handle that
 * gracefully (the two original programs probed exactly these two things in
 * main()). */
void cbhmac_probe_gate(const char *ciphername)
{
    /* The 000102...0f probe key both original programs used for the key
     * setup probe, extended to 32 bytes (the original programs' KEY256
     * pattern) so that the AES-256 variants' 32-byte key read stays in
     * bounds; OpenSSL reads exactly the cipher's key length from here. */
    static const unsigned char probe_key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    EVP_CIPHER_CTX *probe_ctx;
    char msg[160];

    if (probe == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", ciphername);
        test_skip(msg);
    }
    probe_ctx = EVP_CIPHER_CTX_new();
    if (probe_ctx == NULL ||
        EVP_EncryptInit_ex(probe_ctx, probe, NULL, probe_key, NULL) != 1) {
        EVP_CIPHER_CTX_free(probe_ctx);
        EVP_CIPHER_free(probe);
        test_skip("the stitched cipher cannot be initialized on this "
                  "CPU/build");
    }
    EVP_CIPHER_CTX_free(probe_ctx);
    EVP_CIPHER_free(probe);
    ERR_clear_error();
}

/* (1) The embedded KAT record (when the binary carries one) and the
 * composition equality for spec's cipher. */
void cbhmac_test_kat_and_composition(const struct cbhmac_spec *spec)
{
    EVP_CIPHER *cipher;
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(8);
    unsigned char *record_want = ALLOCATE_OUTPUT(80);
    unsigned char *record = ALLOCATE_OUTPUT(80);
    unsigned char *composed = ALLOCATE_OUTPUT(80);
    char msg[160];
    size_t L, bodylen, wantlen = 0;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             spec->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, spec->ciphername, NULL);
    CHECK(cipher != NULL, msg);

    from_hex(spec->keyhex, enckey, 32);
    from_hex(cbhmac_mackey_hex, mackey, 16);
    from_hex(cbhmac_iv_hex, iv, 16);
    from_hex(cbhmac_expiv_hex, expiv, 16);
    from_hex(cbhmac_kat_payload_hex, payload, 8);
    L = 5;

    bodylen = stitch_encrypt_tls12(cipher, spec, enckey, mackey, 16, iv,
                                   expiv, payload, L, record, 80);
    snprintf(msg, sizeof(msg), "the KAT record body length is not %zu",
             spec->kat_bodylen);
    CHECK(bodylen == spec->kat_bodylen, msg);

    if (spec->kat_record_hex != NULL) {
        wantlen = from_hex(spec->kat_record_hex, record_want, 80);
        expect_equal("the stitched record does not match the embedded KAT",
                     record, bodylen, record_want, wantlen);
        compose_tls12_record(cipher, spec, enckey, mackey, 16, iv, expiv,
                             payload, L, composed);
        expect_equal("the composed record differs from the embedded KAT",
                     composed, bodylen, record_want, wantlen);
    } else {
        /* No embedded constant for this cipher: the composition (separate
         * HMAC + CBC rebuild of the record) is the KAT and must equal the
         * stitched record byte for byte. */
        compose_tls12_record(cipher, spec, enckey, mackey, 16, iv, expiv,
                             payload, L, composed);
        expect_equal("the composed record differs from the stitched record",
                     composed, bodylen, record, bodylen);
    }

    /* Decrypt the record: the payload must come back at [16 .. 16+L]. */
    stitch_decrypt_tls12(cipher, spec, enckey, mackey, 16, iv, L, record,
                         bodylen);
    unpoison_all();
    CHECK(memcmp(record + 16, payload, L) == 0,
          "decrypting the KAT record did not recover the payload");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Compose-equals-stitch and roundtrip for several payload sizes on
 * spec's cipher. */
void cbhmac_test_composition_and_roundtrip(const struct cbhmac_spec *spec)
{
    static const size_t sizes[] = { 1, 16, 64, 256, 1024 };
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(1024);
    unsigned char *record = ALLOCATE_OUTPUT(2048);
    unsigned char *composed = ALLOCATE_OUTPUT(2048);
    EVP_CIPHER *cipher;
    char msg[160];
    size_t s, i, L, bodylen;

    snprintf(msg, sizeof(msg),
             "EVP_CIPHER_fetch failed for a core %s cipher",
             spec->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, spec->ciphername, NULL);
    CHECK(cipher != NULL, msg);

    from_hex(spec->keyhex, enckey, 32);
    from_hex(cbhmac_mackey_hex, mackey, 16);
    from_hex(cbhmac_iv_hex, iv, 16);
    from_hex(cbhmac_expiv_hex, expiv, 16);
    for (i = 0; i < 1024; i++)
        payload[i] = (unsigned char)(i * 29 + 7);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        L = sizes[s];
        bodylen = stitch_encrypt_tls12(cipher, spec, enckey, mackey, 16, iv,
                                       expiv, payload, L, record, 2048);
        compose_tls12_record(cipher, spec, enckey, mackey, 16, iv, expiv,
                             payload, L, composed);
        snprintf(msg, sizeof(msg),
                 "%s: the composed record differs from the stitched "
                 "record (L=%zu)",
                 spec->ciphername, unpoisoned(L));
        expect_equal(msg, record, bodylen, composed, bodylen);

        stitch_decrypt_tls12(cipher, spec, enckey, mackey, 16, iv, L,
                             record, bodylen);
        snprintf(msg, sizeof(msg),
                 "%s: decrypting the record did not recover the payload "
                 "(L=%zu)",
                 spec->ciphername, unpoisoned(L));
        unpoison_all();
        CHECK(memcmp(record + 16, payload, L) == 0, msg);
        repoison_all();
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) A tampered record (ciphertext byte flipped) must fail the decrypt
 * update. */
void cbhmac_test_tamper(const struct cbhmac_spec *spec)
{
    EVP_CIPHER *cipher;
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(144);
    unsigned char aad[13];
    EVP_CIPHER_CTX *ctx;
    char msg[160];
    int pad, len = 0;
    size_t L = 64, bodylen, i;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             spec->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, spec->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    from_hex(spec->keyhex, enckey, 32);
    from_hex(cbhmac_mackey_hex, mackey, 16);
    from_hex(cbhmac_iv_hex, iv, 16);
    from_hex(cbhmac_expiv_hex, expiv, 16);
    for (i = 0; i < 64; i++)
        payload[i] = (unsigned char)(i * 3 + 1);

    bodylen = stitch_encrypt_tls12(cipher, spec, enckey, mackey, 16, iv,
                                   expiv, payload, L, record, 144);

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

    free_poison_allocations();
}

/* (4) A wrong AAD length must be rejected by the AAD ctrl; a missing AAD
 * degrades to the documented streaming mode (plain CBC of the raw buffer,
 * no MAC/padding), which differs from the TLS-mode record. */
void cbhmac_test_aad_rules(const struct cbhmac_spec *spec)
{
    EVP_CIPHER *cipher, *cbc;
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *expiv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *raw = ALLOCATE_INPUT(80);
    unsigned char *streamed = ALLOCATE_OUTPUT(128);
    unsigned char aad[13];
    EVP_CIPHER_CTX *ctx;
    char msg[160];
    int pad, len = 0, flen = 0;
    size_t L = 5, bodylen, i;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             spec->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, spec->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             spec->cbcname);
    cbc = EVP_CIPHER_fetch(NULL, spec->cbcname, NULL);
    CHECK(cbc != NULL, msg);
    from_hex(spec->keyhex, enckey, 32);
    from_hex(cbhmac_mackey_hex, mackey, 16);
    from_hex(cbhmac_iv_hex, iv, 16);
    from_hex(cbhmac_expiv_hex, expiv, 16);
    from_hex(cbhmac_kat_payload_hex, payload, 8);
    for (i = 0; i < 80; i++)
        raw[i] = (unsigned char)(i + 1);

    bodylen = tls12_body_len(L, spec->maclen);

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
        stitch_encrypt_tls12(cipher, spec, enckey, mackey, 16, iv, expiv,
                             payload, L, tlsrec, 128);
        unpoison_all();
        CHECK(memcmp(tlsrec, streamed, bodylen) != 0,
              "the streaming fallback equals the TLS-mode record");
        repoison_all();
    }
    EVP_CIPHER_free(cipher);
    EVP_CIPHER_free(cbc);

    free_poison_allocations();
}

/* (5) A TLS 1.0-style record (version 0x0300): no explicit-IV slot, the
 * body is CBC(payload || MAC || padding) from the EVP IV, and the AAD
 * length field is the payload length itself (not rewritten). */
void cbhmac_test_tls10_style(const struct cbhmac_spec *spec)
{
    EVP_CIPHER *cipher;
    unsigned char *enckey = ALLOCATE_KEY(32);
    unsigned char *mackey = ALLOCATE_KEY(16);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *payload = ALLOCATE_INPUT(64);
    unsigned char *record = ALLOCATE_OUTPUT(128);
    unsigned char *composed = ALLOCATE_OUTPUT(128);
    EVP_CIPHER_CTX *ctx;
    unsigned char aad[13];
    unsigned char mac[32];      /* no VLAs: 32 covers both MAC lengths */
    char msg[160];
    int pad, len = 0, flen = 0;
    size_t L = 20, bodylen, padbytes, i;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             spec->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, spec->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    from_hex(spec->keyhex, enckey, 32);
    from_hex(cbhmac_mackey_hex, mackey, 16);
    from_hex(cbhmac_iv_hex, iv, 16);
    for (i = 0; i < 64; i++)
        payload[i] = (unsigned char)(i * 7 + 3);

    bodylen = tls10_body_len(L, spec->maclen);
    padbytes = bodylen - L - spec->maclen;

    /* Compose: MAC over AAD13(length = L) || payload; CBC over
     * payload || MAC || padding. */
    memset(aad, 0, 13);
    aad[7] = 0x01;
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x00; /* TLS 1.0 */
    aad[11] = (unsigned char)(L >> 8);
    aad[12] = (unsigned char)L;
    compute_mac(spec->digestname, spec->maclen, mackey, 16, aad, 13, payload,
                L, mac);

    memcpy(composed, payload, L);
    memcpy(composed + L, mac, spec->maclen);
    for (i = 0; i < padbytes; i++)
        composed[L + spec->maclen + i] = (unsigned char)(padbytes - 1);
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
    unpoison_all();
    CHECK(memcmp(record, payload, L) == 0,
          "the TLS 1.0-style roundtrip did not recover the payload");
    repoison_all();
    EVP_CIPHER_free(cipher);
    ERR_clear_error();

    free_poison_allocations();
}

/* (6) The ETM variants: if a build offers the cipher, at least run an
 * in-place streaming-mode init (no AAD: plain CBC of the raw buffer) to
 * demonstrate the fetch is live; the ETM composition itself is documented
 * in the file comment and left to the build that has the algorithm, since
 * it cannot be validated here. */
void cbhmac_test_etm_streaming_init(const char *ciphername)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    EVP_CIPHER_CTX *ctx;
    unsigned char key[32], iv[16];
    size_t klen, i;
    int rc;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for the ETM cipher");
    klen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < klen && i < sizeof(key); i++)
        key[i] = (unsigned char)(0x60 + i);
    for (i = 0; i < 16; i++)
        iv[i] = (unsigned char)(0x70 + i);
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    rc = EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv);
    CHECK(rc == 1, "the ETM cipher init failed on a build offering it");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}
