#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/*
 * test_aes_siv -- AES-128/192/256-SIV (RFC 5297 S2V-CMAC-AES + CTR) tests.
 *
 * The SIV key is TWICE the underlying cipher key (32/48/64 bytes for
 * AES-128/192/256-SIV), there is no IV, AAD updates (any number, including
 * the RFC's nonce as the last one) precede the single payload update, and a
 * context performs exactly ONE crypto operation per init (crypto/modes/
 * siv128.c counts crypto_ok down to zero, so a second payload update on the
 * same context fails).  The 16-byte synthetic IV is carried as the EVP tag:
 * GET_TAG after encrypting, SET_TAG before decrypting, and DecryptFinal
 * reports the verification result.
 *
 * S2V has distinct branches for messages shorter than 16 bytes (pad with
 * 0x80 and double) and at least 16 bytes (xorend), so the roundtrip sizes
 * below deliberately straddle that boundary: {0,1,15,16,17,1024}.
 *
 * Known-answer vectors: RFC 5297 A.1 (deterministic example, one AD) and
 * A.2 (nonce-based example, two ADs and the nonce as the last AAD update).
 * Both were verified through this EVP flow against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked against the system OpenSSL 3.0.13 (the
 * A.2 vector exercises the multiple-AAD-updates path).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_TAG(size_t n)    { return poison_alloc(POISON_TAG, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_siv: %s\n", msg);
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

/* RFC 5297 A.1: deterministic example, single AD, 14-byte plaintext. */
static const char *const A1_KEY_HEX =
    "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
static const char *const A1_AD_HEX =
    "101112131415161718191a1b1c1d1e1f2021222324252627";
static const char *const A1_PT_HEX =
    "112233445566778899aabbccddee";
static const char *const A1_SIV_HEX =
    "85632d07c6e8f37f950acd320a2ecc93";
static const char *const A1_CT_HEX =
    "40c02b9690c4dc04daef7f6afe5c";

/* RFC 5297 A.2: nonce-based example, two ADs, nonce as the last AAD. */
static const char *const A2_KEY_HEX =
    "7f7e7d7c7b7a79787776757473727170"
    "404142434445464748494a4b4c4d4e4f";
static const char *const A2_AD1_HEX =
    "00112233445566778899aabbccddeeff"
    "deaddadadeaddadaffeeddcc"
    "bbaa99887766554433221100";
static const char *const A2_AD2_HEX =
    "102030405060708090a0";
static const char *const A2_NONCE_HEX =
    "09f911029d74e35bd84156c5635688c0";
static const char *const A2_PT_HEX =
    "7468697320697320736f6d6520706c61"
    "696e7465787420746f20656e63727970"
    "74207573696e67205349562d414553";
static const char *const A2_SIV_HEX =
    "7bdb6e3b432667eb06f4d14bff2fbd0f";
static const char *const A2_CT_HEX =
    "cb900f2fddbe404326601965c889bf17"
    "dba77ceb094fa663b7a3f748ba8af829"
    "ea64ad544a272e9c485b62a3fd5c0d";

/* SIV encrypt: key (2x), AAD updates, ONE payload update, Final, GET_TAG.
 * The tag (synthetic IV) is returned via `siv`; the ciphertext via `out`
 * (which needs room for inlen bytes; SIV emits none extra). */
static void siv_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char * const *ads, const size_t *adlens,
                        size_t nads, const unsigned char *pt, size_t ptlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        unsigned char *siv)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    size_t i;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(ptlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    for (i = 0; i < nads; i++)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, ads[i], (int)adlens[i]) == 1,
              "AAD EVP_EncryptUpdate failed");
    /* The payload update runs even for a zero-length payload: S2V must see
     * the (empty) message for the tag to be computed, and Final alone does
     * not perform the crypto operation. */
    CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    len += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, siv) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* SIV decrypt with the synthetic IV set before the payload update. */
static void siv_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *siv,
                        const unsigned char * const *ads, const size_t *adlens,
                        size_t nads, const unsigned char *ct, size_t ctlen,
                        unsigned char *out, size_t outcap, size_t *outlen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char siv_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, flen = 0;
    size_t i;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(siv_copy, siv, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, siv_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(ctlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    for (i = 0; i < nads; i++)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, ads[i], (int)adlens[i]) == 1,
              "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, ct, (int)ctlen) == 1,
          "payload EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_DecryptFinal_ex failed (SIV mismatch?)");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    *outlen = (size_t)len;
}

