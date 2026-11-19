/*

 * test_aes_wrap_pad_common.c -- shared implementation of the AES key wrap
 * with padding family tests (API in test_aes_wrap_pad_common.h; one binary
 * per cipher).

 * RFC 5649 pads the key data with zeros to the next 64-bit boundary and
 * prepends a 4-byte fixed field (A65959A6) plus the 32-bit plaintext length
 * as the "alternative initial value"; a plaintext shorter than 8 bytes takes
 * the single-block AIV path (crypto/modes/wrap128.c: "If the padded
 * plaintext contains exactly eight octets, then prepend the AIV and encrypt
 * the resulting 128-bit block using AES in ECB mode"), longer ones go
 * through the RFC 3394 wrap.  The wrapped length is always roundup8(n) + 8,
 * which the binaries assert exactly for every size (and for the -INV
 * variants too: they apply the inverse AES block transform per SP 800-38F
 * section 5.1 while keeping the same block structure, so the output sizes
 * are identical).  Where a build does not offer a -INV cipher, its binary
 * SKIPs (that is the whole program's subject).
 *
 * Known-answer vectors: RFC 5649 section 6 publishes a single worked example
 * (a 192-bit KEK wrapping 20 and 7 octets of key data), carried by the
 * AES-192-WRAP-PAD binary.  The AES-256-WRAP-PAD and the AES-192/256-WRAP-
 * PAD-INV constants have no published counterpart and were derived as
 * follows: with a scratch probe against the authoritative pizfix OpenSSL
 * 3.6.5 and cross-checked byte for byte against the system OpenSSL 3.0.13;
 * the AES-256-WRAP-PAD and AES-192-WRAP-PAD-INV constants were additionally
 * reproduced with the pizfix `openssl enc` CLI (which does accept the
 * wrap-pad cipher names).
 */

#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/* Shared KAT material (see the header comment).  The constants are arrays
 * (not pointers). */
const char wrap_pad_k20_hex[] =
    "c37b7e6492584340bed12207808941155068f738";
const char wrap_pad_kek192_hex[] =
    "5840df6e29b02af1ab493b705bf16ea1ae8338f4dcc176a8";
const char wrap_pad_kek256_hex[] =
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";

/* Wrap-pad `inlen` bytes in one update; returns the wrapped length. */
size_t wrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
                const unsigned char *in, size_t inlen,
                unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    size_t expect = ((inlen + 7) / 8) * 8 + 8;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(expect + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "wrap-pad EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap-pad EVP_EncryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Unwrap-pad `inlen` bytes in one update; returns the key data length. */
size_t unwrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
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
          "unwrap-pad EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "unwrap-pad EVP_DecryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) A single wrap-pad KAT, wrap and unwrap directions. */
void wrap_pad_test_kat(const char *ciphername, const char *kekhex,
                       const char *kdatahex, const char *wraphex,
                       const char *source)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *kdata = ALLOCATE_INPUT(32);
    unsigned char *wrap_want = ALLOCATE_OUTPUT(64);
    unsigned char *wrap = ALLOCATE_OUTPUT(64);
    unsigned char *back = ALLOCATE_OUTPUT(64);
    EVP_CIPHER *cipher;
    size_t keklen, kdatalen, wrap_wantlen, wlen, backlen, expect;
    char msg[160];

    /* AES key wrap with padding is core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    keklen = from_hex(kekhex, kek, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keklen,
          "fetched WRAP-PAD cipher has an unexpected key length");
    kdatalen = from_hex(kdatahex, kdata, 32);
    wrap_wantlen = from_hex(wraphex, wrap_want, 64);
    expect = ((kdatalen + 7) / 8) * 8 + 8;

    wlen = wrap_pad(cipher, kek, kdata, kdatalen, wrap, expect + 8);
    snprintf(msg, sizeof(msg), "wrapping %zu octets did not give %zu bytes",
             kdatalen, expect);
    CHECK(wlen == expect, msg);
    snprintf(msg, sizeof(msg),
             "%zu-octet wrapped output does not match the %s KAT",
             kdatalen, source);
    expect_equal(msg, wrap, wlen, wrap_want, wrap_wantlen);

    backlen = unwrap_pad(cipher, kek, wrap_want, wrap_wantlen, back,
                         expect + 8);
    snprintf(msg, sizeof(msg),
             "unwrapping the %zu-octet %s KAT did not give the key data",
             kdatalen, source);
    expect_equal(msg, back, backlen, kdata, kdatalen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) Plaintext sizes {1,4,7,8,13,20,255} round-trip and the wrapped
 * length is exactly roundup8(n) + 8. */
void wrap_pad_test_size_roundtrips(const char *ciphername)
{
    static const size_t sizes[] = { 1, 4, 7, 8, 13, 20, 255 };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(255);
    unsigned char *out = ALLOCATE_OUTPUT(264 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(264 + 8);
    EVP_CIPHER *cipher;
    size_t s, i, keklen, wlen, backlen;
    char msg[128];

    for (i = 0; i < 255; i++)
        in[i] = (unsigned char)(i * 17 + 3);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0xD0 + i);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        size_t expect = ((n + 7) / 8) * 8 + 8;

        wlen = wrap_pad(cipher, kek, in, n, out, 264 + 8);
        snprintf(msg, sizeof(msg),
                 "wrapping %zu octets did not give %zu bytes", n, expect);
        CHECK(wlen == expect, msg);
        backlen = unwrap_pad(cipher, kek, out, wlen, back, 264 + 8);
        snprintf(msg, sizeof(msg),
                 "%s %zu-octet wrap/unwrap roundtrip failed", ciphername, n);
        expect_equal(msg, back, backlen, in, n);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) A corrupted wrapped output must fail to unwrap-pad, whether the
 * corruption hits the AIV block or the padding block: 13 octets wrap to 24
 * bytes; corrupt the AIV block, the middle and the last (padding) byte in
 * turn. */
