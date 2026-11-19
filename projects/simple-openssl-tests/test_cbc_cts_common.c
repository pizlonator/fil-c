/*
 * test_cbc_cts_common.c -- shared implementation of the CBC ciphertext
 * stealing (CS1/CS2/CS3) family tests (API in test_cbc_cts_common.h; one
 * binary per cipher, one struct cts_kat row per binary).
 *
 * The CTS variant is selected with the OSSL_CIPHER_PARAM_CTS_MODE ("cts_mode")
 * UTF8 string context parameter at init time ("CS1" the NIST variant, "CS2"
 * and "CS3" the Kerberos5 variant; see providers/implementations/ciphers/
 * cipher_cts.c).  Observed rules, all asserted here:
 *
 *   - a data update shorter than one block (16 bytes) fails, and only ONE
 *     data update is allowed (ossl_cipher_cbc_cts_block_update);
 *   - CS1 and CS3 round-trip; for non-multiple-of-16 lengths CS1 != CS3 and
 *     CS2 == CS3; for multiple-of-16 lengths CS1 == CS2 == plain CBC, while
 *     CS3 swaps the last two blocks (so CS3 != CS1 once there are at least
 *     two blocks);
 *   - a single-block (16-byte) message: all three variants equal plain CBC.
 *
 * Version note: despite CBC-CTS being documented as new in 3.2, the
 * default provider of OpenSSL 3.0.x already provides AES-*-CBC-CTS and
 * CAMELLIA-*-CBC-CTS and honors cts_mode, so none of the family binaries
 * ever skips.
 *
 * Known-answer constants: no published SP 800-38A-addendum vectors are
 * embedded; instead the constants were derived and cross-checked in one of
 * two ways, per cipher (the per-binary provenance notes say which):
 *
 *   - AES-128 (and Camellia-128): derived with a scratch probe against the
 *     authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 *     against the system OpenSSL 3.0.13 (CS1/CS2/CS3 differences included);
 *   - AES-192 / AES-256 / CAMELLIA-192 / CAMELLIA-256: `openssl enc` accepts
 *     CTS cipher names but has no -cts_mode option, so it always runs in the
 *     default CS1 mode and cannot produce CS2/CS3 vectors; these constants
 *     were therefore derived with the scratch EVP probe against pizfix 3.6.5
 *     and cross-checked byte for byte against system OpenSSL 3.0.13, with
 *     the CS1 values additionally re-confirmed through `openssl enc
 *     -<cipher>` on both builds.
 */

#include "test_cbc_cts_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* Shared KAT material (AES-128 and Camellia-128, key = IV = 00..0F).  The
 * constants are arrays (not pointers) so that the per-cipher binaries can
 * use them in static struct cts_kat initializers. */
const char cts_key_128_hex[] =
    "000102030405060708090A0B0C0D0E0F";
const char cts_key_192_hex[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617";
const char cts_key_256_hex[] =
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
const char cts_iv_hex[] =
    "000102030405060708090A0B0C0D0E0F";
/* 54-byte pattern used for the CS1/CS2/CS3 54-byte KATs. */
const char cts_pt54_hex[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "181920212223242526272829303132333435363738393a3b"
    "3c3d3e3f4041";
/* 32-byte (block-aligned) pattern. */
const char cts_pt32_hex[] =
    "000102030405060708090A0B0C0D0E0F"
    "101112131415161718191A1B1C1D1E1F";
/* Single block. */
const char cts_pt16_hex[] =
    "000102030405060708090A0B0C0D0E0F";

/* Fetches the CBC-CTS cipher of a KAT row, CHECK()ing success. */
static EVP_CIPHER *cts_fetch_cipher(const struct cts_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ctsname);
    cipher = EVP_CIPHER_fetch(NULL, kat->ctsname, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot CTS encrypt/decrypt with the given cts_mode.  Returns the output
 * length (== inlen; CTS preserves length). */
size_t cts_crypt(const EVP_CIPHER *cipher, int enc,
                 const char *cts_mode, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    OSSL_PARAM params[2];
    char mode[4];
    int len = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    strncpy(mode, cts_mode, sizeof(mode) - 1);
    mode[sizeof(mode) - 1] = '\0';
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE,
                                                 mode, 0);
    params[1] = OSSL_PARAM_construct_end();
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, params) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, params) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    if (enc)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "CTS encrypt update failed");
    else
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "CTS decrypt update failed");
    CHECK((size_t)len == inlen, "CTS update length is not inlen");
    if (enc)
        CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
              "CTS encrypt final failed");
    else
        CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
              "CTS decrypt final failed");
    CHECK(flen == 0, "CTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (a) The default mode is CS1 and is readable through the same param. */
void cts_test_default_mode_readback(const struct cts_kat *kat)
{
    EVP_CIPHER *cipher = cts_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    char mode[8];
    size_t mode_len = sizeof(mode);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    getp[0] = OSSL_PARAM_construct_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE,
                                               mode, mode_len);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "reading back cts_mode failed");
    CHECK(strcmp(mode, "CS1") == 0, "the default CTS mode is not CS1");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (b) The 54-byte KAT: CS1 differs from CS2/CS3 (which agree), and the CS3
 * KAT decrypts back to the plaintext. */