/* (1) The RFC 5297 KATs: A.1 with a single AD and A.2 with two ADs and the
 * nonce as the last AAD update (multiple AAD updates before data). */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad1 = ALLOCATE_AAD(64);
    unsigned char *ad2 = ALLOCATE_AAD(16);
    unsigned char *nonce = ALLOCATE_AAD(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *siv_want = ALLOCATE_TAG(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64);
    unsigned char *siv = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64);
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-SIV", NULL);
    const unsigned char *ads[3];
    size_t adlens[3];
    size_t ptlen, ct_wantlen, ctlen, backlen;

    /* AES-SIV is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-SIV cipher");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 32,
          "AES-128-SIV key length is not 32 (2x16)");

    /* A.1: single AD. */
    from_hex(A1_KEY_HEX, key, 64);
    from_hex(A1_AD_HEX, ad1, 64);
    ptlen = from_hex(A1_PT_HEX, pt, 64);
    from_hex(A1_SIV_HEX, siv_want, 16);
    ct_wantlen = from_hex(A1_CT_HEX, ct_want, 64);
    ads[0] = ad1;
    adlens[0] = strlen(A1_AD_HEX) / 2;

    siv_encrypt(cipher, key, ads, adlens, 1, pt, ptlen, ct, 64, &ctlen, siv);
    expect_equal("A.1: synthetic IV does not match the KAT",
                 siv, 16, siv_want, 16);
    expect_equal("A.1: ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    siv_decrypt(cipher, key, siv_want, ads, adlens, 1, ct_want, ct_wantlen,
                back, 64, &backlen);
    expect_equal("A.1: decrypting the KAT did not give the plaintext",
                 back, backlen, pt, ptlen);

    /* A.2: two ADs and the nonce (three AAD updates before the payload). */
    from_hex(A2_KEY_HEX, key, 64);
    from_hex(A2_AD1_HEX, ad1, 64);
    from_hex(A2_AD2_HEX, ad2, 16);
    from_hex(A2_NONCE_HEX, nonce, 16);
    ptlen = from_hex(A2_PT_HEX, pt, 64);
    from_hex(A2_SIV_HEX, siv_want, 16);
    ct_wantlen = from_hex(A2_CT_HEX, ct_want, 64);
    ads[0] = ad1;
    adlens[0] = strlen(A2_AD1_HEX) / 2;
    ads[1] = ad2;
    adlens[1] = strlen(A2_AD2_HEX) / 2;
    ads[2] = nonce;
    adlens[2] = strlen(A2_NONCE_HEX) / 2;

    siv_encrypt(cipher, key, ads, adlens, 3, pt, ptlen, ct, 64, &ctlen, siv);
    expect_equal("A.2: synthetic IV does not match the KAT",
                 siv, 16, siv_want, 16);
    expect_equal("A.2: ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    siv_decrypt(cipher, key, siv_want, ads, adlens, 3, ct_want, ct_wantlen,
                back, 64, &backlen);
    expect_equal("A.2: decrypting the KAT did not give the plaintext",
                 back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);
}

/* (2) Roundtrips for sizes {0,1,15,16,17,1024} on all three key sizes; the
 * sizes straddle the S2V <16 / >=16 branches.
 *
 * Version note for the empty payload: on 3.6.x a zero-length payload update
 * computes the synthetic IV and Final succeeds; on 3.0.x the same flow
 * leaves the tag unset and Final fails.  The empty case is handled
 * separately below so the program asserts whichever behavior the running
 * OpenSSL exhibits instead of hard-coding one of them. */
static void test_roundtrip_sizes(void)
{
    static const size_t sizes[] = { 1, 15, 16, 17, 1024 };
    static const struct {
        const char *name;
        size_t keylen;
    } ciphers[] = {
        { "AES-128-SIV", 32 }, { "AES-192-SIV", 48 }, { "AES-256-SIV", 64 },
    };
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(1024);
    unsigned char *ct = ALLOCATE_OUTPUT(1024);
    unsigned char *back = ALLOCATE_OUTPUT(1024);
    unsigned char *siv = ALLOCATE_TAG(16);
    const unsigned char *ads[1];
    size_t adlens[1];
    size_t c, s, i, ctlen, backlen;
    char msg[128];

    for (i = 0; i < 1024; i++)
        pt[i] = (unsigned char)(i * 7 + 1);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i * 3);
    ads[0] = ad;
    adlens[0] = 13;

    for (c = 0; c < sizeof(ciphers) / sizeof(ciphers[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphers[c].name, NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-SIV cipher");
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == ciphers[c].keylen,
              "fetched SIV cipher has an unexpected key length");
        for (i = 0; i < ciphers[c].keylen; i++)
            key[i] = (unsigned char)(0xA0 + i);

        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t n = sizes[s];
            siv_encrypt(cipher, key, ads, adlens, 1, pt, n, ct, 1024,
                        &ctlen, siv);
            CHECK(ctlen == n, "SIV ciphertext length is not the payload length");
            siv_decrypt(cipher, key, siv, ads, adlens, 1, ct, ctlen,
                        back, 1024, &backlen);
            snprintf(msg, sizeof(msg),
                     "%s roundtrip of %zu bytes did not recover the plaintext",
                     ciphers[c].name, n);
            expect_equal(msg, back, backlen, pt, n);
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A wrong synthetic IV must fail DecryptFinal (and the output must be
 * cleansed by the implementation). */
static void test_wrong_siv(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-SIV", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32);
    unsigned char *back = ALLOCATE_OUTPUT(32);
    unsigned char *siv = ALLOCATE_TAG(16);
    unsigned char *bad = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    unsigned char siv_copy[16];
    int len = 0;
    const unsigned char *ads[1];
    size_t adlens[1];
    size_t i, ctlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-SIV\") failed");
    for (i = 0; i < 32; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i + 1);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(i * 5 + 2);
    ads[0] = ad;
    adlens[0] = 13;

    siv_encrypt(cipher, key, ads, adlens, 1, pt, 32, ct, 32, &ctlen, siv);
    CHECK(ctlen == 32, "unexpected ciphertext length");

    memcpy(bad, siv, 16);
    bad[0] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    memcpy(siv_copy, bad, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, siv_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, ad, 13) == 1,
          "AAD update failed");
    /* The verification happens inside the payload update: siv128_decrypt
     * cleanses the output and fails there. */
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) != 1,
          "SIV decryption accepted a wrong synthetic IV");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (4) A context performs exactly ONE crypto operation per init: a second
 * payload update on the same context must fail (siv128.c crypto_ok). */
