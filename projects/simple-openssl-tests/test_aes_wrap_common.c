/*

 * test_aes_wrap_common.c -- shared implementation of the AES key wrap
 * family tests (API in test_aes_wrap_common.h; one binary per cipher).

 * The wrapped input must be a multiple of 8 bytes and at least 16 bytes
 * (crypto/modes/wrap128.c: "0 if inlen does not consist of n 64-bit blocks,
 * n >= 2"), so 15-byte and 17-byte payloads are rejected while a 16-byte
 * one wraps.  Wrapping appends exactly one 64-bit integrity block; the
 * default A6A6A6A6A6A6A6A6 initial value is used (no IV is set).
 *
 * Known-answer vectors: RFC 3394 sections 4.1 (128-bit KEK), 4.2 (192-bit
 * KEK) and 4.3 (256-bit KEK), each wrapping the same 128-bit key data; the
 * shared CEK lives below and each plain-WRAP binary carries its own row.
 * All three rows were verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13, in both the wrap and unwrap directions.
 *
 * The inverse variants (AES-128/192/256-WRAP-INV) share the RFC 3394 block
 * structure with the wrap and unwrap primitives exchanged (SP 800-38F
 * section 5.1: the -INV ciphers apply the inverse AES block transform), so
 * only self-consistency and the same length rules are asserted; the RFC
 * 3394 KATs do not cover them.  Where a build does not offer a -INV cipher,
 * its binary SKIPs (that is the whole program's subject).
 */

#include "test_aes_wrap_common.h"

#include <openssl/evp.h>

/* RFC 3394 section 4 test vectors: the CEK is the same for all three rows;
 * each plain-WRAP binary carries its own KEK and wrapped-output row.  The
 * constant is an array (not a pointer). */
const char wrap_cek_hex[] = "00112233445566778899AABBCCDDEEFF";

/* Wrap `inlen` bytes in one update; returns the wrapped length (inlen+8). */
size_t wrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                const unsigned char *in, size_t inlen,
                unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "wrap EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap EVP_EncryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Unwrap `inlen` bytes in one update; returns the key data length. */
size_t unwrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                  const unsigned char *in, size_t inlen,
                  unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "unwrap EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "unwrap EVP_DecryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) The row's RFC 3394 KAT, wrap and unwrap directions. */
void wrap_test_kat(const struct wrap_kat *kat)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *cek = ALLOCATE_INPUT(16);
    unsigned char *wrap_want = ALLOCATE_OUTPUT(24);
    unsigned char *wrap = ALLOCATE_OUTPUT(24 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 8);
    EVP_CIPHER *cipher;
    size_t keklen, wrap_wantlen, wlen, backlen;

    /* AES key wrap is core OpenSSL 3 functionality: never SKIP, only
     * FAIL. */
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
    keklen = from_hex(kat->kekhex, kek, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keklen,
          "fetched WRAP cipher has an unexpected key length");
    from_hex(wrap_cek_hex, cek, 16);
    wrap_wantlen = from_hex(kat->wraphex, wrap_want, 24);

    wlen = wrap_key(cipher, kek, cek, 16, wrap, 24 + 8);
    CHECK(wlen == 24, "wrapping a 16-byte CEK did not give 24 bytes");
    expect_equal("wrapped output does not match the RFC 3394 KAT",
                 wrap, wlen, wrap_want, wrap_wantlen);

    backlen = unwrap_key(cipher, kek, wrap_want, wrap_wantlen, back, 24 + 8);
    expect_equal("unwrapping the RFC 3394 KAT did not give the CEK",
                 back, backlen, cek, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Length rules: 15 bytes fail, 16 bytes wrap, 17 bytes fail. */
void wrap_test_length_rules(const char *ciphername)
{
    EVP_CIPHER *cipher;
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t i, keklen, wlen, backlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0xA0 + i);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    /* 15 bytes must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 15) != 1,
          "wrapping 15 bytes was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 17 bytes must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 17) != 1,
          "wrapping 17 bytes was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 16 bytes wrap and round-trip. */
    wlen = wrap_key(cipher, kek, in, 16, out, 64 + 16);
    CHECK(wlen == 24, "wrapping 16 bytes did not give 24 bytes");
    backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
    expect_equal("16-byte wrap/unwrap roundtrip failed", back, backlen, in, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) A 64-byte payload round-trips (wrapping appends exactly one 64-bit
 * integrity block: 64 -> 72). */
void wrap_test_64_byte_roundtrip(const char *ciphername)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher;
    size_t i, keklen, wlen, backlen;
    char msg[128];

    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 13 + 5);

    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0x10 + i);
    wlen = wrap_key(cipher, kek, in, 64, out, 64 + 16);
    CHECK(wlen == 72, "wrapping 64 bytes did not give 72 bytes");
    backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
    snprintf(msg, sizeof(msg), "%s 64-byte roundtrip failed", ciphername);
    expect_equal(msg, back, backlen, in, 64);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) A corrupted wrapped key must fail to unwrap: the RFC 3394 integrity
 * check (the recovered A6A6A6A6A6A6A6A6 initial value) rejects it.
 * Corrupt one byte in the first block (the integrity register), one in the
 * middle and the last byte of the wrapped output, in turn. */
void wrap_test_corrupted_unwrap(const char *ciphername)
{
    EVP_CIPHER *cipher;
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(24 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    EVP_CIPHER_CTX *e, *ctx;
    int len = 0, flen = 0;
    size_t i, keklen, off;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0xA0 + i);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    for (i = 0; i < 3; i++) {
        off = (i == 0) ? 0 : (i == 1) ? 12 : 23;

        e = EVP_CIPHER_CTX_new();
        CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(e, cipher, NULL, kek, NULL) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(e, out, &len, in, 16) == 1,
              "wrap update failed");
        CHECK(EVP_EncryptFinal_ex(e, out + len, &flen) == 1,
              "wrap final failed");
        EVP_CIPHER_CTX_free(e);
        len = 0;
        flen = 0;

        out[off] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
              "unwrap accepted corrupted key material");
        EVP_CIPHER_CTX_free(ctx);
        out[off] ^= 0x01;
        len = 0;
        flen = 0;
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) The -INV variant's roundtrips: 16 bytes wrap to exactly 24 bytes and
 * 64 bytes to exactly 72 bytes, both round-tripping.  Whole-program gate:
 * the -INV cipher is this binary's subject, so an unbuildable fetch SKIPs
 * the program. */
void wrap_test_inv_roundtrips(const char *ciphername)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher;
    size_t i, keklen, wlen, backlen;
    char msg[128];

    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    if (cipher == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", ciphername);
        test_skip(msg);
    }
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0x30 + i);
    for (i = 0; i < 64; i++)
        in[i] = (unsigned char)(i * 11 + 9);

    /* 16-byte payload: wraps to exactly 24 bytes and round-trips (the
     * same length rule for every key size). */
    wlen = wrap_key(cipher, kek, in, 16, out, 64 + 16);
    CHECK(wlen == 24, "inverse wrap of 16 bytes did not give 24 bytes");
    backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
    expect_equal("16-byte inverse wrap/unwrap roundtrip failed",
                 back, backlen, in, 16);
    wlen = wrap_key(cipher, kek, in, 64, out, 64 + 16);
    CHECK(wlen == 72, "inverse wrap of 64 bytes did not give 72 bytes");
    backlen = unwrap_key(cipher, kek, out, wlen, back, 64 + 16);
    snprintf(msg, sizeof(msg), "%s 64-byte roundtrip failed", ciphername);
    expect_equal(msg, back, backlen, in, 64);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