void cts_test_kat_54(const struct cts_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = cts_fetch_cipher(kat);
    char msg[128];
    size_t ptlen, wantlen, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);
    ptlen = from_hex(cts_pt54_hex, pt, 64);

    ctlen = cts_crypt(cipher, 1, "CS1", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(kat->cs1_54hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS1 ciphertext does not match the derived KAT", kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);

    ctlen = cts_crypt(cipher, 1, "CS2", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(kat->cs2_54hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS2 ciphertext does not match the derived KAT", kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);

    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(kat->cs3_54hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS3 ciphertext does not match the derived KAT", kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);

    /* And the 54-byte CS3 KAT round-trips back to the plaintext. */
    backlen = cts_crypt(cipher, 0, "CS3", key, iv, want, wantlen,
                        back, 64 + 16);
    snprintf(msg, sizeof(msg),
             "%s CS3 decryption of the KAT did not give the plaintext",
             kat->ctsname);
    expect_equal(msg, back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (c) Block-aligned (32-byte) KAT: CS1 = plain CBC, CS3 swaps the last two
 * blocks.  NULL constants skip the section. */
void cts_test_kat_32(const struct cts_kat *kat, const char *cs1_32hex,
                     const char *cs3_32hex)
{
    unsigned char *key, *iv, *pt, *want, *ct;
    EVP_CIPHER *cipher;
    char msg[160];
    size_t ptlen, wantlen, ctlen;

    if (cs1_32hex == NULL || cs3_32hex == NULL)
        return;
    key = ALLOCATE_KEY(32);
    iv = ALLOCATE_IV(16);
    pt = ALLOCATE_INPUT(64);
    want = ALLOCATE_OUTPUT(64);
    ct = ALLOCATE_OUTPUT(64 + 16);
    cipher = cts_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);

    ptlen = from_hex(cts_pt32_hex, pt, 64);
    ctlen = cts_crypt(cipher, 1, "CS1", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(cs1_32hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS1 block-aligned ciphertext does not match the KAT",
             kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);

    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(cs3_32hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS3 block-aligned ciphertext does not match the KAT",
             kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (d) Single-block (16-byte) KAT: CS3 is plain CBC.  NULL skips it. */
void cts_test_kat_16_cs3(const struct cts_kat *kat, const char *cs3_16hex)
{
    unsigned char *key, *iv, *pt, *want, *ct;
    EVP_CIPHER *cipher;
    char msg[160];
    size_t ptlen, wantlen, ctlen;

    if (cs3_16hex == NULL)
        return;
    key = ALLOCATE_KEY(32);
    iv = ALLOCATE_IV(16);
    pt = ALLOCATE_INPUT(64);
    want = ALLOCATE_OUTPUT(64);
    ct = ALLOCATE_OUTPUT(64 + 16);
    cipher = cts_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);

    ptlen = from_hex(cts_pt16_hex, pt, 64);
    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(cs3_16hex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s CS3 single-block ciphertext does not match the KAT",
             kat->ctsname);
    expect_equal(msg, ct, ctlen, want, wantlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (e) Generic single pt/ct KAT in the given cts_mode (the Camellia-128
 * 38-byte CS3 vector).  NULL arguments skip it. */
void cts_test_kat_generic(const struct cts_kat *kat, const char *pthex,
                          const char *cthex, const char *ctsmode)
{
    unsigned char *key, *iv, *pt, *want, *ct;
    EVP_CIPHER *cipher;
    char msg[160];
    size_t ptlen, wantlen, ctlen;

    if (pthex == NULL || cthex == NULL || ctsmode == NULL)
        return;
    key = ALLOCATE_KEY(32);
    iv = ALLOCATE_IV(16);
    pt = ALLOCATE_INPUT(64);
    want = ALLOCATE_OUTPUT(64);
    ct = ALLOCATE_OUTPUT(64 + 16);
    cipher = cts_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);

    ptlen = from_hex(pthex, pt, 64);
    ctlen = cts_crypt(cipher, 1, ctsmode, key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(cthex, want, 64);
    snprintf(msg, sizeof(msg),
             "%s %s ciphertext does not match the derived KAT",
             kat->ctsname, ctsmode);
    expect_equal(msg, ct, ctlen, want, wantlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (f) Length sweep {17,31,32,33,48}: CS1/CS3 round-trip everywhere; the
 * documented CS1/CS2/CS3 equalities and differences; the plain-CBC
 * reference for the aligned lengths comes from kat->cbcname. */
void cts_test_length_sweep(const struct cts_kat *kat)
{
    static const size_t lengths[] = { 17, 31, 32, 33, 48 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *cs1 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cs2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cs3 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cbcref = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = cts_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t l, i, n, cs1len, cs2len, cs3len, backlen;
    char msg[160];

    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 5 + 1);

    for (l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
        n = lengths[l];

        cs1len = cts_crypt(cipher, 1, "CS1", key, iv, pt, n, cs1, 64 + 16);
        cs2len = cts_crypt(cipher, 1, "CS2", key, iv, pt, n, cs2, 64 + 16);
        cs3len = cts_crypt(cipher, 1, "CS3", key, iv, pt, n, cs3, 64 + 16);

        /* CS1 and CS3 round-trip. */
        backlen = cts_crypt(cipher, 0, "CS1", key, iv, cs1, cs1len,
                            back, 64 + 16);
        snprintf(msg, sizeof(msg), "%s CS1 roundtrip of %zu bytes failed",
                 kat->ctsname, n);
        expect_equal(msg, back, backlen, pt, n);
        backlen = cts_crypt(cipher, 0, "CS3", key, iv, cs3, cs3len,
                            back, 64 + 16);
        snprintf(msg, sizeof(msg), "%s CS3 roundtrip of %zu bytes failed",
                 kat->ctsname, n);
        expect_equal(msg, back, backlen, pt, n);

        if (n % 16 != 0) {
            /* Non-aligned: CS1 differs from CS3; CS2 == CS3. */
            unpoison_all();
            CHECK(cs1len == cs3len && memcmp(cs1, cs3, cs1len) != 0,
                  "CS1 equals CS3 for a non-aligned length");
            repoison_all();
            expect_equal("CS2 differs from CS3 for a non-aligned length",
                         cs2, cs2len, cs3, cs3len);
        } else {
            /* Aligned: CS1 == CS2 == plain CBC; CS3 swaps the last two
             * blocks, so it differs once there are at least two blocks.
             * The plain-CBC reference is computed with the CBC cipher of
             * the same key size. */
            EVP_CIPHER *cbcsz = EVP_CIPHER_fetch(NULL, kat->cbcname, NULL);

            CHECK(cbcsz != NULL,
                  "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC cipher");
            expect_equal("CS2 differs from CS1 for an aligned length",
                         cs2, cs2len, cs1, cs1len);
            ctx = EVP_CIPHER_CTX_new();
            CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
            CHECK(EVP_EncryptInit_ex2(ctx, cbcsz, key, iv, NULL) == 1,
                  "EVP_EncryptInit_ex2 failed");
            CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
                  "EVP_CIPHER_CTX_set_padding(0) failed");
            CHECK(EVP_EncryptUpdate(ctx, cbcref, &len, pt, (int)n) == 1,
                  "plain CBC update failed");
            CHECK(EVP_EncryptFinal_ex(ctx, cbcref + len, &flen) == 1,
                  "plain CBC final failed");
            EVP_CIPHER_CTX_free(ctx);
            len = 0;
            flen = 0;
            expect_equal("CS1 differs from plain CBC for an aligned length",
                         cs1, cs1len, cbcref, n);
            if (n >= 32) {
                unpoison_all();
                CHECK(memcmp(cs3, cs1, cs1len) != 0,
                      "CS3 equals CS1 for an aligned length >= 32");
                repoison_all();
            } else {
                expect_equal("CS3 differs from plain CBC for one block",
                             cs3, cs3len, cbcref, n);
            }
            EVP_CIPHER_free(cbcsz);
        }
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (g) A single 16-byte message: CS1 == CS2 == CS3. */
void cts_test_single_block(const struct cts_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *cs1 = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *cs2 = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *cs3 = ALLOCATE_OUTPUT(16 + 16);
    EVP_CIPHER *cipher = cts_fetch_cipher(kat);
    size_t cs1len, cs2len, cs3len;

    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);
    from_hex(cts_pt16_hex, pt, 16);

    cs1len = cts_crypt(cipher, 1, "CS1", key, iv, pt, 16, cs1, 16 + 16);
    cs2len = cts_crypt(cipher, 1, "CS2", key, iv, pt, 16, cs2, 16 + 16);
    cs3len = cts_crypt(cipher, 1, "CS3", key, iv, pt, 16, cs3, 16 + 16);
    CHECK(cs1len == 16 && cs2len == 16 && cs3len == 16,
          "single-block CTS lengths are not 16");
    unpoison_all();
    CHECK(memcmp(cs1, cs2, 16) == 0 && memcmp(cs1, cs3, 16) == 0,
          "the CTS variants disagree on a single-block message");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (h) Update rules: fewer than 16 bytes fails; a second update fails
 * (one-shot only, cipher_cts.c ctx->updated).  Encrypt and decrypt side. */
void cts_test_update_rules(const struct cts_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = cts_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(cts_iv_hex, iv, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 2);

    /* A sub-block update fails. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, 15) != 1,
          "a 15-byte CTS update was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* Exactly one block is fine, but a second update fails. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, 16) == 1,
          "a one-block CTS update failed");
    CHECK(EVP_EncryptUpdate(ctx, out + len, &len, pt + 16, 16) != 1,
          "a second CTS update was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* Same on the decrypt side. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, pt, 16) == 1,
          "a one-block CTS decrypt update failed");
    CHECK(EVP_DecryptUpdate(ctx, out + len, &len, pt + 16, 16) != 1,
          "a second CTS decrypt update was accepted");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