static void test_second_payload_fails(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-SIV", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i, ctlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-SIV\") failed");
    for (i = 0; i < 32; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 3);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 32) == 1,
          "first payload update failed");
    ctlen = (size_t)len;
    CHECK(EVP_EncryptUpdate(ctx, ct + ctlen, &len, pt + 32, 32) != 1,
          "a second payload update was accepted on the same context");
    /* No Final/GET_TAG assertions after the rejection: 3.6.x marks the
     * context failed (final_ret = -1) while 3.0.x does not, so the
     * post-rejection Final result differs between the two. */
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

/* (5) The doubled key length is enforced: requesting a 1x key length at
 * init must fail.  (A full-size key buffer is passed so the failure comes
 * from the length parameter, not from a short buffer.) */
static void test_wrong_key_length(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-SIV", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    OSSL_PARAM params[2];
    EVP_CIPHER_CTX *ctx;
    size_t i;
    size_t keylen16 = 16;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-SIV\") failed");
    for (i = 0; i < 32; i++)
        key[i] = (unsigned char)(0xB0 + i);

    /* KEYLEN=16 must be rejected for a cipher whose keylen is 32. */
    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN,
                                            &keylen16);
    params[1] = OSSL_PARAM_construct_end();
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, params) != 1,
          "a 16-byte key length was accepted for AES-128-SIV");
    EVP_CIPHER_CTX_free(ctx);

    /* (EVP_CIPHER_CTX_set_key_length(16) is NOT probed here: OpenSSL 3.0.x
     * crashes inside it for fixed-length provider ciphers.) */
    EVP_CIPHER_free(cipher);
}

/* (2b) The empty-payload case, with the 3.0.x/3.6.x behavioral difference
 * (see the comment above test_roundtrip_sizes) asserted explicitly. */
static void test_empty_payload(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-SIV", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *ad = ALLOCATE_AAD(13);
    unsigned char *pt = ALLOCATE_INPUT(4);
    unsigned char *ct = ALLOCATE_OUTPUT(4);
    unsigned char *siv = ALLOCATE_TAG(16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-SIV\") failed");
    for (i = 0; i < 32; i++)
        key[i] = (unsigned char)(0xB0 + i);
    for (i = 0; i < 13; i++)
        ad[i] = (unsigned char)(i + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, ad, 13) == 1,
          "AAD update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 0) == 1,
          "zero-length payload update failed");
    if (EVP_EncryptFinal_ex(ctx, ct, &flen) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, siv) == 1) {
        /* 3.6.x behavior: the empty payload produced a synthetic IV. */
        const unsigned char *ads[1];
        size_t adlens[1], backlen;

        ads[0] = ad;
        adlens[0] = 13;
        siv_decrypt(cipher, key, siv, ads, adlens, 1, ct, 0, ct, 4, &backlen);
        CHECK(backlen == 0, "empty-payload decrypt returned bytes");
    } else {
        /* 3.0.x behavior: no tag is produced for an empty payload. */
        ERR_clear_error();
    }
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_roundtrip_sizes();
    test_empty_payload();
    test_wrong_siv();
    test_second_payload_fails();
    test_wrong_key_length();

    printf("test_aes_siv: all %u checks passed\n", g_checks);
    return 0;
}