void wrap_pad_test_corrupted_unwrap(const char *ciphername)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(13);
    unsigned char *out = ALLOCATE_OUTPUT(16 + 16 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16 + 8);
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *e, *ctx;
    int len = 0, flen = 0;
    size_t i, keklen, off;
    char msg[128];

    for (i = 0; i < 13; i++)
        in[i] = (unsigned char)(i * 3 + 9);

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0xD0 + i);

    for (i = 0; i < 3; i++) {
        off = (i == 0) ? 0 : (i == 1) ? 12 : 23;

        e = EVP_CIPHER_CTX_new();
        CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(e, cipher, NULL, kek, NULL) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(e, out, &len, in, 13) == 1,
              "wrap-pad update failed");
        CHECK(EVP_EncryptFinal_ex(e, out + len, &flen) == 1,
              "wrap-pad final failed");
        EVP_CIPHER_CTX_free(e);
        len = 0;
        flen = 0;

        out[off] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
              "unwrap-pad accepted corrupted key material");
        EVP_CIPHER_CTX_free(ctx);
        out[off] ^= 0x01;
        len = 0;
        flen = 0;
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) Feeding a plain RFC 3394 wrapped key (no AIV) to unwrap-pad must
 * fail: the recovered initial value will not be A65959A6 || len. */
void wrap_pad_test_plain_wrap_rejected(const char *wrapname,
                                       const char *wpadname)
{
    EVP_CIPHER *wrap = EVP_CIPHER_fetch(NULL, wrapname, NULL);
    EVP_CIPHER *wpad = EVP_CIPHER_fetch(NULL, wpadname, NULL);
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(24 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i, keklen;

    CHECK(wrap != NULL && wpad != NULL,
          "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
    keklen = (size_t)EVP_CIPHER_get_key_length(wpad);
    for (i = 0; i < keklen; i++)
        kek[i] = (unsigned char)(0xD0 + i);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, wrap, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "plain wrap update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "plain wrap final failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(len == 24, "plain wrap of 16 bytes did not give 24 bytes");

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, wpad, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
          "unwrap-pad accepted a plain wrapped key (no AIV)");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(wrap);
    EVP_CIPHER_free(wpad);

    free_poison_allocations();
}

/* (5) The -INV variant's checks (see the header comment).  Whole-program
 * gate: the -INV cipher is this binary's subject, so an unbuildable fetch
 * SKIPs the program. */
void wrap_pad_test_inv(const char *ciphername, const char *kekhex,
                       const char *kathex)
{
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *k20 = ALLOCATE_INPUT(20);
    unsigned char *in = ALLOCATE_INPUT(13);
    unsigned char *out = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i, keklen, wlen, backlen, k20_wantlen;
    char msg[128];

    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    if (cipher == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", ciphername);
        test_skip(msg);
    }
    from_hex(wrap_pad_k20_hex, k20, 20);
    for (i = 0; i < 13; i++)
        in[i] = (unsigned char)(i * 5 + 1);

    if (kekhex != NULL)
        from_hex(kekhex, kek, 32);
    else {
        keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keklen; i++)
            kek[i] = (unsigned char)(0xD0 + i);
    }

    /* KAT: wrapping the 20 RFC 5649 key-data octets gives the derived
     * constant and unwraps back to K20. */
    if (kathex != NULL) {
        k20_wantlen = from_hex(kathex, out, 32);
        wlen = wrap_pad(cipher, kek, k20, 20, back, 32 + 16);
        CHECK(wlen == 32, "inverse wrap-pad of 20 octets did not give "
              "32 bytes");
        expect_equal("inverse wrap-pad output does not match the "
                     "derived KAT", back, wlen, out, k20_wantlen);
        backlen = unwrap_pad(cipher, kek, out, k20_wantlen, back,
                             32 + 16);
        expect_equal("unwrapping the derived -INV KAT did not give the "
                     "key data", back, backlen, k20, 20);
    }

    /* 13 octets wrap to exactly 24 bytes and round-trip. */
    wlen = wrap_pad(cipher, kek, in, 13, out, 32 + 16);
    CHECK(wlen == 24, "inverse wrap-pad of 13 octets did not give 24 bytes");
    backlen = unwrap_pad(cipher, kek, out, wlen, back, 32 + 16);
    expect_equal("inverse wrap-pad roundtrip failed", back, backlen,
                 in, 13);

    /* Corrupting the last (padding/length) byte must fail the unwrap. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 13) == 1,
          "wrap-pad update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap-pad final failed");
    EVP_CIPHER_CTX_free(ctx);
    len = 0;
    flen = 0;
    out[23] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
          "inverse unwrap-pad accepted corrupted key material");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
